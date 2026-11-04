#pragma once
// [private build] automatic port forwarding through the router (UPnP), Windows only
class CUPnP
{
public:
    // starts a background thread that maps UDP `port` on the router to this PC
    static void StartAsync(unsigned short port);
    // removes the mapping (called when the server window closes)
    static void Cleanup();
};
