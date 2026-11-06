#include "stdafx.h"
#include "CFlight.h"
#include "CKamehameha.h"
#include "CGenkidama.h"
#include "UI/CModMenu.h"

bool CFlight::ms_bFlyEnabled = false;
bool CFlight::ms_bSpeedEnabled = false;
int CFlight::ms_nRunLevel = 3;
int CFlight::ms_nFlyLevel = 3;
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
    int kind;
    float pitch;
};
std::vector<Remote> s_remote;

float RunMultiplier(int level)
{
    return 1.0f + (level - 1) * 0.45f;  // 1x .. ~5x
}
float FlySpeed(int level)
{
    return 0.15f + level * 0.17f;  // metres per frame at 50 fps: ~16 m/s .. ~93 m/s (x1.6 with sprint)
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

// point the body along a direction: the sky-dive pose lies along the ped's forward axis, so turning that
// axis towards the travel direction makes him dive, climb or go straight up head first
float s_bodyPitch = 0.0f;
void OrientBody(CPed* ped, float heading, float pitch)
{
    CVector f(-sinf(heading) * cosf(pitch), cosf(heading) * cosf(pitch), sinf(pitch));
    CVector r(cosf(heading), sinf(heading), 0.0f);  // right of the heading, stays horizontal
    CVector a(r.y * f.z - r.z * f.y, r.z * f.x - r.x * f.z, r.x * f.y - r.y * f.x);  // r x f
    CMatrix* m = ped->GetMatrix();
    m->right = r;
    m->up = f;
    m->at = a;
    ped->UpdateRwMatrix();
    ped->UpdateRwFrame();
}

// hovering still = standing on an invisible slab under the feet: the game then treats CJ as standing, so
// every weapon (and the kamehameha) works as on the ground
int s_platform = -1;
constexpr int PLATFORM_MODEL = 3095;  // flat 9x9 slab, used as invisible floor by many mods

void RemovePlatform()
{
    if (s_platform >= 0 && CPools::GetObject(s_platform))
        Command<Commands::DELETE_OBJECT>(s_platform);
    s_platform = -1;
}

bool PlacePlatform(CPed* me)
{
    if (CStreaming::ms_aInfoForModel[PLATFORM_MODEL].m_nLoadState != LOADSTATE_LOADED)
    {
        CStreaming::RequestModel(PLATFORM_MODEL, GAME_REQUIRED);
        return false;
    }
    CVector p = me->GetPosition();
    float top = 0.0f;
    if (CBaseModelInfo* mi = CModelInfo::ms_modelInfoPtrs[PLATFORM_MODEL])
        if (mi->m_pColModel)
            top = mi->m_pColModel->m_boundBox.m_vecMax.z;
    float z = p.z - 1.0f - top - 0.06f;  // the ped's origin is ~1 m above its feet
    if (s_platform < 0 || !CPools::GetObject(s_platform))
    {
        Command<Commands::CREATE_OBJECT>(PLATFORM_MODEL, p.x, p.y, z, &s_platform);
        if (s_platform < 0)
            return false;
        Command<Commands::SET_OBJECT_VISIBLE>(s_platform, false);
        Command<Commands::SET_OBJECT_COLLISION>(s_platform, true);
        Command<Commands::FREEZE_OBJECT_POSITION>(s_platform, true);
    }
    return true;
}

bool s_jetpack = false;
void EndJetpack(CPed* me)
{
    if (!s_jetpack)
        return;
    s_jetpack = false;
    if (CPoolGuard::IsPedAlive(me))
        Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(CPools::GetPedRef(me));  // takes the jetpack off
}

bool s_dropping = false;
int s_animKind = 0;  // 0 float, 1 forward, 2 backwards, 3 left, 4 right
const char* AnimFor(int kind)
{
    switch (kind)
    {
    case 1: return "FALL_SkyDive_Accel";
    case 6: return "FALL_SkyDive";  // forward, slow (bar under 3): the calm pose of the side ones
    case 2: return "PARA_decel";
    case 3: return "FALL_SkyDive_L";
    case 4: return "FALL_SkyDive_R";
    default: return "PARA_float";
    }
}

void Send()
{
    Packets::Skin::PlayerPower p{};
    p.power = 3;
    p.phase = (uint8_t)((s_flying ? 1 : 0) | (CFlight::ms_bSpeedEnabled ? 2 : 0));
    p.level = (float)CFlight::ms_nRunLevel;
    p.heading = (float)s_animKind;
    p.pitch = s_bodyPitch;
    GetPacketFactory().Send(p);
}

void StopFlying(CPlayerPed* me)
{
    if (!s_flying)
        return;
    s_flying = false;
    RemovePlatform();
    EndJetpack(me);
    StopAnim(me, s_anim);
    me->m_nPhysicalFlags.bApplyGravity = true;
    // keep the landing harmless for a moment
    s_flyingSince = GetTickCount();
    Send();
}
}  // namespace

