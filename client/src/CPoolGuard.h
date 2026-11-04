#pragma once
// [private build] safety net around the game's entity pools.
//
// CPool::Delete() trusts the pointer it gets: a pointer that is not inside the pool
// writes the "free" flag out of bounds and leaves m_nFirstFree negative, so the next
// New() hands out an address outside the pool and the constructor crashes (that was
// crashes #4/#5/#6/#7 in docs/NOTAS_PRUEBAS.md). These hooks replace the vehicle and
// ped operator delete with a version that validates the pointer first, ignores bad
// deletes and logs who asked for them.
class CPoolGuard
{
public:
    static void Install();

    // true if `vehicle` is a live object of the vehicle pool
    static bool IsVehicleAlive(CVehicle* vehicle);
    // true if `ped` is a live object of the ped pool
    static bool IsPedAlive(CPed* ped);

    static int ms_nIgnoredDeletes;
};
