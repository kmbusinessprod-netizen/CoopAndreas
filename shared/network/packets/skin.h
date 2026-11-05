#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "network/serializable_types.h"
#include "serialize.h"
#include "CVector.h"

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
// what a player carries in his hands (burglary boxes, TVs...). modelId 0 = nothing
class PlayerHold : public Packet
{
    DEFINE_PACKET_TYPE(PlayerHold, ePacketType::PLAYER_HOLD, ePacketChannel::EVENT);

public:
    SenderPlayerId senderPlayerid{};
    uint16_t modelId = 0;
    CVector offset{};
    uint8_t bone = 0;
    uint8_t boneFlags = 0;
    uint32_t animId = 0;
    uint32_t animGroup = 0;

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_uint16(stream, modelId);
        serialize_float(stream, offset.x);
        serialize_float(stream, offset.y);
        serialize_float(stream, offset.z);
        serialize_uint8(stream, bone);
        serialize_uint8(stream, boneFlags);
        serialize_uint32(stream, animId);
        serialize_uint32(stream, animGroup);
        return true;
    }
};
}  // namespace Packets::Skin
