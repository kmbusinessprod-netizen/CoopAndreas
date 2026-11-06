#include "stdafx.h"
#include "CGenkidama.h"
#include "CFlight.h"
#include "CKamehameha.h"
#include "CSoundFile.h"
#include "CCoronas.h"
#include "UI/CPrompt.h"

namespace
{
using P = Packets::Genkidama::Genkidama;

enum
{
    OFF = 0,
    ASKING = 1,    // waiting for the vote (hands up already)
    CHARGING = 2,  // the ball grows
    THROWING = 3,  // throw sound + animation, aim locked, 2 s
    FLYING = 4     // the ball travels
};

const float BALL_SPEED = 50.0f;     // m/s
const float FLY_MAX_S = 12.0f;      // explodes anyway after this
const float R_KILL = 22.0f;         // touched by the blast: dead
const float R_LAUNCH = 120.0f;      // thrown into the sky, dead on landing
const float R_EFFECT = 100.0f;      // ring of explosions grows up to here
const unsigned int THROW_DELAY = 2000;

struct Ball
{
    CPed* ped = nullptr;
    bool local = false;
    int phase = OFF;
    unsigned int phaseSince = 0;
    unsigned int requestAt = 0;
    unsigned int startAt = 0;  // START received
    int chargeSeconds = 0;
    float progress = 0.0f;
    CVector pos{};
    CVector dir{0.0f, 1.0f, 0.0f};
    CVector shownPos{};  // remote: smoothed
    unsigned int sound = 0;
    bool poseApplied = false;
    int poseKind = 0;  // 1 hands up, 2 throw
    std::string name;
    unsigned int lastSent = 0;
    unsigned int flyStart = 0;
    CVector flyFrom{};
};

struct Blast
{
    CVector pos;
    unsigned int at;
    unsigned int lastRing;
    int rings;
    bool applied;
    bool mine;  // my own genkidama: it throws me, it does not kill me
};

Ball s_local;
std::vector<Ball> s_remote;
std::vector<Blast> s_blasts;
unsigned int s_explosionSound = 0;

// blown away by the blast
bool s_launched = false;
unsigned int s_launchedAt = 0;
bool s_launchNoDeath = false;
CVector s_launchVel{};
bool s_wasExplosionProof = false;
unsigned int s_proofUntil = 0;

float Radius(const Ball& b)
{
    return 0.5f + 6.0f * std::max(0.0f, std::min(1.0f, b.progress));
}

CVector BallPos(const Ball& b)
{
    if (!CPoolGuard::IsPedAlive(b.ped))
        return b.pos;
    CVector p = b.ped->GetPosition();
    p.z += 1.0f + Radius(b) * 0.9f;
    return p;
}

unsigned int CoronaId(const Ball& b, int i)
{
    return 0x47454E00u + ((uintptr_t)b.ped & 0xFFFF) * 32 + i;
}

void Corona(unsigned int id, const CVector& pos, float radius, CRGBA color)
{
    CCoronas::RegisterCorona(id, nullptr, color.r, color.g, color.b, color.a, pos, radius, 600.0f,
        eCoronaType::CORONATYPE_SHINYSTAR, eCoronaFlareType::FLARETYPE_NONE, false, false, 0, 0.0f, false, 1.5f, 0, 15.0f,
        false, false);
}

void Send(const Ball& b, uint8_t phase)
{
    if (!CNetwork::m_bConnected)
        return;
    P p{};
    p.phase = phase;
    p.accept = (uint8_t)b.phase;
    p.value = (uint16_t)(b.progress * 100.0f);
    p.pos = b.pos;
    p.dir = b.dir;
    GetPacketFactory().Send(p);
}

void Sound(Ball& b, const char* file)
{
    if (b.sound)
        CSoundFile::Stop(b.sound);
    b.sound = file ? CSoundFile::Play(file, false) : 0;
    if (b.sound && !b.local)
        CSoundFile::Place(b.sound, b.pos, 300.0f);
}

void ApplyPose(Ball& b, int kind)
{
    if (!CPoolGuard::IsPedAlive(b.ped))
        return;
    int ref = CPools::GetPedRef(b.ped);
    Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(ref);
    if (kind == 1)
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, "handsup", "PED", 1.0f, 0, 1, 1, 1, -1);
    else if (Command<Commands::HAS_ANIMATION_LOADED>("GRENADE"))
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, "WEAPON_throw", "GRENADE", 0.45f, 0, 1, 1, 1, -1);
    b.poseApplied = true;
    b.poseKind = kind;
}

