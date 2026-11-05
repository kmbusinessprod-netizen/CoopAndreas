#include "CEntryExitMarkerSync.h"
#include "CRadarSync.h"
#include "COnscreenTimerSync.h"
#include "network/packet_types.h"
#include "network/packets/system.h"
#include "stdafx.h"
#include "CSkin.h"
#include "CPropertySync.h"
#include "CHoldSync.h"
#include "CKamehameha.h"
#include "CEmoteWheel.h"
#include "CFlight.h"
#include <CWeatherSync.h>
#include <CMoonSync.h>
#include <game_sa/CTagManager.h>

PACKET_HANDLER(ePacketType::PLAYER_CONNECTED, Packets::System::PlayerConnected* pPlayerConnected)
{
    CNetworkPlayer* pNetworkPlayer =
        new CNetworkPlayer(pPlayerConnected->payload.GetPlayerId(), CVector(2246.506f, -1259.552f, 23.9531f));
    CNetworkPlayerManager::Add(pNetworkPlayer);

    strcpy_s(pNetworkPlayer->m_Name, pPlayerConnected->payload.name);

    pNetworkPlayer->m_bHasBeenConnectedBeforeMe = pPlayerConnected->payload.isAlreadyConnected;

    if (!pNetworkPlayer->m_bHasBeenConnectedBeforeMe)
    {
        CChat::AddMessage("{c7a6ff}[Player] {d6dce8}" + pNetworkPlayer->GetName() + " connected");
        if (CSkin::ms_nLocalSkin != 0)
            CSkin::SendLocal();  // [private build] the newcomer must see my skin
    }
    // TODO REFACTOR
    if (CLocalPlayer::m_bIsHost)
    {
        CRadarSync::SendAllTraces();
        COnscreenTimerSync::SendState();
        CEntryExitMarkerSync::ms_bUpdateAfterProcessingThisFrame = true;
        CTagSync::SyncCurrentState();
        CMoonSync::SyncCurrentState();
    }
}

PACKET_HANDLER(ePacketType::PLAYER_DISCONNECTED, Packets::System::PlayerDisconnected* pPlayerDisconnected)
{
    if (pPlayerDisconnected->payload.playerid == -1)
    {
        if (pPlayerDisconnected->payload.reason ==
            Packets::System::PlayerDisconnected::DISCONNECTION_REASON_VERSION_MISMATCH)
        {
            char buffer[23];
            semver_t expected;
            semver_unpack(pPlayerDisconnected->payload.version, &expected);
            semver_to_string(&expected, buffer, sizeof buffer);
            buffer[22] = '\0';
            CChat::AddMessage(
                "{63c7e8}[Network] {e8b86d}Version mismatch, server: %s client: %s", buffer, COOPANDREAS_VERSION);
        }
        else if (pPlayerDisconnected->payload.reason ==
                 Packets::System::PlayerDisconnected::DISCONNECTION_REASON_NAME_TAKEN)
        {
            CChat::AddMessage("{63c7e8}[Network] {e8b86d}Nickname '%s' is already taken.", CLocalPlayer::m_Name);
        }
        else if (pPlayerDisconnected->payload.reason == Packets::System::PlayerDisconnected::DISCONNECTION_REASON_KICKED)
        {
            CChat::AddMessage("{63c7e8}[Network] {f07878}El host te expulso de la partida.");
        }
        else if (pPlayerDisconnected->payload.reason == Packets::System::PlayerDisconnected::DISCONNECTION_REASON_BANNED)
        {
            CChat::AddMessage("{63c7e8}[Network] {f07878}Estas baneado de esta partida.");
        }
        else if (pPlayerDisconnected->payload.reason == Packets::System::PlayerDisconnected::DISCONNECTION_REASON_WRONG_PASSWORD)
        {
            CChat::AddMessage("{63c7e8}[Network] {f07878}Contrasena incorrecta.");
        }
        CNetwork::Disconnect();
        return;
    }

    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(pPlayerDisconnected->payload.GetPlayerId());
    if (player != nullptr)
    {
        CChat::AddMessage("{c7a6ff}[Player] {d6dce8}" + std::string(player->GetName()) + " disconnected");
        CNetworkPlayerManager::Remove(player);
        delete player;
    }
}

PACKET_HANDLER(ePacketType::PLAYER_HANDSHAKE, Packets::System::PlayerHandshake* pPlayerHandshake)
{
    if (!pPlayerHandshake->yourid.valid())
        return;

    CNetworkPlayerManager::m_localPlayerId = pPlayerHandshake->yourid;

    CNetworkVehicleManager::ResetIdAllocator();
    CNetworkPedManager::ResetIdAllocator();
    CNetworkObjectManager::ResetIdAllocator();

    CNetwork::m_bAuthenticated = true;
    CPropertySync::Reset();  // [private build]

    // [private build] my clothes as they are now (loaded from the save): otherwise the others see a
    // default CJ until I change clothes again
    if (CPlayerPed* me = FindPlayerPed(0))
    {
        if (me->m_pPlayerData && me->m_pPlayerData->m_pPedClothesDesc)
        {
            Packets::Players::RebuildPlayer rebuildPacket{};
            rebuildPacket.clothesDesc = *me->m_pPlayerData->m_pPedClothesDesc;
            GetPacketFactory().Send(rebuildPacket);
        }
    }
    CPatch::RevertTemporaryPatches();
    logger::info("Authenticated, playerid %s", to_string(pPlayerHandshake->yourid).c_str());
}

