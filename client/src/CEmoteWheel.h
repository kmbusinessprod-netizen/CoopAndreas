#pragma once
// [private build] emote wheel: hold B, move the mouse towards an emote, release B. Moving cancels it.
// The others see it (PLAYER_POWER packet, power 1).
class CEmoteWheel
{
public:
    static bool IsOpen() { return ms_bOpen; }
    static void Process();  // gameProcessEvent
    static void Draw();     // drawingEvent
    static void OnPacket(Packets::Skin::PlayerPower* packet);
    static float ms_fMouseX, ms_fMouseY;

private:
    static bool ms_bOpen;
    static int ms_nSelected;
    static int ms_nPlaying;  // emote index + 1, 0 none
    static unsigned int ms_nStartedAt;
};
