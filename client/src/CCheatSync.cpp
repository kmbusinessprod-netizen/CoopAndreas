#include "stdafx.h"
#include "CCheatSync.h"
#include "UI/CPrompt.h"

int CCheatSync::ms_nReportPending = -1;
static playerid_t ms_lastProposer{};

static std::string PlayerName(const playerid_t& id)
{
    if (id == CNetworkPlayerManager::m_localPlayerId)
        return CLocalPlayer::m_Name;
    CNetworkPlayer* p = CNetworkPlayerManager::GetPlayer(id);
    std::string name = p ? p->GetName() : std::string("?");
    for (char& c : name)
        if (c == '~')
            c = ' ';
    return name;
}

static void HelpBox(const std::string& text)
{
    CHud::SetHelpMessage(text.c_str(), true, false, false);
}

void CCheatSync::ApplyLocally(int cheatIndex)
{
    if (auto func = CCheat::m_aCheatFunctions[cheatIndex])
        func();
    else
        CCheat::m_aCheatsActive[cheatIndex] = !CCheat::m_aCheatsActive[cheatIndex];
}

void CCheatSync::ReportState()
{
    Packets::Cheats::CheatState state{};
    for (int i = 0; i < CheatInfo::COUNT; i++)
        state.active[i] = CheatInfo::IsWorld(i) && CCheat::m_aCheatsActive[i];
    state.timeScale = (int)(CTimer::ms_fTimeScale * 1000.0f);
    state.forcedWeather = CWeather::ForcedWeatherType;
    GetPacketFactory().Send(state);
}

void CCheatSync::Process()
{
    if (ms_nReportPending >= 0)
    {
        ms_nReportPending = -1;
        if (CNetwork::m_bAuthenticated)
            ReportState();
    }
}

// returns true if the game may apply the cheat right now
bool CCheatSync::OnLocalCheat(int cheatIndex)
{
    if (!CNetwork::m_bAuthenticated || cheatIndex < 0 || cheatIndex >= CheatInfo::COUNT)
        return true;

    if (!IsWorldCheat(cheatIndex))
    {
        // personal: apply, tell the others
        Packets::Cheats::CheatActivated packet{};
        packet.cheatIndex = cheatIndex;
        packet.active = true;
        packet.apply = false;
        GetPacketFactory().Send(packet);
        return true;
    }

    // world: ask everyone first (the server applies it alone when nobody else is connected)
    Packets::Cheats::CheatVote vote{};
    vote.phase = Packets::Cheats::CheatVote::PROPOSE;
    vote.cheatIndex = cheatIndex;
    GetPacketFactory().Send(vote);

    if (!CNetworkPlayerManager::m_pPlayers.empty())
    {
        HelpBox(std::string("Truco ~y~") + GetCheatName(cheatIndex) + "~s~: esperando que los demas acepten...");
        CChat::AddMessage("{f0d060}[truco] {d6dce8}propusiste: %s (los demas deben aceptar con Y)", GetCheatName(cheatIndex));
    }
    return false;
}

void CCheatSync::OnCheatActivated(Packets::Cheats::CheatActivated* packet)
{
    int i = packet->cheatIndex;
    if (i < 0 || i >= CheatInfo::COUNT)
        return;

    std::string name = PlayerName(packet->senderPlayerid.value);
    bool mine = packet->senderPlayerid.value == CNetworkPlayerManager::m_localPlayerId;

    if (!packet->apply)
    {
        HelpBox("~y~" + name + "~s~ activo un truco: " + GetCheatName(i));
        CChat::AddMessage("{f0d060}[truco] {d6dce8}%s activo: %s", name.c_str(), GetCheatName(i));
        return;
    }

    ApplyLocally(i);
    bool nowActive = CCheat::m_aCheatsActive[i];
    const char* func = CCheat::m_aCheatFunctions[i] ? "" : (nowActive ? " (activado)" : " (desactivado)");
    HelpBox("Truco para todos: ~y~" + std::string(GetCheatName(i)) + "~s~" + func + "  (" + name + ")");
    CChat::AddMessage("{f0d060}[truco] {d6dce8}%s para todos: %s%s", name.c_str(), GetCheatName(i), func);

    if (mine)
        ms_nReportPending = i;  // tell the server the resulting world state next frame
}

