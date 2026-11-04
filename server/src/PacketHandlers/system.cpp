#include "network/packet_types.h"
#include "stdafx.h"
#include "network/packet_handler.h"

PACKET_HANDLER(ePacketType::PLAYER_CHAT_MESSAGE, Packets::System::ChatMessage* pChatMessage, CNetworkPlayer* pNetworkPlayer)
{
	pChatMessage->senderPlayerid = pNetworkPlayer->m_playerId;
	GetPacketFactory().SendToAll(*pChatMessage, pNetworkPlayer);
}
// [private build] cheats: relay to everyone else, the clients decide what to apply
PACKET_HANDLER(ePacketType::CHEAT_ACTIVATED, Packets::Cheats::CheatActivated* pCheat, CNetworkPlayer* pNetworkPlayer)
{
	pCheat->senderPlayerid = pNetworkPlayer->m_playerId;
	logger::info("player %s activated cheat #%d", to_string(pNetworkPlayer->m_playerId).c_str(), pCheat->cheatIndex);
	GetPacketFactory().SendToAll(*pCheat, pNetworkPlayer);
}