void HoldPose(Ball& b)
{
    if (!b.poseApplied || !CPoolGuard::IsPedAlive(b.ped))
        return;
    int ref = CPools::GetPedRef(b.ped);
    if (b.poseKind == 1)
    {
        if (Command<Commands::IS_CHAR_PLAYING_ANIM>(ref, "handsup"))
        {
            float t = 0.0f;
            Command<Commands::GET_CHAR_ANIM_CURRENT_TIME>(ref, "handsup", &t);
            if (t >= 0.9f)
            {
                Command<Commands::SET_CHAR_ANIM_SPEED>(ref, "handsup", 0.0f);
                Command<Commands::SET_CHAR_ANIM_CURRENT_TIME>(ref, "handsup", 0.92f);
            }
        }
        else
            ApplyPose(b, 1);
    }
    b.ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
}

void ReleasePose(Ball& b)
{
    if (!b.poseApplied)
        return;
    b.poseApplied = false;
    if (CPoolGuard::IsPedAlive(b.ped))
        Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(CPools::GetPedRef(b.ped));
}

void SetPhase(Ball& b, int phase)
{
    b.phase = phase;
    b.phaseSince = GetTickCount();
    switch (phase)
    {
    case OFF:
        ReleasePose(b);
        Sound(b, nullptr);
        break;
    case ASKING:
        ApplyPose(b, 1);
        break;
    case CHARGING:
        if (!b.poseApplied)
            ApplyPose(b, 1);
        Sound(b, "genki_cargar.wav");
        break;
    case THROWING:
        ApplyPose(b, 2);
        Sound(b, "genki_lanzar.wav");
        break;
    case FLYING:
        ReleasePose(b);
        b.flyStart = GetTickCount();
        b.flyFrom = b.pos;
        break;
    }
}

const char* Name(const Ball& b)
{
    return b.name.c_str();
}

// ---------------------------------------------------------------- the blast

void LaunchPed(CPed* ped, const CVector& center, float dist, float scale)
{
    CVector away = ped->GetPosition() - center;
    away.z = 0.0f;
    if (away.Magnitude() < 0.1f)
        away = CVector(1.0f, 0.0f, 0.0f);
    away.Normalise();
    float k = (0.45f + 0.55f * (1.0f - dist / R_LAUNCH)) * scale;
    CVector v = away * (0.55f * k) + CVector(0.0f, 0.0f, 1.25f * k);
    ped->m_nPedFlags.bIsStanding = false;
    ped->m_nPhysicalFlags.bApplyGravity = true;
    ped->m_vecMoveSpeed = v;
    if (ped == FindPlayerPed())
        s_launchVel = v;
}

