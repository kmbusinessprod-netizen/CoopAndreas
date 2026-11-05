#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "network/serializable_types.h"
#include "serialize.h"

// [private build] a player changed his skin (ped model). 0 = CJ (clothes)
namespace Packets::Skin
{
class PlayerSkin : public Packet
{
    DEFINE_PACKET_TYPE(PlayerSkin, ePacketType::PLAYER_SKIN, ePacketChannel::EVENT);

public:
    SenderPlayerId senderPlayerid{};
    uint16_t modelId = 0;

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_uint16(stream, modelId);
        return true;
    }
};

// a player bought a property (index into the script's property arrays). The others mark it bought too
class PropertyBought : public Packet
{
    DEFINE_PACKET_TYPE(PropertyBought, ePacketType::PROPERTY_BOUGHT, ePacketChannel::EVENT);

public:
    SenderPlayerId senderPlayerid{};
    uint8_t index = 0;

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_uint8(stream, index);
        return true;
    }
};
}  // namespace Packets::Skin
