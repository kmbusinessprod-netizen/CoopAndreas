#pragma once
// [private build] Dragon Ball style powers, switched on per player in F2 > Jugador:
//   Volar:       jump again while in the air -> flying. Mouse = direction, W/S/A/D move, jump = up,
//                crouch (C) = down. Touch the ground going down (or get in a car) to stop.
//   Velocidad:   faster walking / running.
//   Speed bar:   Alt + mouse wheel (1..10), used by both.
// The others see the flight pose (PLAYER_POWER, power 3).
class CFlight
{
public:
    static bool ms_bFlyEnabled;
    static bool ms_bSpeedEnabled;
    static int ms_nLevel;  // 1..10

    static void Process();  // gameProcessEvent
    static void Draw();     // drawingEvent
    static bool IsFlying();
    static bool WantsWheel();  // Alt held with a power on: the wheel is ours, not the weapon switch
    static void OnPacket(Packets::Skin::PlayerPower* packet);
    static int ms_nWheel;      // collected in the pad hook
};