void ApplyBlast(Blast& blast)
{
    blast.applied = true;
    CPlayerPed* me = FindPlayerPed();
    Command<Commands::SHAKE_CAM>(900);
    if (s_explosionSound)
        CSoundFile::Stop(s_explosionSound);
    s_explosionSound = CSoundFile::Play("genki_explosion.wav", false);
    if (s_explosionSound)
        CSoundFile::SetVolume(s_explosionSound, 1.0f);

    bool iAmCaster = blast.mine && me != nullptr;

    // me
    if (me && me->m_fHealth > 0.0f)
    {
        float d = (me->GetPosition() - blast.pos).Magnitude();
        if (iAmCaster)
        {
            // the caster is pushed away but never dies from his own genkidama
            s_wasExplosionProof = me->m_nPhysicalFlags.bExplosionProof;
            me->m_nPhysicalFlags.bExplosionProof = true;
            s_proofUntil = GetTickCount() + 4000;
            if (d < R_LAUNCH)
            {
                if (me->m_nPedFlags.bInVehicle)
                    Command<Commands::WARP_CHAR_FROM_CAR_TO_COORD>(CPools::GetPedRef(me), me->GetPosition().x,
                        me->GetPosition().y, me->GetPosition().z);
                LaunchPed(me, blast.pos, d, 1.0f);
                s_launched = true;
                s_launchedAt = GetTickCount();
                s_launchNoDeath = true;
            }
        }
        else if (d < R_KILL)
        {
            Command<Commands::SET_CHAR_HEALTH>(CPools::GetPedRef(me), 0);
        }
        else if (d < R_LAUNCH)
        {
            if (me->m_nPedFlags.bInVehicle)
                Command<Commands::WARP_CHAR_FROM_CAR_TO_COORD>(CPools::GetPedRef(me), me->GetPosition().x,
                    me->GetPosition().y, me->GetPosition().z);
            LaunchPed(me, blast.pos, d, 1.0f);
            s_launched = true;
            s_launchedAt = GetTickCount();
            s_launchNoDeath = false;
        }
    }

    // the people of the city (the other players take care of themselves)
    for (int i = 0; i < CPools::ms_pPedPool->m_nSize; i++)
    {
        CPed* ped = CPools::ms_pPedPool->GetAt(i);
        if (!ped || ped == me || ped->m_fHealth <= 0.0f || CNetworkPlayerManager::GetPlayer(ped))
            continue;
        float d = (ped->GetPosition() - blast.pos).Magnitude();
        if (d > R_LAUNCH)
            continue;
        if (ped->m_nPedFlags.bInVehicle)
            continue;  // the car takes the hit
        LaunchPed(ped, blast.pos, d, 0.8f);
        if (d < R_LAUNCH * 0.5f)
            ped->m_fHealth = 0.0f;
    }

    // cars: the near ones explode, the rest fly
    for (int i = 0; i < CPools::ms_pVehiclePool->m_nSize; i++)
    {
        CVehicle* v = CPools::ms_pVehiclePool->GetAt(i);
        if (!v || !v->m_pRwObject)
            continue;
        float d = (v->GetPosition() - blast.pos).Magnitude();
        if (d > R_LAUNCH)
            continue;
        if (me && v == me->m_pVehicle && iAmCaster)
            continue;
        CVector away = v->GetPosition() - blast.pos;
        away.z = 0.0f;
        away.Normalise();
        float k = 0.3f + 0.7f * (1.0f - d / R_LAUNCH);
        v->m_vecMoveSpeed += away * (0.35f * k) + CVector(0.0f, 0.0f, 0.7f * k);
        v->m_vecTurnSpeed += CVector(0.08f * k, 0.05f * k, 0.0f);
        if (d < 45.0f && v->m_fHealth > 0.0f)
            Command<Commands::EXPLODE_CAR>(CPools::GetVehicleRef(v));
    }
}

void ProcessBlasts()
{
    unsigned int now = GetTickCount();
    for (auto it = s_blasts.begin(); it != s_blasts.end();)
    {
        Blast& b = *it;
        if (!b.applied)
            ApplyBlast(b);
        float t = (now - b.at) / 1000.0f;
        if (t > 8.0f)
        {
            it = s_blasts.erase(it);
            continue;
        }
        // a ring of explosions that runs outwards for 3 s
        if (t < 3.0f && now - b.lastRing > 110)
        {
            b.lastRing = now;
            float r = 4.0f + R_EFFECT * (t / 3.0f);
            int count = b.rings == 0 ? 6 : 5;
            for (int i = 0; i < count; i++)
            {
                float a = (float)rand() / RAND_MAX * 6.2832f;
                float rr = b.rings == 0 ? r * ((float)rand() / RAND_MAX) : r;
                CVector p(b.pos.x + cosf(a) * rr, b.pos.y + sinf(a) * rr, b.pos.z + 30.0f);
                bool ok = false;
                float z = CWorld::FindGroundZFor3DCoord(p.x, p.y, p.z + 50.0f, &ok, nullptr);
                p.z = ok ? z + 1.0f : b.pos.z;
                Command<Commands::ADD_EXPLOSION_NO_SOUND>(p.x, p.y, p.z, (i % 3 == 0) ? 8 : 7);
            }
            b.rings++;
        }
        ++it;
    }
}

