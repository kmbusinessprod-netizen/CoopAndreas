#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "serialize.h"

// [private build] cheat sync, see client/src/CCheatSync.cpp
namespace Packets::Cheats
{
class CheatActivated : public Packet
{
    DEFINE_PACKET_TYPE(CheatActivated, ePacketType::CHEAT_ACTIVATED, ePacketChannel::SYSTEM);

public:
    SenderPlayerId senderPlayerid{};
    int cheatIndex = 0;  // index into CCheat::m_aCheatFunctions (0..91)
    bool active = true;  // state of the cheat after this activation (for toggles)

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_int(stream, cheatIndex, 0, 127);
        serialize_bool(stream, active);
        return true;
    }
};
}  // namespace Packets::Cheats
