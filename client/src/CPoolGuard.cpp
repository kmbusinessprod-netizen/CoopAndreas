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

void CPoolGuard::Install()
{
    injector::MakeJMP(0x6E2D90, (void*)CVehicle__operator_delete_Hook);
    injector::MakeJMP(0x5E4760, (void*)CPed__operator_delete_Hook);

    patch::RedirectCall(0x523E66, (void*)CTaskManager__GetActiveTask_Safe);
}
