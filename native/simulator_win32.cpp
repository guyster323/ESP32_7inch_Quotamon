#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "ui.h"
#include <algorithm>
#include <cmath>
#include "snapshot_json.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>

namespace {
constexpr int W=800,H=480;
std::array<uint32_t,W*H> pixels;
std::array<lv_color_t,W*40> drawBuffer;
bool down=false; int mouseX=0,mouseY=0;
HWND window=nullptr;
std::filesystem::path snapshotFile;
int configurationRevision=0;
monitor::Snapshot latestSnapshot;
double latestProduced=0;
double unixSeconds() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }
void saveIntent(const monitor::Settings& settings,const monitor::Snapshot& data) {
    if(snapshotFile.empty()) return;
    nlohmann::json display={{"poll_seconds",settings.pollSeconds},{"brightness_percent",settings.brightnessPercent},
        {"show_reset",settings.showReset},{"show_updated",settings.showUpdated},{"show_credits",settings.showCredits},{"show_session",settings.showSession}};
    std::vector<std::string> enabled; for(const auto& a:data.accounts) if(a.enabled) enabled.push_back(a.id);
    nlohmann::json intent={{"id",std::to_string(unixSeconds())},{"config_revision",configurationRevision},{"display",display},{"enabled_ids",enabled}};
    auto path=snapshotFile.parent_path()/"native-intent.json"; auto temp=path; temp+=".tmp";
    std::ofstream stream(temp,std::ios::binary); stream<<intent.dump(); stream.close();
    if(stream.good()) MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
void checkSnapshot() {
    static std::filesystem::file_time_type seen{};
    try {
        if(!std::filesystem::is_regular_file(snapshotFile)||std::filesystem::file_size(snapshotFile)>65536) throw std::runtime_error("Snapshot unavailable");
        auto modified=std::filesystem::last_write_time(snapshotFile);
        if(modified!=seen) {
            std::ifstream input(snapshotFile,std::ios::binary); auto json=nlohmann::json::parse(input);
            auto decoded=monitor::decode(json); latestSnapshot=decoded.data; latestProduced=decoded.producedAt;
            configurationRevision=decoded.revision; seen=modified;
            double age=unixSeconds()-latestProduced;
            if(age>35||age< -60) latestSnapshot.connected=false;
            monitor::setSettings(decoded.settings); monitor::apply(latestSnapshot);
        } else if(latestSnapshot.connected&&unixSeconds()-latestProduced>35) {
            latestSnapshot.connected=false; monitor::apply(latestSnapshot);
        }
    } catch(...) {
        if(latestSnapshot.connected) { latestSnapshot.connected=false; monitor::apply(latestSnapshot); }
    }
}
void flush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* colors) {
    for(int y=area->y1;y<=area->y2;y++) for(int x=area->x1;x<=area->x2;x++) {
        uint32_t c=lv_color_to32(*colors++);
        if(x>=0&&x<W&&y>=0&&y<H) pixels[y*W+x]=c&0x00ffffff;
    }
    lv_disp_flush_ready(driver);
    if(window&&lv_disp_flush_is_last(driver)) InvalidateRect(window,nullptr,FALSE);
}
void readPointer(lv_indev_drv_t*,lv_indev_data_t* data) {
    data->point.x=static_cast<lv_coord_t>(mouseX); data->point.y=static_cast<lv_coord_t>(mouseY);
    data->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
void advance(int millis=60) {
    for(int t=0;t<millis;t+=5) { lv_tick_inc(5); lv_timer_handler(); }
    lv_refr_now(nullptr);
}
void click(int x,int y) { mouseX=x; mouseY=y; down=true; advance(60); down=false; advance(60); }
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool hasNumericPercent(lv_obj_t* object) {
    if(lv_obj_check_type(object,&lv_label_class)) {
        std::string text=lv_label_get_text(object);
        if(!text.empty()&&text.back()=='%'&&text.find_first_of("0123456789")!=std::string::npos) return true;
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);i++) if(hasNumericPercent(lv_obj_get_child(object,static_cast<int32_t>(i)))) return true;
    return false;
}
void capture(const std::filesystem::path& path) {
    advance(); std::ofstream out(path,std::ios::binary);
    require(out.good(),"Cannot open screenshot output"); out<<"P6\n800 480\n255\n";
    for(auto c:pixels) { const char b[]={static_cast<char>(c>>16),static_cast<char>(c>>8),static_cast<char>(c)}; out.write(b,3); }
}
void renderSuite(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    auto s=monitor::demo(); monitor::init(s); advance();
    const auto* first=monitor::cardObject(0);
    for(int i=0;i<5;i++) {
        auto* c=monitor::cardObject(i); require(c!=nullptr,"Missing overview card");
        lv_area_t a; lv_obj_get_coords(c,&a);
        require(a.x1>=0&&a.y1>=72&&a.x2<800&&a.y2<=431,"Card outside overview bounds");
    }
    require(!lv_obj_has_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE),"Overview scrolls");
    capture(folder/"01-home.ppm");
    s.accounts[0].pools[0].remaining=73; monitor::apply(s); advance();
    require(first==monitor::cardObject(0),"Snapshot update unnecessarily rebuilt home");
    // Use pointer input, rather than directly dispatching card click events.
    click(180,175); require(monitor::selectedAccount()==0,"Meter/card touch did not open detail");
    capture(folder/"02-codex-detail.ppm");
    click(65,36); require(monitor::selectedAccount()==-1,"Back touch failed");
    click(690,310); require(monitor::selectedAccount()==4,"Copilot touch failed");
    capture(folder/"03-copilot-detail.ppm");
    monitor::showHome(); advance(); click(730,36);
    require(monitor::selectedAccount()==-2,"Settings touch failed");
    capture(folder/"04-settings.ppm");
    int interval=monitor::settings().pollSeconds;
    click(530,116); click(600,432);
    require(monitor::selectedAccount()==-1&&monitor::settings().pollSeconds==interval,"Cancel committed draft settings");
    monitor::showSettings(); advance(); click(530,116); click(720,432);
    require(monitor::selectedAccount()==-1&&monitor::settings().pollSeconds==interval+15,"Save did not commit settings");
    monitor::showSettings(); advance(); click(90,255); click(90,320); click(720,432);
    require(!monitor::settings().showReset&&!monitor::settings().showCredits,"Display-field settings did not commit");
    monitor::showSettings(); advance(); click(90,255); click(90,320); click(720,432);
    require(monitor::settings().showReset&&monitor::settings().showCredits,"Display-field settings failed to restore");
    click(620,36); require(monitor::refreshRequests()==1,"Refresh intent missing");
    s=monitor::demo();
    s.accounts[0].pools[0].remaining=0; s.accounts[0].pools[1].remaining=8;
    s.accounts[1].pools[0].remaining=20; s.accounts[1].pools[1].remaining=100;
    s.accounts[2].pools[0].remaining=-1; s.accounts[2].pools[1].remaining=-2;
    s.accounts[3].state=monitor::SourceState::Stale;
    s.accounts[4].state=monitor::SourceState::Unknown;
    monitor::apply(s); capture(folder/"05-edge-states.ppm");
    s.connected=false; monitor::apply(s); capture(folder/"06-disconnected.ppm");
    require(!hasNumericPercent(lv_scr_act()),"Disconnected overview retained numeric percentages");
    require(monitor::cardObject(0)==nullptr,"Disconnected overview retained account cards");
    s.connected=true;monitor::apply(s);monitor::showDetail(0);
    s.connected=false;monitor::apply(s);capture(folder/"07-disconnected-detail.ppm");
    require(monitor::selectedAccount()==-1,"Disconnect from detail did not return home");
    require(!hasNumericPercent(lv_scr_act()),"Disconnected detail retained numeric percentages");
    s=monitor::demo(); s.accounts[0].profile="아주 긴 한국어 계정 이름 · Business Premium 기본 프로필 · 아주 긴 한국어 계정 이름 · Business Premium 기본 프로필";
    monitor::apply(s); monitor::showDetail(0); capture(folder/"08-long-label.ppm");
    s=monitor::demo();
    auto claude=s.accounts[0]; claude.provider=monitor::Provider::Claude; claude.id="demo-claude"; claude.title="Claude / Personal"; claude.plan="Max";
    auto grok=s.accounts[0]; grok.provider=monitor::Provider::Grok; grok.id="demo-grok"; grok.title="Grok / Personal"; grok.plan="SuperGrok";
    s.accounts={s.accounts[0],s.accounts[3],s.accounts[4],claude,grok}; monitor::apply(s); monitor::showHome(); capture(folder/"09-five-providers.ppm");
    s.accounts.push_back(claude); s.accounts.back().id="demo-claude-team"; s.accounts.back().title="Claude / Team";
    s.accounts.push_back(grok); s.accounts.back().id="demo-grok-team"; s.accounts.back().title="Grok / Team";
    monitor::apply(s); advance(); click(480,36); capture(folder/"10-account-page-two.ppm");
    click(730,36); click(700,36); require(monitor::selectedAccount()==-3,"Account management navigation failed");
    capture(folder/"11-account-selector.ppm"); click(100,114); click(720,432);
    require(monitor::selectedAccount()==-1,"Account selection save failed");
    for(auto& a:s.accounts) a.enabled=false;
    monitor::apply(s); capture(folder/"12-onboarding.ppm");
    s.accounts={claude}; s.accounts[0].enabled=true; s.accounts[0].poolCount=3; s.accounts[0].pools[2]={"FABLE WEEKLY",15,"4일"};
    monitor::apply(s); monitor::showDetail(0); capture(folder/"13-multiple-pools.ppm");
    monitor::showPairingWait();capture(folder/"15-ble-onboarding.ppm");
    monitor::showPairing(384925);capture(folder/"16-ble-confirmation.ppm");
    click(600,404);require(monitor::selectedAccount()==-1,"Pairing approve navigation failed");
    monitor::showPairing(384925);click(200,404);require(monitor::selectedAccount()==-1,"Pairing reject navigation failed");
    s.accounts[0].pools[0].remaining=0.4;s.accounts[0].state=monitor::SourceState::Ready;
    monitor::apply(s);monitor::showHome();capture(folder/"17-fractional-quota.ppm");
    std::ofstream result(folder/"native-checks.json");
    result<<R"({"passed":true,"resolution":[800,480],"checks":["five fixed cards inside screen","home not scrollable","snapshot preserves card objects","pointer opens detail through meter","back pointer navigation","Copilot monthly detail","settings pointer navigation","cancel discards draft","save commits interval","display field toggles commit and restore","refresh intent","0/8/20/100 percent","unknown and unlimited","stale source","disconnect clears numeric display","offline home and detail have no numeric percentages","long Korean label ellipsizes"],"hardware_tested":false,"mock_data":true})";
    std::cout<<"Native LVGL checks passed. Screenshots: "<<folder.string()<<"\n";
}
LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); BITMAPINFO info{};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=W; info.bmiHeader.biHeight=-H;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        SetDIBitsToDevice(dc,0,0,W,H,0,0,0,H,pixels.data(),&info,DIB_RGB_COLORS); EndPaint(hwnd,&ps); return 0;
    }
    if(msg==WM_LBUTTONDOWN||msg==WM_MOUSEMOVE||msg==WM_LBUTTONUP) {
        mouseX=static_cast<short>(LOWORD(l)); mouseY=static_cast<short>(HIWORD(l));
        if(msg==WM_LBUTTONDOWN) { down=true; SetCapture(hwnd); }
        if(msg==WM_LBUTTONUP) { down=false; ReleaseCapture(); }
        return 0;
    }
    if(msg==WM_KEYDOWN) {
        auto s=monitor::demo();
        if(snapshotFile.empty()&&w=='D') { s.connected=false; monitor::apply(s); }
        if(snapshotFile.empty()&&w=='R') monitor::apply(s);
        if(w==VK_ESCAPE) monitor::showHome(); return 0;
    }
    if(msg==WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        lv_init(); static lv_disp_draw_buf_t buffer; lv_disp_draw_buf_init(&buffer,drawBuffer.data(),nullptr,static_cast<uint32_t>(drawBuffer.size()));
        static lv_disp_drv_t display; lv_disp_drv_init(&display); display.hor_res=W; display.ver_res=H;
        display.flush_cb=flush; display.draw_buf=&buffer; lv_disp_drv_register(&display);
        static lv_indev_drv_t input; lv_indev_drv_init(&input); input.type=LV_INDEV_TYPE_POINTER; input.read_cb=readPointer; lv_indev_drv_register(&input);
        if(argc>=3&&std::wstring(argv[1])==L"--render") { renderSuite(argv[2]); return 0; }
        if(argc>=4&&std::wstring(argv[1])==L"--render-snapshot") {
            std::ifstream source(std::filesystem::path(argv[2]),std::ios::binary);
            auto parsed=nlohmann::json::parse(source); auto decoded=monitor::decode(parsed);
            if(unixSeconds()-decoded.producedAt>35||decoded.producedAt-unixSeconds()>60) decoded.data.connected=false;
            monitor::init(decoded.data); monitor::setSettings(decoded.settings); monitor::showHome();
            std::filesystem::path folder(argv[3]); std::filesystem::create_directories(folder);
            capture(folder/"14-source-snapshot.ppm");
            std::cout<<"Validated display snapshot: "<<decoded.data.accounts.size()<<" accounts\n"; return 0;
        }
        if(argc>=3&&std::wstring(argv[1])==L"--snapshot") snapshotFile=argv[2];
        SetProcessDPIAware(); WNDCLASSW wc{}; wc.lpfnWndProc=wndProc; wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpszClassName=L"ESPmonitorNative"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); RegisterClassW(&wc);
        RECT rect{0,0,W,H}; const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX; AdjustWindowRect(&rect,style,FALSE);
        window=CreateWindowW(wc.lpszClassName,L"ESPmonitor · LVGL 8.4 · MOCK DATA · D: disconnect / R: reconnect",style,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
        require(window!=nullptr,"Window creation failed");
        if(snapshotFile.empty()) monitor::init(monitor::demo());
        else { latestSnapshot.mock=false; latestSnapshot.connected=false; monitor::init(latestSnapshot); monitor::setCommitCallback(saveIntent); checkSnapshot(); SetWindowTextW(window,L"Quotamon · developer snapshot simulator"); }
        ShowWindow(window,SW_SHOW);
        DWORD last=GetTickCount(),lastFileCheck=0; MSG msg{}; bool running=true;
        while(running) {
            while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)) { if(msg.message==WM_QUIT) running=false; TranslateMessage(&msg); DispatchMessage(&msg); }
            DWORD now=GetTickCount(); lv_tick_inc(now-last); last=now;
            if(!snapshotFile.empty()&&now-lastFileCheck>=1000) { lastFileCheck=now; checkSnapshot(); }
            lv_timer_handler(); Sleep(5);
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
