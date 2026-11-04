#pragma once
// [private build] playit.gg tunnel helper: starts the playit agent if needed and shows the address to share
class CTunnel
{
public:
    // returns true if a tunnel address is configured (then UPnP is not needed)
    static bool Start();
};
