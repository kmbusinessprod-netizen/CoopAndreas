#pragma once
// [private build] the Genkidama (spirit bomb). Only while floating still (Volar): Alt + K asks everyone for
// their energy (Y / N). Everybody gives it -> 10 s charge, at least half -> 20 s, less -> nothing. The ball
// grows over the caster's head (everyone sees it and the bar), aimed with the camera until the throw
// sound starts; 2 s later it flies off and explodes on the first thing it touches: nuke-sized blast for
// everyone (players, peds, cars). Close = dead; a bit further = thrown into the sky, no flying, dead on
// landing; the caster is thrown but survives. 5 minutes between two genkidamas (server wide).
class CGenkidama
{
public:
    static void Process();  // gameProcessEvent
    static void Draw();     // drawingEvent
    static void OnPacket(Packets::Genkidama::Genkidama* packet);
    static bool IsLocalActive();  // asking / charging / throwing: the player holds still in the air
    static bool IsLaunched();     // blown away by the blast: no flying until he lands (and dies)
};
