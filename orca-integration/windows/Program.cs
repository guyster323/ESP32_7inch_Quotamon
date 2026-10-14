using System.Text.Json;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Security.Cryptography;

// Private stdin/stdout BLE resource. No provider authentication or quota scraping.
internal static class Program {
    static readonly Guid Service = Guid.Parse("7155e001-9df0-4b75-9d9b-796d6f6e6974");
    static readonly Guid Snapshot = Guid.Parse("7155e002-9df0-4b75-9d9b-796d6f6e6974");
    static readonly Guid Control = Guid.Parse("7155e003-9df0-4b75-9d9b-796d6f6e6974");
    static readonly object OutputLock = new();
    static readonly SemaphoreSlim ConnectionGate = new(1);
    static readonly SemaphoreSlim WriteGate = new(1);
    static BluetoothLEAdvertisementWatcher? watcher;
    static BluetoothLEDevice? device;
    static GattDeviceService? service;
    static GattSession? session;
    static GattCharacteristic? writer, notifier;
    static TaskCompletionSource<bool>? confirmation;
    static string? pendingPin;
    static readonly HashSet<ulong> Discovered = [];
    static void Emit(object value) { lock(OutputLock) { Console.WriteLine(JsonSerializer.Serialize(value)); Console.Out.Flush(); } }
    public static async Task Main(string[] args) {
        if(args.Contains("--probe")) { await Probe(); return; }
        Emit(new {type="ready",protocol=1});
        var tasks=new List<Task>();
        while(await Console.In.ReadLineAsync() is { } line) {
            if(line.Length>65536) { Emit(new {type="error",code="input-too-large"}); continue; }
            tasks.RemoveAll(t=>t.IsCompleted);
            if(tasks.Count>=32) { Emit(new {type="error",code="command-limit"}); continue; }
            tasks.Add(Handle(line));
        }
        confirmation?.TrySetResult(false); watcher?.Stop(); Disconnect();
    }
    static async Task Probe() {
        var adapter=await BluetoothAdapter.GetDefaultAsync();
        Emit(new {type="adapter",available=adapter!=null,central=adapter?.IsCentralRoleSupported==true,low_energy=adapter?.IsLowEnergySupported==true});
    }
    static async Task Handle(string line) {
        string? id=null;
        try {
            using var doc=JsonDocument.Parse(line); var root=doc.RootElement;
            id=root.GetProperty("id").GetString();
            switch(root.GetProperty("command").GetString()) {
                case "probe": await Probe(); break;
                case "scan": Scan(); break;
                case "confirm":
                    var pin=root.GetProperty("pin").GetString();
                    if(pin!=pendingPin || confirmation==null) throw new InvalidOperationException("No matching pairing challenge");
                    confirmation.TrySetResult(root.GetProperty("accept").GetBoolean()); break;
                case "connect":
                    var address=ulong.Parse(root.GetProperty("address").GetString()!);
                    await ConnectionGate.WaitAsync();
                    try { await Connect(address).WaitAsync(TimeSpan.FromSeconds(90)); }
                    finally { ConnectionGate.Release(); } break;
                case "write":
                    var bytes=Convert.FromBase64String(root.GetProperty("frame").GetString()!);
                    if(bytes.Length<9 || bytes.Length>512) throw new InvalidOperationException("Frame size");
                    await WriteGate.WaitAsync();
                    try {
                        if(writer==null || bytes.Length>Math.Min(512,(session?.MaxPduSize??23)-3)) throw new InvalidOperationException("Not connected or frame exceeds MTU");
                        var result=await writer.WriteValueWithResultAsync(CryptographicBuffer.CreateFromByteArray(bytes),GattWriteOption.WriteWithResponse).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
                        if(result.Status!=GattCommunicationStatus.Success) throw new InvalidOperationException("GATT write failed");
                    } finally { WriteGate.Release(); } break;
                case "disconnect":
                    confirmation?.TrySetResult(false);
                    await ConnectionGate.WaitAsync(); try { Disconnect(); } finally { ConnectionGate.Release(); } break;
                default: throw new InvalidOperationException("Unknown command");
            }
            Emit(new {type="result",id,ok=true});
        } catch(Exception ex) { Emit(new {type="result",id,ok=false,code=ex is TimeoutException?"timeout":"operation-failed"}); }
    }
    static void Scan() {
        watcher?.Stop();
        watcher=new BluetoothLEAdvertisementWatcher {ScanningMode=BluetoothLEScanningMode.Active};
        watcher.AdvertisementFilter.Advertisement.ServiceUuids.Add(Service);
        watcher.Received+=(_,e)=> {
            lock(Discovered) Discovered.Add(e.BluetoothAddress);
            Emit(new {type="device",address=e.BluetoothAddress.ToString(),name=e.Advertisement.LocalName,rssi=e.RawSignalStrengthInDBm});
        };
        watcher.Start();
        var current=watcher; _=Task.Run(async()=> { await Task.Delay(20000); if(current==watcher) current.Stop(); });
    }
    static async Task Connect(ulong address) {
        Disconnect(); watcher?.Stop();
        // Reconnect to previously bonded devices is allowed; new devices must have advertised our service.
        device=await BluetoothLEDevice.FromBluetoothAddressAsync(address);
        if(device==null) throw new InvalidOperationException("Device unavailable");
        if(!device.DeviceInformation.Pairing.IsPaired) {
            lock(Discovered) if(!Discovered.Contains(address)) throw new InvalidOperationException("Discover device first");
            var custom=device.DeviceInformation.Pairing.Custom;
            custom.PairingRequested+=OnPairing;
            try {
                var paired=await custom.PairAsync(DevicePairingKinds.ConfirmPinMatch,DevicePairingProtectionLevel.EncryptionAndAuthentication);
                if(paired.Status!=DevicePairingResultStatus.Paired && paired.Status!=DevicePairingResultStatus.AlreadyPaired) throw new InvalidOperationException("Pairing rejected");
            } finally { custom.PairingRequested-=OnPairing; confirmation=null; pendingPin=null; }
        }
        session=await GattSession.FromDeviceIdAsync(device.BluetoothDeviceId); session.MaintainConnection=true;
        var services=await device.GetGattServicesForUuidAsync(Service,BluetoothCacheMode.Uncached);
        if(services.Status!=GattCommunicationStatus.Success || services.Services.Count!=1) throw new InvalidOperationException("Quota service missing");
        service=services.Services[0];
        var writes=await service.GetCharacteristicsForUuidAsync(Snapshot,BluetoothCacheMode.Uncached);
        var notices=await service.GetCharacteristicsForUuidAsync(Control,BluetoothCacheMode.Uncached);
        if(writes.Status!=GattCommunicationStatus.Success||notices.Status!=GattCommunicationStatus.Success||writes.Characteristics.Count!=1||notices.Characteristics.Count!=1) throw new InvalidOperationException("Quota characteristics missing");
        writer=writes.Characteristics[0]; notifier=notices.Characteristics[0];
        writer.ProtectionLevel=GattProtectionLevel.EncryptionAndAuthenticationRequired;
        notifier.ProtectionLevel=GattProtectionLevel.EncryptionAndAuthenticationRequired;
        notifier.ValueChanged+=OnNotification;
        var subscribed=await notifier.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Notify);
        if(subscribed!=GattCommunicationStatus.Success) throw new InvalidOperationException("Notification subscribe failed");
        device.ConnectionStatusChanged+=OnConnection;
        Emit(new {type="connected",address=address.ToString(),value_size=Math.Min(512,session.MaxPduSize-3)});
    }
    static async void OnPairing(DeviceInformationCustomPairing sender,DevicePairingRequestedEventArgs args) {
        var deferral=args.GetDeferral();
        try {
            if(args.PairingKind!=DevicePairingKinds.ConfirmPinMatch) return;
            pendingPin=args.Pin; confirmation=new(TaskCreationOptions.RunContinuationsAsynchronously);
            Emit(new {type="pairing",pin=args.Pin});
            if(await confirmation.Task.WaitAsync(TimeSpan.FromSeconds(60))) args.Accept();
        } catch(TimeoutException) { Emit(new {type="pairing-expired"}); }
        finally { deferral.Complete(); }
    }
    static void OnNotification(GattCharacteristic sender,GattValueChangedEventArgs args) {
        CryptographicBuffer.CopyToByteArray(args.CharacteristicValue,out var bytes);
        if(bytes.Length<=512) Emit(new {type="notification",frame=Convert.ToBase64String(bytes)});
    }
    static void OnConnection(BluetoothLEDevice sender,object args) {
        if(sender.ConnectionStatus==BluetoothConnectionStatus.Disconnected) Emit(new {type="disconnected"});
    }
    static void Disconnect() {
        if(notifier!=null) notifier.ValueChanged-=OnNotification;
        if(device!=null) device.ConnectionStatusChanged-=OnConnection;
        writer=null; notifier=null;
        if(session!=null) session.MaintainConnection=false;
        session?.Dispose(); session=null; service?.Dispose(); service=null; device?.Dispose(); device=null;
    }
}
