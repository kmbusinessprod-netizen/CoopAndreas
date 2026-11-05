#include "stdafx.h"
#include "CCheatVote.h"
#include "network/packets/cheats.h"

// a player applied a personal cheat (or a world cheat while alone): just tell the others
PACKET_HANDLER(ePacketType::CHEAT_ACTIVATED, Packets::Cheats::CheatActivated* pCheat, CNetworkPlayer* pNetworkPlayer)
{
	pCheat->senderPlayerid = pNetworkPlayer->m_playerId;
	pCheat->apply = false;
	logger::info("player %s activated cheat #%d (%s)", to_string(pNetworkPlayer->m_playerId).c_str(), pCheat->cheatIndex,
		CheatInfo::Name(pCheat->cheatIndex));
	GetPacketFactory().SendToAll(*pCheat, pNetworkPlayer);
}

PACKET_HANDLER(ePacketType::CHEAT_VOTE, Packets::Cheats::CheatVote* pVote, CNetworkPlayer* pNetworkPlayer)
{
	switch (pVote->phase)
	{
	case Packets::Cheats::CheatVote::PROPOSE:
		CCheatVote::Propose(pNetworkPlayer, pVote->cheatIndex);
		break;
	case Packets::Cheats::CheatVote::REPLY:
		CCheatVote::Reply(pNetworkPlayer, pVote->accept);
		break;
	default:
		break;
	}
}

PACKET_HANDLER(ePacketType::CHEAT_STATE, Packets::Cheats::CheatState* pState, CNetworkPlayer* pNetworkPlayer)
{
	CCheatVote::RecordState(*pState);
}

// teleport requests: REQUEST goes to the target, the answer goes back to the requester
PACKET_HANDLER(ePacketType::TELEPORT_REQUEST, Packets::Teleport::TeleportRequest* pRequest, CNetworkPlayer* pNetworkPlayer)
{
	CNetworkPlayer* pTarget = CNetworkPlayerManager::GetPlayer(pRequest->targetPlayerid);
	if (pTarget == nullptr || pTarget == pNetworkPlayer)
		return;

	pRequest->senderPlayerid = pNetworkPlayer->m_playerId;
	GetPacketFactory().Send(*pRequest, pTarget);
}
