#pragma once
// [private build] proximity ownership.
//
// Random peds and traffic are simulated by the player that created them. When that player
// drives away, his game removes them as "too far" (they vanish for the player standing next
// to them) and, before that, they freeze because his game has no collision loaded there.
// This hands random peds/vehicles over to the player closest to them, so the world around
// every player is always simulated by someone who is actually there.
class CProximityOwnership
{
public:
    static void Update();  // call from the main loop, cheap (runs every ~700 ms)
    static bool IsEnabled();
};
