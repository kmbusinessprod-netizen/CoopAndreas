#include "stdafx.h"
#include "CPoolGuard.h"
#include <intrin.h>

int CPoolGuard::ms_nIgnoredDeletes = 0;

// slot index of `ptr` inside a pool whose objects are `objectSize` bytes, -1 if it is not a pool object
static int SlotOf(void* objects, size_t objectSize, int poolSize, void* ptr)
{
    if (objects == nullptr || ptr == nullptr)
        return -1;

    ptrdiff_t diff = (uint8_t*)ptr - (uint8_t*)objects;
    if (diff < 0 || (diff % (ptrdiff_t)objectSize) != 0)
        return -1;

    ptrdiff_t slot = diff / (ptrdiff_t)objectSize;
    return slot < poolSize ? (int)slot : -1;
}

static void LogBadDelete(const char* what, void* ptr, int slot, void* caller)
{
    CPoolGuard::ms_nIgnoredDeletes++;

    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)caller, &self);
    HMODULE mod = GetModuleHandleA("CoopAndreasSA.dll");

    char where[64];
    if (self != nullptr && self == mod)
        sprintf_s(where, "CoopAndreasSA.dll+0x%X", (unsigned)((uintptr_t)caller - (uintptr_t)mod));
    else
        sprintf_s(where, "0x%08X", (unsigned)(uintptr_t)caller);

    if (slot < 0)
        printf("[guard] ignored delete of %s %p: not a pool object (caller %s)\n", what, ptr, where);
    else
        printf("[guard] ignored delete of %s %p: slot %d already free (caller %s)\n", what, ptr, slot, where);

    CChat::AddMessage("{f0a050}[guard] {d6dce8}se evito un crash (%s borrado dos veces, desde %s)", what, where);
}

template <class P>
static void GuardedPoolDelete(P* pool, size_t objectSize, void* ptr, const char* what, void* caller)
{
    if (pool == nullptr)
        return;

    int slot = SlotOf(pool->m_pObjects, objectSize, pool->m_nSize, ptr);
    if (slot < 0 || pool->m_byteMap[slot].bEmpty)
    {
        LogBadDelete(what, ptr, slot, caller);
        return;
    }

    // what the original CPool::Delete does
    pool->m_byteMap[slot].bEmpty = true;
    if (slot < pool->m_nFirstFree)
        pool->m_nFirstFree = slot;
}

// CVehicle::operator delete, 0x6E2D90
static void __cdecl CVehicle__operator_delete_Hook(void* ptr)
{
    GuardedPoolDelete(CPools::ms_pVehiclePool, sizeof(CHeli), ptr, "vehiculo", _ReturnAddress());
}

// CPed::operator delete, 0x5E4760
static void __cdecl CPed__operator_delete_Hook(void* ptr)
{
    GuardedPoolDelete(CPools::ms_pPedPool, sizeof(CCopPed), ptr, "ped", _ReturnAddress());
}

bool CPoolGuard::IsVehicleAlive(CVehicle* vehicle)
{
    auto pool = CPools::ms_pVehiclePool;
    if (pool == nullptr)
        return false;
    int slot = SlotOf(pool->m_pObjects, sizeof(CHeli), pool->m_nSize, vehicle);
    return slot >= 0 && !pool->m_byteMap[slot].bEmpty;
}

bool CPoolGuard::IsPedAlive(CPed* ped)
{
    auto pool = CPools::ms_pPedPool;
    if (pool == nullptr)
        return false;
    int slot = SlotOf(pool->m_pObjects, sizeof(CCopPed), pool->m_nSize, ped);
    return slot >= 0 && !pool->m_byteMap[slot].bEmpty;
}

template <class P>
static int CountFree(P* pool)
{
    int n = 0;
    for (int i = 0; i < pool->m_nSize; i++)
        n += pool->m_byteMap[i].bEmpty ? 1 : 0;
    return n;
}

static bool IsLocalOrUnsynced(CEntity* entity, bool isVehicle)
{
    if (isVehicle)
    {
        CNetworkVehicle* nv = CNetworkVehicleManager::GetVehicle(entity);
        return nv == nullptr || nv->m_bSyncing;
    }
    CNetworkPed* np = CNetworkPedManager::GetPed((CPed*)entity);
    return np == nullptr || np->m_bSyncing;
}

