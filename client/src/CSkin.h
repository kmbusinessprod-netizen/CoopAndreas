#pragma once
// [private build] skins: any ped model on a player, CJ's animations are kept (CPlayerPed picks its anim
// groups from the fat/muscle stats, not from the model). 0 = CJ with his clothes.
class CSkin
{
public:
    static uint16_t ms_nLocalSkin;

    static bool IsValidPedModel(int skin);
    static int SpecialCount();
    static uint16_t SpecialSkin(int i);  // skin id of story character i
    static const char* ModelName(int modelId);
    static void ApplyToPed(CPlayerPed* ped, uint16_t modelId);  // requests the model, applies when loaded
    static void Process();  // gameProcessEvent: applies pending skins
    static void Process2();  // my saved skin from the server

    static void SetLocal(uint16_t modelId);        // apply here and tell everybody
    static void SendLocal();                        // re-send (a player joined)
    static void OnPacket(Packets::Skin::PlayerSkin* packet);
};