void DrawBlasts()
{
    unsigned int now = GetTickCount();
    int n = 0;
    for (Blast& b : s_blasts)
    {
        float t = (now - b.at) / 1000.0f;
        unsigned int base = 0x424C5300u + n * 24;
        n++;
        // fireball, then a column of light that rises
        if (t < 2.5f)
        {
            float k = t / 2.5f;
            float r = 12.0f + 70.0f * k;
            int a = (int)(255 * (1.0f - k));
            Corona(base + 0, b.pos + CVector(0.0f, 0.0f, 6.0f), r, CRGBA(255, 255, 240, a));
            Corona(base + 1, b.pos + CVector(0.0f, 0.0f, 8.0f), r * 1.6f, CRGBA(255, 170, 60, a / 2));
            Corona(base + 2, b.pos + CVector(0.0f, 0.0f, 10.0f), r * 2.3f, CRGBA(120, 170, 255, a / 4));
        }
        if (t < 7.0f)
        {
            float k = t / 7.0f;
            for (int i = 0; i < 6; i++)
            {
                float h = 10.0f + (i * 14.0f + 90.0f * k);
                int a = (int)(160 * (1.0f - k));
                Corona(base + 3 + i, b.pos + CVector(0.0f, 0.0f, h), 14.0f + 10.0f * k + i * 2.0f, CRGBA(255, 190, 90, a));
            }
            Corona(base + 9, b.pos + CVector(0.0f, 0.0f, 100.0f + 60.0f * k), 45.0f + 30.0f * k, CRGBA(255, 220, 160, (int)(120 * (1.0f - k))));
        }
        Command<Commands::DRAW_LIGHT_WITH_RANGE>(b.pos.x, b.pos.y, b.pos.z + 5.0f, 255, 200, 120, 150.0f * (1.0f - std::min(1.0f, t / 4.0f)));
    }
}

void DrawFlash()
{
    unsigned int now = GetTickCount();
    float best = 0.0f;
    for (Blast& b : s_blasts)
    {
        float t = (now - b.at) / 1000.0f;
        float a = t < 0.25f ? t / 0.25f : std::max(0.0f, 1.0f - (t - 0.25f) / 2.2f);
        best = std::max(best, a);
    }
    if (best <= 0.0f)
        return;
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, (float)RsGlobal.maximumWidth, (float)RsGlobal.maximumHeight),
        CRGBA(255, 255, 255, (int)(235 * best)));
}

void Explode(const CVector& pos, bool mine)
{
    s_blasts.push_back({pos, GetTickCount(), 0, 0, false, mine});
}

// ---------------------------------------------------------------- the ball

