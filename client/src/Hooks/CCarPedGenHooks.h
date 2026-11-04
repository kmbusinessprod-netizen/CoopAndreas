#pragma once
class CCarPedGenHooks
{
public:
    static constexpr float NEAR_PLAYER_DISTANCE = 60.0f;
    static bool IsNearAnyNetworkPlayer(const CVector& pos, float fDistance);
    static bool IsSphereVisibleForNetworkPlayer(CVector* pOrigin, float fRadius, CNetworkPlayer* pNetworkPlayer);
    static bool IsSphereVisibleForAnyNetworkPlayer(CVector* pOrigin, float fRadius);
    static bool IsSphereVisibleForAnyPlayer(CVector* pOrigin, float fRadius);
    static bool IsEntityOnAnyPlayersScreen(CEntity* pEntity);
    static void InjectHooks();
};
