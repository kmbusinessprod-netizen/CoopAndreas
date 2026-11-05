#pragma once
// [private build] Kamehameha (native version of the CLEO mod by Alan Cortes, so every player sees it).
// Tab + H on foot: charge, keep Tab held to fire, release Tab to stop. The beam follows the camera.
// The caster's machine makes the explosions (damage); the others draw the same beam and visual-only
// blasts on that player.
class CKamehameha
{
public:
    static void Process();  // gameProcessEvent
    static bool IsLocalActive();
    static void OnPacket(Packets::Skin::PlayerPower* packet);
    static void OnPlayerRemoved(CPed* ped);
};
