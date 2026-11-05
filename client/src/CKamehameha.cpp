#include "stdafx.h"
#include "CKamehameha.h"
#include "CSoundFile.h"
#include "CFlight.h"
#include "FxManager_c.h"
#include "CCoronas.h"

// How it works now:
//   charge: up to 8 s (sound "kame_cargar.wav"); the longer, the bigger and longer the beam
//   fire:   8 s (sound "kame_lanzar.wav"), can be cut short; the mouse steers it
// Controls:
//   keyboard: Alt+H starts charging, H fires, H (or a click) during the beam stops it
//   weapon:   with the "Kamehameha" weapon (on the brass knuckles) in the hands: hold right click to charge,
//             left click fires, left click during the beam stops it (letting go of right click while
//             charging cancels)
namespace
{
enum
{
    OFF = 0,
    CHARGE = 1,
    FIRE = 2
};

constexpr unsigned int MAX_CHARGE_MS = 8000;
constexpr unsigned int FIRE_MS = 8000;
constexpr int KAME_WEAPON = 1;  // brass knuckles

struct Caster
{
    CPed* ped = nullptr;
    int phase = OFF;
    unsigned int phaseSince = 0;
    float heading = 0.0f;  // radians
    float pitch = 0.0f;    // radians
    float level = 0.5f;    // 0..1 charge (fixed when firing)
    unsigned int lastBlast = 0;
    unsigned int lastSent = 0;
    bool local = false;
    bool animApplied = false;
    bool wasExplosionProof = false;
    unsigned int sound = 0;
    bool weaponMode = false;
};

Caster s_local;
std::vector<Caster> s_remote;
bool s_weaponEnabled = false;

// size/length from the charge: 4 s of charge = the size it had before (scale 1, 50 m)
float Scale(const Caster& c)
{
    return 0.3f + 1.4f * c.level;
}
float Length(const Caster& c)
{
    return 20.0f + 60.0f * c.level;
}

CVector Direction(const Caster& c)
{
    return CVector(-sinf(c.heading) * cosf(c.pitch), cosf(c.heading) * cosf(c.pitch), sinf(c.pitch));
}

CVector HandsPos(const Caster& c)
{
    CVector pos = c.ped->GetPosition();
    CVector dir = Direction(c);
    return pos + CVector(dir.x * 0.6f, dir.y * 0.6f, 0.3f);
}

float ChargeLevel(const Caster& c)
{
    if (c.phase != CHARGE)
        return c.level;
    return std::min(1.0f, (GetTickCount() - c.phaseSince) / (float)MAX_CHARGE_MS);
}

void Send(const Caster& c)
{
    Packets::Skin::PlayerPower p{};
    p.power = 0;
    p.phase = (uint8_t)c.phase;
    p.heading = c.heading;
    p.pitch = c.pitch;
    p.level = ChargeLevel(c);
    GetPacketFactory().Send(p);
}

void Sound(Caster& c, const char* file)
{
    CSoundFile::Stop(c.sound);
    c.sound = file ? CSoundFile::Play(file) : 0;
}

void UpdateSound(Caster& c)
{
    if (!c.sound)
        return;
    if (c.local)
        CSoundFile::SetVolume(c.sound, 1.0f);
    else if (CPoolGuard::IsPedAlive(c.ped))
        CSoundFile::Place(c.sound, c.ped->GetPosition());
}

void ApplyPose(Caster& c)
{
    if (c.animApplied || !CPoolGuard::IsPedAlive(c.ped))
        return;
    int ref = CPools::GetPedRef(c.ped);
    Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, "GunMove_BWD", "PED", 4.0f, 1, 1, 1, 1, -1);
    c.animApplied = true;
}

// GunMove_BWD is "walk backwards": the original pauses it at 27% so CJ stands with the arms forward
void HoldPose(Caster& c)
{
    if (!c.animApplied || !CPoolGuard::IsPedAlive(c.ped))
        return;
    int ref = CPools::GetPedRef(c.ped);
    if (Command<Commands::IS_CHAR_PLAYING_ANIM>(ref, "GunMove_BWD"))
    {
        Command<Commands::SET_CHAR_ANIM_SPEED>(ref, "GunMove_BWD", 0.0f);
        Command<Commands::SET_CHAR_ANIM_CURRENT_TIME>(ref, "GunMove_BWD", 0.27f);
    }
    c.ped->m_vecMoveSpeed.x = c.ped->m_vecMoveSpeed.y = 0.0f;
}

void ReleasePose(Caster& c)
{
    if (!c.animApplied)
        return;
    c.animApplied = false;
    if (CPoolGuard::IsPedAlive(c.ped))
        Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(CPools::GetPedRef(c.ped));
}