void DrawBall(Ball& b)
{
    if (b.phase == OFF)
        return;
    unsigned int now = GetTickCount();
    float t = now / 1000.0f;
    CVector p = b.local || b.phase == FLYING ? b.pos : b.shownPos;
    float r = Radius(b);
    float pulse = 0.92f + 0.08f * sinf(t * 9.0f);
    if (b.phase == ASKING)
        r = 0.4f + 0.15f * sinf(t * 6.0f);
    Corona(CoronaId(b, 0), p, r * 1.0f * pulse, CRGBA(255, 255, 255, 240));
    Corona(CoronaId(b, 1), p, r * 1.9f * pulse, CRGBA(140, 200, 255, 160));
    Corona(CoronaId(b, 2), p, r * 2.9f * pulse, CRGBA(90, 150, 255, 60));
    // sparks that spiral into the ball: the energy coming in
    int sparks = b.phase == ASKING ? 4 : 10;
    for (int i = 0; i < sparks; i++)
    {
        float a = t * (1.5f + i * 0.13f) + i * 0.63f;
        float rr = r * (1.4f + 1.6f * (0.5f + 0.5f * sinf(t * 0.7f + i)));
        CVector s(p.x + cosf(a) * rr, p.y + sinf(a) * rr, p.z + sinf(a * 1.7f + i) * rr * 0.6f);
        Corona(CoronaId(b, 3 + i), s, 0.25f + 0.1f * r, CRGBA(200, 230, 255, 200));
    }
    if (b.phase == FLYING)
    {
        for (int i = 0; i < 8; i++)
        {
            CVector s = p - b.dir * (r * 0.8f * (i + 1));
            Corona(CoronaId(b, 13 + i), s, r * (1.5f - i * 0.15f), CRGBA(150, 200, 255, 110 - i * 12));
        }
    }
    Command<Commands::DRAW_LIGHT_WITH_RANGE>(p.x, p.y, p.z, 150, 200, 255, 10.0f + 12.0f * r);
}

void FlyStep(Ball& b, bool authoritative)
{
    unsigned int now = GetTickCount();
    float dt = CTimer::ms_fTimeStep / 50.0f;
    CVector next = b.pos + b.dir * (BALL_SPEED * dt);
    bool hit = false;
    CVector hitPos = next;
    if (authoritative)
    {
        CColPoint colPoint{};
        CEntity* entity = nullptr;
        bool oldCol = CPoolGuard::IsPedAlive(b.ped) ? b.ped->m_bUsesCollision : false;
        if (CPoolGuard::IsPedAlive(b.ped))
            b.ped->m_bUsesCollision = false;
        // the ball is big: it explodes a bit before its centre touches
        CVector probe = next + b.dir * Radius(b) * 0.6f;
        hit = CWorld::ProcessLineOfSight(b.pos, probe, colPoint, entity, true, true, false, true, false, false, false, false);
        if (CPoolGuard::IsPedAlive(b.ped))
            b.ped->m_bUsesCollision = oldCol;
        if (hit)
            hitPos = colPoint.m_vecPoint;
        if (!hit && (now - b.flyStart) / 1000.0f > FLY_MAX_S)
            hit = true;
        if (!hit && next.z < -5.0f)
            hit = true;
    }
    b.pos = hit ? hitPos : next;
    if (hit && authoritative)
    {
        Explode(b.pos, true);
        Send(b, P::EXPLODE);
        SetPhase(b, OFF);
    }
}

void DrawBar(const Ball& b)
{
    float w = 220.0f, h = 10.0f;
    float x = (CUtil::SCREEN_BASE_WIDTH - w) * 0.5f, y = 360.0f;
    CSprite2d::DrawRect(CRect(CUtil::HUD_X(x - 3.0f), CUtil::HUD_Y(y - 3.0f), CUtil::HUD_X(x + w + 3.0f), CUtil::HUD_Y(y + h + 3.0f)),
        CRGBA(0, 0, 0, 200));
    float fill = b.phase == ASKING ? 0.5f + 0.5f * sinf(GetTickCount() / 250.0f) : std::min(1.0f, b.progress);
    if (b.phase == THROWING || b.phase == FLYING)
        fill = 1.0f;
    CRGBA col = b.phase == ASKING ? CRGBA(120, 120, 140, 255) : CRGBA(150, 200, 255, 255);
    CSprite2d::DrawRect(CRect(CUtil::HUD_X(x), CUtil::HUD_Y(y), CUtil::HUD_X(x + w * fill), CUtil::HUD_Y(y + h)), col);
    char text[96];
    if (b.phase == ASKING)
        sprintf_s(text, "%s PIDE ENERGIA...", Name(b));
    else if (b.phase == CHARGING)
        sprintf_s(text, "GENKIDAMA DE %s  %d%%", Name(b), (int)(b.progress * 100.0f));
    else
        sprintf_s(text, "GENKIDAMA DE %s  ~r~!LANZADA!", Name(b));
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
    CFont::PrintString(CUtil::HUD_X(CUtil::SCREEN_BASE_WIDTH * 0.5f), CUtil::HUD_Y(y - 17.0f), text);
}

