#pragma once

// [private build] HUD icons of our own for the two "weapon mode" powers: the kamehameha (on the brass
// knuckles) and the gravity gun (on the combat shotgun). The game takes a weapon's icon from a texture
// "<model>icon" in the weapon's TXD; we step in that lookup and hand over our textures instead.
class CWeaponIcons
{
public:
    static void Install();
};