unsigned int CoronaId(const Caster& c, int i)
{
    return 0x4B414D00u + ((uintptr_t)c.ped & 0xFFFF) * 64 + i;
}

void Corona(const Caster& c, int i, const CVector& pos, float radius, CRGBA color)
{
    CCoronas::RegisterCorona(CoronaId(c, i), nullptr, color.r, color.g, color.b, color.a, pos, radius, 300.0f,
        eCoronaType::CORONATYPE_SHINYSTAR, eCoronaFlareType::FLARETYPE_NONE, false, false, 0, 0.0f, false, 1.5f, 0, 15.0f,
        false, false);
}

float BeamHit(const Caster& c, const CVector& from, const CVector& dir, float maxLength, CVector& hit)
{
    CColPoint colPoint{};
    CEntity* entity = nullptr;
    CVector to = from + dir * maxLength;
    bool oldUsesCollision = c.ped->m_bUsesCollision;
    c.ped->m_bUsesCollision = false;
    bool found = CWorld::ProcessLineOfSight(from, to, colPoint, entity, true, true, true, true, false, false, false, false);
    c.ped->m_bUsesCollision = oldUsesCollision;
    hit = found ? colPoint.m_vecPoint : to;
    return found ? (hit - from).Magnitude() : maxLength;
}

void Draw(Caster& c)
{
    if (!CPoolGuard::IsPedAlive(c.ped))
        return;

    unsigned int now = GetTickCount();
    float t = (now - c.phaseSince) / 1000.0f;
    CVector hands = HandsPos(c);

    if (c.phase == CHARGE)
    {
        // the ball in the hands grows with the charge
        float k = ChargeLevel(c);
        float pulse = 0.85f + 0.15f * sinf(t * (18.0f + 20.0f * k));
        Corona(c, 0, hands, (0.15f + 0.9f * k) * pulse, CRGBA(255, 255, 255, 230));
        Corona(c, 1, hands, (0.3f + 1.8f * k) * pulse, CRGBA(90, 170, 255, 150));
        Command<Commands::DRAW_LIGHT_WITH_RANGE>(hands.x, hands.y, hands.z, 120, 190, 255, 5.0f + 15.0f * k);
        return;
    }
    if (c.phase != FIRE)
        return;

    float scale = Scale(c);
    CVector dir = Direction(c);
    CVector hit;
    float length = BeamHit(c, hands, dir, Length(c), hit);

    // a cone: thin at the hands, wider further away; white core, blue glow, rainbow edge
    static const CRGBA rainbow[] = {CRGBA(255, 80, 200, 90), CRGBA(255, 230, 60, 90), CRGBA(80, 255, 120, 90),
        CRGBA(80, 200, 255, 90)};
    int i = 2;
    float grow = std::min(1.0f, t / 0.35f);
    float shown = length * grow;
    float stepLen = 1.6f * std::max(1.0f, Length(c) / 50.0f);
    for (float d = 0.6f; d < shown && i < 62; d += stepLen)
    {
        CVector p = hands + dir * d;
        float k = d / 50.0f;
        float wobble = 0.92f + 0.08f * sinf(t * 28.0f + d * 0.7f);
        Corona(c, i++, p, (0.55f + 2.4f * k) * wobble * scale, CRGBA(255, 255, 255, 220));
        Corona(c, i++, p, (1.1f + 4.2f * k) * wobble * scale, CRGBA(110, 180, 255, 120));
        if (((int)(d / stepLen)) % 2 == 0)
            Corona(c, i++, p, (1.4f + 5.2f * k) * wobble * scale, rainbow[((int)(d / 3.2f) + (int)(t * 8.0f)) % 4]);
    }
    Corona(c, i++, hands, 1.4f * scale, CRGBA(200, 230, 255, 210));
    if (grow >= 1.0f)
        Corona(c, i++, hit, 3.5f * scale, CRGBA(220, 235, 255, 200));
    Command<Commands::DRAW_LIGHT_WITH_RANGE>(hands.x, hands.y, hands.z, 120, 190, 255, 12.0f * scale);
    Command<Commands::DRAW_LIGHT_WITH_RANGE>(hit.x, hit.y, hit.z, 150, 200, 255, 20.0f * scale);

    if (now - c.lastBlast > 280 && grow >= 1.0f)
    {
        c.lastBlast = now;
        if (c.local)
        {
            if (length > 3.5f)
                Command<Commands::ADD_EXPLOSION_NO_SOUND>(hit.x, hit.y, hit.z, c.level > 0.6f ? 7 : 8);
        }
        else
        {
            RwV3d pos{hit.x, hit.y, hit.z};
            if (FxSystem_c* fx = g_fxMan.CreateFxSystem((char*)(c.level > 0.6f ? "explosion_large" : "explosion_medium"), &pos,
                    nullptr, false))
                fx->PlayAndKill();
        }
    }
}

