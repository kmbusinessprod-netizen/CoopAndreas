#pragma once
// [private build] what each player carries in his hands (burglary loot): the others see the object and
// the carry animation. Interiors are local to every machine, only the carried object is shared.
class CHoldSync
{
public:
    static void Process();  // local: detect changes; remote: keep tasks alive
    static void OnPacket(Packets::Skin::PlayerHold* packet);
    static void OnPlayerRemoved(CPlayerPed* ped);
    // put an object in the local player's hand (the change is picked up and sent like any other hold)
    static void HoldLocally(CObject* object, const CVector& offset, int bone, int boneFlags);
};
