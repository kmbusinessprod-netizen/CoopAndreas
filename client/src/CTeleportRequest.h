#pragma once
// [private build] "go to player" / "bring player" with a yes/no prompt on the other side
class CTeleportRequest
{
public:
    static void RequestGoTo(CNetworkPlayer* target);
    static void RequestBring(CNetworkPlayer* target);
    static void OnPacket(Packets::Teleport::TeleportRequest* packet);

private:
    static void TeleportNextTo(CNetworkPlayer* player);
    static void Reply(playerid_t to, uint8_t kind, bool accept);
};
