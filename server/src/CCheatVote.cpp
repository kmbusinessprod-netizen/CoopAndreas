#include "stdafx.h"
#include "CCheatVote.h"
#include "network/packets/cheats.h"
#include <chrono>

bool CCheatVote::ms_bActive = false;
int CCheatVote::ms_nCheatIndex = 0;
CNetworkPlayer* CCheatVote::ms_pProposer = nullptr;
std::vector<CNetworkPlayer*> CCheatVote::ms_vPending;
uint32_t CCheatVote::ms_nDeadline = 0;
Packets::Cheats::CheatState CCheatVote::ms_state{};

static const uint32_t VOTE_TIMEOUT_MS = 20000;

static uint32_t NowMs()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void CCheatVote::Apply(int cheatIndex, CNetworkPlayer* proposer)
{
    Packets::Cheats::CheatActivated packet{};
    packet.senderPlayerid = proposer->m_playerId;
    packet.cheatIndex = cheatIndex;
    packet.active = !ms_state.active[cheatIndex];  // the client reports the real state afterwards
    packet.apply = true;
    GetPacketFactory().SendToAll(packet);
    printf("[Trucos] : %s activo \"%s\" para todos\n", proposer->m_Name, CheatInfo::Name(cheatIndex));
}

void CCheatVote::Finish(bool accepted, CNetworkPlayer* rejecter)
{
    Packets::Cheats::CheatVote result{};
    result.phase = Packets::Cheats::CheatVote::RESULT;
    result.cheatIndex = ms_nCheatIndex;
    result.accept = accepted;
    result.senderPlayerid = rejecter ? rejecter->m_playerId : ms_pProposer->m_playerId;
    GetPacketFactory().SendToAll(result);

    if (accepted)
        Apply(ms_nCheatIndex, ms_pProposer);

    ms_bActive = false;
    ms_vPending.clear();
    ms_pProposer = nullptr;
}

void CCheatVote::Propose(CNetworkPlayer* proposer, int cheatIndex)
{
    if (cheatIndex < 0 || cheatIndex >= CheatInfo::COUNT || !CheatInfo::IsWorld(cheatIndex))
        return;

    if (ms_bActive)
    {
        Packets::Cheats::CheatVote busy{};
        busy.phase = Packets::Cheats::CheatVote::RESULT;
        busy.cheatIndex = cheatIndex;
        busy.accept = false;
        busy.senderPlayerid.value = playerid_t{};  // "nobody": busy
        GetPacketFactory().Send(busy, proposer);
        return;
    }

    ms_vPending.clear();
    for (auto p : CNetworkPlayerManager::m_pPlayers)
        if (p != proposer)
            ms_vPending.push_back(p);

    ms_bActive = true;
    ms_nCheatIndex = cheatIndex;
    ms_pProposer = proposer;
    ms_nDeadline = NowMs() + VOTE_TIMEOUT_MS;

    if (ms_vPending.empty())
    {
        Finish(true, nullptr);  // alone: no vote needed
        return;
    }

    Packets::Cheats::CheatVote ask{};
    ask.phase = Packets::Cheats::CheatVote::ASK;
    ask.cheatIndex = cheatIndex;
    ask.senderPlayerid = proposer->m_playerId;
    for (auto p : ms_vPending)
        GetPacketFactory().Send(ask, p);

    printf("[Trucos] : %s propone \"%s\", esperando a %d jugador(es)\n", proposer->m_Name, CheatInfo::Name(cheatIndex),
        (int)ms_vPending.size());
}

void CCheatVote::Reply(CNetworkPlayer* player, bool accept)
{
    if (!ms_bActive)
        return;
    auto it = std::find(ms_vPending.begin(), ms_vPending.end(), player);
    if (it == ms_vPending.end())
        return;

    if (!accept)
    {
        printf("[Trucos] : %s rechazo \"%s\"\n", player->m_Name, CheatInfo::Name(ms_nCheatIndex));
        Finish(false, player);
        return;
    }

    ms_vPending.erase(it);
    if (ms_vPending.empty())
        Finish(true, nullptr);
}

void CCheatVote::PlayerLeft(CNetworkPlayer* player)
{
    if (!ms_bActive)
        return;
    if (player == ms_pProposer)
    {
        ms_bActive = false;
        ms_vPending.clear();
        ms_pProposer = nullptr;
        return;
    }
    ms_vPending.erase(std::remove(ms_vPending.begin(), ms_vPending.end(), player), ms_vPending.end());
    if (ms_vPending.empty())
        Finish(true, nullptr);
}

void CCheatVote::Update()
{
    if (ms_bActive && NowMs() > ms_nDeadline)
    {
        printf("[Trucos] : votacion de \"%s\" sin respuesta\n", CheatInfo::Name(ms_nCheatIndex));
        Finish(false, nullptr);
    }
}

void CCheatVote::RecordState(const Packets::Cheats::CheatState& state)
{
    ms_state = state;
}

void CCheatVote::SendStateTo(CNetworkPlayer* player)
{
    GetPacketFactory().Send(ms_state, player);
}
