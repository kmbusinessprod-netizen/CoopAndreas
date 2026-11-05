#include "stdafx.h"
#include "CFixes.h"
#include "CEntryExitManager.h"

void CFixes::Process()
{
    if (!CNetwork::m_bAuthenticated)
        return;

    CPlayerPed* me = FindPlayerPed(0);

    // radio: only while *I* sit in a vehicle. After dying/respawning the game sometimes keeps the radio
    // of a vehicle another player drives (it is a CPlayerPed too) playing.
    static unsigned int lastRadioStop = 0;
    if (me != nullptr && !me->m_nPedFlags.bInVehicle && AudioEngine.IsVehicleRadioActive() &&
        GetTickCount() - lastRadioStop > 1000)
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