Ball* RemoteFor(CPed* ped, bool create)
{
    for (Ball& b : s_remote)
        if (b.ped == ped)
            return &b;
    if (!create)
        return nullptr;
    Ball b;
    b.ped = ped;
    s_remote.push_back(b);
    return &s_remote.back();
}

void CancelLocal(bool tell)
{
    if (s_local.phase == OFF)
        return;
    if (tell)
        Send(s_local, P::CANCEL);
    SetPhase(s_local, OFF);
}

void StartLocal(int seconds)
{
    s_local.chargeSeconds = seconds;
    s_local.startAt = GetTickCount();
    SetPhase(s_local, CHARGING);
    CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Cargando: %d segundos. Los demas ya pueden alejarse...", seconds);
}

void ProcessLaunched(CPlayerPed* me)
{
    if (!s_launched)
        return;
    unsigned int now = GetTickCount();
    float t = (now - s_launchedAt) / 1000.0f;
    if (!CPoolGuard::IsPedAlive(me) || me->m_fHealth <= 0.0f || t > 25.0f)
    {
        s_launched = false;
        return;
    }
    int ref = CPools::GetPedRef(me);
    if (t < 0.6f)
    {
        me->m_vecMoveSpeed = s_launchVel;  // the first frames: nothing stops the push
        me->m_nPedFlags.bIsStanding = false;
    }
    if (!Command<Commands::IS_CHAR_PLAYING_ANIM>(ref, "FALL_skyDive"))
        Command<Commands::TASK_PLAY_ANIM>(ref, "FALL_skyDive", "PED", 4.0f, 1, 0, 0, 0, -1);
    bool ok = false;
    CVector p = me->GetPosition();
    float ground = CWorld::FindGroundZFor3DCoord(p.x, p.y, p.z + 1.0f, &ok, nullptr);
    bool landed = (t > 1.5f) && ((ok && p.z - ground < 1.6f && me->m_vecMoveSpeed.z > -0.05f) ||
                                    me->m_nPhysicalFlags.bTouchingWater || me->m_nPedFlags.bIsStanding);
    if (landed)
    {
        s_launched = false;
        Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(ref);
        if (!s_launchNoDeath)
            Command<Commands::SET_CHAR_HEALTH>(ref, 0);
    }
}
}  // namespace

bool CGenkidama::IsLocalActive()
{
    return s_local.phase == ASKING || s_local.phase == CHARGING || s_local.phase == THROWING;
}

bool CGenkidama::IsLaunched()
{
    return s_launched;
}

