#include "stdafx.h"
#include "CGenkidama.h"
#include <chrono>

bool CGenkidama::ms_bVoting = false;
CNetworkPlayer* CGenkidama::ms_pCaster = nullptr;
std::vector<CNetworkPlayer*> CGenkidama::ms_vPending;
int CGenkidama::ms_nAsked = 0, CGenkidama::ms_nAccepted = 0;
uint32_t CGenkidama::ms_nDeadline = 0;
uint32_t CGenkidama::ms_nCooldownUntil = 0;
uint32_t CGenkidama::ms_nBusyUntil = 0;

static const uint32_t VOTE_TIMEOUT_MS = 10000;
static const uint32_t COOLDOWN_MS = 5 * 60 * 1000;
static const uint32_t BUSY_MS = 60000;  // longest a genkidama can take, if the caster never says it exploded

static uint32_t NowMs()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void CGenkidama::Deny(CNetworkPlayer* player, int reason, int seconds)
{
    Packets::Genkidama::Genkidama p{};
    p.phase = Packets::Genkidama::Genkidama::DENIED;
    p.value = (uint16_t)reason;
    p.pos.x = (float)seconds;
    p.senderPlayerid = player->m_playerId;
    GetPacketFactory().Send(p, player);
}

void CGenkidama::Resolve()
{
    ms_bVoting = false;
    bool all = ms_nAccepted >= ms_nAsked;
    bool half = ms_nAccepted * 2 >= ms_nAsked;
    if (!half)
    {
        printf("[Genkidama] : %s no junto energia (%d de %d)\n", ms_pCaster->m_Name, ms_nAccepted, ms_nAsked);
        Deny(ms_pCaster, 2, 0);
        ms_pCaster = nullptr;
        return;
    }
    Packets::Genkidama::Genkidama p{};
    p.phase = Packets::Genkidama::Genkidama::START;
    p.value = all ? 10 : 20;
    p.senderPlayerid = ms_pCaster->m_playerId;
    GetPacketFactory().SendToAll(p);
    ms_nCooldownUntil = NowMs() + COOLDOWN_MS;
    ms_nBusyUntil = NowMs() + BUSY_MS;
    printf("[Genkidama] : %s carga una genkidama de %d s (%d de %d dieron energia)\n", ms_pCaster->m_Name, (int)p.value,
        ms_nAccepted, ms_nAsked);
}

void CGenkidama::OnPacket(Packets::Genkidama::Genkidama* packet, CNetworkPlayer* player)
{
    using P = Packets::Genkidama::Genkidama;
    uint32_t now = NowMs();
    switch (packet->phase)
    {
    case P::REQUEST:
    {
        if (ms_bVoting || (ms_pCaster && now < ms_nBusyUntil))
        {
            Deny(player, 3, 0);
            return;
        }
        if (now < ms_nCooldownUntil)
        {
            Deny(player, 1, (int)((ms_nCooldownUntil - now + 999) / 1000));
            return;
        }
        ms_pCaster = player;
        ms_vPending.clear();
        for (auto p : CNetworkPlayerManager::m_pPlayers)
            if (p != player)
                ms_vPending.push_back(p);
        ms_nAsked = (int)ms_vPending.size();
        ms_nAccepted = 0;
        ms_bVoting = true;
        ms_nDeadline = now + VOTE_TIMEOUT_MS;
        if (ms_vPending.empty())
        {
            Resolve();  // alone: all the energy there is
            return;
        }
        P ask{};
        ask.phase = P::ASK;
        ask.senderPlayerid = player->m_playerId;
        for (auto p : ms_vPending)
            GetPacketFactory().Send(ask, p);
        printf("[Genkidama] : %s pide energia a %d jugador(es)\n", player->m_Name, ms_nAsked);
        break;
    }
    case P::REPLY:
    {
        if (!ms_bVoting)
            return;
        auto it = std::find(ms_vPending.begin(), ms_vPending.end(), player);
        if (it == ms_vPending.end())
            return;
        ms_vPending.erase(it);
        if (packet->accept)
            ms_nAccepted++;
        printf("[Genkidama] : %s %s su energia\n", player->m_Name, packet->accept ? "da" : "niega");
        if (ms_vPending.empty())
            Resolve();
        break;
    }
    case P::STATE:
    case P::THROW:
    case P::EXPLODE:
    case P::CANCEL:
    {
        if (player != ms_pCaster)
            return;
        packet->senderPlayerid = player->m_playerId;
        GetPacketFactory().SendToAll(*packet, player);
        if (packet->phase == P::EXPLODE || packet->phase == P::CANCEL)
        {
            ms_pCaster = nullptr;
            ms_nBusyUntil = 0;
        }
        break;
    }
    default:
        break;
    }
}

void CGenkidama::PlayerLeft(CNetworkPlayer* player)
{
    if (player == ms_pCaster)
    {
        if (ms_bVoting)
            ms_bVoting = false;
        else
        {
            Packets::Genkidama::Genkidama p{};
            p.phase = Packets::Genkidama::Genkidama::CANCEL;
            p.senderPlayerid = player->m_playerId;
            GetPacketFactory().SendToAll(p, player);
        }
        ms_pCaster = nullptr;
        ms_vPending.clear();
        ms_nBusyUntil = 0;
        return;
    }
    if (!ms_bVoting)
        return;
    auto it = std::find(ms_vPending.begin(), ms_vPending.end(), player);
    if (it != ms_vPending.end())
    {
        ms_vPending.erase(it);
        ms_nAsked--;
        if (ms_vPending.empty())
            Resolve();
    }
}

void CGenkidama::Update()
{
    if (ms_bVoting && NowMs() > ms_nDeadline)
    {
        printf("[Genkidama] : %d jugador(es) no contestaron\n", (int)ms_vPending.size());
        ms_vPending.clear();
        Resolve();
    }
}
