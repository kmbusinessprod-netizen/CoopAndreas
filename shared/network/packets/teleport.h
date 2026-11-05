#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "serialize.h"

// [private build] teleport requests between players (both must agree)
namespace Packets::Teleport
{
class TeleportRequest : public Packet
{
    DEFINE_PACKET_TYPE(TeleportRequest, ePacketType::TELEPORT_REQUEST, ePacketChannel::SYSTEM);

public:
    enum eKind : uint8_t
    {
        GO_TO_TARGET = 0,   // sender wants to teleport to target
        BRING_TARGET = 1,   // sender wants target to teleport to him
    };
    enum ePhase : uint8_t
    {
        REQUEST = 0,  // sender -> server -> target
        ACCEPT = 1,   // target -> server -> sender
        REJECT = 2,
    };

    SenderPlayerId senderPlayerid{};
    playerid_t targetPlayerid{};
    uint8_t kind = GO_TO_TARGET;
    uint8_t phase = REQUEST;

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_playerid(stream, targetPlayerid);
        serialize_uint8(stream, kind);
        serialize_uint8(stream, phase);
        return true;
    }
};
}  // namespace Packets::Teleport
