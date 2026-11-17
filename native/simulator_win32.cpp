#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "ui.h"
#include <algorithm>
#include <cmath>
#include "snapshot_json.h"
#include "screen_schedule.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <set>

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
        {"language",settings.language},{"sleep_enabled",settings.sleepEnabled},{"sleep_start_minute",settings.sleepStartMinute},{"sleep_end_minute",settings.sleepEndMinute},
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
lv_obj_t* findLabel(lv_obj_t* object,const std::string& needle) {
    if(lv_obj_check_type(object,&lv_label_class)&&std::string(lv_label_get_text(object)).find(needle)!=std::string::npos) return object;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);i++)
        if(auto* found=findLabel(lv_obj_get_child(object,static_cast<int32_t>(i)),needle)) return found;
    return nullptr;
}
void checkFooter(const std::string& refreshed,const std::string& sourceLabel) {
    auto* footer=findLabel(lv_scr_act(),"최근 갱신 "+refreshed);
    require(footer!=nullptr,"Latest refresh time missing from footer");
    const std::string text=lv_label_get_text(footer);
    require(text.find(sourceLabel+"14:32:00")!=std::string::npos,"Provider collection time missing from footer");
    const auto newline=text.find('\n');
    require(newline!=std::string::npos&&text.find('\n',newline+1)==std::string::npos,"Footer must occupy two lines");
    const auto* font=lv_obj_get_style_text_font(footer,0);
    for(const auto& line:{text.substr(0,newline),text.substr(newline+1)})
        require(lv_txt_get_width(line.c_str(),static_cast<uint32_t>(line.size()),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(footer),"Footer time is horizontally clipped");
    lv_area_t area; lv_obj_get_coords(footer,&area);
    require(area.y1>431&&area.y2<480&&lv_obj_get_height(footer)>=font->line_height*2,"Footer time is vertically clipped");
}
void writePixels(const std::filesystem::path& path) {
    std::ofstream out(path,std::ios::binary);
    require(out.good(),"Cannot open screenshot output"); out<<"P6\n800 480\n255\n";
    for(auto c:pixels) { const char b[]={static_cast<char>(c>>16),static_cast<char>(c>>8),static_cast<char>(c)}; out.write(b,3); }
}
void capture(const std::filesystem::path& path) { advance(); writePixels(path); }
lv_obj_t* findImage(lv_obj_t* object) {
    if(lv_obj_check_type(object,&lv_img_class)) return object;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);i++)
        if(auto* found=findImage(lv_obj_get_child(object,static_cast<int32_t>(i)))) return found;
    return nullptr;
}
unsigned timerCount() {
    unsigned count=0;
    for(auto* timer=lv_timer_get_next(nullptr);timer;timer=lv_timer_get_next(timer)) ++count;
    return count;
}
void renderAnimation(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    auto s=monitor::demo();
    constexpr monitor::Provider providers[]={monitor::Provider::Codex,monitor::Provider::Cursor,
        monitor::Provider::Copilot,monitor::Provider::Claude,monitor::Provider::Grok};
    constexpr const char* names[]={"Codex","Cursor","Copilot","Claude","Grok"};
    for(int i=0;i<5;i++) {
        auto& a=s.accounts[i]; a.provider=providers[i]; a.title=names[i];
        a.poolCount=2; a.hasAmounts=false; a.hasSession=false;
        a.pools[0]={"5H",72,"2h 18m"}; a.pools[1]={"WEEKLY",54,"3d 4h"};
    }
    monitor::init(s); advance();
    const unsigned timers=timerCount();
    std::array<lv_obj_t*,5> images{};
    std::array<std::set<const void*>,5> sources;
    std::array<std::set<int>,5> positions;
    unsigned minEyes=100000, maxEyes=0;
    for(int i=0;i<5;i++) { images[i]=findImage(monitor::cardObject(i)); require(images[i]!=nullptr,"Animation image missing"); }
    // A ten-second recording also checks that only sprite regions change.
    const std::vector<uint32_t> reference(pixels.begin(),pixels.end());
    std::array<lv_area_t,5> regions;
    for(int i=0;i<5;i++) {
        lv_area_t card;lv_obj_get_coords(monitor::cardObject(i),&card);
        const int bodyY=std::max(34,(lv_obj_get_height(monitor::cardObject(i))-108)/2);
        regions[i]={static_cast<lv_coord_t>(card.x1+6),static_cast<lv_coord_t>(card.y1+bodyY-6),
                    static_cast<lv_coord_t>(card.x1+102),static_cast<lv_coord_t>(card.y1+bodyY+108)};
    }
    for(int frame=0;frame<100;frame++) {
        advance(100);
        for(int i=0;i<5;i++) {
            sources[i].insert(lv_img_get_src(images[i]));
            positions[i].insert(lv_obj_get_y(images[i])*1000+lv_obj_get_x(images[i]));
        }
        lv_area_t copilot;lv_obj_get_coords(images[2],&copilot);
        const int zoom=lv_img_get_zoom(images[2]);
        unsigned eyes=0;
        for(int y=copilot.y1+45*zoom/256;y<copilot.y1+60*zoom/256;y++)
            for(int x=copilot.x1+38*zoom/256;x<copilot.x1+64*zoom/256;x++) {
                const auto p=pixels[y*W+x];
                if(((p>>16)&255)>170&&((p>>8)&255)>210&&(p&255)>210) ++eyes;
            }
        minEyes=std::min(minEyes,eyes);maxEyes=std::max(maxEyes,eyes);
        for(int y=0;y<H;y++) for(int x=0;x<W;x++) if(pixels[y*W+x]!=reference[y*W+x]) {
            bool allowed=false;
            for(const auto& area:regions) allowed|=x>=area.x1&&x<=area.x2&&y>=area.y1&&y<=area.y2;
            if(!allowed) {
                writePixels(folder/"unexpected-change.ppm");
                throw std::runtime_error("Animation changed pixels outside sprite lane at "+std::to_string(x)+","+std::to_string(y)+" frame "+std::to_string(frame));
            }
        }
        wchar_t file[40]; swprintf_s(file,L"home-%03d.ppm",frame);writePixels(folder/file);
    }
    for(int i=0;i<5;i++) {
        require(positions[i].size()>=3,"Character did not breathe or gesture");
        if(providers[i]!=monitor::Provider::Copilot) require(sources[i].size()==2,"Expression frame never displayed");
    }
    require(maxEyes>minEyes*2,"Copilot eyelids never close over the rendered eyes");
    auto* card=monitor::cardObject(0);
    monitor::apply(s);require(card==monitor::cardObject(0),"Animation snapshot rebuilt the home cards");
    // Every provider/state combination must move, retaining its real severity.
    for(int state=0;state<6;state++) {
        auto states=s;
        for(auto& a:states.accounts) {
            a.state=state==4?monitor::SourceState::Unknown:state==5?monitor::SourceState::Stale:monitor::SourceState::Ready;
            a.pools[0].remaining=state==1?18:state==2?8:state==3?0:72;
        }
        monitor::apply(states);advance();
        std::array<std::set<int>,5> poses;
        std::array<std::set<const void*>,5> frames;
        for(int tick=0;tick<100;tick++) {
            advance(100);
            for(int i=0;i<5;i++) {
                poses[i].insert(lv_obj_get_y(images[i])*1000+lv_obj_get_x(images[i]));
                frames[i].insert(lv_img_get_src(images[i]));
            }
            if(state>0&&tick<40) {
                wchar_t file[40]; swprintf_s(file,L"state-%d-%03d.ppm",state,tick);writePixels(folder/file);
            }
        }
        for(int i=0;i<5;i++) {
            require(poses[i].size()>=2,"Severity state is static");
            if(providers[i]!=monitor::Provider::Copilot) require(frames[i].size()==2,"Severity expression did not play");
        }
    }
    monitor::apply(s);monitor::showDetail(2);advance();
    auto* detail=findImage(lv_scr_act());require(detail!=nullptr,"Detail character missing");
    std::set<int> detailPositions;
    for(int tick=0;tick<80;tick++) {
        advance(100);detailPositions.insert(lv_obj_get_y(detail));
        wchar_t file[40]; swprintf_s(file,L"detail-%03d.ppm",tick);writePixels(folder/file);
    }
    require(detailPositions.size()>=3,"Detail character is static");
    // Repeated page deletion, pairing, snapshot updates and reinitialization
    // must never accumulate timers or leave deleted image pointers active.
    monitor::showHome();advance();lv_mem_monitor_t before;lv_mem_monitor(&before);
    for(int i=0;i<40;i++) {
        monitor::showDetail(i%5);advance(150);
        monitor::showSettings();advance(200);
        const std::vector<uint32_t> settingsPixels(pixels.begin(),pixels.end());
        advance(1000);require(std::equal(settingsPixels.begin(),settingsPixels.end(),pixels.begin()),"Animation runs on settings page");
        monitor::showPairingWait();advance(150);
        monitor::showPairing(384925);advance(150);
        monitor::init(s);advance(150);
        require(timerCount()==timers,"Page navigation leaked an animation timer");
    }
    lv_mem_monitor_t after;lv_mem_monitor(&after);
    require(after.free_size+1024>=before.free_size,"Animation/page lifecycle leaks LVGL heap");
    s.connected=false;monitor::apply(s);advance();
    const std::vector<uint32_t> disconnected(pixels.begin(),pixels.end());
    advance(5000);require(std::equal(disconnected.begin(),disconnected.end(),pixels.begin()),"Deleted characters keep redrawing disconnected page");
    std::ofstream report(folder/"animation-checks.json");
    report<<nlohmann::json({{"passed",true},{"tick_ms",100},{"provider_state_combinations",30},
        {"page_cycles",40},{"timer_count",timers},{"heap_free_before",before.free_size},{"heap_free_after",after.free_size},
        {"copilot_eye_pixels_open",maxEyes},{"copilot_eye_pixels_closed",minEyes},
        {"checks",{"expression pairs play","all providers and all six states move","only sprite regions change",
            "detail animates","snapshot preserves cards","settings and disconnect stop redraws","page lifecycle retains one timer and stable heap"}},
        {"hardware_tested",false}}).dump(2);
    std::cout<<"Character animation checks passed: 30 provider/state combinations, 40 page cycles.\n";
}
void renderSuite(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    nlohmann::json wire={{"protocol","quotamon.v1"},{"produced_at",1800000000},{"config_revision",0},
        {"connected",true},{"mock",false},{"accounts",nlohmann::json::array()},
        {"display",{{"poll_seconds",60},{"brightness_percent",70},{"show_reset",true},{"show_updated",true},{"show_credits",true},{"show_session",true}}}};
    require(monitor::decode(wire).data.refreshed=="미확인","Legacy snapshot refresh fallback missing");
    wire["refreshed"]="14:33:00";
    require(monitor::decode(wire).data.refreshed=="14:33:00","Snapshot refresh time was not decoded");
    wire["refreshed"]=std::string(65,'x');
    bool rejected=false;try{monitor::decode(wire);}catch(const std::exception&){rejected=true;}
    require(rejected,"Oversized refresh label accepted");
    nlohmann::json creditWire={{"protocol","quotamon.v1"},{"produced_at",1800000000},{"utc_offset_minutes",540},{"config_revision",0},
        {"connected",true},{"mock",false},{"refreshed","14:33:00"},
        {"accounts",nlohmann::json::array({{
            {"id","codex-0123456789abcdef"},{"provider","codex"},{"title","Codex"},{"profile","Codex"},{"plan","Plus"},
            {"enabled",true},{"state","ready"},{"pools",nlohmann::json::array({{{"name","5H"},{"remaining",70},{"reset","2h"}}})},
            {"reset_credits",nlohmann::json::array({
                {{"count",2},{"expires_at",1800003600},{"timezone","UTC"}},
                {{"count",1},{"expires_at",1800086400},{"timezone","not a zone"}}
            })}
        }})},
        {"display",{{"poll_seconds",60},{"brightness_percent",70},{"show_reset",true},{"show_updated",true},{"show_credits",true},{"show_session",true}}}};
    auto decodedCredits=monitor::decode(creditWire);
    require(decodedCredits.data.utcOffsetMinutes==540,"UTC offset was not kept for local expiry labels");
    require(decodedCredits.data.accounts[0].resetCreditCount==2,"Reset credits missing from the device snapshot");
    require(decodedCredits.data.accounts[0].resetCredits[0].timezone=="UTC","Explicit reset-credit zone was dropped");
    require(decodedCredits.data.accounts[0].resetCredits[1].timezone.empty(),"Unknown reset-credit zone was invented");
    auto s=monitor::demo(); monitor::init(s); advance();
    // Test time-based colours independently of remaining quota, including clamps.
    auto timing=s;
    const auto now=static_cast<double>(std::time(nullptr));
    timing.accounts[0].resetCreditCount=2;
    timing.accounts[0].resetCredits[0]={1,now-60,""};
    timing.accounts[0].resetCredits[1]={1,now+15*86400,""};
    timing.accounts[0].pools[0].reset="5h";
    timing.accounts[0].pools[1].reset="7d";
    for(int account:{3,4}) for(int p=0;p<timing.accounts[account].poolCount;p++) timing.accounts[account].pools[p].reset="30d";
    monitor::apply(timing); advance();
    auto* timed=findLabel(monitor::cardObject(0),"Plan 5h :");
    require(timed&&lv_label_get_recolor(timed),"Plan countdown colours disabled");
    require(std::string(lv_label_get_text(timed)).find("#ff817f Plan 5h : 5h#")!=std::string::npos,"5-hour maximum is not red");
    require(std::string(lv_label_get_text(timed)).find("#ff817f W 7d#")!=std::string::npos,"Weekly maximum is not red");
    for(int account:{3,4}) require(findLabel(monitor::cardObject(account),"#ff817f Plan Reset M : 30d#")!=nullptr,"Monthly maximum is not red");
    auto checkCreditInk=[&](lv_obj_t* parent,const char* text,unsigned ink) {
        auto* item=findLabel(parent,text);
        require(item&&lv_color_to32(lv_obj_get_style_text_color(item,0))==lv_color_to32(lv_color_hex(ink)),"R.T. expiry colour is incorrect");
    };
    checkCreditInk(monitor::cardObject(0),"R.T.1 0m",0xff817f);
    checkCreditInk(monitor::cardObject(0),"R.T.2 ",0x19d79c);
    capture(folder/"22-countdown-maxima.ppm");
    monitor::showDetail(0); advance();
    checkCreditInk(lv_scr_act(),"R.T.1 0m",0xff817f);
    require(findLabel(lv_scr_act(),"#ff817f Plan 5h : 5h#")!=nullptr,"Detail countdown maximum is not red");
    monitor::showHome();
    timing.accounts[0].pools[0].reset="2h 30m";
    timing.accounts[0].pools[1].reset="0m";
    monitor::apply(timing); advance();
    require(findLabel(monitor::cardObject(0),"#8cac8e Plan 5h : 2h 30m#")!=nullptr,"Plan colour does not interpolate with time");
    require(findLabel(monitor::cardObject(0),"#19d79c W 0m#")!=nullptr,"Weekly segment does not have its own colour");
    capture(folder/"23-countdown-midpoint.ppm");
    for(auto& account:timing.accounts) for(int p=0;p<account.poolCount;p++) account.pools[p].reset="0m";
    monitor::apply(timing); advance();
    require(findLabel(monitor::cardObject(0),"#19d79c Plan 5h : 0m#")!=nullptr,"Zero plan countdown is not green");
    for(int account:{3,4}) require(findLabel(monitor::cardObject(account),"#19d79c Plan Reset M : 0m#")!=nullptr,"Monthly minimum is not green");
    capture(folder/"24-countdown-minima.ppm");
    timing.accounts[0].pools[0].reset="시각 미확인";
    monitor::apply(timing); advance();
    require(findLabel(monitor::cardObject(0),"#8ea8a8 Plan 5h :")!=nullptr,"Unknown countdown is not neutral");
    monitor::apply(s); advance();
    const auto* first=monitor::cardObject(0);
    for(int i=0;i<5;i++) {
        auto* c=monitor::cardObject(i); require(c!=nullptr,"Missing overview card");
        lv_area_t a; lv_obj_get_coords(c,&a);
        require(a.x1>=0&&a.y1>=72&&a.x2<800&&a.y2<=431,"Card outside overview bounds");
    }
    require(!lv_obj_has_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE),"Overview scrolls");
    capture(folder/"01-home.ppm");
    require(findLabel(monitor::cardObject(4),"Plan Reset M : 16d 0h")!=nullptr,"Copilot home omitted the plan reset countdown");
    require(findLabel(monitor::cardObject(4),"AIC")==nullptr,"Copilot home still displays AIC amounts");
    checkFooter("14:33:00","첫 카드 수집 ");
    auto* combinedReset=findLabel(monitor::cardObject(0),"Plan 5h : 2h 18m");
    require(combinedReset!=nullptr&&std::string(lv_label_get_text(combinedReset)).find("W 3d 4h")!=std::string::npos,"5-hour card omitted weekly reset");
    require(lv_obj_get_style_text_font(combinedReset,0)->line_height>=22,"Plan reset text was not enlarged");
    require(findLabel(monitor::cardObject(3),"Plan Reset M : 17d 0h")!=nullptr,"Cursor common reset missing");
    auto wrapped=s;
    wrapped.accounts[0].pools[0].reset="리셋 후 확인 대기";
    wrapped.accounts[0].pools[1].reset="리셋 후 확인 대기";
    monitor::apply(wrapped); advance();
    auto* wrappedReset=findLabel(monitor::cardObject(0),"Plan 5h :");
    require(wrappedReset!=nullptr&&lv_label_get_long_mode(wrappedReset)==LV_LABEL_LONG_DOT,"Dense reset schedule must ellipsize without shrinking primary content");
    require(lv_img_get_zoom(findImage(monitor::cardObject(0)))==256,"Long reset schedule shrank the character");
    require(lv_obj_get_style_text_font(findLabel(monitor::cardObject(0),"72%"),0)->line_height==25,"Long reset schedule changed the percentage font");
    lv_area_t resetArea;lv_obj_get_coords(wrappedReset,&resetArea);
    require(resetArea.y2<=241,"Wrapped reset schedule escaped its card");
    capture(folder/"18-wrapped-reset.ppm");
    monitor::apply(s); advance();
    s.refreshed="14:34:00";
    s.accounts[0].pools[0].remaining=73; monitor::apply(s); advance();
    require(first==monitor::cardObject(0),"Snapshot update unnecessarily rebuilt home");
    checkFooter("14:34:00","첫 카드 수집 ");
    auto displaySettings=monitor::settings(); displaySettings.showUpdated=false;
    monitor::setSettings(displaySettings); monitor::showHome(); advance();
    require(findLabel(lv_scr_act(),"최근 갱신 ")==nullptr,"Refresh time toggle was ignored");
    displaySettings.showUpdated=true; monitor::setSettings(displaySettings); monitor::showHome(); advance();
    // Use pointer input, rather than directly dispatching card click events.
    click(180,175); require(monitor::selectedAccount()==0,"Meter/card touch did not open detail");
    capture(folder/"02-codex-detail.ppm");
    checkFooter("14:34:00","계정 수집 ");
    require(findLabel(lv_scr_act(),"Plan 5h : 2h 18m")!=nullptr&&findLabel(lv_scr_act(),"Plan W : 3d 4h")!=nullptr,"Detail reset abbreviations missing");
    click(65,36); require(monitor::selectedAccount()==-1,"Back touch failed");
    click(690,310); require(monitor::selectedAccount()==4,"Copilot touch failed");
    capture(folder/"03-copilot-detail.ppm");
    require(findLabel(lv_scr_act(),"Plan Reset M : 16d 0h")!=nullptr,"Copilot detail omitted the plan reset countdown");
    require(findLabel(lv_scr_act(),"AIC")==nullptr,"Copilot detail still displays AIC amounts or session usage");
    monitor::showHome(); advance(); click(730,36);
    require(monitor::selectedAccount()==-2,"Settings touch failed");
    capture(folder/"04-settings.ppm");
    int interval=monitor::settings().pollSeconds;
    click(530,116); click(600,432);
    require(monitor::selectedAccount()==-1&&monitor::settings().pollSeconds==interval,"Cancel committed draft settings");
    monitor::showSettings(); advance(); click(530,116); click(720,432);
    require(monitor::selectedAccount()==-1&&monitor::settings().pollSeconds==interval+15,"Save did not commit settings");
    monitor::showSettings(); advance(); click(90,255); click(480,255); click(720,432);
    require(!monitor::settings().showReset&&!monitor::settings().showUpdated,"Display-field settings did not commit");
    monitor::showSettings(); advance(); click(90,255); click(480,255); click(720,432);
    require(monitor::settings().showReset&&monitor::settings().showUpdated,"Display-field settings failed to restore");
    click(620,36); require(monitor::refreshRequests()==1,"Refresh intent missing");
    checkFooter("14:34:00","첫 카드 수집 ");
    advance(8100);
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
    require(findLabel(lv_scr_act(),"최근 갱신 ")==nullptr,"Disconnected overview retained refresh time");
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
lv_obj_t* findDropdown(lv_obj_t* object) {
    if(lv_obj_check_type(object,&lv_dropdown_class))return object;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);i++)
        if(auto* found=findDropdown(lv_obj_get_child(object,static_cast<int32_t>(i))))return found;
    return nullptr;
}
void renderAccountCounts(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    auto data=monitor::demo();monitor::init(data);
    nlohmann::json variants=nlohmann::json::array();
    for(int count=0;count<=5;count++) {
        std::vector<int> selected;
        for(int i=0;i<5;i++) {
            data.accounts[i].enabled=(i*3)%5<count;
            if(data.accounts[i].enabled)selected.push_back(i);
        }
        monitor::apply(data);monitor::showHome();advance();
        nlohmann::json rectangles=nlohmann::json::array();
        std::vector<lv_obj_t*> previous;
        std::vector<lv_area_t> areas;
        for(int slot=0;slot<5;slot++) {
            auto* card=monitor::cardObject(slot);
            require((card!=nullptr)==(slot<count),"Wrong visible card count");
            if(!card)continue;
            previous.push_back(card);lv_area_t area;lv_obj_get_coords(card,&area);
            require(area.x1>=20&&area.x2<780&&area.y1>=72&&area.y2<432,"Account card outside content area");
            for(const auto& other:areas)require(area.x1>other.x2||other.x1>area.x2||area.y1>other.y2||other.y1>area.y2,"Account cards overlap");
            areas.push_back(area);rectangles.push_back({area.x1,area.y1,area.x2-area.x1+1,area.y2-area.y1+1});
        }
        data.accounts[0].pools[0].remaining=70-count;
        monitor::apply(data);advance();
        for(int slot=0;slot<count;slot++)require(monitor::cardObject(slot)==previous[slot],"Quota refresh rebuilt a card");
        capture(folder/("home-"+std::to_string(count)+".ppm"));
        for(int slot=0;slot<count;slot++) {
            const auto& area=areas[slot];click((area.x1+area.x2)/2,(area.y1+area.y2)/2);
            require(monitor::selectedAccount()==selected[slot],"Card touch opened a disabled or wrong account");
            click(70,35);require(monitor::selectedAccount()==-1,"Account detail back navigation failed");
        }
        if(count==0) {
            require(!hasNumericPercent(lv_scr_act()),"Empty selection retained quota percentages");
            click(360,360);require(monitor::selectedAccount()==-3,"Empty selection account picker failed");
            monitor::showHome();
        }
        variants.push_back({{"selected_accounts",count},{"card_rectangles",rectangles}});
    }
    auto original=data.accounts;
    for(int count:{6,20}) {
        data.accounts.clear();
        for(int i=0;i<count;i++) {
            auto account=original[static_cast<size_t>(i%5)];account.enabled=true;
            account.id="public-page-"+std::to_string(i);data.accounts.push_back(account);
        }
        monitor::apply(data);monitor::showHome();advance();
        for(int pageIndex=0;pageIndex<(count+4)/5;pageIndex++) {
            const int visible=std::min(5,count-pageIndex*5);
            for(int slot=0;slot<5;slot++)require((monitor::cardObject(slot)!=nullptr)==(slot<visible),"Page has wrong card count");
            if(pageIndex+1<(count+4)/5)click(480,35);
        }
        capture(folder/("home-"+std::to_string(count)+"-last-page.ppm"));
        auto* first=monitor::cardObject(0);lv_area_t area;lv_obj_get_coords(first,&area);click(area.x1+30,area.y1+30);
        require(monitor::selectedAccount()==(count-1)/5*5,"Last page touch opened the wrong account");click(70,35);
        for(size_t i=0;i<data.accounts.size();i++)data.accounts[i].enabled=i<2;
        monitor::apply(data);advance();
        require(monitor::cardObject(0)&&monitor::cardObject(1)&&!monitor::cardObject(2),"Selection shrink failed to clamp the page");
    }
    std::ofstream report(folder/"checks.json");
    report<<nlohmann::json({{"passed",true},{"mock_data",true},{"hardware_tested",false},{"production_layout_changed",false},
        {"maximum_per_page",5},{"maximum_total",20},{"variants",variants},
        {"checks",{"0 through 5 selected","non-contiguous selections","cards in bounds and non-overlapping","quota update preserves widgets",
        "every visible card opens correct detail","back navigation","zero-selection account picker","6 and 20 account pagination","selection shrink clamps last page"}}}).dump(2)<<'\n';
    std::cout<<"Account count layouts and pointer navigation: PASS\n";
}
void renderReadme(const std::string& language,const std::filesystem::path& folder) {
    require(monitor::i18n::valid(language),"Unsupported demo language");
    std::filesystem::create_directories(folder);
    auto data=monitor::demo();
    const char* profiles[]={"Orca","Personal","Team","Ultra","Business"};
    for(size_t i=0;i<data.accounts.size();i++) {
        data.accounts[i].id="public-demo-"+std::to_string(i);
        data.accounts[i].profile=profiles[i];
    }
    data.refreshed="14:33:00";
    monitor::init(data);auto settings=monitor::settings();settings.language=language;
    monitor::setSettings(settings);monitor::showHome();advance();
    lv_obj_t* dimmer=nullptr;
    for(int frame=0;frame<240;frame++) {
        if(frame==50) {click(150,165);require(monitor::selectedAccount()==0,"Demo card touch failed");}
        if(frame==80) {click(70,35);click(730,35);require(monitor::selectedAccount()==-2,"Demo settings touch failed");}
        if(frame==90) {auto* dropdown=findDropdown(lv_scr_act());require(dropdown!=nullptr,"Demo language dropdown missing");lv_dropdown_open(dropdown);}
        if(frame==100) {
            auto* dropdown=findDropdown(lv_scr_act());lv_dropdown_close(dropdown);
            lv_dropdown_set_selected(dropdown,static_cast<uint16_t>(monitor::i18n::index(language)));
            lv_event_send(dropdown,LV_EVENT_VALUE_CHANGED,nullptr);
        }
        if(frame==110) {click(665,185);require(monitor::selectedAccount()==-7,"Demo schedule touch failed");}
        if(frame==120) click(65,118);
        if(frame==130) click(440,176);
        if(frame==140) click(400,140); // 2
        if(frame==146) click(520,140); // 3
        if(frame==152) click(520,140); // 3
        if(frame==158) click(400,338); // 0
        if(frame==166) {click(520,338);require(findLabel(lv_scr_act(),"23:30")!=nullptr,"Demo keypad entry failed");}
        if(frame==180) {
            click(735,432);
            require(monitor::settings().sleepEnabled&&monitor::settings().sleepStartMinute==1410,"Demo schedule save failed");
        }
        if(frame==200) {
            require(monitor::screenShouldSleep(true,1410,420,23*3600+30*60,0,true,0,0),"Demo schedule fixture invalid");
            monitor::setDisplayAwake(false);
            dimmer=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(dimmer);lv_obj_set_size(dimmer,W,H);
            lv_obj_set_style_bg_color(dimmer,lv_color_black(),0);lv_obj_set_style_bg_opa(dimmer,LV_OPA_COVER,0);
            lv_obj_add_event_cb(dimmer,[](lv_event_t* event) {
                auto* overlay=lv_event_get_target(event);lv_obj_set_style_bg_opa(overlay,LV_OPA_TRANSP,0);
                lv_obj_clear_flag(overlay,LV_OBJ_FLAG_CLICKABLE);monitor::setDisplayAwake(true);
            },LV_EVENT_PRESSED,nullptr);
        }
        if(frame==220) click(400,240);
        advance(100);
        char filename[24];std::snprintf(filename,sizeof(filename),"frame-%03d.ppm",frame);writePixels(folder/filename);
    }
    if(dimmer)lv_obj_del(dimmer);
    std::ofstream report(folder/"checks.json");
    report<<nlohmann::json({{"passed",true},{"language",language},{"frames",240},{"frame_ms",100},
        {"mock_data",true},{"hardware_capture",false},{"screen_off_touch_simulated",true},
        {"checks",{"animated home","card pointer opens detail","settings navigation","language dropdown",
                    "schedule enabled","four digit keypad input","23:30 saved","screen-off pauses animation","touch wake resumes animation"}}}).dump(2)<<'\n';
    std::cout<<"README native frames: "<<language<<" PASS\n";
}
void renderPairedTrial(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    auto data=monitor::demo();monitor::init(data);
    static int starts=0,forgets=0,cancels=0,approvals=0;
    monitor::setPairedTrialCallbacks([]{++starts;},[]{++forgets;},[]{++cancels;});
    monitor::setPairingCallbacks(nullptr,[](bool accepted){if(accepted)++approvals;});
    for(const auto* language:monitor::i18n::languages) {
        monitor::Settings settings;settings.language=language;monitor::setSettings(settings);
        monitor::showSettings();monitor::showPairingWait();advance();click(300,430);
        require(monitor::selectedAccount()==-4,"Phone trial enrollment navigation failed");
        monitor::updatePairedTrial(true,false,false);
        capture(folder/(std::string(language)+"-enrollment.ppm"));
        data.connected=false;monitor::apply(data);advance();
        require(monitor::selectedAccount()==-4,"PC disconnect closed phone enrollment");
        monitor::showPhonePairing(123456);advance();
        data.connected=true;monitor::apply(data);advance();
        require(monitor::selectedAccount()==-5,"PC snapshot closed phone comparison");
        capture(folder/(std::string(language)+"-comparison.ppm"));
        click(600,404);require(monitor::selectedAccount()==-4,"Phone approval did not preserve trial status page");
        monitor::updatePairedTrial(true,true,true);
        capture(folder/(std::string(language)+"-connected.ppm"));
        monitor::updatePairedTrial(true,true,false);advance();
        capture(folder/(std::string(language)+"-disconnected.ppm"));
        click(630,430);require(monitor::selectedAccount()==-6,"Phone-only forget confirmation missing");
        click(170,360);require(monitor::selectedAccount()==-4,"Forget cancel failed");
        require(forgets==monitor::i18n::index(language),"Cancel deleted phone pairing");
        click(630,430);click(600,360);require(monitor::selectedAccount()==-4,"Forget did not return to phone trial");
        monitor::updatePairedTrial(false,false,false);
        capture(folder/(std::string(language)+"-unavailable.ppm"));
        click(100,430);require(monitor::selectedAccount()==-1,"Phone trial back failed");
    }
    require(starts==5&&forgets==5&&approvals==5&&cancels==10,"Phone trial callbacks or cancel isolation failed");
    static int toggles=0;static bool enabled=true;
    monitor::setPairedPresenceControl([](bool value){++toggles;enabled=value;});
    for(const auto* language:monitor::i18n::languages) {
        monitor::Settings settings;settings.language=language;monitor::setSettings(settings);
        monitor::PairedPresenceStatus status;
        status.available=status.bonded=status.connected=status.enabled=true;
        status.state=monitor::PairedPresenceState::Connected;
        monitor::updatePairedPresence(status);monitor::showPhonePairingWait();advance();
        capture(folder/(std::string(language)+"-presence-connected.ppm"));
        const int before=toggles;click(80,195);
        require(toggles==before+1&&!enabled,"Paired presence disable callback failed");
        status.enabled=false;status.state=monitor::PairedPresenceState::Disabled;
        monitor::updatePairedPresence(status);advance();
        capture(folder/(std::string(language)+"-presence-disabled.ppm"));
        click(80,195);require(toggles==before+2&&enabled,"Paired presence enable callback failed");
        status.enabled=true;status.connected=false;status.state=monitor::PairedPresenceState::DisconnectedGrace;status.graceRemainingSeconds=42;
        monitor::updatePairedPresence(status);advance();
        capture(folder/(std::string(language)+"-presence-grace.ppm"));
        status.state=monitor::PairedPresenceState::Absent;status.graceRemainingSeconds=0;
        monitor::updatePairedPresence(status);advance();
        capture(folder/(std::string(language)+"-presence-absent.ppm"));
        status.available=false;status.state=monitor::PairedPresenceState::Unavailable;
        monitor::updatePairedPresence(status);advance();
        capture(folder/(std::string(language)+"-presence-unavailable.ppm"));
    }
    std::ofstream report(folder/"checks.json");
    report<<nlohmann::json({{"passed",true},{"mock_ble",true},{"hardware_tested",false},
        {"paired_presence_controls",true},{"presence_state_screens",true},
        {"languages",{"ko","zh-CN","ja","en","de"}},
        {"checks",{"enrollment navigation","PC disconnect preserves phone page","PC snapshot preserves phone comparison",
         "number approval","connected/disconnected/unavailable status","phone-only forget and cancel","back cancels enrollment"}}}).dump(2)<<'\n';
    std::cout<<"Five-language paired trial LCD: PASS\n";
}
void renderLocales(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    auto data=monitor::demo();monitor::init(data);
    for(int index=0;index<5;index++) {
        monitor::showSettings();click(530,116); // Keep an unsaved interval while translating.
        auto* dropdown=findDropdown(lv_scr_act());require(dropdown!=nullptr,"Language selector missing");
        lv_dropdown_set_selected(dropdown,static_cast<uint16_t>(index));
        lv_event_send(dropdown,LV_EVENT_VALUE_CHANGED,nullptr);advance();
        const std::string code=monitor::i18n::languages[index];
        const auto* title=monitor::i18n::text("설정",code);
        require(findLabel(lv_scr_act(),title)!=nullptr,"Settings language did not change");
        capture(folder/("lcd-"+code+"-settings.ppm"));
        click(735,432);require(monitor::settings().language==code,"Language save failed");
        require(monitor::settings().pollSeconds==60+15*(index+1),"Language switch lost another draft field");
        capture(folder/("lcd-"+code+"-home.ppm"));
        monitor::showSettings();click(665,185);require(monitor::selectedAccount()==-7,"Schedule button failed");
        capture(folder/("lcd-"+code+"-schedule.ppm"));
        click(440,176);capture(folder/("lcd-"+code+"-time-input.ppm"));
        click(400,432); // Cancel direct entry without touching the schedule draft.
        click(600,432);require(!monitor::settings().sleepEnabled,"Cancel changed the screen schedule");
        monitor::showPairingWait();capture(folder/("lcd-"+code+"-pairing.ppm"));
        monitor::showPairing(632814);capture(folder/("lcd-"+code+"-code.ppm"));
        monitor::showHome();monitor::showDetail(0);capture(folder/("lcd-"+code+"-detail.ppm"));
    }
    monitor::showHome();advance();
    monitor::showSettings();click(665,185);click(65,118);click(530,176);click(735,432);
    require(monitor::settings().sleepEnabled,"Schedule checkbox/save failed");
    require(monitor::settings().sleepStartMinute==1395,"Schedule time adjustment/save failed");
    monitor::showSettings();click(665,185);click(440,176);
    click(400,140);click(280,207);click(520,207);click(400,338);click(520,338);
    require(findLabel(lv_scr_act(),"24:60")!=nullptr,"Invalid direct time was not kept for correction");
    click(280,140);click(520,140);click(400,338);click(280,273);click(520,338);
    click(735,432);
    require(monitor::settings().sleepStartMinute==787,"Direct HH:MM entry/save failed");
    monitor::showSettings();click(665,185);click(440,244);
    click(400,140);click(520,140);click(400,207);click(520,273);click(520,338);
    click(65,432);click(665,185);click(735,432);
    require(monitor::settings().sleepEndMinute==1439,"Direct end time was lost through settings navigation");
    monitor::showSettings();click(665,185);click(440,244);click(280,140);click(520,338);
    require(findLabel(lv_scr_act(),"1_:__")!=nullptr,"Incomplete direct time was accepted");
    click(400,432);click(735,432);
    require(monitor::settings().sleepEndMinute==1439,"Keypad cancel changed the saved end time");
    monitor::showSettings();click(665,185);click(65,118);click(735,432);
    require(!monitor::settings().sleepEnabled,"Schedule disable/save failed");
    monitor::showHome();advance();
    {
        auto credited=monitor::demo();
        const auto now=std::time(nullptr);
        const double soon=static_cast<double>(now)+14*3600+19*60+50;
        const double later=static_cast<double>(now)+30*3600+5*60;
        credited.utcOffsetMinutes=9*60;
        credited.accounts[0].resetCreditCount=3;
        credited.accounts[0].resetCredits[0]={1,soon,"UTC"};
        credited.accounts[0].resetCredits[1]={1,later,""};
        credited.accounts[0].resetCredits[2]={1,static_cast<double>(now)+200*3600,""};
        credited.accounts[1].resetCreditCount=2;
        credited.accounts[1].resetCredits[0]={1,static_cast<double>(now)+2*3600+9*60+50,""};
        credited.accounts[1].resetCredits[1]={1,static_cast<double>(now)+52*3600,""};
        credited.accounts[2].resetCreditCount=2;
        credited.accounts[2].resetCredits[0]={1,static_cast<double>(now)+7*3600+44*60+50,""};
        credited.accounts[2].resetCredits[1]={1,static_cast<double>(now)+74*3600,""};
        auto display=monitor::settings(); display.language="ko"; monitor::setSettings(display);
        monitor::apply(credited); advance();
        for(int slot=0;slot<5;slot++) require(findLabel(monitor::cardObject(slot),"리셋권")==nullptr,"Home card still displays the redundant reset-credit badge");
        const char* creditTimes[]={"14h 20m","2h 10m","7h 45m"};
        const char* secondCreditTimes[]={"1d 6h 5m","2d 4h","3d 2h"};
        for(int slot=0;slot<3;slot++) {
            auto* card=monitor::cardObject(slot);
            auto* expiry=findLabel(card,creditTimes[slot]);
            require(expiry!=nullptr,"A Codex card omitted its reset-credit expiry on the five-account home screen");
            require(std::string(lv_label_get_text(expiry)).find("R.T.1 ")==0,"First credit is not labelled R.T.1");
            require(!lv_obj_has_flag(expiry,LV_OBJ_FLAG_HIDDEN),"A Codex card hid its reset-credit expiry");
            lv_area_t cardBounds,expiryBounds,imageBounds;
            lv_obj_get_coords(card,&cardBounds);lv_obj_get_coords(expiry,&expiryBounds);
            lv_obj_get_coords(findImage(card),&imageBounds);
            require(expiryBounds.y1>imageBounds.y2&&expiryBounds.y2<cardBounds.y2,"Codex reset credit overlaps primary content or leaves its card");
            auto* secondExpiry=findLabel(card,secondCreditTimes[slot]);
            auto* planReset=findLabel(card,"Plan 5h :");
            require(secondExpiry!=nullptr,"A Codex card omitted its second reset-credit expiry");
            require(std::string(lv_label_get_text(secondExpiry)).find("R.T.2 ")==0,"Second credit is not labelled R.T.2");
            require(findLabel(card,"UTC")==nullptr,"Home still displays reset-credit timezones");
            require(planReset!=nullptr,"Showing Codex reset credits removed the plan reset schedule");
            lv_area_t secondBounds,planBounds;
            lv_obj_get_coords(secondExpiry,&secondBounds);lv_obj_get_coords(planReset,&planBounds);
            require(secondBounds.y1>expiryBounds.y2&&secondBounds.y2<planBounds.y1,"Codex credit rows overlap one another or the plan reset");
            require(secondBounds.x1>=cardBounds.x1&&secondBounds.x2<=cardBounds.x2&&planBounds.y2<=cardBounds.y2,"Codex credit or plan reset leaves its card");
        }
        capture(folder/"credits-home-5-codex.ppm");
        for(int count=1;count<=5;count++) {
            auto layout=credited;
            layout.accounts.resize(count);
            for(auto& account:layout.accounts) {
                account.resetCreditCount=credited.accounts[0].resetCreditCount;
                account.resetCredits=credited.accounts[0].resetCredits;
            }
            monitor::apply(layout); advance();
            for(int slot=0;slot<count;slot++) {
                auto* card=monitor::cardObject(slot);
                require(lv_img_get_zoom(findImage(card))==256,"Credits changed a home character scale");
                lv_area_t cardBounds,firstBounds,secondBounds;
                lv_obj_get_coords(card,&cardBounds);
                auto* firstExpiry=findLabel(card,"R.T.1 ");
                auto* secondExpiry=findLabel(card,"R.T.2 ");
                require(firstExpiry&&secondExpiry,"Changing the account count hid reset-credit expiries");
                lv_obj_get_coords(firstExpiry,&firstBounds);lv_obj_get_coords(secondExpiry,&secondBounds);
                for(const auto& bounds:{firstBounds,secondBounds})
                    require(bounds.x1>=cardBounds.x1&&bounds.x2<=cardBounds.x2&&bounds.y1>=cardBounds.y1&&bounds.y2<=cardBounds.y2,"A home reset-credit expiry is clipped");
                require(firstBounds.x2<secondBounds.x1||firstBounds.y2<secondBounds.y1,"Home reset-credit expiries overlap");
                const auto& account=layout.accounts[slot];
                for(int pool=0;pool<std::min(account.poolCount,2);pool++) {
                    const auto percentage=std::to_string(static_cast<int>(account.pools[pool].remaining))+"%";
                    auto* value=findLabel(card,percentage);
                    require(value!=nullptr&&std::string(lv_label_get_text(value))==percentage,"Credits merged the pool name into the percentage");
                    require(lv_obj_get_style_text_font(value,0)->line_height==25,"Credits changed the pixel percentage font");
                    require(lv_obj_get_height(lv_obj_get_parent(lv_obj_get_parent(value)))==51,"Credits compressed a quota meter");
                }
            }
            capture(folder/("credits-home-"+std::to_string(count)+".ppm"));
        }
        auto roomy=credited; roomy.accounts.resize(3);
        monitor::apply(roomy); advance();
        auto* remain=findLabel(monitor::cardObject(0),"14h 20m");
        auto* second=findLabel(monitor::cardObject(0),"1d 6h 5m");
        require(remain!=nullptr,"Reset credit remaining time missing on the home card");
        require(lv_obj_get_style_text_font(remain,0)->line_height>=27,"First reset credit remaining time is not enlarged");
        require(second!=nullptr,"Second reset credit missing on the home card");
        require(lv_obj_get_style_text_font(second,0)->line_height<lv_obj_get_style_text_font(remain,0)->line_height,"Second reset credit is not smaller than the first");
        require(findLabel(monitor::cardObject(0),"UTC")==nullptr,"Home still displays reset-credit timezones");
        require(findLabel(monitor::cardObject(0),"8d 8h")==nullptr,"Third reset credit was drawn on the home card");
        require(findLabel(monitor::cardObject(0),"개")==nullptr,"Reset credit count prefix was drawn");
        lv_area_t cardArea, textArea; lv_obj_get_coords(monitor::cardObject(0),&cardArea); lv_obj_get_coords(remain,&textArea);
        require(textArea.x1>=cardArea.x1&&textArea.y1>=cardArea.y1&&textArea.x2<=cardArea.x2&&textArea.y2<=cardArea.y2,"Reset credit text escaped its card");
        lv_area_t firstArea; lv_obj_get_coords(remain,&firstArea); lv_obj_get_coords(second,&textArea);
        require(textArea.y1>=cardArea.y1&&textArea.y2<=cardArea.y2,"Second reset credit escaped its card");
        require(textArea.y1>firstArea.y1,"Second reset credit is not below the first");
        capture(folder/"19-reset-credits.ppm");
        monitor::apply(credited); advance();
        auto* plan=findLabel(monitor::cardObject(3),"Plan Reset M :");
        require(plan!=nullptr&&lv_obj_get_style_text_font(plan,0)->line_height>=22,"Plan reset stayed at the old size");
        monitor::showDetail(0); advance();
        require(lv_img_get_zoom(findImage(lv_scr_act()))==512,"Reset credits shrank the detail character");
        auto* detailValue=findLabel(lv_scr_act(),"72%");
        require(detailValue!=nullptr&&lv_obj_get_style_text_font(detailValue,0)->line_height==40,"Detail percentage lost its original font");
        lv_area_t meterBounds,valueBounds;
        lv_obj_get_coords(lv_obj_get_parent(detailValue),&meterBounds);lv_obj_get_coords(detailValue,&valueBounds);
        require(valueBounds.y1>=meterBounds.y1&&valueBounds.y2<=meterBounds.y2,"Detail percentage is clipped vertically");
        require(findLabel(lv_scr_act(),"14h 20m")!=nullptr,"Detail reset-credit remaining time missing");
        auto* detailRemain=findLabel(lv_scr_act(),"14h 20m");
        auto* detailSecond=findLabel(lv_scr_act(),"1d 6h 5m");
        require(detailRemain!=nullptr&&lv_obj_get_style_text_font(detailRemain,0)->line_height>=27,"Detail remaining time was not enlarged");
        require(detailSecond!=nullptr&&lv_obj_get_style_text_font(detailSecond,0)->line_height<lv_obj_get_style_text_font(detailRemain,0)->line_height,"Detail second reset credit is not smaller");
        require(findLabel(lv_scr_act(),"8d 8h")==nullptr,"Third reset credit was drawn on the detail screen");
        require(findLabel(lv_scr_act(),"1개  ")==nullptr,"Detail reset credit count prefix was drawn");
        require(findLabel(lv_scr_act(),"UTC")==nullptr,"Detail still displays reset-credit timezones");
        const time_t local=static_cast<time_t>(soon+9*3600);
        std::tm parts{}; require(gmtime_s(&parts,&local)==0,"Could not format the expected expiry");
        char when[24]; std::snprintf(when,sizeof(when),"%d/%d %02d:%02d",parts.tm_mon+1,parts.tm_mday,parts.tm_hour,parts.tm_min);
        require(findLabel(lv_scr_act(),when)==nullptr,"Detail still displays the absolute credit expiry");
        auto* detailPlan=findLabel(lv_scr_act(),"Plan 5h : 2h 18m");
        require(detailPlan!=nullptr&&lv_obj_get_style_text_font(detailPlan,0)->line_height>=22,"Detail plan reset was not enlarged");
        capture(folder/"20-reset-credit-detail.ppm");
        auto* limits=lv_obj_get_parent(detailRemain);
        require(lv_obj_has_flag(limits,LV_OBJ_FLAG_SCROLLABLE),"Detail credits are not reachable by scrolling");
        lv_obj_scroll_to_y(limits,LV_COORD_MAX,LV_ANIM_OFF); advance();
        lv_area_t viewport, creditBounds;
        lv_obj_get_coords(limits,&viewport);lv_obj_get_coords(detailSecond,&creditBounds);
        require(creditBounds.y1>=viewport.y1&&creditBounds.y2<=viewport.y2,"Scrolled detail credit is outside the viewport");
        capture(folder/"21-reset-credit-detail-scrolled.ppm");
        monitor::showHome(); advance();
    }
    monitor::setDisplayAwake(false);const std::vector<uint32_t> sleepingPixels(pixels.begin(),pixels.end());advance(2000);
    require(std::equal(pixels.begin(),pixels.end(),sleepingPixels.begin()),"Sleeping display kept animating");
    monitor::setDisplayAwake(true);advance(1500);
    require(!std::equal(pixels.begin(),pixels.end(),sleepingPixels.begin()),"Character animation did not resume after waking");
    // Invalid language/settings must fail before they can change the screen.
    auto fixture=nlohmann::json{{"protocol","quotamon.v1"},{"produced_at",1800000000},
        {"config_revision",0},{"connected",true},{"mock",true},{"accounts",nlohmann::json::array()},
        {"display",{{"poll_seconds",60},{"brightness_percent",70},{"show_reset",true},
                    {"show_updated",true},{"show_credits",true},{"show_session",true}}}};
    require(monitor::decode(fixture).settings.language=="ko","Legacy language default failed");
    fixture["display"]["language"]="fr";bool rejected=false;
    try {monitor::decode(fixture);}catch(const std::exception&){rejected=true;}
    require(rejected,"Unsupported language accepted");
    fixture["display"]["language"]="de";fixture["display"]["sleep_enabled"]=true;
    fixture["display"]["sleep_start_minute"]=600;fixture["display"]["sleep_end_minute"]=600;rejected=false;
    try {monitor::decode(fixture);}catch(const std::exception&){rejected=true;}
    require(rejected,"Equal sleep endpoints accepted");
    std::ofstream result(folder/"lcd-checks.json");
    result<<R"({"passed":true,"mock_data":true,"hardware_tested":false,"languages":["ko","zh-CN","ja","en","de"],"checks":["real language selector event","unsaved interval survives language switch","language save","schedule checkbox/time/save","direct HH:MM keypad entry/save","invalid direct time kept for correction","keypad cancel preserves draft","home/settings/schedule/keypad/detail/pairing rendered","legacy snapshot","invalid language and schedule rejected"]})";
    std::cout<<"Localized LCD UI: PASS\n";
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
        if(argc>=3&&std::wstring(argv[1])==L"--render-animation") { renderAnimation(argv[2]); return 0; }
        if(argc>=3&&std::wstring(argv[1])==L"--render-locales") { renderLocales(argv[2]); return 0; }
        if(argc>=3&&(std::wstring(argv[1])==L"--render-paired-trial"||std::wstring(argv[1])==L"--render-paired-presence")) { renderPairedTrial(argv[2]); return 0; }
        if(argc>=3&&std::wstring(argv[1])==L"--render-account-counts") {renderAccountCounts(argv[2]);return 0;}
        if(argc>=4&&std::wstring(argv[1])==L"--render-readme") {
            std::string language;for(const auto c:std::wstring(argv[2])) {require(c<=127,"Invalid language code");language.push_back(static_cast<char>(c));}
            renderReadme(language,argv[3]);return 0;
        }
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