void SetPhase(Caster& c, int phase)
{
    int old = c.phase;
    if (old == CHARGE && phase == FIRE)
        c.level = std::max(0.12f, ChargeLevel(c));
    c.phase = phase;
    c.phaseSince = GetTickCount();
    if (phase == OFF)
    {
        ReleasePose(c);
        Sound(c, nullptr);
        if (c.local)
        {
            if (CPoolGuard::IsPedAlive(c.ped))
                c.ped->m_nPhysicalFlags.bExplosionProof = c.wasExplosionProof;
            Command<Commands::RESTORE_CAMERA_JUMPCUT>();
        }
        return;
    }
    ApplyPose(c);
    Sound(c, phase == CHARGE ? "kame_cargar.wav" : "kame_lanzar.wav");
    if (old == OFF && c.local && CPoolGuard::IsPedAlive(c.ped))
    {
        c.wasExplosionProof = c.ped->m_nPhysicalFlags.bExplosionProof;
        c.ped->m_nPhysicalFlags.bExplosionProof = true;
    }
}

// camera like the original mod: behind and above the caster, looking along the beam, shaking while firing
void LocalCamera(Caster& c)
{
    if (!CPoolGuard::IsPedAlive(c.ped))
        return;
    CVector dir = Direction(c);
    CVector flat(-sinf(c.heading), cosf(c.heading), 0.0f);
    // from the side and a bit behind, low, so the whole beam is seen growing out of the hands (like the reference)
    CVector side(-flat.y, flat.x, 0.0f);  // left of the caster
    CVector pos = c.ped->GetPosition() + side * 3.2f - flat * 3.6f + CVector(0.0f, 0.0f, 0.8f);
    CVector look = c.ped->GetPosition() + dir * 11.0f + CVector(0.0f, 0.0f, 0.3f);
    Command<Commands::SET_FIXED_CAMERA_POSITION>(pos.x, pos.y, pos.z, 0.0f, 0.0f, 0.0f);
    Command<Commands::POINT_CAMERA_AT_POINT>(look.x, look.y, look.z, 2 /*JUMP_CUT*/);
    if (c.phase == FIRE)
        Command<Commands::SHAKE_CAM>(90);
}

bool HoldingKameWeapon(CPlayerPed* me)
{
    return s_weaponEnabled && me->m_aWeapons[me->m_nActiveWeaponSlot].m_eWeaponType == KAME_WEAPON;
}
}  // namespace

float CKamehameha::ms_fMouseX = 0.0f;
float CKamehameha::ms_fMouseY = 0.0f;
bool CKamehameha::ms_bLmb = false;
bool CKamehameha::ms_bRmb = false;

bool CKamehameha::IsLocalActive()
{
    return s_local.phase != OFF;
}

bool CKamehameha::IsLocalFiring()
{
    return s_local.phase == FIRE;
}

bool CKamehameha::IsWeaponEnabled()
{
    return s_weaponEnabled;
}

void CKamehameha::ToggleWeapon()
{
    CPlayerPed* me = FindPlayerPed(0);
    if (!me)
        return;
    s_weaponEnabled = !s_weaponEnabled;
    if (s_weaponEnabled)
    {
        me->GiveWeapon((eWeaponType)KAME_WEAPON, 1, true);
        me->SetCurrentWeapon((eWeaponType)KAME_WEAPON);
        CChat::AddMessage("{a0f0a0}[coop] {d6dce8}kamehameha en la manopla: clic derecho carga, clic izquierdo lanza / corta");
    }
    else
    {
        Command<Commands::SET_PLAYER_FIRE_BUTTON>(0, true);
        CChat::AddMessage("{a0f0a0}[coop] {d6dce8}kamehameha quitado de la manopla");
    }
}

