#include "stdafx.h"
#include "CProximityOwnership.h"
#include "CNetworkPedManager.h"
#include "CNetworkVehicleManager.h"
#include <chrono>

static const float HANDOVER_MIN_SYNCER_DIST = 45.0f;  // syncer must be at least this far
static const float HANDOVER_MARGIN = 20.0f;           // and the other player this much closer
static const float HANDOVER_MAX_NEW_DIST = 120.0f;    // never hand to someone this far away
static const uint32_t HANDOVER_COOLDOWN_MS = 6000;

static uint32_t NowMs()
{
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static float Dist2D(const CVector& a, const CVector& b)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

bool CProximityOwnership::IsEnabled()
{
    static int cached = -1;
    if (cached < 0)
        cached = CConfigManager::GetConfigString("proximity_ownership", "1") != "0" ? 1 : 0;
    return cached == 1;
}

// closest player (with a known position) to `pos`, other than `except`
static CNetworkPlayer* ClosestPlayer(const CVector& pos, CNetworkPlayer* except, float& outDist)
{
    CNetworkPlayer* best = nullptr;
    outDist = 1e9f;
    for (auto p : CNetworkPlayerManager::m_pPlayers)
    {
        if (p == except || !p->m_bHasPos)
            continue;
        float d = Dist2D(pos, p->m_vecPos);
        if (d < outDist)
        {
            outDist = d;
            best = p;
        }
    }
    return best;
}

static bool ShouldHandOver(const CVector& pos, CNetworkPlayer* syncer, CNetworkPlayer*& outNew)
{
    outNew = nullptr;
    if (syncer == nullptr || !syncer->m_bHasPos)
        return false;

    float dSyncer = Dist2D(pos, syncer->m_vecPos);
    if (dSyncer < HANDOVER_MIN_SYNCER_DIST)
        return false;

    float dNew;
    CNetworkPlayer* candidate = ClosestPlayer(pos, syncer, dNew);
    if (candidate == nullptr || dNew > HANDOVER_MAX_NEW_DIST || dNew > dSyncer - HANDOVER_MARGIN)
        return false;

    outNew = candidate;
    return true;
}

static void HandOverPed(CNetworkPed* ped, CNetworkPlayer* newSyncer, uint32_t now)
{
    Packets::Peds::AssignPedSyncer packet{};
    packet.pedid = ped->m_pedId;

    if (ped->m_pSyncer)
    {
        GetPacketFactory().Send(packet, ped->m_pSyncer);  // "not yours anymore"
        auto& claims = ped->m_pSyncer->m_vPedClaims;
        claims.erase(std::remove(claims.begin(), claims.end(), ped), claims.end());
    }
    GetPacketFactory().Send(packet, newSyncer);  // "yours now"

    ped->m_pSyncer = newSyncer;
    ped->m_nLastHandover = now;
}

static void HandOverVehicle(CNetworkVehicle* vehicle, CNetworkPlayer* newSyncer, uint32_t now)
{
    vehicle->ReassignSyncer(newSyncer);
    vehicle->m_nLastHandover = now;
}

void CProximityOwnership::Update()
{
    static uint32_t lastRun = 0;
    uint32_t now = NowMs();
    if (now - lastRun < 700)
        return;
    lastRun = now;

    if (!IsEnabled() || CNetworkPlayerManager::m_pPlayers.size() < 2)
        return;

    // --- vehicles without any player inside: random traffic and parked cars -------------------
    for (auto vehicle : CNetworkVehicleManager::m_pVehicles)
    {
        if (vehicle->m_pSyncer == nullptr || vehicle->m_bIsTrain)
            continue;
        if (vehicle->m_nCreatedBy != RANDOM_VEHICLE && vehicle->m_nCreatedBy != PARKED_VEHICLE)
            continue;
        if (now - vehicle->m_nLastHandover < HANDOVER_COOLDOWN_MS)
            continue;

        bool playerInside = false;
        for (auto p : vehicle->m_pPlayers)
            playerInside |= p != nullptr;
        if (playerInside)
            continue;

        CNetworkPlayer* newSyncer;
        if (!ShouldHandOver(vehicle->m_vecPosition, vehicle->m_pSyncer, newSyncer))
            continue;

        CNetworkPlayer* oldSyncer = vehicle->m_pSyncer;
        HandOverVehicle(vehicle, newSyncer, now);

        // the NPCs inside go with the car
        for (auto ped : CNetworkPedManager::m_pPeds)
        {
            if (ped->m_vehicleId == vehicle->m_vehicleId && ped->m_pSyncer == oldSyncer)
                HandOverPed(ped, newSyncer, now);
        }

        printf("[Owner] : vehiculo %s %s -> %s\n", to_string(vehicle->m_vehicleId).c_str(),
            oldSyncer ? oldSyncer->m_Name : "?", newSyncer->m_Name);
    }

    // --- random peds on foot ------------------------------------------------------------------
    for (auto ped : CNetworkPedManager::m_pPeds)
    {
        if (ped->m_pSyncer == nullptr || ped->m_nCreatedBy != RANDOM_CHAR || !ped->m_bOnFoot)
            continue;
        if (now - ped->m_nLastHandover < HANDOVER_COOLDOWN_MS)
            continue;

        CNetworkPlayer* newSyncer;
        if (!ShouldHandOver(ped->m_vecPos, ped->m_pSyncer, newSyncer))
            continue;

        CNetworkPlayer* oldSyncer = ped->m_pSyncer;
        HandOverPed(ped, newSyncer, now);
        printf("[Owner] : ped %s %s -> %s\n", to_string(ped->m_pedId).c_str(), oldSyncer->m_Name, newSyncer->m_Name);
    }
}
