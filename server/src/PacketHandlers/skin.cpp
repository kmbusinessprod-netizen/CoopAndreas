#include "stdafx.h"
#include "network/packets/skin.h"

// [private build] skins and bought properties: relay, and remember them for players that join later
std::vector<uint8_t> g_boughtProperties;

PACKET_HANDLER(ePacketType::PLAYER_SKIN, Packets::Skin::PlayerSkin* pSkin, CNetworkPlayer* pNetworkPlayer)
{
	pSkin->senderPlayerid = pNetworkPlayer->m_playerId;
	pNetworkPlayer->m_nSkin = pSkin->modelId;
	logger::info("player %s skin -> %d", to_string(pNetworkPlayer->m_playerId).c_str(), pSkin->modelId);
	GetPacketFactory().SendToAll(*pSkin, pNetworkPlayer);
}

PACKET_HANDLER(ePacketType::PROPERTY_BOUGHT, Packets::Skin::PropertyBought* pProp, CNetworkPlayer* pNetworkPlayer)
{
	pProp->senderPlayerid = pNetworkPlayer->m_playerId;
	if (std::find(g_boughtProperties.begin(), g_boughtProperties.end(), pProp->index) == g_boughtProperties.end())
		g_boughtProperties.push_back(pProp->index);
	logger::info("player %s bought property #%d", to_string(pNetworkPlayer->m_playerId).c_str(), pProp->index);
	GetPacketFactory().SendToAll(*pProp, pNetworkPlayer);
}

PACKET_HANDLER(ePacketType::PLAYER_HOLD, Packets::Skin::PlayerHold* pHold, CNetworkPlayer* pNetworkPlayer)
{
	pHold->senderPlayerid = pNetworkPlayer->m_playerId;
	GetPacketFactory().SendToAll(*pHold, pNetworkPlayer);
}