void CGenkidama::Process()
{
    CPlayerPed* me = FindPlayerPed(0);
    unsigned int now = GetTickCount();
    ProcessBlasts();
    if (s_proofUntil && now > s_proofUntil)
    {
        s_proofUntil = 0;
        if (me)
            me->m_nPhysicalFlags.bExplosionProof = s_wasExplosionProof;
    }

    if (me)
    {
        s_local.ped = me;
        s_local.local = true;
        s_local.name = CLocalPlayer::m_Name;
        ProcessLaunched(me);

        static unsigned int lastAnimRequest = 0;
        if (now - lastAnimRequest > 3000 && !Command<Commands::HAS_ANIMATION_LOADED>("GRENADE"))
        {
            lastAnimRequest = now;
            Command<Commands::REQUEST_ANIMATION>("GRENADE");
        }

        bool alt = CPad::NewKeyState.lmenu != 0 || CPad::NewKeyState.rmenu != 0;
        bool k = CPad::NewKeyState.standardKeys['K'] != 0 || CPad::NewKeyState.standardKeys['k'] != 0;
        static bool kWas = false;
        bool kPressed = k && !kWas;
        kWas = k;

        bool alive = me->m_fHealth > 0.0f && !me->m_nPedFlags.bInVehicle;
        if (IsLocalActive() && !alive)
            CancelLocal(true);

        if (alt && kPressed && s_local.phase == OFF && alive && !CChat::m_bInputActive && !FrontEndMenuManager.m_bMenuActive)
        {
            if (!CFlight::IsHovering() || CKamehameha::IsLocalActive())
                CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Solo flotando quieto en el aire (Volar, sin moverte)");
            else if (s_launched)
                CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Ahora no...");
            else
            {
                s_local.requestAt = now;
                s_local.progress = 0.0f;
                s_local.pos = BallPos(s_local);
                SetPhase(s_local, ASKING);
                if (CNetwork::m_bConnected)
                {
                    Send(s_local, P::REQUEST);
                    CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Pidiendo energia a todos...");
                }
                else
                    StartLocal(10);
            }
        }

        if (s_local.phase == ASKING && now - s_local.phaseSince > 15000)
        {
            CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Sin respuesta del servidor");
            CancelLocal(true);
        }

        if (IsLocalActive())
        {
            // aim with the camera until the throw begins
            if (s_local.phase != THROWING)
            {
                const CVector& front = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
                CVector d(front.x, front.y, front.z);
                d.Normalise();
                s_local.dir = d;
            }
            float heading = atan2f(-s_local.dir.x, s_local.dir.y);
            me->m_fCurrentRotation = me->m_fAimingRotation = heading;
            me->SetHeading(heading);
            HoldPose(s_local);
            s_local.pos = BallPos(s_local);

            if (s_local.phase == CHARGING)
            {
                float total = s_local.chargeSeconds * 1000.0f;
                float p = (now - s_local.requestAt) / total;
                // the vote may have eaten most of the time: at least 3 s of visible charge
                float pMin = (now - s_local.startAt) / 3000.0f;
                s_local.progress = std::min(p, pMin);
                if (s_local.progress >= 1.0f)
                {
                    s_local.progress = 1.0f;
                    SetPhase(s_local, THROWING);
                    Send(s_local, P::STATE);
                    s_local.lastSent = now;
                }
            }
            else if (s_local.phase == THROWING && now - s_local.phaseSince >= THROW_DELAY)
            {
                SetPhase(s_local, FLYING);
                Send(s_local, P::THROW);
                CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}!Lanzada!");
            }
            if (s_local.phase != OFF && now - s_local.lastSent > 150)
            {
                s_local.lastSent = now;
                Send(s_local, P::STATE);
            }
        }
        else if (s_local.phase == FLYING)
            FlyStep(s_local, true);
    }

    for (auto it = s_remote.begin(); it != s_remote.end();)
    {
        Ball& b = *it;
        if (!CPoolGuard::IsPedAlive(b.ped) || b.phase == OFF)
        {
            ReleasePose(b);
            CSoundFile::Stop(b.sound);
            it = s_remote.erase(it);
            continue;
        }
        if (b.phase == FLYING)
        {
            FlyStep(b, false);
            if ((now - b.flyStart) / 1000.0f > FLY_MAX_S + 3.0f)
                SetPhase(b, OFF);  // the EXPLODE never came
        }
        else
        {
            if (now - b.phaseSince > 40000)
                SetPhase(b, OFF);
            float heading = atan2f(-b.dir.x, b.dir.y);
            b.ped->m_fCurrentRotation = b.ped->m_fAimingRotation = heading;
            HoldPose(b);
            CVector target = BallPos(b);
            b.shownPos += (target - b.shownPos) * 0.3f;
            b.pos = b.shownPos;
        }
        if (b.sound)
            CSoundFile::Place(b.sound, b.pos, 300.0f);
        ++it;
    }

    // world effects (coronas, lights) are registered during the game update, like the kamehameha
    DrawBlasts();
    if (s_local.phase != OFF)
        DrawBall(s_local);
    for (Ball& b : s_remote)
        DrawBall(b);
}

