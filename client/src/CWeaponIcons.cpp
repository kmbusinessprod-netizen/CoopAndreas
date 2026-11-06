#include "stdafx.h"
#include "CWeaponIcons.h"
#include "CKamehameha.h"
#include "CGravityGun.h"
#include "WeaponIconData.h"

namespace
{
RwTexture* s_kame = nullptr;
RwTexture* s_gravity = nullptr;

RwTexture* Decode(const unsigned char* rle, size_t size)
{
    RwRaster* raster = RwRasterCreate(64, 64, 0, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    if (!raster)
        return nullptr;
    RwUInt32* pixels = (RwUInt32*)RwRasterLock(raster, 0, rwRASTERLOCKWRITE);
    if (!pixels)
        return nullptr;
    size_t i = 0, p = 0;
    while (i + 5 <= size && p < 64 * 64)
    {
        unsigned int count = rle[i], r = rle[i + 1], g = rle[i + 2], b = rle[i + 3], a = rle[i + 4];
        i += 5;
        RwUInt32 px = (a << 24) | (r << 16) | (g << 8) | b;  // D3D ARGB
        for (unsigned int k = 0; k < count && p < 64 * 64; k++)
            pixels[p++] = px;
    }
    RwRasterUnlock(raster);
    RwTexture* tex = RwTextureCreate(raster);
    if (tex)
        RwTextureSetFilterMode(tex, rwFILTERLINEAR);
    return tex;
}

uintptr_t s_find_orig = 0;

RwTexture* __cdecl FindIcon_Hook(RwTexDictionary* txd, const char* name)
{
    CPlayerPed* me = FindPlayerPed();
    if (me)
    {
        int weapon = me->m_aWeapons[me->m_nActiveWeaponSlot].m_eWeaponType;
        if (weapon == 1 && CKamehameha::IsWeaponEnabled())
        {
            if (!s_kame)
                s_kame = Decode(ICON_KAME, sizeof(ICON_KAME));
            if (s_kame)
                return s_kame;
        }
        if (weapon == 27 && CGravityGun::IsActive())
        {
            if (!s_gravity)
                s_gravity = Decode(ICON_GRAVITY, sizeof(ICON_GRAVITY));
            if (s_gravity)
                return s_gravity;
        }
    }
    return plugin::CallAndReturnDynGlobal<RwTexture*>(s_find_orig, txd, name);
}
}  // namespace

void CWeaponIcons::Install()
{
    // CHud::DrawWeaponIcon -> RwTexDictionaryFindNamedTexture(weapon txd, "<model>icon")
    s_find_orig = injector::GetBranchDestination(0x58D84B).as_int();
    patch::RedirectCall(0x58D84B, (void*)FindIcon_Hook);
}
