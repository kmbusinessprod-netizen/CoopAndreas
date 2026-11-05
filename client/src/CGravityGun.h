#pragma once
// [private build] gravity gun (native version of "ggun.cs" by Mr. Seemann so it works in coop).
// F2 > Armas > Gravity gun (or Alt+G) switches it on: a gun built from game objects appears in the hand.
// Right mouse: grab what the camera points at (cars, people, objects, up to 25 m). It floats in front
// of you. Left mouse: throw it. Right mouse again: drop it. Alt+G switches it off.
class CGravityGun
{
public:
    static void Toggle();
    static bool IsActive() { return ms_bEnabled; }
    static void Process();  // gameProcessEvent
    static void Shutdown();

private:
    static bool ms_bActive;   // gun model in the hand right now
    static bool ms_bEnabled;  // gravity mode on the combat shotgun
    static int ms_nGunObject, ms_nPart1, ms_nPart2, ms_nPart3, ms_nPart4;
    static CEntity* ms_pHeld;
    static int ms_nHeldType;  // 1 vehicle, 2 ped, 3 object
    static unsigned int ms_nLastShot;

    static void BuildGun();
    static void DestroyGun();
    static CEntity* FindTarget(const CVector& from, const CVector& dir, int& type);
    static void Grab(CEntity* e, int type);
    static void Hold(const CVector& from, const CVector& dir);
    static void Throw(const CVector& dir);
    static void Drop();
};
