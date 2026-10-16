#pragma once
#include "ui.h"
#include "generated/translations.h"
#include <algorithm>
#include <cmath>
#include "../third_party/nlohmann/json.hpp"
#include <stdexcept>

namespace monitor {
struct DecodedSnapshot { Snapshot data; Settings settings; int revision=0; double producedAt=0; int utcOffsetMinutes=0; };
inline Provider decodeProvider(const std::string& name) {
    if(name=="codex") return Provider::Codex;
    if(name=="cursor") return Provider::Cursor;
    if(name=="claude") return Provider::Claude;
    if(name=="grok") return Provider::Grok;
    if(name=="copilot") return Provider::Copilot;
    throw std::runtime_error("Unknown provider");
}
inline DecodedSnapshot decode(const nlohmann::json& json) {
    if(!json.is_object()||json.at("protocol")!="quotamon.v1") throw std::runtime_error("Invalid snapshot protocol");
    DecodedSnapshot out;
    out.producedAt=json.at("produced_at").get<double>();
    if(!std::isfinite(out.producedAt)||out.producedAt<0||out.producedAt>4102444800.0) throw std::runtime_error("Invalid heartbeat timestamp");
    out.revision=json.at("config_revision").get<int>(); if(out.revision<0) throw std::runtime_error("Invalid config revision");
    out.utcOffsetMinutes=json.value("utc_offset_minutes",0);
    if(out.utcOffsetMinutes< -840||out.utcOffsetMinutes>840)throw std::runtime_error("UTC offset bounds");
    out.data.connected=json.at("connected").get<bool>(); out.data.mock=json.at("mock").get<bool>();
    out.data.refreshed=json.value("refreshed",std::string("미확인"));
    if(out.data.refreshed.size()>64) throw std::runtime_error("Refresh label size bound");
    const auto& rows=json.at("accounts"); if(!rows.is_array()||rows.size()>20) throw std::runtime_error("Account count bound");
    std::vector<std::string> identities;
    for(const auto& row:rows) {
        Account a;
        a.id=row.at("id").get<std::string>();
        if(a.id.empty()||a.id.size()>96||std::find(identities.begin(),identities.end(),a.id)!=identities.end()) throw std::runtime_error("Duplicate account id");
        identities.push_back(a.id);
        a.provider=decodeProvider(row.at("provider").get<std::string>());
        a.title=row.at("title").get<std::string>(); a.profile=row.at("profile").get<std::string>(); a.plan=row.at("plan").get<std::string>();
        if(a.title.size()>256||a.profile.size()>256||a.plan.size()>256) throw std::runtime_error("Label size bound");
        a.enabled=row.at("enabled").get<bool>();
        std::string state=row.at("state").get<std::string>();
        if(state=="ready") a.state=SourceState::Ready;
        else if(state=="stale") a.state=SourceState::Stale;
        else if(state=="unknown") a.state=SourceState::Unknown;
        else throw std::runtime_error("Unknown source state");
        a.updated=row.value("updated",std::string("미확인")); a.sourceNote=row.value("source_note",std::string());
        if(a.updated.size()>64||a.sourceNote.size()>256) throw std::runtime_error("Status size bound");
        const auto& pools=row.at("pools"); if(!pools.is_array()||pools.empty()||pools.size()>8) throw std::runtime_error("Pool count bound");
        a.poolCount=static_cast<int>(pools.size());
        for(int i=0;i<a.poolCount;i++) {
            const auto& p=pools[i]; auto& target=a.pools[i]; double value=p.at("remaining").get<double>();
            if(!std::isfinite(value)||!((value>=0&&value<=100)||value==-1||value==-2)) throw std::runtime_error("Remaining quota bound");
            target.remaining=value; target.name=p.at("name").get<std::string>(); target.reset=p.at("reset").get<std::string>();
            if(target.name.size()>96||target.reset.size()>160) throw std::runtime_error("Pool label size bound");
        }
        a.hasAmounts=row.value("has_amounts",false); a.hasSession=row.value("has_session",false);
        if(a.hasAmounts) {
            a.monthlyUsed=row.at("monthly_used").get<double>(); a.monthlyLimit=row.at("monthly_limit").get<double>();
            if(!std::isfinite(a.monthlyUsed)||!std::isfinite(a.monthlyLimit)||a.monthlyUsed<0||a.monthlyLimit<=0) throw std::runtime_error("AIC bounds");
        }
        if(a.hasSession) { a.sessionUsed=row.at("session_used").get<double>(); if(!std::isfinite(a.sessionUsed)||a.sessionUsed<0) throw std::runtime_error("Session AIC bounds"); }
        out.data.accounts.push_back(a);
    }
    const auto& d=json.at("display"); out.settings.pollSeconds=d.at("poll_seconds").get<int>(); out.settings.brightnessPercent=d.at("brightness_percent").get<int>();
    if(out.settings.pollSeconds<15||out.settings.pollSeconds>3600||out.settings.brightnessPercent<10||out.settings.brightnessPercent>100) throw std::runtime_error("Settings bounds");
    out.settings.showReset=d.at("show_reset").get<bool>(); out.settings.showUpdated=d.at("show_updated").get<bool>();
    out.settings.showCredits=d.at("show_credits").get<bool>(); out.settings.showSession=d.at("show_session").get<bool>();
    out.settings.language=d.value("language",std::string("ko"));
    if(!i18n::valid(out.settings.language))throw std::runtime_error("Unsupported language");
    out.settings.sleepEnabled=d.value("sleep_enabled",false);
    out.settings.sleepStartMinute=d.value("sleep_start_minute",1380);
    out.settings.sleepEndMinute=d.value("sleep_end_minute",420);
    if(out.settings.sleepStartMinute<0||out.settings.sleepStartMinute>1439||out.settings.sleepEndMinute<0||out.settings.sleepEndMinute>1439||
       (out.settings.sleepEnabled&&out.settings.sleepStartMinute==out.settings.sleepEndMinute))throw std::runtime_error("Screen schedule bounds");
    return out;
}
}