bool CFlight::ms_bJump = false;
bool CFlight::ms_bCrouch = false;

bool CFlight::IsHovering()
{
    return s_flying && (s_animKind == 0 || s_animKind == 5);
}

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
        // two separate levels: the wheel changes the flight one while flying, the running one on the ground
        int& level = s_flying ? ms_nFlyLevel : ms_nRunLevel;
        level = std::max(1, std::min(10, level + ms_nWheel));
        ms_nWheel = 0;
        s_barUntil = now + 2500;
        Send();
    }

    if (!me)
        return;

    // allowed while floating: we just hold still in the air
    bool kame = CKamehameha::IsLocalActive() || CGenkidama::IsLocalActive();
    bool busy = me->m_nPedFlags.bInVehicle || me->m_fHealth <= 0.0f || (kame && !s_flying) || CGenkidama::IsLaunched();
    CPad* pad = CPad::GetPad(0);

    // ---- running speed
    if (ms_bSpeedEnabled && !s_flying && !busy)
        ApplyRunSpeed(me, RunMultiplier(ms_nRunLevel));

    // ---- flight
    if (!ms_bFlyEnabled || busy)
    {
        StopFlying(me);
    }
    else if (pad)
    {
        static bool jumpWas = false;
        bool jump = s_flying ? ms_bJump : pad->NewState.ButtonSquare != 0;
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

        // double tap crouch while floating: stop flying and fall (landing stays harmless)
        static bool crouchWas = false;
        static unsigned int lastCrouch = 0;
        bool crouch = ms_bCrouch;
        if (crouch && !crouchWas && s_flying && !kame)
        {
            if (now - lastCrouch < 350)
            {
                StopFlying(me);
                s_dropping = true;
            }
            lastCrouch = now;
        }
        crouchWas = crouch;
        if (s_dropping && me->m_nPedFlags.bIsStanding)
        {
            s_dropping = false;
            s_flyingSince = now;  // 1.5 s more without fall damage, then back to normal
        }
        if (s_dropping)
            s_flyingSince = now;

        bool anyInput = pad && (abs(pad->NewState.LeftStickX) > 25 || abs(pad->NewState.LeftStickY) > 25 || ms_bJump ||
                                   ms_bCrouch);
        // aiming while floating, with a weapon the jetpack can use (pistols and SMGs): the game's own jetpack
        // task does the aiming/shooting with its animations; we keep CJ in place
        CWeaponInfo* wi = CWeaponInfo::GetWeaponInfo(me->m_aWeapons[me->m_nActiveWeaponSlot].m_eWeaponType, 1);
        bool jetpackWeapon = wi && wi->m_nFlags.bAimWithArm;
        bool aiming = pad && pad->NewState.RightShoulder1 != 0 && jetpackWeapon;
        if (s_flying && !kame && aiming && !anyInput)
        {
            if (!s_jetpack)
            {
                OrientBody(me, me->m_fCurrentRotation, 0.0f);
                if (s_anim)
                    StopAnim(me, s_anim);
                Command<Commands::TASK_JETPACK>(CPools::GetPedRef(me));
                s_jetpack = true;
            }
            me->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
            me->m_nPhysicalFlags.bApplyGravity = false;
            s_animKind = 5;
            s_bodyPitch = 0.0f;
            static unsigned int lastAimSent = 0;
            if (now - lastAimSent > 500)
            {
                lastAimSent = now;
                Send();
            }
        }
        else if (s_flying && (kame || !anyInput))
        {
            EndJetpack(me);
            // floating still: no gravity, no movement, the floating pose (the kamehameha uses its own pose)
            RemovePlatform();
            me->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
            me->m_nPhysicalFlags.bApplyGravity = false;
            me->m_nPedFlags.bIsStanding = false;
            if (!kame)
            {
                s_animKind = 0;
                s_bodyPitch += (0.0f - s_bodyPitch) * 0.2f;
                PlayAnim(me, s_anim, AnimFor(0));
                OrientBody(me, me->m_fCurrentRotation, s_bodyPitch);
            }
            else
            {
                s_anim = nullptr;  // the floating pose comes back after the kamehameha
                s_animKind = 0;
            }
            static unsigned int lastHoverSent = 0;
            if (now - lastHoverSent > 500)
            {
                lastHoverSent = now;
                Send();
            }
        }
        else if (s_flying)
        {
            EndJetpack(me);
            const CVector& front = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
            CVector fwd(front.x, front.y, front.z);
            fwd.Normalise();
            float heading = atan2f(-front.x, front.y);
            CVector right(cosf(heading), sinf(heading), 0.0f);
            float sx = pad->NewState.LeftStickX / 128.0f, sy = -pad->NewState.LeftStickY / 128.0f;
            float speed = FlySpeed(ms_nFlyLevel) * (pad->NewState.ButtonCross ? 1.6f : 1.0f);  // sprint key = boost

            CVector v(0.0f, 0.0f, 0.0f);
            float targetPitch = 0.0f;
            if (sy > 0.2f)
            {
                // forward: where the camera looks, up and down included - head first
                s_animKind = ms_nFlyLevel < 3 ? 6 : 1;
                // the camera can't look straight up/down, so its angle is stretched: looking ~55° up = flying
                // practically straight up
                float camPitch = asinf(std::max(-1.0f, std::min(1.0f, fwd.z)));
                float pitch = std::max(-1.55f, std::min(1.55f, camPitch * 2.4f));
                CVector flatFwd(fwd.x, fwd.y, 0.0f);
                flatFwd.Normalise();
                CVector dir3 = flatFwd * cosf(pitch) + CVector(0.0f, 0.0f, sinf(pitch));
                v = dir3 * (sy * speed) + right * (sx * speed * 0.5f);
                // climbing is faster than cruising: the more the mouse points at the sky, the more the
                // vertical speed grows over the forward one (up to 2.3x straight up)
                if (pitch > 0.0f)
                    v.z *= 1.0f + 2.5f * sinf(pitch);  // straight up = 3.5x the bar speed
                targetPitch = pitch;
            }
            else if (sy < -0.2f)
            {
                // backwards: slower, upright, leaning back a bit
                s_animKind = 2;
                CVector flat(fwd.x, fwd.y, 0.0f);
                flat.Normalise();
                v = flat * (sy * speed * 0.55f) + right * (sx * speed * 0.4f);
                targetPitch = 0.0f;
            }
            else if (fabsf(sx) > 0.2f)
            {
                s_animKind = sx < 0.0f ? 3 : 4;
                v = right * (sx * speed * 0.7f);
            }
            else
                s_animKind = 0;

            // jump / crouch: float up and down without turning the body
            if (ms_bJump)
                v.z += speed * 0.95f;
            if (ms_bCrouch)
                v.z -= speed * 0.95f;

            me->m_vecMoveSpeed = v;
            me->m_nPhysicalFlags.bApplyGravity = false;
            me->m_nPedFlags.bIsStanding = false;
            me->m_fCurrentRotation = me->m_fAimingRotation = heading;

            s_bodyPitch += (targetPitch - s_bodyPitch) * 0.2f;  // smooth
            PlayAnim(me, s_anim, AnimFor(s_animKind));
            OrientBody(me, heading, (s_animKind == 1 || s_animKind == 6) ? s_bodyPitch : 0.0f);

            // land: going down onto the ground
            float groundZ = CWorld::FindGroundZForCoord(me->GetPosition().x, me->GetPosition().y);
            if (now - s_flyingSince > 800 && v.z <= 0.0f && me->GetPosition().z - groundZ < 1.3f)
                StopFlying(me);

            static int lastKind = -1;
            if (now - s_lastSent > 150 || lastKind != s_animKind)
            {
                lastKind = s_animKind;
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
        if (it->flying && it->kind == 5)
        {
            it->ped->m_vecMoveSpeed.z = std::max(it->ped->m_vecMoveSpeed.z, 0.0f) * 0.5f;
            it->ped->m_nPhysicalFlags.bApplyGravity = false;
        }
        else if (it->flying)
        {
            OrientBody(it->ped, it->ped->m_fCurrentRotation, (it->kind == 1 || it->kind == 6) ? it->pitch : 0.0f);
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
        s_remote.push_back({player->m_pPed, false, 0, nullptr, 0, 0.0f});
        r = &s_remote.back();
    }
    bool flying = (packet->phase & 1) != 0;
    r->level = (packet->phase & 2) ? (int)packet->level : 0;
    r->kind = (int)packet->heading;
    r->pitch = packet->pitch;
    if (flying && r->kind == 5)
        StopAnim(r->ped, r->anim);  // standing in the air: his own animations (aiming, shooting...)
    else if (flying)
        PlayAnim(r->ped, r->anim, AnimFor(r->kind));
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
        int lvl = s_flying ? ms_nFlyLevel : ms_nRunLevel;
        CRGBA col = i < lvl ? CRGBA(172, 203, 241, 255) : CRGBA(50, 60, 75, 255);  // HUD blue
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
