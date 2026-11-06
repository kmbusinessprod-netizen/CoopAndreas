#include "stdafx.h"
#include "CGenkidama.h"
#include "CFlight.h"
#include "CKamehameha.h"
#include "CSoundFile.h"
#include "CCoronas.h"
#include "UI/CPrompt.h"
#include <set>

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
// nuke-sized: the shock wave runs out to R_WAVE in WAVE_S seconds and hits everything as it passes
const float R_KILL = 60.0f;      // inside the fireball: dead on the spot
const float R_WAVE = 260.0f;     // thrown into the sky (dead on landing), cars blown away
const float R_CARS_BOOM = 170.0f;  // cars explode
const float R_PEDS_DIE = 200.0f;   // people of the city die
const float WAVE_S = 4.0f;
const float BLAST_S = 14.0f;     // fire and smoke last this long
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
    bool poseCustom = false;
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
    bool hitMe = false;
    std::set<void*> hit;  // reached by the shock wave already
    int fires = 0;
    int fireIds[48] = {};
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
bool s_wasFireProof = false;
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

// animations of the "Genkidama" mod (CoopAndreas\anims\yuanqidan.ifp): dbz_1 hands to the sky (charge,
// held on its last frame), dbz_2 the throw. Without the file: the game's hands-up and grenade throw.
const char* GENKI_BLOCK = "yuanqidan";
bool GenkiAnimsReady()
{
    if (Command<Commands::HAS_ANIMATION_LOADED>(GENKI_BLOCK))
        return true;
    static unsigned int lastRequest = 0;
    if (GetTickCount() - lastRequest > 2000)
    {
        lastRequest = GetTickCount();
        Command<Commands::REQUEST_ANIMATION>(GENKI_BLOCK);
        Command<Commands::REQUEST_ANIMATION>("GRENADE");
    }
    return false;
}

const char* PoseAnim(int kind, bool custom)
{
    if (custom)
        return kind == 1 ? "dbz_1" : "dbz_2";
    return kind == 1 ? "handsup" : "WEAPON_throw";
}

void ApplyPose(Ball& b, int kind)
{
    if (!CPoolGuard::IsPedAlive(b.ped))
        return;
    int ref = CPools::GetPedRef(b.ped);
    Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(ref);
    bool custom = GenkiAnimsReady();
    if (custom)
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, PoseAnim(kind, true), GENKI_BLOCK, 4.0f, 0, 1, 1, 1, -1);
    else if (kind == 1)
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, "handsup", "PED", 4.0f, 0, 1, 1, 1, -1);
    else if (Command<Commands::HAS_ANIMATION_LOADED>("GRENADE"))
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(ref, "WEAPON_throw", "GRENADE", 4.0f, 0, 1, 1, 1, -1);
    b.poseApplied = true;
    b.poseKind = kind;
    b.poseCustom = custom;
}

// the animations stop on their last frame (lockF); if something replaced the task, back into the pose
void HoldPose(Ball& b)
{
    if (!b.poseApplied || !CPoolGuard::IsPedAlive(b.ped))
        return;
    int ref = CPools::GetPedRef(b.ped);
    if (b.poseKind == 1 && !Command<Commands::IS_CHAR_PLAYING_ANIM>(ref, PoseAnim(1, b.poseCustom)))
        ApplyPose(b, 1);
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
    float k = (0.55f + 0.75f * (1.0f - dist / R_WAVE)) * scale;
    CVector v = away * (0.9f * k) + CVector(0.0f, 0.0f, 1.8f * k);
    ped->m_nPedFlags.bIsStanding = false;
    ped->m_nPhysicalFlags.bApplyGravity = true;
    ped->m_vecMoveSpeed = v;
    if (ped == FindPlayerPed())
        s_launchVel = v;
}

void Fx(const char* name, const CVector& p)
{
    int fx = 0;
    Command<Commands::CREATE_FX_SYSTEM>(name, p.x, p.y, p.z, 4, &fx);
    if (fx)
        Command<Commands::PLAY_AND_KILL_FX_SYSTEM>(fx);
}

