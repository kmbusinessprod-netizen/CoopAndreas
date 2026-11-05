#pragma once
// [private build] cheat sync.
// Personal cheats (weapons, health, body, vehicle spawns...) apply locally and the others are told.
// World cheats (wanted level, weather, time, peds, traffic...) are proposed to everyone: they are
// applied on every machine only when all players press Y. The server remembers the world cheat
// state and sends it to players that join later.
class CCheatSync
{
public:
    static void Install();

    // game dispatcher, local player typed a cheat. returns true if the game may apply it now
    static bool OnLocalCheat(int cheatIndex);
    static void Process();  // sends the "state report" a frame after a world cheat was applied

    static void OnCheatActivated(Packets::Cheats::CheatActivated* packet);
    static void OnVote(Packets::Cheats::CheatVote* packet);
    static void OnState(Packets::Cheats::CheatState* packet);

    static bool IsWorldCheat(int cheatIndex) { return CheatInfo::IsWorld(cheatIndex); }
    static const char* GetCheatName(int cheatIndex) { return CheatInfo::Name(cheatIndex); }

private:
    static void ApplyLocally(int cheatIndex);
    static void ReportState();
    static int ms_nReportPending;
};
