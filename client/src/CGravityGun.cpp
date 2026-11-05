#include "stdafx.h"
#include "CGravityGun.h"
#include "CHoldSync.h"

bool CGravityGun::ms_bActive = false;
int CGravityGun::ms_nGunObject = -1, CGravityGun::ms_nPart1 = -1, CGravityGun::ms_nPart2 = -1, CGravityGun::ms_nPart3 = -1,
    CGravityGun::ms_nPart4 = -1;
CEntity* CGravityGun::ms_pHeld = nullptr;
int CGravityGun::ms_nHeldType = 0;
unsigned int CGravityGun::ms_nLastShot = 0;

static constexpr float RANGE = 25.0f;
static constexpr float HOLD_DISTANCE = 4.0f;

static bool EntityAlive(CEntity* e, int type)
{
    if (type == 1)
        return CPoolGuard::IsVehicleAlive((CVehicle*)e);
    if (type == 2)
        return CPoolGuard::IsPedAlive((CPed*)e);
    return e != nullptr && CPools::ms_pObjectPool && CPools::ms_pObjectPool->IsObjectValid((CObject*)e);
}

static bool LoadModelNow(int model)
{
    if (CStreaming::ms_aInfoForModel[model].m_nLoadState == LOADSTATE_LOADED)
        return true;
    CStreaming::RequestModel(model, GAME_REQUIRED);
    CStreaming::LoadAllRequestedModels(false);
    return CStreaming::ms_aInfoForModel[model].m_nLoadState == LOADSTATE_LOADED;
}

void CGravityGun::BuildGun()
{
    CPlayerPed* me = FindPlayerPed(0);
    if (!me)
        return;
    // the look of the original mod: a pistol body with scaled parts, in the right hand
    for (int m : {348, 2976, 2956})
        if (!LoadModelNow(m))
            return;
    CVector p = me->GetPosition();
    Command<Commands::CREATE_OBJECT>(348, p.x, p.y, p.z, &ms_nGunObject);
    Command<Commands::SET_OBJECT_COLLISION>(ms_nGunObject, false);
    Command<Commands::CREATE_OBJECT>(2976, p.x, p.y, p.z, &ms_nPart1);
    Command<Commands::SET_OBJECT_COLLISION>(ms_nPart1, false);
    Command<Commands::SET_OBJECT_SCALE>(ms_nPart1, 0.75f);
    Command<Commands::ATTACH_OBJECT_TO_OBJECT>(ms_nPart1, ms_nGunObject, 0.1f, 0.025f, 0.1f, 0.0f, 82.5f, 0.0f);
    int* parts[3] = {&ms_nPart2, &ms_nPart3, &ms_nPart4};
    float rot[3][3] = {{0.0f, 15.0f, 0.0f}, {120.0f, 15.0f, 0.0f}, {240.0f, 15.0f, 0.0f}};
    float off[3][3] = {{0.5f, 0.025f, 0.2f}, {0.5f, -0.025f, 0.125f}, {0.5f, 0.025f, 0.125f}};
    for (int i = 0; i < 3; i++)
    {
        Command<Commands::CREATE_OBJECT>(2956, p.x, p.y, p.z, parts[i]);
        Command<Commands::SET_OBJECT_COLLISION>(*parts[i], false);
        Command<Commands::SET_OBJECT_SCALE>(*parts[i], 0.025f);
        Command<Commands::ATTACH_OBJECT_TO_OBJECT>(*parts[i], ms_nGunObject, off[i][0], off[i][1], off[i][2], rot[i][0],
            rot[i][1], rot[i][2]);
    }
    // in the hand exactly like the original (070A, bone 6, flags 16, no anim); the hold sync shows it to the others
    Command<Commands::TASK_PICK_UP_OBJECT>(CPools::GetPedRef(me), ms_nGunObject, 0.0f, 0.0f, 0.0f, 6, 16, "NULL", "NULL", -1);
}

void CGravityGun::DestroyGun()
{
    CPlayerPed* me = FindPlayerPed(0);
    if (me && me->GetHoldingTask())
        me->DropEntityThatThisPedIsHolding(1);
    for (int* h : {&ms_nPart1, &ms_nPart2, &ms_nPart3, &ms_nPart4, &ms_nGunObject})
    {
        if (*h >= 0 && CPools::GetObject(*h))
            Command<Commands::DELETE_OBJECT>(*h);
        *h = -1;
    }
}

// Like the original mod, the gravity gun takes the place of the combat shotgun (weapon 27): it shows up
// in the weapon wheel and works while that weapon is in your hands (it does not shoot shells).
static constexpr int GG_WEAPON = 27;
bool CGravityGun::ms_bEnabled = false;