CVector Ground(float x, float y, float fromZ)
{
    bool ok = false;
    float z = CWorld::FindGroundZFor3DCoord(x, y, fromZ + 80.0f, &ok, nullptr);
    return CVector(x, y, ok ? z + 0.5f : fromZ);
}

// the moment it explodes (once): sound, shake, the player himself
void BlastStart(Blast& blast)
{
    blast.applied = true;
    CPlayerPed* me = FindPlayerPed();
    Command<Commands::SHAKE_CAM>(2500);
    if (s_explosionSound)
        CSoundFile::Stop(s_explosionSound);
    s_explosionSound = CSoundFile::Play("genki_explosion.wav", false);
    if (s_explosionSound)
        CSoundFile::SetVolume(s_explosionSound, 1.0f);
    if (me && blast.mine)
    {
        // the caster never dies from his own genkidama, he is only blown away
        if (!s_proofUntil)
        {
            s_wasExplosionProof = me->m_nPhysicalFlags.bExplosionProof;
            s_wasFireProof = me->m_nPhysicalFlags.bFireProof;
        }
        me->m_nPhysicalFlags.bExplosionProof = true;
        me->m_nPhysicalFlags.bFireProof = true;
        s_proofUntil = GetTickCount() + 10000;
    }
}

// the shock wave reached `ped` / `v`
void HitMe(Blast& blast, CPlayerPed* me, float d)
{
    if (me->m_fHealth <= 0.0f)
        return;
    if (!blast.mine && d < R_KILL)
    {
        Command<Commands::SET_CHAR_HEALTH>(CPools::GetPedRef(me), 0);
        return;
    }
    if (me->m_nPedFlags.bInVehicle)
        Command<Commands::WARP_CHAR_FROM_CAR_TO_COORD>(CPools::GetPedRef(me), me->GetPosition().x, me->GetPosition().y,
            me->GetPosition().z + 2.0f);
    LaunchPed(me, blast.pos, d, 1.0f);
    s_launched = true;
    s_launchedAt = GetTickCount();
    s_launchNoDeath = blast.mine;
}

void ProcessWave(Blast& blast, float radius)
{
    CPlayerPed* me = FindPlayerPed();
    if (me && !blast.hitMe)
    {
        float d = (me->GetPosition() - blast.pos).Magnitude();
        if (d <= radius && d < R_WAVE)
        {
            blast.hitMe = true;
            HitMe(blast, me, d);
        }
    }
    // the people of the city (the other players take care of themselves)
    for (int i = 0; i < CPools::ms_pPedPool->m_nSize; i++)
    {
        CPed* ped = CPools::ms_pPedPool->GetAt(i);
        if (!ped || ped == me || CNetworkPlayerManager::GetPlayer(ped))
            continue;
        float d = (ped->GetPosition() - blast.pos).Magnitude();
        if (d > radius || d > R_WAVE || blast.hit.count(ped))
            continue;
        blast.hit.insert(ped);
        if (ped->m_nPedFlags.bInVehicle)
            continue;  // the car takes the hit
        LaunchPed(ped, blast.pos, d, 0.9f);
        if (d < R_PEDS_DIE)
            ped->m_fHealth = 0.0f;
    }
    // cars: blown away, the nearer ones explode
    for (int i = 0; i < CPools::ms_pVehiclePool->m_nSize; i++)
    {
        CVehicle* v = CPools::ms_pVehiclePool->GetAt(i);
        if (!v || !v->m_pRwObject)
            continue;
        float d = (v->GetPosition() - blast.pos).Magnitude();
        if (d > radius || d > R_WAVE || blast.hit.count(v))
            continue;
        blast.hit.insert(v);
        CVector away = v->GetPosition() - blast.pos;
        away.z = 0.0f;
        if (away.Magnitude() < 0.1f)
            away = CVector(0.0f, 1.0f, 0.0f);
        away.Normalise();
        float k = 0.4f + 0.9f * (1.0f - d / R_WAVE);
        float massK = std::min(1.5f, 1500.0f / std::max(300.0f, v->m_fMass));  // buses fly less than bikes
        v->m_vecMoveSpeed += (away * (0.6f * k) + CVector(0.0f, 0.0f, 0.9f * k)) * massK;
        v->m_vecTurnSpeed += CVector(0.12f * k * (rand() % 2 ? 1.0f : -1.0f), 0.1f * k, 0.05f * k);
        if (d < R_CARS_BOOM && v->m_fHealth > 0.0f)
        {
            v->m_fHealth = 0.0f;  // burning wreck in a moment
            if (d < R_CARS_BOOM * 0.6f)
                Command<Commands::EXPLODE_CAR>(CPools::GetVehicleRef(v));
        }
    }
}

