#pragma once
// [private build] "everything unlocked": the three cities (no barriers, no wanted level, roads open),
// every shop / gym / club entrance the story would unlock, max health, stamina, skills.
class CUnlockAll
{
public:
    static bool ms_bEnabled;      // server-config `unlock_all`, default on
    static void Process();        // applies once after connecting (and keeps the stat/enex state)
    static void ApplyNow();       // force (menu)
private:
    static bool ms_bApplied;
    static unsigned int ms_nLastCheck;
};
