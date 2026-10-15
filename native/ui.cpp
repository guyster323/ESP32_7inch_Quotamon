#include "ui.h"
#include "generated/assets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

extern "C" {
LV_FONT_DECLARE(font_ko14);
LV_FONT_DECLARE(font_ko16);
LV_FONT_DECLARE(font_ko20);
LV_FONT_DECLARE(font_pixel25);
LV_FONT_DECLARE(font_pixel40);
}
namespace monitor {
namespace {
constexpr unsigned BG=0x081019, PANEL=0x101c29, BORDER=0x29414a;
constexpr unsigned TEXT=0xf7fbf7, MUTED=0x8ea8a8, TRACK=0xdee7de;
constexpr unsigned GREEN=0x19d79c, BLUE=0x65bbff, AMBER=0xffcb66, RED=0xff817f;
Snapshot current;
Settings committed, draft;
int page=-1, refreshCount=0;
int homePage=0;
CommitCallback onCommit=nullptr;
ActionCallback onRefresh=nullptr, onPairingStart=nullptr, onPairingForget=nullptr, onEditing=nullptr;
std::string feedback;
uint32_t feedbackAt=0;
PairingCallback onPairingConfirm=nullptr;
std::vector<bool> accountDraft;
std::vector<std::string> accountDraftIds;
lv_obj_t* detailLimits=nullptr;
std::array<int,5> visible{{-1,-1,-1,-1,-1}};
lv_obj_t *root=nullptr, *back=nullptr, *save=nullptr, *cancel=nullptr, *footer=nullptr;
lv_obj_t *intervalLabel=nullptr, *brightnessLabel=nullptr;
struct Meter { lv_obj_t *outer=nullptr, *fill=nullptr, *dark=nullptr, *light=nullptr, *track=nullptr; int width=0; const lv_font_t* font=&font_pixel25; };
struct Card { lv_obj_t* obj=nullptr; lv_obj_t* image=nullptr; lv_obj_t* note=nullptr; lv_obj_t* extra=nullptr; std::array<Meter,2> bars; };
std::array<Card,5> cards;
lv_color_t color(unsigned value) { return lv_color_hex(value); }
lv_obj_t* box(lv_obj_t* parent,int x,int y,int w,int h,unsigned bg,unsigned border=0,int radius=0) {
    auto* o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,static_cast<lv_coord_t>(x),static_cast<lv_coord_t>(y)); lv_obj_set_size(o,static_cast<lv_coord_t>(w),static_cast<lv_coord_t>(h));
    lv_obj_set_style_bg_color(o,color(bg),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,static_cast<lv_coord_t>(radius),0);
    if(border) { lv_obj_set_style_border_color(o,color(border),0); lv_obj_set_style_border_width(o,1,0); }
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE); return o;
}
lv_obj_t* label(lv_obj_t* parent,int x,int y,int w,const char* value,const lv_font_t* font=&font_ko14,unsigned ink=TEXT) {
    auto* o=lv_label_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,color(ink),0);
    lv_obj_set_pos(o,static_cast<lv_coord_t>(x),static_cast<lv_coord_t>(y)); lv_obj_set_width(o,static_cast<lv_coord_t>(w));
    lv_obj_set_height(o,font->line_height); lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_label_set_text(o,value); return o;
}
void action(lv_event_t* e);
lv_obj_t* button(lv_obj_t* parent,int x,int y,int w,const char* text,int actionId) {
    auto* o=box(parent,x,y,w,48,PANEL,BORDER,8);
    lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(o,color(0x223949),LV_STATE_PRESSED);
    auto* t=label(o,0,0,w,text,&font_ko16); lv_obj_center(t);
    lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_event_cb(o,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<intptr_t>(actionId)));
    return o;
}
unsigned meterColor(double value,int index) {
    if(value>=0&&value<=10) return RED;
    if(value>=0&&value<=20) return AMBER;
    switch(current.accounts[index].provider) {
        case Provider::Cursor: return BLUE;
        case Provider::Claude: return 0xffa765;
        case Provider::Grok: return 0xd886ff;
        case Provider::Copilot: return 0xab8cff;
        default: return GREEN;
    }
}
Meter meter(lv_obj_t* parent,int x,int y,int w,int h,const Pool& p,int index) {
    auto* outer=box(parent,x,y,w,h,0x0f1f29,BORDER,2);
    Meter m; m.outer=outer; m.width=w-4; m.font=h>=66?&font_pixel40:&font_pixel25; m.track=box(outer,2,2,w-4,h-4,TRACK);
    label(m.track,4,1,w-12,p.name.c_str(),&font_ko14,0x081010);
    m.dark=label(m.track,4,h-m.font->line_height-6,w-12,"",m.font,0x081010);
    lv_obj_set_style_text_align(m.dark,LV_TEXT_ALIGN_RIGHT,0);
    m.fill=box(m.track,0,0,w-4,h-4,meterColor(p.remaining,index));
    // Default LVGL parent clipping gives white text only where the fill exists.
    label(m.fill,4,1,w-12,p.name.c_str(),&font_ko14,TEXT);
    m.light=label(m.fill,4,h-m.font->line_height-6,w-12,"",m.font,TEXT);
    lv_obj_set_style_text_align(m.light,LV_TEXT_ALIGN_RIGHT,0);
    return m;
}
void setMeter(Meter& m,const Pool& p,int index,bool valid) {
    if(!m.fill) return;
    char buf[24]; double value=valid?p.remaining:-1;
    if(value>0&&value<1) std::snprintf(buf,sizeof(buf),"<1%%");
    else if(value>=0) std::snprintf(buf,sizeof(buf),"%d%%",static_cast<int>(std::floor(std::clamp(value,0.0,100.0))));
    else std::snprintf(buf,sizeof(buf),"%s",value==-2?"무제한":"--");
    auto* f=value==-2||(value>0&&value<1)?&font_ko20:m.font;
    lv_obj_set_style_text_font(m.dark,f,0); lv_obj_set_style_text_font(m.light,f,0);
    lv_label_set_text(m.dark,buf); lv_label_set_text(m.light,buf);
    int width=value>=0 ? static_cast<int>(std::lround(m.width*std::clamp(value,0.0,100.0)/100.0)):0;
    lv_obj_set_width(m.fill,static_cast<lv_coord_t>(std::max(1,width)));
    lv_obj_set_style_bg_color(m.fill,color(meterColor(value,index)),0);
    if(width==0) lv_obj_add_flag(m.fill,LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(m.fill,LV_OBJ_FLAG_HIDDEN);
}
const lv_img_dsc_t* sprite(int i,const Account& a,bool connected) {
    (void)i;
    int state=0; double minimum=100;
    for(int j=0;j<a.poolCount;j++) if(a.pools[j].remaining>=0) minimum=std::min(minimum,a.pools[j].remaining);
    if(!connected||a.state==SourceState::Unknown) state=4;
    else if(a.state==SourceState::Stale) state=5;
    else if(minimum==0) state=3;
    else if(minimum<=10) state=2;
    else if(minimum<=20) state=1;
    switch(a.provider) {
        case Provider::Cursor: return cursor_sprites[state];
        case Provider::Claude: return claude_sprites[state];
        case Provider::Grok: return grok_sprites[state];
        case Provider::Copilot: return copilot_sprites[state];
        default: return codex_sprites[state];
    }
}
std::string resetName(const std::string& name) {
    return name=="5H"?"5":name=="WEEKLY"?"W":name;
}
std::string resetTime(const Pool& pool) {
    return pool.reset.empty()?"미확인":pool.reset;
}
std::string resetSchedule(const Account& a) {
    if(a.provider==Provider::Cursor) {
        for(int j=0;j<a.poolCount;j++) if(!a.pools[j].reset.empty()&&a.pools[j].reset!="시각 미확인")
            return "Reset W "+resetTime(a.pools[j]);
        return "Reset W 미확인";
    }
    const Pool *five=nullptr,*weekly=nullptr;
    for(int j=0;j<a.poolCount;j++) {
        if(a.pools[j].name=="5H") five=&a.pools[j];
        if(a.pools[j].name=="WEEKLY") weekly=&a.pools[j];
    }
    if(five) return "Reset 5 "+resetTime(*five)+(weekly?" | W "+resetTime(*weekly):"");
    if(weekly) return "Reset W "+resetTime(*weekly);
    int selected=0;
    for(int j=0;j<a.poolCount;j++) if(a.pools[j].remaining>=0&&(a.pools[selected].remaining<0||a.pools[j].remaining<a.pools[selected].remaining)) selected=j;
    const auto& p=a.pools[selected];
    if(p.remaining==-1) return "일부 잔여량 미확인";
    if(p.remaining==-2) return "무제한 · 제공된 정책 기준";
    return "Reset "+resetName(p.name)+" "+resetTime(p);
}
std::string note(int index) {
    const auto& a=current.accounts[index];
    if(!current.connected) return "연결 대기 · 데이터 없음";
    if(a.state==SourceState::Unknown) return a.sourceNote.empty()?"확인 불가 · 수집 상태 확인":a.sourceNote;
    if(a.state==SourceState::Stale) return "오래된 정보 · 마지막 수집 확인";
    if(a.provider==Provider::Copilot&&committed.showCredits&&a.hasAmounts) { char b[96]; std::snprintf(b,sizeof(b),"%.1f / %.1f AIC 남음",std::max(0.0,a.monthlyLimit-a.monthlyUsed),a.monthlyLimit); return b; }
    if(!committed.showReset) return a.plan;
    return resetSchedule(a);
}
void footerText() {
    if(!footer) return;
    std::string text=current.connected?"Orca · BLE 연결 · "+std::to_string(committed.pollSeconds)+"초 갱신":"Orca 연결 대기 · 표시 데이터 삭제됨";
    if(current.mock) text="예시 데이터 · "+text;
    int enabled=0; for(const auto& a:current.accounts) if(a.enabled) ++enabled;
    if(enabled>5) text+="   |   "+std::to_string(homePage+1)+" / "+std::to_string((enabled+4)/5);
    if(!feedback.empty()&&lv_tick_get()-feedbackAt<8000) text=feedback;
    if(current.connected&&committed.showUpdated) {
        text+="\n최근 갱신 "+current.refreshed;
        int source=page>=0?page:visible[0]>=0?visible[0]:-1;
        if(source>=0&&static_cast<size_t>(source)<current.accounts.size())
            text+=(page>=0?"   |   계정 수집 ":"   |   첫 카드 수집 ")+current.accounts[source].updated;
    }
    lv_label_set_text(footer,text.c_str());
}
void createFooter() {
    footer=label(root,20,436,760,"",&font_ko14,MUTED);
    lv_obj_set_height(footer,40);
    lv_obj_set_style_text_line_space(footer,0,0);
    lv_label_set_long_mode(footer,LV_LABEL_LONG_CLIP);
}
void updateNote(Card& c,int account) {
    auto text=note(account);
    const int width=lv_obj_get_width(c.obj)-20,height=lv_obj_get_height(c.obj);
    const bool twoLines=lv_txt_get_width(text.c_str(),static_cast<uint32_t>(text.size()),&font_ko14,0,LV_TEXT_FLAG_NONE)>width;
    if(twoLines) {
        const auto divider=text.find(" | ");
        if(divider!=std::string::npos) text.replace(divider,3,"\n");
    }
    const int bodyY=std::max(34,(height-108)/2),noteY=height-(twoLines?43:23);
    lv_obj_set_y(c.note,static_cast<lv_coord_t>(noteY));
    lv_obj_set_height(c.note,twoLines?40:20);
    lv_label_set_text(c.note,text.c_str());
    if(c.extra) {
        lv_obj_set_y(c.extra,static_cast<lv_coord_t>(noteY-21));
    }
    const int barHeight=twoLines?(c.extra?33:44):51;
    for(int j=0;j<2;j++) if(c.bars[j].outer) {
        auto& m=c.bars[j];
        lv_obj_set_y(m.outer,static_cast<lv_coord_t>(bodyY+j*(barHeight+4)));
        lv_obj_set_height(m.outer,static_cast<lv_coord_t>(barHeight));
        lv_obj_set_height(m.track,static_cast<lv_coord_t>(barHeight-4));
        lv_obj_set_height(m.fill,static_cast<lv_coord_t>(barHeight-4));
        lv_obj_set_y(m.dark,static_cast<lv_coord_t>(barHeight-m.font->line_height-6));
        lv_obj_set_y(m.light,static_cast<lv_coord_t>(barHeight-m.font->line_height-6));
    }
    const int imageHeight=std::clamp(noteY-bodyY-(c.extra?21:0),1,108);
    if(c.image) lv_img_set_zoom(c.image,static_cast<uint16_t>(imageHeight*256/108));
}
void updateCards() {
    lv_obj_update_layout(root);
    for(int i=0;i<5;i++) {
        auto& c=cards[i]; if(!c.obj||visible[i]<0) continue;
        int account=visible[i]; const auto& a=current.accounts[account];
        const bool valid=current.connected&&a.state!=SourceState::Unknown;
        updateNote(c,account);
        for(int j=0;j<std::min(a.poolCount,2);j++) setMeter(c.bars[j],a.pools[j],account,valid);
        if(c.image) lv_img_set_src(c.image,sprite(account,a,current.connected));
        double minimum=100; for(int j=0;j<a.poolCount;j++) if(a.pools[j].remaining>=0) minimum=std::min(minimum,a.pools[j].remaining);
        unsigned ink=!valid?MUTED:a.state==SourceState::Stale?AMBER:minimum<=10?RED:minimum<=20?AMBER:MUTED;
        lv_obj_set_style_text_color(c.note,color(ink),0);
    }
    footerText();
}
void newPage() {
    lv_obj_t* old=root; root=lv_obj_create(nullptr); lv_obj_remove_style_all(root);
    lv_obj_set_style_bg_color(root,color(BG),0); lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE); lv_scr_load(root);
    if(old) lv_obj_del(old);
    cards={}; back=save=cancel=footer=nullptr; detailLimits=nullptr;
}
void card(int slot,int i,int x,int y,int w,int h) {
    auto& c=cards[slot]; const auto& a=current.accounts[i];
    c.obj=box(root,x,y,w,h,PANEL,BORDER,8); lv_obj_add_flag(c.obj,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(c.obj,color(0x1b3040),LV_STATE_PRESSED);
    lv_obj_add_event_cb(c.obj,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<intptr_t>(100+i)));
    label(c.obj,10,7,w-20,a.title.c_str(),&font_ko16);
    int bodyY=std::max(34,(h-108)/2);
    c.image=lv_img_create(c.obj); lv_obj_set_pos(c.image,10,static_cast<lv_coord_t>(bodyY)); lv_img_set_pivot(c.image,0,0); lv_img_set_src(c.image,sprite(i,a,current.connected));
    const int mx=108,mw=w-118;
    for(int j=0;j<std::min(a.poolCount,2);j++) c.bars[j]=meter(c.obj,mx,bodyY+j*55,mw,51,a.pools[j],i);
    if(a.poolCount==1) {
        label(c.obj,mx,bodyY+62,mw,a.plan.c_str(),&font_ko14,MUTED);
        label(c.obj,mx,bodyY+85,mw,"터치해서 상세 확인",&font_ko14,MUTED);
    } else if(a.poolCount>2) {
        c.extra=label(c.obj,10,h-44,w-20,("추가 "+std::to_string(a.poolCount-2)+"개 한도 · 상세에서 확인").c_str(),&font_ko14,MUTED);
    }
    c.note=label(c.obj,10,h-23,w-20,"",&font_ko14,MUTED);
    lv_label_set_long_mode(c.note,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(c.note,0,0);
}
void settingsLabels() {
    lv_label_set_text_fmt(intervalLabel,"%d초",draft.pollSeconds);
    lv_label_set_text_fmt(brightnessLabel,"%d%%",draft.brightnessPercent);
}
void action(lv_event_t* e) {
    const int id=static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if(id>=100&&id<200) showDetail(id-100);
    else if(id==1) showHome();
    else if(id==2) { ++refreshCount; if(onRefresh) onRefresh(); notify("Orca에 새로고침 요청됨"); }
    else if(id==3) showSettings();
    else if(id==4) {
        committed=draft;
        if(page==-3) for(auto& a:current.accounts) {
            auto found=std::find(accountDraftIds.begin(),accountDraftIds.end(),a.id);
            if(found!=accountDraftIds.end()) a.enabled=accountDraft[static_cast<size_t>(found-accountDraftIds.begin())];
        }
        if(onCommit) onCommit(committed,current);
        homePage=0; showHome();
    }
    else if(id==5) showHome();
    else if(id==6||id==7) { draft.pollSeconds=std::clamp(draft.pollSeconds+(id==6?-15:15),15,3600); settingsLabels(); }
    else if(id==8||id==9) { draft.brightnessPercent=std::clamp(draft.brightnessPercent+(id==8?-10:10),10,100); settingsLabels(); }
    else if(id>=10&&id<=13) {
        bool value=lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED);
        if(id==10) draft.showReset=value;
        else if(id==11) draft.showUpdated=value;
        else if(id==12) draft.showCredits=value;
        else draft.showSession=value;
    }
    else if(id==14) showAccounts();
    else if(id==15) { homePage=std::max(0,homePage-1); showHome(); }
    else if(id==16) { ++homePage; showHome(); }
    else if(id==17) { if(onPairingStart) onPairingStart(); showPairingWait(); }
    else if(id==18||id==19) { if(onPairingConfirm) onPairingConfirm(id==18); showHome(); }
    else if(id==20) {
        page=-6;newPage();label(root,40,40,720,"저장된 PC 페어링을 지울까요?",&font_ko20);
        label(root,40,130,720,"현재 연결이 끊어지고 Windows에서도 기존 장치를 제거해야 합니다.",&font_ko16,MUTED);
        button(root,40,340,344,"취소",1);button(root,416,340,344,"페어링 지우기",21);
    }
    else if(id==21) {if(onPairingForget)onPairingForget();if(onPairingStart)onPairingStart();showPairingWait();}
    else if(id>=200&&id<300) { const size_t i=static_cast<size_t>(id-200); if(i<accountDraft.size()) accountDraft[i]=lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED); }
}
}
void init(const Snapshot& data) { current=data; homePage=0; showHome(); }
void apply(const Snapshot& data) {
    std::string selectedId=page>=0&&static_cast<size_t>(page)<current.accounts.size()?current.accounts[page].id:"";
    bool shapeChanged=data.accounts.size()!=current.accounts.size();
    for(size_t i=0;!shapeChanged&&i<data.accounts.size();i++) {
        const auto& a=data.accounts[i]; const auto& b=current.accounts[i];
        shapeChanged=a.id!=b.id||a.title!=b.title||a.enabled!=b.enabled||a.provider!=b.provider||a.poolCount!=b.poolCount;
        for(int j=0;!shapeChanged&&j<std::clamp(a.poolCount,0,8);j++) shapeChanged=a.pools[j].name!=b.pools[j].name;
    }
    current=data;
    if(!current.connected) {
        current.accounts.clear();
        if(page>=-3) {showHome();return;}
    }
    for(auto& a:current.accounts) a.poolCount=std::clamp(a.poolCount,0,8);
    if(page==-1) { if(shapeChanged) showHome(); else updateCards(); }
    else if(page>=0) {
        auto found=std::find_if(current.accounts.begin(),current.accounts.end(),[&selectedId](const Account& a){return a.id==selectedId;});
        if(found!=current.accounts.end()) showDetail(static_cast<int>(found-current.accounts.begin())); else showHome();
    }
}
void showHome() {
    page=-1; newPage();
    label(root,20,17,340,"QUOTA  /  구독 잔여량",&font_ko20);
    button(root,570,12,100,"새로고침",2); button(root,682,12,98,"설정",3);
    std::vector<int> selected;
    for(size_t i=0;i<current.accounts.size();i++) if(current.accounts[i].enabled) selected.push_back(static_cast<int>(i));
    homePage=std::clamp(homePage,0,std::max(0,(static_cast<int>(selected.size())-1)/5));
    visible.fill(-1);
    if(selected.size()>5) { button(root,380,12,60,"<",15); button(root,450,12,60,">",16); }
    int count=std::min(5,static_cast<int>(selected.size())-homePage*5);
    if(count==0) {
        auto* p=box(root,20,72,760,360,PANEL,BORDER,8);
        label(p,32,48,680,current.connected?"나만의 구독 모니터를 시작하세요":"Orca 연결을 기다리고 있습니다",&font_ko20);
        label(p,32,104,680,current.connected?"Orca에서 표시할 계정을 선택하세요.":"Orca 실행 상태를 확인하거나 BLE로 새로 연결하세요.",&font_ko16,MUTED);
        label(p,32,149,680,"Codex · Cursor · Copilot · Claude · Grok",&font_ko16,GREEN);
        label(p,32,201,680,"Orca 사용자는 기존 로그인과 사용량을 그대로 연결할 수 있습니다.",&font_ko14,MUTED);
        button(p,32,264,200,"BLE 연결 시작",17);
        button(p,248,264,200,"계정 선택",14);
    }
    for(int slot=0;slot<count;slot++) {
        int i=selected[homePage*5+slot]; visible[slot]=i;
        if(count==5) {
            if(slot<3) card(slot,i,20+slot*257,72,slot==2?246:245,170);
            else card(slot,i,20+(slot-3)*386,254,374,178);
        } else if(count==4) card(slot,i,20+(slot%2)*386,72+(slot/2)*182,374,slot<2?170:178);
        else { int w=(760-(count-1)*12)/count; card(slot,i,20+slot*(w+12),72,w,360); }
    }
    createFooter(); updateCards();
}
void showDetail(int i) {
    if(i<0||static_cast<size_t>(i)>=current.accounts.size()) return;
    int scroll=(page==i&&detailLimits)?lv_obj_get_scroll_y(detailLimits):0;
    page=i; newPage(); const auto& a=current.accounts[i];
    back=button(root,20,12,100,"뒤로",1); label(root,140,22,500,a.title.c_str(),&font_ko20);
    button(root,680,12,100,"새로고침",2);
    auto* panel=box(root,20,80,760,350,PANEL,BORDER,8);
    label(panel,20,17,720,a.profile.c_str(),&font_ko20);
    label(panel,20,51,720,a.plan.c_str(),&font_ko14,MUTED);
    auto* img=lv_img_create(panel); lv_img_set_src(img,sprite(i,a,current.connected)); lv_img_set_antialias(img,false); lv_img_set_zoom(img,512); lv_obj_set_pos(img,64,124);
    auto* limits=box(panel,258,92,480,207,PANEL);
    detailLimits=limits;
    if(a.poolCount>2) {
        lv_obj_add_flag(limits,LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(limits,LV_DIR_VER);
        lv_obj_set_scrollbar_mode(limits,LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_bg_color(limits,color(MUTED),LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(limits,LV_OPA_70,LV_PART_SCROLLBAR);
        lv_obj_set_style_width(limits,4,LV_PART_SCROLLBAR);
        label(panel,260,51,475,(std::to_string(a.poolCount)+"개 한도 · 막대 영역을 위아래로 스크롤").c_str(),&font_ko14,MUTED);
    }
    for(int j=0;j<a.poolCount;j++) {
        auto m=meter(limits,2,j*98,465,66,a.pools[j],i);
        setMeter(m,a.pools[j],i,current.connected&&a.state!=SourceState::Unknown);
        const std::string reset=!current.connected?"연결 대기":a.state==SourceState::Unknown?"확인 불가":a.pools[j].remaining==-2?"무제한 · 비율 계산 없음":a.pools[j].remaining==-1?"잔여량 확인 불가":committed.showReset&&a.provider!=Provider::Cursor?"Reset "+resetName(a.pools[j].name)+" "+resetTime(a.pools[j]):"";
        label(limits,8,69+j*98,450,reset.c_str(),&font_ko14,MUTED);
    }
    if(a.provider==Provider::Copilot) {
        char b[180];
        bool valid=current.connected&&a.state!=SourceState::Unknown;
        if(valid&&a.hasAmounts) std::snprintf(b,sizeof(b),"월간 %.1f / %.1f AIC 사용",a.monthlyUsed,a.monthlyLimit);
        else std::snprintf(b,sizeof(b),"월간 사용량 --");
        if(committed.showCredits) label(limits,8,109,450,b,&font_ko16);
        if(valid&&a.hasSession) std::snprintf(b,sizeof(b),"Session %.1f AIC 사용 · 월간 한도와 별도",a.sessionUsed);
        else std::snprintf(b,sizeof(b),"Session -- · 원천 제공 없음");
        if(committed.showSession) label(limits,8,149,450,b,&font_ko16);
    }
    label(panel,260,303,474,note(i).c_str(),&font_ko14,MUTED);
    lv_obj_update_layout(limits); if(scroll>0) lv_obj_scroll_to_y(limits,static_cast<lv_coord_t>(scroll),LV_ANIM_OFF);
    createFooter(); footerText();
}
void showSettings() {
    if(onEditing)onEditing();
    page=-2; draft=committed; newPage();
    label(root,20,22,540,"설정",&font_ko20); button(root,630,12,150,"계정 관리",14);
    button(root,490,12,128,"BLE 연결",17);
    auto* panel=box(root,20,80,760,300,PANEL,BORDER,8);
    label(panel,20,22,220,"데이터 갱신 주기",&font_ko16);
    intervalLabel=label(panel,354,23,130,"",&font_ko20);
    button(panel,280,12,60,"−",6); button(panel,480,12,60,"+",7);
    label(panel,20,92,220,"화면 밝기",&font_ko16);
    brightnessLabel=label(panel,354,93,130,"",&font_ko20);
    button(panel,280,82,60,"−",8); button(panel,480,82,60,"+",9);
    auto check=[panel](int x,int y,const char* text,bool value,int id) {
        auto* cb=lv_checkbox_create(panel); lv_obj_set_pos(cb,static_cast<lv_coord_t>(x),static_cast<lv_coord_t>(y)); lv_checkbox_set_text(cb,text);
        lv_obj_set_style_text_font(cb,&font_ko16,0); lv_obj_set_style_text_color(cb,color(TEXT),0);
        lv_obj_set_style_pad_all(cb,12,0); lv_obj_set_size(cb,345,48);
        if(value) lv_obj_add_state(cb,LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb,action,LV_EVENT_VALUE_CHANGED,reinterpret_cast<void*>(static_cast<intptr_t>(id)));
    };
    check(20,146,"리셋까지 남은 시간 표시",draft.showReset,10);
    check(390,146,"마지막 갱신 시각 표시",draft.showUpdated,11);
    check(20,212,"Copilot 월간 AIC 표시",draft.showCredits,12);
    check(390,212,"Copilot Session AIC 표시",draft.showSession,13);
    settingsLabels();
    cancel=button(root,548,408,108,"취소",5); save=button(root,672,408,108,"저장",4);
    label(root,20,422,510,"저장한 설정은 연결된 Orca와 동기화됩니다.",&font_ko14,MUTED);
}
const Settings& settings() { return committed; }
void setSettings(const Settings& value) { committed=value; if(page==-1) updateCards(); }
void setCommitCallback(CommitCallback callback) { onCommit=callback; }
void setRefreshCallback(ActionCallback callback) { onRefresh=callback; }
void setEditingCallback(ActionCallback callback) { onEditing=callback; }
void notify(const char* message) {feedback=message;feedbackAt=lv_tick_get();footerText();}
void setPairingCallbacks(ActionCallback start, PairingCallback confirm, ActionCallback forget) { onPairingStart=start; onPairingConfirm=confirm; onPairingForget=forget; }
void showPairingWait() {
    page=-4; newPage();
    label(root,40,36,720,"Orca와 연결",&font_ko20);
    auto* panel=box(root,40,92,720,280,PANEL,BORDER,8);
    label(panel,32,38,656,"Orca 설정에서 디스플레이를 찾아 선택하세요.",&font_ko20);
    label(panel,32,98,656,"이름: Quotamon · 연결 허용 시간: 2분",&font_ko16,GREEN);
    label(panel,32,150,656,"두 화면의 6자리 번호가 일치할 때만 승인하세요.",&font_ko16,MUTED);
    back=button(root,40,404,120,"뒤로",1);
    button(root,560,404,200,"페어링 초기화",20);
}
void showPairing(uint32_t passkey) {
    page=-5; newPage();
    label(root,40,32,720,"Orca와 같은 번호인가요?",&font_ko20);
    auto* panel=box(root,40,92,720,252,PANEL,BORDER,8);
    char digits[8]; std::snprintf(digits,sizeof(digits),"%06u",static_cast<unsigned>(passkey%1000000));
    auto* pin=label(panel,32,52,656,digits,&font_pixel40,GREEN); lv_obj_set_style_text_align(pin,LV_TEXT_ALIGN_CENTER,0);
    label(panel,32,134,656,"Orca와 이 화면에서 각각 승인해야 연결됩니다.",&font_ko16,MUTED);
    label(panel,32,178,656,"요청하지 않은 연결이거나 번호가 다르면 거절하세요.",&font_ko14,MUTED);
    button(root,40,380,344,"다름 · 거절",19); button(root,416,380,344,"일치 · 승인",18);
}
void showAccounts() {
    if(onEditing)onEditing();
    page=-3; draft=committed; newPage(); accountDraft.clear(); accountDraftIds.clear();
    label(root,20,22,760,"표시할 구독 계정 선택",&font_ko20);
    auto* panel=box(root,20,80,760,300,PANEL,BORDER,8);
    lv_obj_add_flag(panel,LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(panel,LV_DIR_VER);
    if(current.accounts.empty()) label(panel,20,30,710,"Orca 설정에서 먼저 계정을 선택하세요.",&font_ko16,MUTED);
    for(size_t i=0;i<current.accounts.size();i++) {
        const auto& a=current.accounts[i]; accountDraft.push_back(a.enabled); accountDraftIds.push_back(a.id);
        auto* cb=lv_checkbox_create(panel); lv_obj_set_pos(cb,20,static_cast<lv_coord_t>(12+i*56));
        lv_checkbox_set_text(cb,(a.title+" · "+a.plan).c_str()); lv_obj_set_size(cb,710,48);
        lv_obj_set_style_text_font(cb,&font_ko16,0); lv_obj_set_style_text_color(cb,color(TEXT),0); lv_obj_set_style_pad_all(cb,10,0);
        if(a.enabled) lv_obj_add_state(cb,LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb,action,LV_EVENT_VALUE_CHANGED,reinterpret_cast<void*>(static_cast<intptr_t>(200+i)));
    }
    cancel=button(root,548,408,108,"취소",5); save=button(root,672,408,108,"저장",4);
    label(root,20,422,510,"표시 순서와 계정 이름은 Orca 설정에서 변경합니다.",&font_ko14,MUTED);
}
int selectedAccount() { return page; }
int refreshRequests() { return refreshCount; }
lv_obj_t* cardObject(int i) { return i>=0&&i<5?cards[i].obj:nullptr; }
lv_obj_t* homeButton() { return back; }
lv_obj_t* saveButton() { return save; }
lv_obj_t* cancelButton() { return cancel; }
Snapshot demo() {
    Snapshot s; s.accounts.resize(5);
    s.refreshed="14:33:00";
    s.accounts[0]={"Codex / Orca","Orca 시스템 기본값","Business Premium",{{{"5H",72,"2h 18m"},{"WEEKLY",54,"3d 4h"}}}};
    s.accounts[1]={"Codex / Personal","Personal","Plus",{{{"5H",38,"1h 40m"},{"WEEKLY",18,"2d 8h"}}}};
    s.accounts[2]={"Codex / Team","Team","Business standard",{{{"5H",9,"46m"},{"WEEKLY",61,"4d 2h"}}}};
    s.accounts[3]={"Cursor / Ultra","개인 계정","Ultra",{{{"Cursor Models",82,"17d 0h"},{"Other Models",47,"17d 0h"}}}};
    s.accounts[4]={"Copilot / Business","회사 제공 계정","Business",{{{"MONTHLY",100,"16d 0h"},{"",-1,""}}}};
    s.accounts[4].poolCount=1;
    s.accounts[4].monthlyLimit=6500; // Illustrative fixture, never a production entitlement default.
    s.accounts[3].provider=Provider::Cursor;
    s.accounts[4].provider=Provider::Copilot; s.accounts[4].hasAmounts=true; s.accounts[4].hasSession=true;
    for(size_t i=0;i<s.accounts.size();i++) s.accounts[i].id="demo-"+std::to_string(i);
    return s;
}
}
