#include "stdafx.h"
#include "CKamehameha.h"
#include "FxManager_c.h"
#include "CCoronas.h"

namespace
{
enum
{
    OFF = 0,
    CHARGE = 1,
    FIRE = 2
};

constexpr unsigned int CHARGE_MS = 1500;
constexpr unsigned int MAX_FIRE_MS = 8000;
constexpr float BEAM_LENGTH = 50.0f;

struct Caster
{
    CPed* ped = nullptr;
    int phase = OFF;
    unsigned int phaseSince = 0;
    float heading = 0.0f;  // radians
    float pitch = 0.0f;    // radians
    unsigned int lastBlast = 0;
    unsigned int lastSent = 0;
    bool local = false;
    bool animApplied = false;
};

Caster s_local;
std::vector<Caster> s_remote;

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

void Send(const Caster& c)
{
    Packets::Skin::PlayerPower p{};
    p.power = 0;
    p.phase = (uint8_t)c.phase;
    p.heading = c.heading;
    p.pitch = c.pitch;
    GetPacketFactory().Send(p);
}

void ApplyPose(Caster& c)
{
    if (c.animApplied || !CPoolGuard::IsPedAlive(c.ped))
        return;
    int ref = CPools::GetPedRef(c.ped);
    Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, "GunMove_BWD", "PED", 4.0f, 1, 1, 1, 1, -1);
    c.animApplied = true;
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
    return 0x4B414D00u + ((uintptr_t)c.ped & 0xFFFF) * 64 + i;  // "KAM" + ped + index, unique per frame
}

void Corona(const Caster& c, int i, const CVector& pos, float radius, CRGBA color)
{
    CCoronas::RegisterCorona(CoronaId(c, i), nullptr, color.r, color.g, color.b, color.a, pos, radius, 300.0f,
        eCoronaType::CORONATYPE_SHINYSTAR, eCoronaFlareType::FLARETYPE_NONE, false, false, 0, 0.0f, false, 1.5f, 0, 15.0f,
        false, false);
}

// beam length until the first wall/car/ped
float BeamHit(const Caster& c, const CVector& from, const CVector& dir, CVector& hit)
{
    CColPoint colPoint{};
    CEntity* entity = nullptr;
    CVector to = from + dir * BEAM_LENGTH;
    bool oldUsesCollision = c.ped->m_bUsesCollision;
    c.ped->m_bUsesCollision = false;
    bool found = CWorld::ProcessLineOfSight(from, to, colPoint, entity, true, true, true, true, false, false, false, false);
    c.ped->m_bUsesCollision = oldUsesCollision;
    hit = found ? colPoint.m_vecPoint : to;
    return found ? (hit - from).Magnitude() : BEAM_LENGTH;
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
        // growing ball of light in the hands
        float k = std::min(1.0f, t / (CHARGE_MS / 1000.0f));
        float pulse = 0.85f + 0.15f * sinf(t * 25.0f);
        Corona(c, 0, hands, (0.25f + 1.6f * k) * pulse, CRGBA(255, 255, 255, 255));
        Corona(c, 1, hands, (0.6f + 2.4f * k) * pulse, CRGBA(90, 170, 255, 200));
        Command<Commands::DRAW_LIGHT_WITH_RANGE>(hands.x, hands.y, hands.z, 120, 190, 255, 6.0f + 10.0f * k);
        return;
    }

    if (c.phase != FIRE)
        return;

    CVector dir = Direction(c);
    CVector hit;
    float length = BeamHit(c, hands, dir, hit);

    // core and glow along the beam
    int i = 2;
    for (float d = 0.5f; d < length && i < 60; d += 2.0f)
    {
        CVector p = hands + dir * d;
        float wobble = 0.9f + 0.1f * sinf(t * 30.0f + d);
        Corona(c, i++, p, 1.1f * wobble, CRGBA(255, 255, 255, 255));
        Corona(c, i++, p, 2.6f * wobble, CRGBA(70, 150, 255, 170));
    }
    Corona(c, i++, hands, 3.0f, CRGBA(180, 220, 255, 255));
    Corona(c, i++, hit, 5.0f, CRGBA(200, 230, 255, 255));
    Command<Commands::DRAW_LIGHT_WITH_RANGE>(hands.x, hands.y, hands.z, 120, 190, 255, 20.0f);
    Command<Commands::DRAW_LIGHT_WITH_RANGE>(hit.x, hit.y, hit.z, 150, 200, 255, 25.0f);

    // blasts at the impact point
    if (now - c.lastBlast > 280)
    {
        c.lastBlast = now;
        if (c.local)
        {
            Command<Commands::ADD_EXPLOSION_NO_SOUND>(hit.x, hit.y, hit.z, 8);  // damage: only the caster's game
            Command<Commands::SHAKE_CAM>(120);
        }
        else
        {
            RwV3d pos{hit.x, hit.y, hit.z};
            if (FxSystem_c* fx = g_fxMan.CreateFxSystem((char*)"explosion_medium", &pos, nullptr, false))
                fx->PlayAndKill();
        }
    }
}