void CCheatSync::OnVote(Packets::Cheats::CheatVote* packet)
{
    int i = packet->cheatIndex;
    switch (packet->phase)
    {
    case Packets::Cheats::CheatVote::ASK:
    {
        std::string name = PlayerName(packet->senderPlayerid.value);
        ms_lastProposer = packet->senderPlayerid.value;
        CPrompt::Ask(name + " quiere activar para todos: ~y~" + GetCheatName(i) + "~s~",
            [](bool accept)
            {
                Packets::Cheats::CheatVote reply{};
                reply.phase = Packets::Cheats::CheatVote::REPLY;
                reply.accept = accept;
                GetPacketFactory().Send(reply);
            });
        CChat::AddMessage("{f0d060}[truco] {d6dce8}%s propone %s: Y acepta / N rechaza", name.c_str(), GetCheatName(i));
        break;
    }
    case Packets::Cheats::CheatVote::RESULT:
    {
        if (packet->accept)
            break;  // the CheatActivated that follows shows the message
        if (!packet->senderPlayerid.value.valid())
        {
            HelpBox("Ya hay otra votacion de truco en curso");
            break;
        }
        // the server names the rejecter; when nobody answered it names the proposer instead
        bool timeout = packet->senderPlayerid.value == ms_lastProposer ||
                       packet->senderPlayerid.value == CNetworkPlayerManager::m_localPlayerId;
        std::string who = PlayerName(packet->senderPlayerid.value);
        HelpBox("Truco ~y~" + std::string(GetCheatName(i)) + "~s~ rechazado" + (timeout ? " (sin respuesta)" : " por " + who));
        CChat::AddMessage("{f0d060}[truco] {d6dce8}%s rechazado%s", GetCheatName(i), timeout ? " (sin respuesta)" : (" por " + who).c_str());
        break;
    }
    default:
        break;
    }
}

void CCheatSync::OnState(Packets::Cheats::CheatState* packet)
{
    int changed = 0;
    for (int i = 0; i < CheatInfo::COUNT; i++)
    {
        if (!CheatInfo::IsWorld(i))
            continue;
        if (CCheat::m_aCheatsActive[i] != packet->active[i])
        {
            CCheat::m_aCheatsActive[i] = packet->active[i];
            changed++;
        }
    }
    CTimer::ms_fTimeScale = packet->timeScale / 1000.0f;
    if (packet->forcedWeather >= 0)
        CWeather::ForceWeatherNow((short)packet->forcedWeather);
    else
        CWeather::ForcedWeatherType = -1;

    if (changed > 0)
        CChat::AddMessage("{f0d060}[truco] {d6dce8}%d truco(s) de mundo ya activos en esta partida", changed);
}

// 0x43856A: `mov eax, [0x8A5B58 + esi*4]` (7 bytes) inside CCheat::AddToCheatString, right
// before the matched cheat function is called; esi = cheat index. Returning a no-op function
// pointer keeps the game from applying (and from toggling) the cheat.
static void __cdecl NoOpCheat() {}

static void* __cdecl CheatDispatchDecide(int cheatIndex)
{
    if (CCheatSync::OnLocalCheat(cheatIndex))
        return CCheat::m_aCheatFunctions[cheatIndex];  // may be NULL: the game toggles the flag
    return (void*)NoOpCheat;
}

static __declspec(naked) void CheatDispatch_Hook()
{
    __asm
    {
        pushfd
        push ecx
        push edx
        push esi
        call CheatDispatchDecide
        add esp, 4
        pop edx
        pop ecx
        popfd
        ret
    }
}

void CCheatSync::Install()
{
    patch::RedirectCall(0x43856A, (void*)CheatDispatch_Hook);
    patch::Nop(0x43856F, 2);
    Events::gameProcessEvent += Process;
}