static bool HoldingGravityWeapon(CPlayerPed* me)
{
    return me->m_aWeapons[me->m_nActiveWeaponSlot].m_eWeaponType == GG_WEAPON;
}

void CGravityGun::Toggle()
{
    CPlayerPed* me = FindPlayerPed(0);
    if (!me)
        return;
    if (ms_bEnabled)
    {
        ms_bEnabled = false;
        Drop();
        DestroyGun();
        ms_bActive = false;
        Command<Commands::SET_PLAYER_FIRE_BUTTON>(0, true);
        CChat::AddMessage("{a0f0a0}[coop] {d6dce8}gravity gun desactivada (la escopeta de combate vuelve a disparar)");
        return;
    }
    ms_bEnabled = true;
    CWeaponInfo* info = CWeaponInfo::GetWeaponInfo((eWeaponType)GG_WEAPON, 1);
    if (info && info->m_nModelId1 > 0)
    {
        CStreaming::RequestModel(info->m_nModelId1, GAME_REQUIRED);
        CStreaming::LoadAllRequestedModels(false);
    }
    me->GiveWeapon((eWeaponType)GG_WEAPON, 1, true);
    me->SetCurrentWeapon((eWeaponType)GG_WEAPON);
    CChat::AddMessage("{a0f0a0}[coop] {d6dce8}gravity gun en el lugar de la escopeta de combate: apunta (clic der.) y clic izq. agarra / lanza, E suelta. Alt+G la quita");
}

void CGravityGun::Shutdown()
{
    if (ms_bActive)
        Toggle();
}

CEntity* CGravityGun::FindTarget(const CVector& from, const CVector& dir, int& type)
{
    CPlayerPed* me = FindPlayerPed(0);
    CColPoint colPoint{};
    CEntity* entity = nullptr;
    CVector to = from + dir * RANGE;
    bool old = me->m_bUsesCollision;
    me->m_bUsesCollision = false;
    bool found = CWorld::ProcessLineOfSight(from, to, colPoint, entity, false, true, true, true, false, false, false, false);
    me->m_bUsesCollision = old;
    if (!found || entity == nullptr)
        return nullptr;
    if (entity->m_nType == ENTITY_TYPE_VEHICLE)
        type = 1;
    else if (entity->m_nType == ENTITY_TYPE_PED)
    {
        if (((CPed*)entity)->IsPlayer())  // other players are not toys
            return nullptr;
        type = 2;
    }
    else if (entity->m_nType == ENTITY_TYPE_OBJECT)
    {
        CObject* o = (CObject*)entity;
        if (o == CPools::GetObject(ms_nGunObject) || o->m_pAttachedTo != nullptr)
            return nullptr;
        type = 3;
    }
    else
        return nullptr;
    return entity;
}

void CGravityGun::Grab(CEntity* e, int type)
{
    ms_pHeld = e;
    ms_nHeldType = type;
    CPhysical* ph = (CPhysical*)e;
    if (type == 3)
    {
        CObject* o = (CObject*)e;
        o->m_bIsStatic = false;  // scenery objects become dynamic (the original made them moveable)
        o->m_bIsStaticWaitingForCollision = false;
    }
    ph->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
    ph->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
    if (type == 2)
    {
        CPed* ped = (CPed*)e;
        Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(CPools::GetPedRef(ped));
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(CPools::GetPedRef(ped), "KO_skid_back", "PED", 4.0f, 0, 0, 0, 1, -1);
    }
}

void CGravityGun::Hold(const CVector& from, const CVector& dir)
{
    if (!EntityAlive(ms_pHeld, ms_nHeldType))
    {
        ms_pHeld = nullptr;
        return;
    }
    CPhysical* ph = (CPhysical*)ms_pHeld;
    CVector target = from + dir * HOLD_DISTANCE;
    CVector delta = target - ph->GetPosition();
    // pull towards the point in front of the gun, in the units the game uses (m per frame at 50 fps)
    CVector speed = delta * 0.25f;
    float maxSpeed = 1.5f;
    if (speed.Magnitude() > maxSpeed)
        speed = speed * (maxSpeed / speed.Magnitude());
    ph->m_vecMoveSpeed = speed;
    ph->m_vecTurnSpeed = ph->m_vecTurnSpeed * 0.9f;
    if (ms_nHeldType == 2)
    {
        CPed* ped = (CPed*)ms_pHeld;
        ped->m_nPedFlags.bIsStanding = false;
        if (delta.Magnitude() > 8.0f)
            ped->SetPosn(target);
    }
    else if (ms_nHeldType == 3 && delta.Magnitude() > 8.0f)
        ph->SetPosn(target);
    ph->m_nPhysicalFlags.bDisableMoveForce = false;
    ph->m_nPhysicalFlags.bApplyGravity = false;
}

