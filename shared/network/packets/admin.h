#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "network/serializable_types.h"
#include "serialize.h"

// [private build] host (admin) actions, see client/src/UI/CNetworkPlayerList.cpp and server/src/PacketHandlers/admin.cpp
namespace Packets::Admin
{
enum eAdminAction : uint8_t
{
    ADMIN_KICK = 0,   // disconnect the target
    ADMIN_BAN = 1,    // disconnect the target and refuse it from now on
    ADMIN_BRING = 2,  // teleport the target to `pos`
};

class AdminAction : public Packet
{
    DEFINE_PACKET_TYPE(AdminAction, ePacketType::ADMIN_ACTION, ePacketChannel::SYSTEM);

public:
    SenderPlayerId senderPlayerid{};
    playerid_t targetPlayerid{};
    uint8_t action = ADMIN_KICK;
    WorldPositionCompressed pos{};
    uint8_t areaCode = 0;

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_playerid(stream, targetPlayerid);
        serialize_uint8(stream, action);
        serialize_object(stream, pos);
        serialize_uint8(stream, areaCode);
        return true;
    }
};
}  // namespace Packets::Admin
