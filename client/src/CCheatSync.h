#pragma once
// [private build] cheat sync.
// Every cheat typed by a player is announced to the others ("X activo un truco: ...").
// Cheats that change the world (wanted level, weather, time, peds, traffic) are applied on
// every machine so everybody sees the same thing; personal cheats (weapons, health, body,
// vehicle spawns) stay local to the player who typed them.
class CCheatSync
{
public:
    static void Install();

    // called from the game's cheat dispatcher for the local player
    static void OnLocalCheat(int cheatIndex);
    // called when another player activated a cheat
    static void OnRemoteCheat(const char* playerName, int cheatIndex, bool active);

    static bool IsWorldCheat(int cheatIndex);
    static const char* GetCheatName(int cheatIndex);
};
