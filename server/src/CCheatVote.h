#pragma once
// [private build] world cheats are voted: everyone must press Y. The server also remembers the
// world cheat state so a player that joins later starts with the same world.
class CCheatVote
{
public:
    static void Propose(CNetworkPlayer* proposer, int cheatIndex);
    static void Reply(CNetworkPlayer* player, bool accept);
    static void PlayerLeft(CNetworkPlayer* player);
    static void Update();  // timeouts

    static void RecordState(const Packets::Cheats::CheatState& state);
    static void SendStateTo(CNetworkPlayer* player);

private:
    static void Apply(int cheatIndex, CNetworkPlayer* proposer);
    static void Finish(bool accepted, CNetworkPlayer* rejecter);

    static bool ms_bActive;
    static int ms_nCheatIndex;
    static CNetworkPlayer* ms_pProposer;
    static std::vector<CNetworkPlayer*> ms_vPending;
    static uint32_t ms_nDeadline;
    static Packets::Cheats::CheatState ms_state;
};
