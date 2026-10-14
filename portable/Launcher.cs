using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using Microsoft.Win32;

internal static class PortableProgram {
    [STAThread]
    static void Main(string[] args) {
        if(args.Contains("--ble-resource")) { Program.Main([]).GetAwaiter().GetResult(); return; }
        if(args.Contains("--probe")) { Program.Main(["--probe"]).GetAwaiter().GetResult(); return; }
        if(args.Contains("--version")) return;
        // One bridge per Windows logon session. BLE resource subprocesses bypass this lock.
        using var mutex=new Mutex(true,@"Local\Quotamon.Portable",out var owner);
        if(!owner) {
            try { using var request=EventWaitHandle.OpenExisting(@"Local\Quotamon.OpenSettings");request.Set(); } catch {}
            return;
        }
        ApplicationConfiguration.Initialize();
        using var context=new TrayContext(args.Contains("--minimized"),args.Contains("--headless"));
        Application.Run(context);
    }
}

internal sealed class TrayContext : ApplicationContext {
    const string RunKey=@"Software\Microsoft\Windows\CurrentVersion\Run";
    readonly NotifyIcon? tray;
    readonly Icon? brandIcon;
    readonly Control dispatcher=new();
    readonly Process backend=new();
    readonly ProcessJob job=new();
    readonly bool headless;
    readonly bool minimized;
    readonly EventWaitHandle openRequest=new(false,EventResetMode.AutoReset,@"Local\Quotamon.OpenSettings");
    RegisteredWaitHandle? openWait;
    bool stopping;
    string? url;
    public TrayContext(bool minimized,bool headless) {
        this.minimized=minimized; this.headless=headless;
        dispatcher.CreateControl();
        openWait=ThreadPool.RegisterWaitForSingleObject(openRequest,(_,_)=> {
            if(!stopping&&!headless) dispatcher.BeginInvoke(()=>OpenSettings());
        },null,-1,false);
        if(!headless) {
            brandIcon=CreateBrandIcon();
            tray=new NotifyIcon {Text="Quotamon · 시작 중",Icon=brandIcon,Visible=true};
            var menu=new ContextMenuStrip();
            menu.Items.Add("설정 열기",null,(_,_)=>OpenSettings());
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add("종료",null,(_,_)=>ExitThread());
            tray.ContextMenuStrip=menu; tray.DoubleClick+=(_,_)=>OpenSettings();
        }
        var root=AppContext.BaseDirectory;
        var node=Path.Combine(root,"node.exe");
        var script=Path.Combine(root,"bridge.cjs");
        if(!File.Exists(node)||!File.Exists(script)) { Fail("압축 파일을 모두 풀어 같은 폴더에서 실행하세요."); return; }
        backend.StartInfo=new ProcessStartInfo(node) {
            UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=root,
            RedirectStandardInput=true,RedirectStandardOutput=true,RedirectStandardError=true,
            StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8
        };
        backend.StartInfo.ArgumentList.Add(script);
        backend.StartInfo.ArgumentList.Add("--root"); backend.StartInfo.ArgumentList.Add(root);
        backend.StartInfo.ArgumentList.Add("--helper"); backend.StartInfo.ArgumentList.Add(Environment.ProcessPath!);
        if(StartupEnabled()) backend.StartInfo.ArgumentList.Add("--startup-enabled");
        backend.OutputDataReceived+=(_,e)=> {
            if(e.Data==null||e.Data.Length>8192) return;
            try {
                using var doc=JsonDocument.Parse(e.Data);
                var value=doc.RootElement;
                if(!value.TryGetProperty("type",out var kind)) return;
                switch(kind.GetString()) {
                    case "ready":
                        var candidate=value.GetProperty("url").GetString();
                        if(!Uri.TryCreate(candidate,UriKind.Absolute,out var uri)||uri.Scheme!="http"||uri.Host!="127.0.0.1") return;
                        dispatcher.BeginInvoke(()=> {url=candidate;if(tray!=null) tray.Text="Quotamon · 설정 열기";if(!minimized&&!headless) OpenSettings();});
                        break;
                    case "startup":
                        var id=value.GetProperty("id").GetString();var enabled=value.GetProperty("enabled").GetBoolean();
                        var ok=SetStartup(enabled);
                        backend.StandardInput.WriteLine(JsonSerializer.Serialize(new{id,ok}));backend.StandardInput.Flush();
                        break;
                    case "error":
                        dispatcher.BeginInvoke(()=>Fail("중계를 시작하지 못했습니다. 설정 파일과 포터블 폴더를 확인하세요."));break;
                    case "status":
                        dispatcher.BeginInvoke(()=> { if(tray!=null) tray.Text="Quotamon · 설정 저장 확인 필요"; });break;
                }
            } catch { /* Never display raw child output or account data. */ }
        };
        backend.ErrorDataReceived+=(_,_)=>{};
        backend.EnableRaisingEvents=true;
        backend.Exited+=(_,_)=> {
            if(!stopping) dispatcher.BeginInvoke(()=>Fail("중계가 종료되었습니다. Quotamon을 다시 실행하세요."));
        };
        try {
            backend.Start();job.Attach(backend);
            backend.BeginOutputReadLine();backend.BeginErrorReadLine();
        } catch { Fail("포터블 런타임을 시작하지 못했습니다."); }
    }
    static bool StartupEnabled() {
        using var key=Registry.CurrentUser.OpenSubKey(RunKey);
        return key?.GetValue("Quotamon") is string command &&
            command.Contains(Environment.ProcessPath!,StringComparison.OrdinalIgnoreCase);
    }
    static Icon CreateBrandIcon() {
        using var bitmap=new Bitmap(48,48);
        using var graphics=Graphics.FromImage(bitmap);
        graphics.Clear(Color.FromArgb(23,26,32));
        graphics.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        using var pen=new Pen(Color.FromArgb(188,163,255),5);
        graphics.DrawEllipse(pen,11,10,24,24);graphics.DrawLine(pen,29,29,38,38);
        var native=bitmap.GetHicon();
        try {using var icon=Icon.FromHandle(native);return (Icon)icon.Clone();}
        finally {DestroyIcon(native);}
    }
    [DllImport("user32.dll")] [return:MarshalAs(UnmanagedType.Bool)] static extern bool DestroyIcon(nint icon);
    static bool SetStartup(bool enabled) {
        try {
            using var key=Registry.CurrentUser.CreateSubKey(RunKey);
            if(enabled) key.SetValue("Quotamon","\""+Environment.ProcessPath+"\" --minimized");
            else key.DeleteValue("Quotamon",false);
            return true;
        } catch { return false; }
    }
    void OpenSettings() {
        if(url==null) return;
        try { Process.Start(new ProcessStartInfo(url){UseShellExecute=true}); }
        catch { if(!headless) MessageBox.Show("기본 브라우저를 확인해주세요.","Quotamon",MessageBoxButtons.OK,MessageBoxIcon.Information); }
    }
    void Fail(string message) {
        if(stopping) return;
        if(!headless) MessageBox.Show(message,"Quotamon",MessageBoxButtons.OK,MessageBoxIcon.Information);
        dispatcher.BeginInvoke(()=>ExitThread());
    }
    protected override void ExitThreadCore() {
        if(stopping) return;
        stopping=true;
        if(tray!=null) {tray.Visible=false;tray.Dispose();}
        brandIcon?.Dispose();
        try {
            backend.StandardInput.Close();
            if(!backend.WaitForExit(5000)) backend.Kill(true);
        } catch { /* Job close also kills child processes. */ }
        openWait?.Unregister(null);openRequest.Dispose();
        job.Dispose();backend.Dispose();dispatcher.Dispose();base.ExitThreadCore();
    }
}

