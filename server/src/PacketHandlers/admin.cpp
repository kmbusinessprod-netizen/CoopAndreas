#include "stdafx.h"
#include "CBanList.h"

// [private build] actions only the host may perform (kick / ban / bring a player)
PACKET_HANDLER(ePacketType::ADMIN_ACTION, Packets::Admin::AdminAction* pAction, CNetworkPlayer* pNetworkPlayer)
{
	if (!pNetworkPlayer->m_bIsHost)
	{
		logger::warn("player %s tried an admin action without being host", to_string(pNetworkPlayer->m_playerId).c_str());
		return;
	}

	CNetworkPlayer* pTarget = CNetworkPlayerManager::GetPlayer(pAction->targetPlayerid);
	if (pTarget == nullptr || pTarget == pNetworkPlayer)
		return;

	pAction->senderPlayerid = pNetworkPlayer->m_playerId;

	switch (pAction->action)
	{
	case Packets::Admin::ADMIN_KICK:
	case Packets::Admin::ADMIN_BAN:
	{
		bool ban = pAction->action == Packets::Admin::ADMIN_BAN;
		if (ban)
			CBanList::Add(pTarget->m_Name);

		Packets::System::PlayerDisconnected disconnected{};
		disconnected.payload.playerid = -1;
		disconnected.payload.reason = ban ? Packets::System::PlayerDisconnected::DISCONNECTION_REASON_BANNED
		                                  : Packets::System::PlayerDisconnected::DISCONNECTION_REASON_KICKED;
		GetPacketFactory().Send(disconnected, pTarget);
		enet_peer_disconnect_later(pTarget->m_pPeer, 0);

		printf("[Admin] : el host %s a %s\n", ban ? "baneo" : "expulso", pTarget->m_Name);
		break;
	}
	case Packets::Admin::ADMIN_BRING:
		GetPacketFactory().Send(*pAction, pTarget);
		break;
	}
}