bool CPoolGuard::EnsureFreeVehicleSlot()
{
    auto pool = CPools::ms_pVehiclePool;
    if (pool == nullptr)
        return false;
    if (CountFree(pool) > 2)
        return true;

    CVector me = FindPlayerCoors(0);
    CVehicle* best = nullptr;
    float bestDist = 0.0f;
    for (int i = 0; i < pool->m_nSize; i++)
    {
        if (pool->m_byteMap[i].bEmpty)
            continue;
        CVehicle* v = pool->GetAt(i);
        if (v == nullptr || v->m_nCreatedBy == MISSION_VEHICLE || v->m_pDriver && v->m_pDriver->IsPlayer())
            continue;
        if (!IsLocalOrUnsynced(v, true))
            continue;
        float d = (v->GetPosition() - me).Magnitude();
        if (d > bestDist)
        {
            bestDist = d;
            best = v;
        }
    }

    if (best)
    {
        printf("[guard] vehicle pool full: removing a random vehicle %.0f m away\n", bestDist);
        CWorld::Remove(best);
        delete best;
    }
    bool ok = CountFree(pool) > 0;
    if (!ok)
        printf("[guard] vehicle pool full: skipping creation\n");
    return ok;
}

bool CPoolGuard::EnsureFreePedSlot()
{
    auto pool = CPools::ms_pPedPool;
    if (pool == nullptr)
        return false;
    if (CountFree(pool) > 2)
        return true;

    CVector me = FindPlayerCoors(0);
    CPed* best = nullptr;
    float bestDist = 0.0f;
    for (int i = 0; i < pool->m_nSize; i++)
    {
        if (pool->m_byteMap[i].bEmpty)
            continue;
        CPed* p = pool->GetAt(i);
        if (p == nullptr || p->IsPlayer() || p->m_nCreatedBy != 1 /*RANDOM_CHAR*/ || p->m_nPedFlags.bInVehicle)
            continue;
        if (!IsLocalOrUnsynced(p, false))
            continue;
        float d = (p->GetPosition() - me).Magnitude();
        if (d > bestDist)
        {
            bestDist = d;
            best = p;
        }
    }

    if (best)
    {
        printf("[guard] ped pool full: removing a random ped %.0f m away\n", bestDist);
        CPopulation::RemovePed(best);
    }
    bool ok = CountFree(pool) > 0;
    if (!ok)
        printf("[guard] ped pool full: skipping creation\n");
    return ok;
}

// --- crash #2: two players on one bike -----------------------------------------------------
// 0x523E5D..0x523E72 does `ped->m_pIntelligence->m_TaskMgr.GetActiveTask()->GetTaskType()` without
// a null check. A remote ped can have no active task for a frame. We hand the code a dummy task
// whose GetTaskType() never matches, so it simply skips that block.
static int __fastcall FakeTask_GetTaskType(void*, int)
{
    return -1;
}
static void* s_fakeTaskVtable[16] = {
    nullptr, nullptr, nullptr, nullptr, (void*)FakeTask_GetTaskType,  // slot 4 = +0x10 = GetTaskType
};
static void* s_fakeTask[2] = {s_fakeTaskVtable, nullptr};

static CTask* __fastcall CTaskManager__GetActiveTask_Safe(CTaskManager* mgr, int)
{
    CTask* task = mgr ? mgr->GetActiveTask() : nullptr;
    return task ? task : (CTask*)s_fakeTask;
}

// [private build] CAEVehicleAudioEntity::Service (0x502302) turns the radio on for any vehicle whose
// driver "is a player" - remote players are CPlayerPed too, so everybody heard everybody's radio.
// Only the local player counts.
static bool __fastcall CPed__IsLocalPlayer_Hook(CPed* ped, int)
{
    return ped != nullptr && ped == FindPlayerPed(0);
}

// [private build] dying must not change the world for everybody: the game passes 12 hours and
// wipes every vehicle/ped within 4 km when the player is wasted/busted. In a coop game the
// time comes from the host and the vehicles belong to everyone, so skip both while connected.
static void __cdecl CGameLogic__PassTime_Hook(unsigned int minutes)
{
    if (CNetwork::m_bConnected)
        return;
    plugin::Call<0x4414C0, unsigned int>(minutes);
}

static void __cdecl CWorld__ClearExcitingStuffFromArea_Hook(const CVector* point, float radius, unsigned char removeProjectiles)
{
    if (CNetwork::m_bConnected)
        return;
    plugin::Call<0x56A0D0, const CVector*, float, unsigned char>(point, radius, removeProjectiles);
}

void CPoolGuard::Install()
{
    patch::RedirectCall(0x4429C5, (void*)CGameLogic__PassTime_Hook);
    patch::RedirectCall(0x442FCF, (void*)CGameLogic__PassTime_Hook);
    patch::RedirectCall(0x4429BB, (void*)CWorld__ClearExcitingStuffFromArea_Hook);
    patch::RedirectCall(0x442F08, (void*)CWorld__ClearExcitingStuffFromArea_Hook);

    patch::RedirectCall(0x502302, (void*)CPed__IsLocalPlayer_Hook);

    injector::MakeJMP(0x6E2D90, (void*)CVehicle__operator_delete_Hook);
    injector::MakeJMP(0x5E4760, (void*)CPed__operator_delete_Hook);

    patch::RedirectCall(0x523E66, (void*)CTaskManager__GetActiveTask_Safe);
}