void CKamehameha::Process()
{
    CPlayerPed* me = FindPlayerPed(0);
    unsigned int now = GetTickCount();

    if (me)
    {
        s_local.ped = me;
        s_local.local = true;
        bool alt = CPad::NewKeyState.lmenu != 0 || CPad::NewKeyState.rmenu != 0;
        bool h = CPad::NewKeyState.standardKeys['H'] != 0 || CPad::NewKeyState.standardKeys['h'] != 0;
        static bool hWas = false, lmbWas = false;
        bool hPressed = h && !hWas, lmbPressed = ms_bLmb && !lmbWas;
        hWas = h;
        lmbWas = ms_bLmb;

        // on foot, or floating in the air (not while flying around)
        bool canUse = !me->m_nPedFlags.bInVehicle && me->m_fHealth > 0.0f && !CChat::m_bInputActive &&
                      !FrontEndMenuManager.m_bMenuActive && (!CFlight::IsFlying() || CFlight::IsHovering() ||
                                                                s_local.phase != OFF);
        bool weapon = canUse && HoldingKameWeapon(me);

        // the knuckles must not punch while they are the kamehameha
        static bool fireBlocked = false;
        if (weapon != fireBlocked)
        {
            fireBlocked = weapon;
            Command<Commands::SET_PLAYER_FIRE_BUTTON>(0, !weapon);
        }

        const CVector& front = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
        float camHeading = atan2f(-front.x, front.y);

        switch (s_local.phase)
        {
        case OFF:
            if (canUse && alt && hPressed)
            {
                s_local.weaponMode = false;
                s_local.heading = camHeading;
                s_local.pitch = 0.05f;
                SetPhase(s_local, CHARGE);
                Send(s_local);
            }
            else if (weapon && ms_bRmb)
            {
                s_local.weaponMode = true;
                s_local.heading = camHeading;
                s_local.pitch = 0.05f;
                SetPhase(s_local, CHARGE);
                Send(s_local);
            }
            break;

        case CHARGE:
        {
            // turn with the camera while charging
            s_local.heading = camHeading;
            s_local.pitch = std::max(-0.26f, std::min(0.78f, asinf(std::max(-1.0f, std::min(1.0f, front.z))) + 0.1f));
            bool fire = s_local.weaponMode ? lmbPressed : (hPressed && !alt);
            bool cancel = !canUse || (s_local.weaponMode && (!ms_bRmb || !HoldingKameWeapon(me)));
            if (cancel)
            {
                SetPhase(s_local, OFF);
                Send(s_local);
            }
            else if (fire)
            {
                SetPhase(s_local, FIRE);
                Send(s_local);
            }
            break;
        }

        case FIRE:
        {
            s_local.heading -= ms_fMouseX * 0.004f;
            s_local.pitch = std::max(-0.26f, std::min(0.78f, s_local.pitch + ms_fMouseY * 0.003f));
            bool stop = !canUse || now - s_local.phaseSince > FIRE_MS || hPressed || lmbPressed;
            if (stop)
            {
                SetPhase(s_local, OFF);
                Send(s_local);
            }
            break;
        }
        }
        ms_fMouseX = ms_fMouseY = 0.0f;

        if (s_local.phase != OFF)
        {
            me->m_fCurrentRotation = me->m_fAimingRotation = s_local.heading;
            me->SetHeading(s_local.heading);
            if (s_local.phase == FIRE)
                LocalCamera(s_local);
            HoldPose(s_local);
            UpdateSound(s_local);
            Draw(s_local);
            if (now - s_local.lastSent > 100)
            {
                s_local.lastSent = now;
                Send(s_local);
            }
        }
    }

    for (auto it = s_remote.begin(); it != s_remote.end();)
    {
        if (!CPoolGuard::IsPedAlive(it->ped) || it->phase == OFF)
        {
            ReleasePose(*it);
            CSoundFile::Stop(it->sound);
            it = s_remote.erase(it);
            continue;
        }
        it->ped->m_fCurrentRotation = it->ped->m_fAimingRotation = it->heading;
        HoldPose(*it);
        UpdateSound(*it);
        Draw(*it);
        ++it;
    }
}

void CKamehameha::OnPacket(Packets::Skin::PlayerPower* packet)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    if (player == nullptr || player->m_pPed == nullptr || packet->power != 0)
        return;

    Caster* c = nullptr;
    for (auto& r : s_remote)
        if (r.ped == player->m_pPed)
            c = &r;
    if (c == nullptr)
    {
        if (packet->phase == OFF)
            return;
        s_remote.push_back(Caster{});
        c = &s_remote.back();
        c->ped = player->m_pPed;
    }
    c->heading = packet->heading;
    c->pitch = packet->pitch;
    if (c->phase != packet->phase)
    {
        SetPhase(*c, packet->phase);
        if (packet->phase == FIRE)
            c->level = packet->level;
    }
    if (packet->phase == CHARGE)  // show the remote ball at the same size
        c->phaseSince = GetTickCount() - (unsigned int)(packet->level * MAX_CHARGE_MS);
}

void CKamehameha::OnPlayerRemoved(CPed* ped)
{
    for (auto it = s_remote.begin(); it != s_remote.end(); ++it)
        if (it->ped == ped)
        {
            CSoundFile::Stop(it->sound);
            s_remote.erase(it);
            return;
        }
}
