#include "stdafx.h"
#include "CFixes.h"
#include "CEntryExitManager.h"

// ---- radio --------------------------------------------------------------------------------------
// The game turns the radio on for "the player" when a player gets in a vehicle. Remote players are players
// too and their tasks run with CWorld::PlayerInFocus pointing at them, so their cars switched OUR radio on.
// Every radio-on call goes through here: only a vehicle the local player sits in may play.
static bool IsLocalPlayersVehicle(CVehicle* vehicle)
{
    CPlayerPed* me = FindPlayerPed(0);
    if (me == nullptr || vehicle == nullptr)
        return false;
    if (me->m_pVehicle == vehicle)
        return true;
    if (vehicle->m_pDriver == me)
        return true;
    for (int i = 0; i < 8; i++)
        if (vehicle->m_apPassengers[i] == me)
            return true;
    return false;
}

static CVehicle* VehicleOfAudioEntity(void* audioEntity)
{
    return *(CVehicle**)((uintptr_t)audioEntity + 4);  // CAEAudioEntity::m_pEntity
}

static void __fastcall RadioOn_Hook(void* audioEntity, int)  // CAEVehicleAudioEntity 0x4F5BA0
{
    if (IsLocalPlayersVehicle(VehicleOfAudioEntity(audioEntity)))
        plugin::CallMethod<0x4F5BA0>(audioEntity);
}

static void __fastcall RadioOnForVehicle_Hook(void* audioEntity, int)  // CAEVehicleAudioEntity 0x4F5B20
{
    if (IsLocalPlayersVehicle(VehicleOfAudioEntity(audioEntity)))
        plugin::CallMethod<0x4F5B20>(audioEntity);
}

void CFixes::Install()
{
    patch::RedirectCall({0x5022C9, 0x642113, 0x648A36, 0x64CAAA}, (void*)RadioOn_Hook);
    patch::RedirectCall({0x45F159, 0x50237C}, (void*)RadioOnForVehicle_Hook);
}

void CFixes::Process()
{
    if (!CNetwork::m_bAuthenticated)
        return;

    CPlayerPed* me = FindPlayerPed(0);

    // radio: only while *I* sit in a vehicle. After dying/respawning the game sometimes keeps the radio
    // of a vehicle another player drives (it is a CPlayerPed too) playing.
    static unsigned int lastRadioStop = 0;
    if (me != nullptr && AudioEngine.IsVehicleRadioActive() && GetTickCount() - lastRadioStop > 1000 &&
        (!me->m_nPedFlags.bInVehicle || !IsLocalPlayersVehicle(me->m_pVehicle)))
    {
        lastRadioStop = GetTickCount();
        AudioEngine.StopRadio(nullptr, false);
    }

    // entrances: a client inside an interior must always be able to leave, even when the host's script
    // has disabled the entry/exit markers (missions do that) - otherwise he is locked in
    if (!CLocalPlayer::m_bIsHost && CGame::currArea != 0 && CEntryExitManager::ms_bDisabled)
    {
        CEntryExitManager::ms_bDisabled = false;
    }
}
