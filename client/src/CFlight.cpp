#include "stdafx.h"
#include "CFlight.h"
#include "CKamehameha.h"
#include "UI/CModMenu.h"

bool CFlight::ms_bFlyEnabled = false;
bool CFlight::ms_bSpeedEnabled = false;
int CFlight::ms_nLevel = 3;
int CFlight::ms_nWheel = 0;

namespace
{
bool s_flying = false;
unsigned int s_flyingSince = 0;
unsigned int s_barUntil = 0;
unsigned int s_lastJumpInAir = 0;
bool s_wasCollisionProof = false;
const char* s_anim = nullptr;
unsigned int s_lastSent = 0;

struct Remote
{
    CPed* ped;
    bool flying;
    int level;  // run speed
    const char* anim;
};
std::vector<Remote> s_remote;

float RunMultiplier(int level)
{
    return 1.0f + (level - 1) * 0.45f;  // 1x .. ~5x
}
float FlySpeed(int level)
{
    return 0.12f + level * 0.09f;  // metres per frame at 50 fps: ~6 m/s .. ~50 m/s
}

void PlayAnim(CPed* ped, const char*& current, const char* anim)
{
    if (current == anim)
        return;
    current = anim;
    if (!Command<Commands::HAS_ANIMATION_LOADED>("PARACHUTE"))
    {
        Command<Commands::REQUEST_ANIMATION>("PARACHUTE");
        current = nullptr;  // try again next frame
        return;
    }
    Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(CPools::GetPedRef(ped), anim, "PARACHUTE", 4.0f, 1, 0, 0, 0, -1);
}

void StopAnim(CPed* ped, const char*& current)
{
    if (current && CPoolGuard::IsPedAlive(ped))
        Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(CPools::GetPedRef(ped));
    current = nullptr;
}

// faster walk/run/sprint: the movement animations move CJ, so playing them faster moves him faster
void ApplyRunSpeed(CPed* ped, float mult)
{
    if (!ped || !ped->m_pRwClump)
        return;
    for (CAnimBlendAssociation* a = RpAnimBlendClumpGetFirstAssociation(ped->m_pRwClump); a;
         a = RpAnimBlendGetNextAssociation(a))
    {
        if (a->m_nAnimId >= 0 && a->m_nAnimId <= 2)  // walk, run, sprint
            a->m_fSpeed = mult;
    }
}

void Send()
{
    Packets::Skin::PlayerPower p{};
    p.power = 3;
    p.phase = (uint8_t)((s_flying ? 1 : 0) | (CFlight::ms_bSpeedEnabled ? 2 : 0));
    p.level = (float)CFlight::ms_nLevel;
    p.heading = s_anim && strcmp(s_anim, "FALL_SkyDive_Accel") == 0 ? 1.0f : 0.0f;
    GetPacketFactory().Send(p);
}

void StopFlying(CPlayerPed* me)
{
    if (!s_flying)
        return;
    s_flying = false;
    StopAnim(me, s_anim);
    me->m_nPhysicalFlags.bApplyGravity = true;
    // keep the landing harmless for a moment
    s_flyingSince = GetTickCount();
    Send();
}
}  // namespace

bool CFlight::IsFlying()
{
    return s_flying;
}

bool CFlight::WantsWheel()
{
    bool alt = CPad::NewKeyState.lmenu != 0 || CPad::NewKeyState.rmenu != 0;
    return alt && (ms_bFlyEnabled || ms_bSpeedEnabled);
}

