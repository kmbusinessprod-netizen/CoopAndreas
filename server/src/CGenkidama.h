#pragma once
#include "network/packets/genkidama.h"

// [private build] the Genkidama vote: the caster asks everyone for energy. Everybody gives -> 10 s charge,
// at least half -> 20 s, less -> denied. One at a time, and 3 minutes between two (server wide).
class CGenkidama
{
public:
    static void OnPacket(Packets::Genkidama::Genkidama* packet, CNetworkPlayer* player);
    static void PlayerLeft(CNetworkPlayer* player);
    static void Update();  // vote timeout

private:
    static void Resolve();
    static void Deny(CNetworkPlayer* player, int reason, int seconds);

    static bool ms_bVoting;
    static CNetworkPlayer* ms_pCaster;  // voting or charging/throwing
    static std::vector<CNetworkPlayer*> ms_vPending;
    static int ms_nAsked, ms_nAccepted;
    static uint32_t ms_nDeadline;
    static uint32_t ms_nCooldownUntil;
    static uint32_t ms_nBusyUntil;  // a genkidama is in the air / charging
};