PACKET_HANDLER(ePacketType::RTT_BROADCAST, Packets::System::RTTBroadcast* pRTTBroadcast)
{
    for (int nRTTIndex = 0; nRTTIndex < pRTTBroadcast->playerCount; nRTTIndex++)
    {
        Packets::System::RTTBroadcast::SPlayerRTT& playerRTT = pRTTBroadcast->rtts[nRTTIndex];
        if (playerRTT.playerid == CNetworkPlayerManager::m_localPlayerId)
        {
            continue;
        }

        CNetworkPlayer* pNetworkPlayer = CNetworkPlayerManager::GetPlayer(playerRTT.playerid);
        if (pNetworkPlayer != nullptr)
        {
            pNetworkPlayer->m_nRTT = playerRTT.rtt;
        }
    }
}

PACKET_HANDLER(ePacketType::PLAYER_ASSIGN_HOST, Packets::System::PlayerAssignHost* pPlayerAssignHost)
{
    if (pPlayerAssignHost->playerid == CNetworkPlayerManager::m_localPlayerId)
    {
        CLocalPlayer::m_bIsHost = true;

        CPatch::RevertTemporaryPatchesForHost();  // TODO is this needed?

        CNetworkPedManager::AssignHost();
        CWeatherSync::SyncCurrentState();
        CTagSync::SyncCurrentState();
        CRadarSync::SendAllTraces();
        COnscreenTimerSync::ClearNetworkEntries();
        COnscreenTimerSync::SendState();
        CChat::AddMessage("{c7a6ff}[Player] {d6dce8}You are the host now");
        return;
    }

    for (auto pNetworkPlayer : CNetworkPlayerManager::m_pPlayers)
    {
        if (pNetworkPlayer->m_playerId == pPlayerAssignHost->playerid)
        {
            CChat::AddMessage("{c7a6ff}[Player] {d6dce8}" + pNetworkPlayer->GetName() + " is the host now");
            pNetworkPlayer->m_bIsHost = true;
        }
        else
        {
            pNetworkPlayer->m_bIsHost = false;
        }
    }

    CLocalPlayer::m_bIsHost = false;
}

PACKET_HANDLER(ePacketType::PLAYER_CHAT_MESSAGE, Packets::System::ChatMessage* pChatMessage)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(pChatMessage->senderPlayerid);

    if (player)
    {
        CChat::AddPlayerChatMessage(player->GetName().c_str(), pChatMessage->message);
    }
}

// [private build] cheats, see CCheatSync
PACKET_HANDLER(ePacketType::CHEAT_ACTIVATED, Packets::Cheats::CheatActivated* pCheat)
{
    CCheatSync::OnCheatActivated(pCheat);
}

PACKET_HANDLER(ePacketType::CHEAT_VOTE, Packets::Cheats::CheatVote* pVote)
{
    CCheatSync::OnVote(pVote);
}

PACKET_HANDLER(ePacketType::CHEAT_STATE, Packets::Cheats::CheatState* pState)
{
    CCheatSync::OnState(pState);
}

// [private build] teleport requests (Tab menu), both sides must agree
PACKET_HANDLER(ePacketType::TELEPORT_REQUEST, Packets::Teleport::TeleportRequest* pRequest)
{
    CTeleportRequest::OnPacket(pRequest);
}

// [private build] the host teleported us somewhere (ADMIN_BRING)
PACKET_HANDLER(ePacketType::ADMIN_ACTION, Packets::Admin::AdminAction* pAction)
{
    if (pAction->action != Packets::Admin::ADMIN_BRING)
        return;

    CNetworkPlayer* host = CNetworkPlayerManager::GetPlayer(pAction->senderPlayerid);
    CUtil::TeleportLocalPlayer(pAction->pos, pAction->areaCode);
    CChat::AddMessage("{c7a6ff}[Player] {d6dce8}%s te trajo a su posicion", host ? host->GetName().c_str() : "El host");
}

// [private build]
PACKET_HANDLER(ePacketType::PLAYER_SKIN, Packets::Skin::PlayerSkin* pSkin)
{
    CSkin::OnPacket(pSkin);
}

PACKET_HANDLER(ePacketType::PROPERTY_BOUGHT, Packets::Skin::PropertyBought* pProp)
{
    CPropertySync::OnPacket(pProp);
}

PACKET_HANDLER(ePacketType::PLAYER_HOLD, Packets::Skin::PlayerHold* pHold)
{
    CHoldSync::OnPacket(pHold);
}

PACKET_HANDLER(ePacketType::PLAYER_POWER, Packets::Skin::PlayerPower* pPower)
{
    if (pPower->power == 1)
        CEmoteWheel::OnPacket(pPower);
    else if (pPower->power == 3)
        CFlight::OnPacket(pPower);
    else if (pPower->power == 2)
    {
        if (CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(pPower->senderPlayerid))
            CHoldSync::SetGravityHolding(player->m_pPed, pPower->phase != 0);
    }
    else
        CKamehameha::OnPacket(pPower);
}