void CFlight::Process()
{
    CPlayerPed* me = FindPlayerPed(0);
    unsigned int now = GetTickCount();

    // speed bar: Alt + wheel
    if (ms_nWheel != 0)
    {
        ms_nLevel = std::max(1, std::min(10, ms_nLevel + ms_nWheel));
        ms_nWheel = 0;
        s_barUntil = now + 2500;
        Send();
    }

    if (!me)
        return;

    bool busy = me->m_nPedFlags.bInVehicle || me->m_fHealth <= 0.0f || CKamehameha::IsLocalActive();
    CPad* pad = CPad::GetPad(0);

    // ---- running speed
    if (ms_bSpeedEnabled && !s_flying && !busy)
        ApplyRunSpeed(me, RunMultiplier(ms_nLevel));

    // ---- flight
    if (!ms_bFlyEnabled || busy)
    {
        StopFlying(me);
    }
    else if (pad)
    {
        static bool jumpWas = false;
        bool jump = pad->NewState.ButtonSquare != 0;
        bool jumpPressed = jump && !jumpWas;
        jumpWas = jump;

        if (!s_flying)
        {
            // second jump while already in the air
            bool airborne = !me->m_nPedFlags.bIsStanding && !me->m_nPedFlags.bInVehicle && !me->m_nPhysicalFlags.bTouchingWater;
            if (jumpPressed && airborne && now - s_lastJumpInAir < 2000)
            {
                s_flying = true;
                s_flyingSince = now;
                s_wasCollisionProof = me->m_nPhysicalFlags.bCollisionProof;
                me->m_nPhysicalFlags.bCollisionProof = true;
                Send();
            }
            if (jumpPressed)
                s_lastJumpInAir = now;
            // no fall damage right after landing
            if (now - s_flyingSince > 1500 && me->m_nPhysicalFlags.bCollisionProof && !s_wasCollisionProof &&
                s_flyingSince != 0)
            {
                me->m_nPhysicalFlags.bCollisionProof = false;
                s_flyingSince = 0;
            }
        }

        if (s_flying)
        {
            const CVector& front = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
            CVector fwd(front.x, front.y, front.z);
            fwd.Normalise();
            CVector right(fwd.y, -fwd.x, 0.0f);
            right.Normalise();
            float sx = pad->NewState.LeftStickX / 128.0f, sy = -pad->NewState.LeftStickY / 128.0f;
            float speed = FlySpeed(ms_nLevel) * (pad->NewState.ButtonCross ? 1.6f : 1.0f);  // sprint key = boost
            CVector v = (fwd * sy + right * sx) * speed;
            if (pad->NewState.ButtonSquare)
                v.z += speed * 0.7f;
            if (pad->NewState.ShockButtonL)
                v.z -= speed * 0.7f;
            me->m_vecMoveSpeed = v;
            me->m_nPhysicalFlags.bApplyGravity = false;
            me->m_nPedFlags.bIsStanding = false;

            float heading = atan2f(-front.x, front.y);
            me->m_fCurrentRotation = me->m_fAimingRotation = heading;
            me->SetHeading(heading);
            bool moving = fabsf(sx) + fabsf(sy) > 0.2f;
            PlayAnim(me, s_anim, moving ? "FALL_SkyDive_Accel" : "PARA_float");

            // land: going down onto the ground
            float groundZ = CWorld::FindGroundZForCoord(me->GetPosition().x, me->GetPosition().y);
            if (now - s_flyingSince > 800 && v.z <= 0.0f && me->GetPosition().z - groundZ < 1.3f)
                StopFlying(me);

            if (now - s_lastSent > 500)
            {
                s_lastSent = now;
                Send();
            }
        }
    }

    // ---- the others
    for (auto it = s_remote.begin(); it != s_remote.end();)
    {
        if (!CPoolGuard::IsPedAlive(it->ped))
        {
            it = s_remote.erase(it);
            continue;
        }
        if (it->flying)
        {
            it->ped->m_vecMoveSpeed.z = std::max(it->ped->m_vecMoveSpeed.z, 0.0f) * 0.5f;  // no falling between updates
            it->ped->m_nPhysicalFlags.bApplyGravity = false;
        }
        else if (it->level > 0)
            ApplyRunSpeed(it->ped, RunMultiplier(it->level));
        ++it;
    }
}

void CFlight::OnPacket(Packets::Skin::PlayerPower* packet)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    if (!player || !player->m_pPed)
        return;
    Remote* r = nullptr;
    for (auto& x : s_remote)
        if (x.ped == player->m_pPed)
            r = &x;
    if (!r)
    {
        s_remote.push_back({player->m_pPed, false, 0, nullptr});
        r = &s_remote.back();
    }
    bool flying = (packet->phase & 1) != 0;
    r->level = (packet->phase & 2) ? (int)packet->level : 0;
    if (flying)
        PlayAnim(r->ped, r->anim, packet->heading > 0.5f ? "FALL_SkyDive_Accel" : "PARA_float");
    else if (r->flying)
    {
        StopAnim(r->ped, r->anim);
        r->ped->m_nPhysicalFlags.bApplyGravity = true;
    }
    r->flying = flying;
}

// SA-style bar at the bottom: shown while flying or for a moment after changing the level
void CFlight::Draw()
{
    if (!FrontEndMenuManager.m_bPrefsShowHud || !(ms_bFlyEnabled || ms_bSpeedEnabled))
        return;
    if (!s_flying && GetTickCount() > s_barUntil)
        return;
    float w = 160.0f, h = 9.0f;
    float x = (CUtil::SCREEN_BASE_WIDTH - w) * 0.5f, y = 400.0f;
    CSprite2d::DrawRect(CRect(CUtil::HUD_X(x - 3.0f), CUtil::HUD_Y(y - 3.0f), CUtil::HUD_X(x + w + 3.0f), CUtil::HUD_Y(y + h + 3.0f)),
        CRGBA(0, 0, 0, 200));
    float seg = w / 10.0f;
    for (int i = 0; i < 10; i++)
    {
        CRGBA col = i < ms_nLevel ? CRGBA(172, 203, 241, 255) : CRGBA(50, 60, 75, 255);  // HUD blue
        CSprite2d::DrawRect(CRect(CUtil::HUD_X(x + i * seg + 1.0f), CUtil::HUD_Y(y), CUtil::HUD_X(x + (i + 1) * seg - 1.0f),
                                CUtil::HUD_Y(y + h)),
            col);
    }
    CFont::SetBackground(false, false);
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_WIDTH);
    CFont::SetFontStyle(2);
    CFont::SetEdge(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));
    CFont::SetScale(CUtil::HUD_X(0.3f), CUtil::HUD_Y(0.7f));
    CFont::SetColor(CRGBA(225, 225, 225, 255));
    CFont::PrintString(CUtil::HUD_X(CUtil::SCREEN_BASE_WIDTH * 0.5f), CUtil::HUD_Y(y - 17.0f), s_flying ? "VUELO" : "VELOCIDAD");
}