void ProcessBlasts()
{
    unsigned int now = GetTickCount();
    for (auto it = s_blasts.begin(); it != s_blasts.end();)
    {
        Blast& b = *it;
        if (!b.applied)
            BlastStart(b);
        float t = (now - b.at) / 1000.0f;
        if (t > 30.0f)
        {
            for (int i = 0; i < b.fires; i++)  // the fires go out after 30 s
                Command<Commands::REMOVE_SCRIPT_FIRE>(b.fireIds[i]);
            it = s_blasts.erase(it);
            continue;
        }
        float k = std::min(1.0f, t / WAVE_S);
        float radius = 8.0f + (R_WAVE - 8.0f) * (1.0f - (1.0f - k) * (1.0f - k));  // fast at first, then slows
        if (t < WAVE_S + 0.2f)
            ProcessWave(b, radius);

        if (now - b.lastRing >= 70)
        {
            b.lastRing = now;
            // the fireball and the mushroom: big explosions stacked up the column for the first seconds
            if (t < 5.0f)
            {
                float h = 15.0f + 140.0f * std::min(1.0f, t / 5.0f);
                for (int i = 0; i < 3; i++)
                {
                    float a = (float)rand() / RAND_MAX * 6.2832f, rr = 6.0f + 18.0f * ((float)rand() / RAND_MAX);
                    float hh = h * ((float)rand() / RAND_MAX);
                    Fx("explosion_large", b.pos + CVector(cosf(a) * rr, sinf(a) * rr, hh));
                }
                // the cap of the mushroom
                float a = (float)rand() / RAND_MAX * 6.2832f, rr = 20.0f + 35.0f * std::min(1.0f, t / 4.0f);
                Fx("explosion_large", b.pos + CVector(cosf(a) * rr, sinf(a) * rr, h));
                Fx("prt_smoke_huge", b.pos + CVector(cosf(a) * rr * 0.5f, sinf(a) * rr * 0.5f, h * 0.8f));
            }
            // the wave front: explosions on the ground along the ring
            if (t < WAVE_S)
            {
                for (int i = 0; i < 6; i++)
                {
                    float a = (float)rand() / RAND_MAX * 6.2832f;
                    float rr = radius * (0.85f + 0.15f * ((float)rand() / RAND_MAX));
                    CVector p = Ground(b.pos.x + cosf(a) * rr, b.pos.y + sinf(a) * rr, b.pos.z);
                    Fx(i % 2 ? "explosion_large" : "explosion_medium", p);
                    if (i == 0)
                        Fx("prt_smoke_huge", p);
                }
                // real explosions (the game has 16 at a time: a few, they set things on fire and break things)
                CVector p = Ground(b.pos.x + cosf(t * 7.0f) * radius * 0.9f, b.pos.y + sinf(t * 7.0f) * radius * 0.9f, b.pos.z);
                Command<Commands::ADD_EXPLOSION_NO_SOUND>(p.x, p.y, p.z, 7);
            }
            // afterwards: fires and smoke all over the area
            if (t > 1.0f && b.fires < 45)
            {
                float a = (float)rand() / RAND_MAX * 6.2832f;
                float rr = R_WAVE * 0.7f * sqrtf((float)rand() / RAND_MAX);
                CVector p = Ground(b.pos.x + cosf(a) * rr, b.pos.y + sinf(a) * rr, b.pos.z);
                Command<Commands::START_SCRIPT_FIRE>(p.x, p.y, p.z, 1, 3, &b.fireIds[b.fires]);
                b.fires++;
            }
            if (t > 2.0f && t < BLAST_S - 2.0f && rand() % 3 == 0)
            {
                float a = (float)rand() / RAND_MAX * 6.2832f, rr = R_WAVE * 0.5f * ((float)rand() / RAND_MAX);
                Fx("prt_smoke_huge", Ground(b.pos.x + cosf(a) * rr, b.pos.y + sinf(a) * rr, b.pos.z));
            }
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
        if (t > BLAST_S)
            continue;
        unsigned int base = 0x424C5300u + n * 24;
        n++;
        // fireball, then a column of light that rises
        if (t < 3.5f)
        {
            float k = t / 3.5f;
            float r = 25.0f + 140.0f * k;
            int a = (int)(255 * (1.0f - k));
            Corona(base + 0, b.pos + CVector(0.0f, 0.0f, 10.0f), r, CRGBA(255, 255, 240, a));
            Corona(base + 1, b.pos + CVector(0.0f, 0.0f, 14.0f), r * 1.6f, CRGBA(255, 170, 60, a / 2));
            Corona(base + 2, b.pos + CVector(0.0f, 0.0f, 18.0f), r * 2.3f, CRGBA(120, 170, 255, a / 4));
        }
        if (t < 10.0f)
        {
            float k = t / 10.0f;
            for (int i = 0; i < 8; i++)
            {
                float h = 15.0f + (i * 18.0f + 140.0f * k);
                int a = (int)(170 * (1.0f - k));
                Corona(base + 3 + i, b.pos + CVector(0.0f, 0.0f, h), 22.0f + 16.0f * k + i * 3.0f, CRGBA(255, 180, 80, a));
            }
            Corona(base + 11, b.pos + CVector(0.0f, 0.0f, 150.0f + 60.0f * k), 80.0f + 50.0f * k,
                CRGBA(255, 210, 150, (int)(140 * (1.0f - k))));
        }
        Command<Commands::DRAW_LIGHT_WITH_RANGE>(b.pos.x, b.pos.y, b.pos.z + 5.0f, 255, 200, 120,
            250.0f * (1.0f - std::min(1.0f, t / 6.0f)));
    }
}

void DrawFlash()
{
    unsigned int now = GetTickCount();
    float best = 0.0f;
    for (Blast& b : s_blasts)
    {
        float t = (now - b.at) / 1000.0f;
        float a = t < 0.25f ? t / 0.25f : std::max(0.0f, 1.0f - (t - 0.25f) / 3.0f);
        best = std::max(best, a);
    }
    if (best <= 0.0f)
        return;
    CSprite2d::DrawRect(CRect(0.0f, 0.0f, (float)RsGlobal.maximumWidth, (float)RsGlobal.maximumHeight),
        CRGBA(255, 255, 255, (int)(235 * best)));
}

void Explode(const CVector& pos, bool mine)
{
    Blast b{pos, GetTickCount(), 0, 0, false, mine};
    s_blasts.push_back(b);
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
        {
            me->m_nPhysicalFlags.bExplosionProof = s_wasExplosionProof;
            me->m_nPhysicalFlags.bFireProof = s_wasFireProof;
        }
    }

    if (me)
    {
        s_local.ped = me;
        s_local.local = true;
        s_local.name = CLocalPlayer::m_Name;
        ProcessLaunched(me);

        GenkiAnimsReady();

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