// The portable launcher owns the Node runtime, BLE subprocess and SDK children.
// Kill-on-close prevents background processes from surviving a launcher crash.
internal sealed class ProcessJob : IDisposable {
    readonly nint handle;
    public ProcessJob() {
        handle=CreateJobObjectW(0,null);
        if(handle==0) throw new InvalidOperationException("Job creation failed");
        var info=new ExtendedLimit {Basic=new BasicLimit {LimitFlags=0x2000}};
        var size=Marshal.SizeOf<ExtendedLimit>();var memory=Marshal.AllocHGlobal(size);
        try {
            Marshal.StructureToPtr(info,memory,false);
            if(!SetInformationJobObject(handle,9,memory,(uint)size)) throw new InvalidOperationException("Job configuration failed");
        } finally {Marshal.FreeHGlobal(memory);}
    }
    public void Attach(Process process) {
        if(!AssignProcessToJobObject(handle,process.Handle)) throw new InvalidOperationException("Job assignment failed");
    }
    public void Dispose() {if(handle!=0) CloseHandle(handle);}
    [StructLayout(LayoutKind.Sequential)] struct BasicLimit {
        public long ProcessTime,JobTime;public uint LimitFlags;public nuint MinWorking,MaxWorking;
        public uint ActiveProcesses;public nuint Affinity;public uint Priority,Scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IoCounters {public ulong ReadOperations,WriteOperations,OtherOperations,ReadBytes,WriteBytes,OtherBytes;}
    [StructLayout(LayoutKind.Sequential)] struct ExtendedLimit {
        public BasicLimit Basic;public IoCounters Io;public nuint ProcessMemory,JobMemory,PeakProcessMemory,PeakJobMemory;
    }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern nint CreateJobObjectW(nint attributes,string? name);
    [DllImport("kernel32.dll",SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] static extern bool SetInformationJobObject(nint job,int type,nint information,uint length);
    [DllImport("kernel32.dll",SetLastError=true)] [return:MarshalAs(UnmanagedType.Bool)] static extern bool AssignProcessToJobObject(nint job,nint process);
    [DllImport("kernel32.dll")] [return:MarshalAs(UnmanagedType.Bool)] static extern bool CloseHandle(nint handle);
}
