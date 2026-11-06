#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "network/serializable_types.h"
#include "serialize.h"
#include "CVector.h"

// [private build] the Genkidama (spirit bomb): one packet, many phases.
//   REQUEST  caster -> server              "I want to make one"
//   ASK      server -> everyone else       "<caster> asks for your energy"
//   REPLY    voter  -> server              accept = gives energy or not
//   START    server -> everyone            value = seconds the charge takes (10 if everyone gave, 20 if half)
//   DENIED   server -> caster              value = reason (1 cooldown, seconds in pos.x; 2 not enough energy;
//                                          3 someone else is making one)
//   STATE    caster -> server -> everyone  the ball: pos, dir (aim), value = progress in %, accept = phase
//   THROW    caster -> server -> everyone  thrown from pos towards dir
//   EXPLODE  caster -> server -> everyone  boom at pos
//   CANCEL   caster -> server -> everyone  gave up / died
namespace Packets::Genkidama
{
class Genkidama : public Packet
{
    DEFINE_PACKET_TYPE(Genkidama, ePacketType::GENKIDAMA, ePacketChannel::SYSTEM);

public:
    enum ePhase : uint8_t
    {
        REQUEST = 0,
        ASK = 1,
        REPLY = 2,
        START = 3,
        DENIED = 4,
        STATE = 5,
        THROW = 6,
        EXPLODE = 7,
        CANCEL = 8,
    };

    SenderPlayerId senderPlayerid{};  // the caster (REPLY: the voter)
    uint8_t phase = REQUEST;
    uint8_t accept = 0;
    uint16_t value = 0;
    CVector pos{};
    CVector dir{};

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_uint8(stream, phase);
        serialize_uint8(stream, accept);
        serialize_uint16(stream, value);
        serialize_float(stream, pos.x);
        serialize_float(stream, pos.y);
        serialize_float(stream, pos.z);
        serialize_float(stream, dir.x);
        serialize_float(stream, dir.y);
        serialize_float(stream, dir.z);
        return true;
    }
};
}  // namespace Packets::Genkidama