void CGravityGun::Throw(const CVector& dir)
{
    if (!EntityAlive(ms_pHeld, ms_nHeldType))
    {
        ms_pHeld = nullptr;
        return;
    }
    CPhysical* ph = (CPhysical*)ms_pHeld;
    ph->m_nPhysicalFlags.bApplyGravity = true;
    float speed = ms_nHeldType == 1 ? 1.6f : 2.5f;  // m per frame
    ph->m_vecMoveSpeed = dir * speed;
    if (ms_nHeldType == 2)
    {
        CPed* ped = (CPed*)ms_pHeld;
        ped->m_fHealth = 0.0f;  // the original kills what it throws
        Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(CPools::GetPedRef(ped), "KO_skid_front", "PED", 4.0f, 0, 0, 0, 1, -1);
    }
    CVector p = ph->GetPosition();
    Command<Commands::ADD_SPARKS>(p.x, p.y, p.z, -dir.x * 2.5f, -dir.y * 2.5f, -dir.z * 2.5f, 150);
    ms_pHeld = nullptr;
    ms_nHeldType = 0;
}

void CGravityGun::Drop()
{
    if (EntityAlive(ms_pHeld, ms_nHeldType))
    {
        CPhysical* ph = (CPhysical*)ms_pHeld;
        ph->m_nPhysicalFlags.bApplyGravity = true;
        if (ms_nHeldType == 2)
            Command<Commands::CLEAR_CHAR_TASKS_IMMEDIATELY>(CPools::GetPedRef((CPed*)ms_pHeld));
    }
    ms_pHeld = nullptr;
    ms_nHeldType = 0;
}

void CGravityGun::Process()
{
    CPlayerPed* me = FindPlayerPed(0);
    bool alt = CPad::NewKeyState.lmenu != 0 || CPad::NewKeyState.rmenu != 0;
    bool g = CPad::NewKeyState.standardKeys['G'] != 0 || CPad::NewKeyState.standardKeys['g'] != 0;
    static bool comboWas = false;
    bool combo = alt && g;
    if (combo && !comboWas && me && !CChat::m_bInputActive)
        Toggle();
    comboWas = combo;

    if (!me)
        return;

    // the gun model is in the hands only while the gravity weapon is selected, on foot and alive
    bool want = ms_bEnabled && !me->m_nPedFlags.bInVehicle && me->m_fHealth > 0.0f && HoldingGravityWeapon(me);
    if (want && !ms_bActive)
    {
        BuildGun();
        ms_bActive = ms_nGunObject >= 0;
        Command<Commands::SET_PLAYER_FIRE_BUTTON>(0, false);
    }
    else if (!want && ms_bActive)
    {
        Drop();
        DestroyGun();
        ms_bActive = false;
        Command<Commands::SET_PLAYER_FIRE_BUTTON>(0, true);
    }
    if (!ms_bActive)
        return;

    // keep it in the hand (the game drops held objects now and then: jumping, falling...)
    if (!me->GetHoldingTask() && CPools::GetObject(ms_nGunObject))
        Command<Commands::TASK_PICK_UP_OBJECT>(CPools::GetPedRef(me), ms_nGunObject, 0.0f, 0.0f, 0.0f, 6, 16, "NULL", "NULL", -1);

    const CVector& front = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
    CVector dir = front;
    dir.Normalise();
    CVector from = me->GetPosition() + CVector(0.0f, 0.0f, 0.5f);

    CPad* pad = CPad::GetPad(0);
    static bool lmbWas = false, eWas = false;
    bool aiming = pad && pad->NewState.RightShoulder1 != 0;
    bool lmb = pad && pad->NewState.ButtonCircle != 0;
    bool e = CPad::NewKeyState.standardKeys['E'] != 0 || CPad::NewKeyState.standardKeys['e'] != 0;
    bool lmbPressed = lmb && !lmbWas, ePressed = e && !eWas;
    lmbWas = lmb;
    eWas = e;

    if (ms_pHeld && !EntityAlive(ms_pHeld, ms_nHeldType))
        Drop();

    if (lmbPressed && GetTickCount() - ms_nLastShot > 200)
    {
        ms_nLastShot = GetTickCount();
        if (ms_pHeld)
            Throw(dir);
        else if (aiming)
        {
            int type = 0;
            if (CEntity* target = FindTarget(from, dir, type))
                Grab(target, type);
        }
    }
    else if (ePressed && ms_pHeld)
        Drop();

    if (ms_pHeld)
    {
        float heading = atan2f(-front.x, front.y);
        me->m_fCurrentRotation = me->m_fAimingRotation = heading;
        Hold(from, dir);
    }
}
