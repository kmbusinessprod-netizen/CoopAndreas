#include "stdafx.h"
#include "CCheatSync.h"

// index = position in CCheat::m_aCheatFunctions (eCheats order of the 1.0 US exe)
static const char* s_cheatNames[92] = {
    /*0*/ "armas 1", "armas 2", "armas 3", "vida, chaleco y $250.000", "nivel de busqueda +2",
    /*5*/ "sin policia", "clima soleado", "clima muy soleado", "clima nublado", "lluvia",
    /*10*/ "niebla", "reloj rapido", "juego rapido", "camara lenta", "peatones con palos de golf",
    /*15*/ "recompensa por tu cabeza", "todos armados", "Rhino", "Bloodring Banger", "Rancher",
    /*20*/ "Hotring A", "Hotring B", "Romero", "Stretch", "Trashmaster",
    /*25*/ "Caddy", "explotar todos los autos", "autos invisibles", "manejo perfecto", "suicidio",
    /*30*/ "semaforos en verde", "conductores agresivos", "trafico rosa", "trafico negro", "autos sobre el agua",
    /*35*/ "botes que vuelan", "gordo", "musculoso", "flaco", "Elvis en todos lados",
    /*40*/ "peatones con lanzacohetes", "fiesta en la playa", "pandilleros en todos lados", "pandillas controlan las calles", "tema ninja",
    /*45*/ "iman de chicas", "trafico barato", "trafico rapido", "autos que vuelan", "super salto en bici",
    /*50*/ "Hydra", "Vortex", "autos explotan al chocar", "nitro en todos los autos", "autos flotan al chocar",
    /*55*/ "siempre medianoche", "reloj detenido, cielo naranja", "tormenta", "tormenta de arena", "(sin uso)",
    /*60*/ "mega salto", "vida infinita", "oxigeno infinito", "paracaidas", "jetpack",
    /*65*/ "nunca buscado", "6 estrellas", "mega punetazo", "nunca tiene hambre", "disturbios",
    /*70*/ "tema circo", "modo adrenalina", "municion infinita", "apuntar al conducir", "menos trafico",
    /*75*/ "trafico de campo", "reclutar a cualquiera", "reclutar con lanzacohetes", "caos con cohetes", "respeto maximo",
    /*80*/ "sex appeal maximo", "resistencia maxima", "nivel hitman en armas", "manejo maximo", "Hunter",
    /*85*/ "Quad", "camion cisterna", "Dozer", "avion de acrobacias", "Monster",
    /*90*/ "prostitutas te pagan", "nitro en los taxis",
};

// cheats whose effect belongs to the world and not to one player
static const bool s_worldCheat[92] = {
    /*0*/ 0, 0, 0, 0, 1, /*5*/ 1, 1, 1, 1, 1, /*10*/ 1, 1, 1, 1, 1, /*15*/ 1, 1, 0, 0, 0,
    /*20*/ 0, 0, 0, 0, 0, /*25*/ 0, 1, 1, 1, 0, /*30*/ 1, 1, 1, 1, 1, /*35*/ 1, 0, 0, 0, 1,
    /*40*/ 1, 1, 1, 1, 1, /*45*/ 1, 1, 1, 1, 1, /*50*/ 0, 0, 1, 1, 1, /*55*/ 1, 1, 1, 1, 0,
    /*60*/ 0, 0, 0, 0, 0, /*65*/ 1, 1, 0, 0, 1, /*70*/ 1, 0, 0, 0, 1, /*75*/ 1, 0, 0, 1, 0,
    /*80*/ 0, 0, 0, 0, 0, /*85*/ 0, 0, 0, 0, 0, /*90*/ 0, 1,
};

bool CCheatSync::IsWorldCheat(int i)
{
    return i >= 0 && i < 92 && s_worldCheat[i];
}

const char* CCheatSync::GetCheatName(int i)
{
    return (i >= 0 && i < 92) ? s_cheatNames[i] : "?";
}

void CCheatSync::OnLocalCheat(int cheatIndex)
{
    if (!CNetwork::m_bAuthenticated || cheatIndex < 0 || cheatIndex >= 92)
        return;

    Packets::Cheats::CheatActivated packet{};
    packet.cheatIndex = cheatIndex;
    // the dispatcher runs right after us: function cheats "activate", the others toggle
    packet.active = CCheat::m_aCheatFunctions[cheatIndex] != nullptr ? true : !CCheat::m_aCheatsActive[cheatIndex];
    GetPacketFactory().Send(packet);
}

void CCheatSync::OnRemoteCheat(const char* playerName, int cheatIndex, bool active)
{
    if (cheatIndex < 0 || cheatIndex >= 92)
        return;

    char name[Config::MAX_NICKNAME_LENGTH + 1]{};
    strncpy_s(name, playerName, Config::MAX_NICKNAME_LENGTH);
    for (char* c = name; *c; c++)
        if (*c == '~')
            *c = ' ';

    char text[160];
    sprintf_s(text, "~y~%s~s~ activo un truco: %s", name, GetCheatName(cheatIndex));
    CHud::SetHelpMessage(text, true, false, false);
    CChat::AddMessage("{f0d060}[truco] {d6dce8}%s activo: %s", name, GetCheatName(cheatIndex));

    if (!IsWorldCheat(cheatIndex))
        return;

    if (auto func = CCheat::m_aCheatFunctions[cheatIndex])
        func();
    else
        CCheat::m_aCheatsActive[cheatIndex] = active;
}

// 0x43856A: `mov eax, [0x8A5B58 + esi*4]` (7 bytes) inside CCheat::AddToCheatString, right
// before the matched cheat function is called; esi = cheat index
static __declspec(naked) void CheatDispatch_Hook()
{
    __asm
    {
        pushad
        pushfd
        push esi
        call CCheatSync::OnLocalCheat
        add esp, 4
        popfd
        popad
        mov eax, dword ptr [esi * 4 + 0x8A5B58]
        ret
    }
}

void CCheatSync::Install()
{
    patch::RedirectCall(0x43856A, (void*)CheatDispatch_Hook);
    patch::Nop(0x43856F, 2);
}
