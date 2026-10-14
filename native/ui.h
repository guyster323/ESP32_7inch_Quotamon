#pragma once
#include "lvgl.h"
#include <array>
#include <string>
#include <vector>

namespace monitor {
enum class SourceState { Ready, Stale, Unknown };
enum class Provider { Codex, Cursor, Claude, Grok, Copilot };
struct Pool {
    std::string name;
    double remaining = -1; // -1 unknown, -2 unlimited, otherwise 0..100
    std::string reset;
};
struct Account {
    std::string title, profile, plan;
    std::array<Pool, 8> pools;
    int poolCount = 2;
    SourceState state = SourceState::Ready;
    std::string updated = "14:32:00";
    double monthlyUsed = 0, monthlyLimit = 0, sessionUsed = 0;
    Provider provider = Provider::Codex;
    std::string id;
    bool enabled = true;
    bool hasAmounts = false;
    bool hasSession = false;
    std::string sourceNote;
};
struct Snapshot {
    bool connected = true;
    bool mock = true;
    std::vector<Account> accounts;
};
struct Settings {
    int pollSeconds = 60;
    int brightnessPercent = 70;
    bool showReset = true;
    bool showUpdated = true;
    bool showCredits = true;
    bool showSession = true;
};
// All functions run on the LVGL UI thread. Transport and credential code are external.
void init(const Snapshot& data);
void apply(const Snapshot& data);
void showHome();
void showDetail(int account);
void showSettings();
void showAccounts();
void setSettings(const Settings& value);
using CommitCallback = void(*)(const Settings&, const Snapshot&);
void setCommitCallback(CommitCallback callback);
using ActionCallback = void(*)();
using PairingCallback = void(*)(bool accept);
void setRefreshCallback(ActionCallback callback);
void setEditingCallback(ActionCallback callback);
void notify(const char* message);
void setPairingCallbacks(ActionCallback start, PairingCallback confirm, ActionCallback forget = nullptr);
void showPairingWait();
void showPairing(uint32_t passkey);
const Settings& settings();
int selectedAccount(); // -1 home, -2 settings
int refreshRequests(); // simulator intent only; no network or credentials
lv_obj_t* cardObject(int index);
lv_obj_t* homeButton();
lv_obj_t* saveButton();
lv_obj_t* cancelButton();
Snapshot demo();
}