void SetPhase(Caster& c, int phase)
{
    c.phase = phase;
    c.phaseSince = GetTickCount();
    if (phase == OFF)
        ReleasePose(c);
    else
        ApplyPose(c);
}
}  // namespace

bool CKamehameha::IsLocalActive()
{
    return s_local.phase != OFF;
}

void CKamehameha::Process()
{
    CPlayerPed* me = FindPlayerPed(0);
    unsigned int now = GetTickCount();

    // ---- local player
    if (me)
    {
        s_local.ped = me;
        s_local.local = true;
        bool tab = CPad::NewKeyState.tab != 0;
        bool h = CPad::NewKeyState.standardKeys['H'] != 0 || CPad::NewKeyState.standardKeys['h'] != 0;
        bool canUse = !me->m_nPedFlags.bInVehicle && me->m_fHealth > 0.0f && !CChat::m_bInputActive &&
                      !FrontEndMenuManager.m_bMenuActive;

        // aim follows the camera
        const CVector& front = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
        float heading = atan2f(-front.x, front.y);
        float pitch = std::max(-0.26f, std::min(0.8f, asinf(std::max(-1.0f, std::min(1.0f, front.z))) + 0.12f));

        if (s_local.phase == OFF)
        {
            if (canUse && tab && h)
            {
                s_local.heading = heading;
                s_local.pitch = pitch;
                SetPhase(s_local, CHARGE);
                Send(s_local);
                s_local.lastSent = now;
            }
        }
        else
        {
            s_local.heading = heading;
            s_local.pitch = pitch;
            me->m_fCurrentRotation = me->m_fAimingRotation = heading;
            me->SetHeading(heading);

            bool stop = !canUse || (s_local.phase == FIRE && (!tab || now - s_local.phaseSince > MAX_FIRE_MS));
            if (stop)
            {
                SetPhase(s_local, OFF);
                Send(s_local);
            }
            else if (s_local.phase == CHARGE && now - s_local.phaseSince > CHARGE_MS)
            {
                SetPhase(s_local, FIRE);
                Send(s_local);
                s_local.lastSent = now;
            }
            else if (now - s_local.lastSent > 100)
            {
                s_local.lastSent = now;
                Send(s_local);  // aim updates
            }
            Draw(s_local);
        }
    }

    // ---- remote players
    for (auto it = s_remote.begin(); it != s_remote.end();)
    {
        if (!CPoolGuard::IsPedAlive(it->ped) || it->phase == OFF)
        {
            ReleasePose(*it);
            it = s_remote.erase(it);
            continue;
        }
        it->ped->m_fCurrentRotation = it->ped->m_fAimingRotation = it->heading;
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
        SetPhase(*c, packet->phase);
}

void CKamehameha::OnPlayerRemoved(CPed* ped)
{
    for (auto it = s_remote.begin(); it != s_remote.end(); ++it)
        if (it->ped == ped)
        {
            s_remote.erase(it);
            return;
        }
}
