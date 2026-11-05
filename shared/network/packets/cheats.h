#pragma once

#include "config.h"
#include "network/packet.h"
#include "network/packet_types.h"
#include "serialize.h"

// [private build] cheat sync, see client/src/CCheatSync.cpp and server/src/CCheatVote.cpp
namespace Packets::Cheats
{
// server -> clients: apply / announce a cheat now
class CheatActivated : public Packet
{
    DEFINE_PACKET_TYPE(CheatActivated, ePacketType::CHEAT_ACTIVATED, ePacketChannel::SYSTEM);

public:
    SenderPlayerId senderPlayerid{};
    int cheatIndex = 0;  // index into CCheat::m_aCheatFunctions (0..91)
    bool active = true;  // state of the cheat after this activation (for toggles)
    bool apply = true;   // false = only announce (personal cheat of another player)

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_int(stream, cheatIndex, 0, 127);
        serialize_bool(stream, active);
        serialize_bool(stream, apply);
        return true;
    }
};

// world cheats are voted: client -> server PROPOSE, server -> clients ASK, client -> server REPLY,
// server -> clients RESULT (then a CheatActivated follows when accepted)
class CheatVote : public Packet
{
    DEFINE_PACKET_TYPE(CheatVote, ePacketType::CHEAT_VOTE, ePacketChannel::SYSTEM);

public:
    enum ePhase : uint8_t
    {
        PROPOSE = 0,
        ASK = 1,
        REPLY = 2,
        RESULT = 3,
    };

    SenderPlayerId senderPlayerid{};  // proposer (ASK) / replier (REPLY) / rejecter (RESULT)
    uint8_t phase = PROPOSE;
    int cheatIndex = 0;
    bool accept = false;  // REPLY: the answer; RESULT: accepted or not

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_object(stream, senderPlayerid);
        serialize_uint8(stream, phase);
        serialize_int(stream, cheatIndex, 0, 127);
        serialize_bool(stream, accept);
        return true;
    }
};

// client -> server after applying a world cheat (so the server knows the current world state);
// server -> client when joining (so the newcomer starts with the same world)
class CheatState : public Packet
{
    DEFINE_PACKET_TYPE(CheatState, ePacketType::CHEAT_STATE, ePacketChannel::SYSTEM);

public:
    bool active[96] = {};
    int timeScale = 1000;     // CTimer::ms_fTimeScale * 1000
    int forcedWeather = -1;   // CWeather::ForcedWeatherType

private:
    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        for (int i = 0; i < 96; i++)
            serialize_bool(stream, active[i]);
        serialize_int(stream, timeScale, 0, 8000);
        serialize_int(stream, forcedWeather, -1, 255);
        return true;
    }
};
}  // namespace Packets::Cheats