void CGenkidama::Draw()
{
    if (!FrontEndMenuManager.m_bPrefsShowHud)
        return;
    DrawFlash();
    if (s_local.phase != OFF && s_local.phase != FLYING)
        DrawBar(s_local);
    else
        for (Ball& b : s_remote)
            if (b.phase != OFF && b.phase != FLYING)
            {
                DrawBar(b);
                break;
            }
}

void CGenkidama::OnPacket(Packets::Genkidama::Genkidama* packet)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    bool mine = packet->senderPlayerid.value == CNetworkPlayerManager::m_localPlayerId;
    std::string name = player ? player->GetName() : "?";

    switch (packet->phase)
    {
    case P::ASK:
    {
        if (!player)
            return;
        CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}%s te pide tu energia (Y / N)", name.c_str());
        CPrompt::Ask(name + " te pide tu energia para una ~y~GENKIDAMA~s~. Si la hace, destruye todo a su alrededor.",
            [](bool accept)
            {
                P reply{};
                reply.phase = P::REPLY;
                reply.accept = accept ? 1 : 0;
                GetPacketFactory().Send(reply);
            },
            9000);
        break;
    }
    case P::START:
    {
        if (mine)
        {
            if (s_local.phase == ASKING)
                StartLocal(packet->value);
            return;
        }
        if (!player || !player->m_pPed)
            return;
        Ball* b = RemoteFor(player->m_pPed, true);
        b->name = name;
        b->chargeSeconds = packet->value;
        b->requestAt = b->startAt = GetTickCount();
        b->progress = 0.0f;
        b->shownPos = BallPos(*b);
        b->pos = b->shownPos;
        SetPhase(*b, CHARGING);
        CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}!%s esta cargando una GENKIDAMA (%d s)! Alejate...", name.c_str(),
            (int)packet->value);
        break;
    }
    case P::DENIED:
    {
        if (!mine && player)
            return;
        if (packet->value == 1)
            CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Todavia no: disponible en %d s", (int)packet->pos.x);
        else if (packet->value == 2)
            CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}No juntaste energia suficiente (hace falta la mitad)");
        else
            CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}Alguien ya esta haciendo una");
        CancelLocal(false);
        break;
    }
    case P::STATE:
    {
        if (mine || !player || !player->m_pPed)
            return;
        Ball* b = RemoteFor(player->m_pPed, true);
        b->name = name;
        b->dir = packet->dir;
        b->progress = packet->value / 100.0f;
        if (b->phase == OFF)
        {
            b->shownPos = packet->pos;
            SetPhase(*b, packet->accept == THROWING ? THROWING : CHARGING);
        }
        else if (packet->accept == THROWING && b->phase != THROWING)
            SetPhase(*b, THROWING);
        break;
    }
    case P::THROW:
    {
        if (mine || !player || !player->m_pPed)
            return;
        Ball* b = RemoteFor(player->m_pPed, true);
        b->name = name;
        b->progress = 1.0f;
        b->pos = packet->pos;
        b->dir = packet->dir;
        SetPhase(*b, FLYING);
        CChat::AddMessage("{c7a6ff}[Genkidama] {d6dce8}!%s lanzo la Genkidama!", name.c_str());
        break;
    }
    case P::EXPLODE:
    {
        if (mine)
            return;
        if (player && player->m_pPed)
            if (Ball* b = RemoteFor(player->m_pPed, false))
                SetPhase(*b, OFF);
        Explode(packet->pos, false);
        break;
    }
    case P::CANCEL:
    {
        if (mine || !player || !player->m_pPed)
            return;
        if (Ball* b = RemoteFor(player->m_pPed, false))
            SetPhase(*b, OFF);
        break;
    }
    default:
        break;
    }
}
