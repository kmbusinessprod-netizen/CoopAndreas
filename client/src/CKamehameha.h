#pragma once
// [private build] Kamehameha (native version of the CLEO mod by Alan Cortes, so every player sees it).
// Charge up to 8 s (bigger and longer beam the more you charge), fire for 8 s, steer it with the mouse.
// Keyboard: Alt+H charge, H fire, H/click stop. Weapon (brass knuckles): right click charge, left click fire/stop.
class CKamehameha
{
public:
    static void Process();  // gameProcessEvent
    static bool IsLocalActive();
    static bool IsLocalFiring();
    static bool IsWeaponEnabled();
    static void ToggleWeapon();
    static void OnPacket(Packets::Skin::PlayerPower* packet);
    static void OnPlayerRemoved(CPed* ped);
    static float ms_fMouseX, ms_fMouseY;  // mouse movement collected before the camera eats it
    static bool ms_bLmb, ms_bRmb;         // mouse buttons read before they are taken from the game
};
