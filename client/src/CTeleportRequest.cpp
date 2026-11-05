#include "stdafx.h"
#include "CTeleportRequest.h"
#include "UI/CPrompt.h"

static std::string NameOf(CNetworkPlayer* p)
{
    std::string name = p ? p->GetName() : std::string("?");
    for (char& c : name)
        if (c == '~')
            c = ' ';
    return name;
}

void CTeleportRequest::TeleportNextTo(CNetworkPlayer* player)
{
    if (player == nullptr || player->m_pPed == nullptr)
    {
        CChat::AddMessage("{c7a6ff}[TP] {f07878}No se encuentra a ese jugador");
        return;
    }
    CVector pos = player->m_pPed->GetPosition();
    pos.x += 1.5f;
    pos.z += 0.5f;
    CUtil::TeleportLocalPlayer(pos, player->m_pPed->m_nAreaCode);
}

void CTeleportRequest::Reply(playerid_t to, uint8_t kind, bool accept)
{
    Packets::Teleport::TeleportRequest packet{};
    packet.targetPlayerid = to;
    packet.kind = kind;
    packet.phase = accept ? Packets::Teleport::TeleportRequest::ACCEPT : Packets::Teleport::TeleportRequest::REJECT;
    GetPacketFactory().Send(packet);
}

void CTeleportRequest::RequestGoTo(CNetworkPlayer* target)
{
    if (target == nullptr)
        return;
    Packets::Teleport::TeleportRequest packet{};
    packet.targetPlayerid = target->m_playerId;
    packet.kind = Packets::Teleport::TeleportRequest::GO_TO_TARGET;
    packet.phase = Packets::Teleport::TeleportRequest::REQUEST;
    GetPacketFactory().Send(packet);
    CChat::AddMessage("{c7a6ff}[TP] {d6dce8}Le pediste a %s teletransportarte hasta el", NameOf(target).c_str());
}

void CTeleportRequest::RequestBring(CNetworkPlayer* target)
{
    if (target == nullptr)
        return;
    Packets::Teleport::TeleportRequest packet{};
    packet.targetPlayerid = target->m_playerId;
    packet.kind = Packets::Teleport::TeleportRequest::BRING_TARGET;
    packet.phase = Packets::Teleport::TeleportRequest::REQUEST;
    GetPacketFactory().Send(packet);
    CChat::AddMessage("{c7a6ff}[TP] {d6dce8}Le pediste a %s que venga a tu posicion", NameOf(target).c_str());
}

void CTeleportRequest::OnPacket(Packets::Teleport::TeleportRequest* packet)
{
    using Packets::Teleport::TeleportRequest;
    playerid_t otherId = packet->senderPlayerid.value;
    CNetworkPlayer* other = CNetworkPlayerManager::GetPlayer(otherId);
    std::string name = NameOf(other);
    uint8_t kind = packet->kind;

    switch (packet->phase)
    {
    case TeleportRequest::REQUEST:
    {
        std::string question = kind == TeleportRequest::GO_TO_TARGET
                                   ? "~y~" + name + "~s~ quiere teletransportarse hasta vos"
                                   : "~y~" + name + "~s~ quiere traerte a su posicion";
        CPrompt::Ask(question,
            [otherId, kind](bool accept)
            {
                CNetworkPlayer* p = CNetworkPlayerManager::GetPlayer(otherId);
                Reply(otherId, kind, accept);
                if (accept && kind == TeleportRequest::BRING_TARGET)
                    TeleportNextTo(p);
                if (!accept)
                    CChat::AddMessage("{c7a6ff}[TP] {d6dce8}Rechazaste el teletransporte de %s", NameOf(p).c_str());
            });
        CChat::AddMessage("{c7a6ff}[TP] {d6dce8}%s: Y acepta / N rechaza", question.c_str());
        break;
    }
    case TeleportRequest::ACCEPT:
        if (kind == TeleportRequest::GO_TO_TARGET)
        {
            CChat::AddMessage("{c7a6ff}[TP] {d6dce8}%s acepto, alla vamos", name.c_str());
            TeleportNextTo(other);
        }
        else
            CChat::AddMessage("{c7a6ff}[TP] {d6dce8}%s acepto, esta viniendo", name.c_str());
        break;
    case TeleportRequest::REJECT:
        CChat::AddMessage("{c7a6ff}[TP] {f07878}%s rechazo el teletransporte", name.c_str());
        CHud::SetHelpMessage((name + " rechazo el teletransporte").c_str(), true, false, false);
        break;
    }
}
