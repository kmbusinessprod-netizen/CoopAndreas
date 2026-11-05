#pragma once
// [private build] shared knowledge about the 92 San Andreas cheats (index = CCheat::m_aCheatFunctions order)
namespace CheatInfo
{
constexpr int COUNT = 92;

// cheats whose effect belongs to the world and not to one player: applied on every machine,
// and only after everyone agrees
inline bool IsWorld(int i)
{
    static const bool table[COUNT] = {
        /*0*/ 0, 0, 0, 0, 1, /*5*/ 1, 1, 1, 1, 1, /*10*/ 1, 1, 1, 1, 1, /*15*/ 1, 1, 0, 0, 0,
        /*20*/ 0, 0, 0, 0, 0, /*25*/ 0, 1, 1, 1, 0, /*30*/ 1, 1, 1, 1, 1, /*35*/ 1, 0, 0, 0, 1,
        /*40*/ 1, 1, 1, 1, 1, /*45*/ 1, 1, 1, 1, 1, /*50*/ 0, 0, 1, 1, 1, /*55*/ 1, 1, 1, 1, 0,
        /*60*/ 0, 0, 0, 0, 0, /*65*/ 1, 1, 0, 0, 1, /*70*/ 1, 0, 0, 0, 1, /*75*/ 1, 0, 0, 1, 0,
        /*80*/ 0, 0, 0, 0, 0, /*85*/ 0, 0, 0, 0, 0, /*90*/ 0, 1,
    };
    return i >= 0 && i < COUNT && table[i];
}

inline const char* Name(int i)
{
    static const char* names[COUNT] = {
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
    return (i >= 0 && i < COUNT) ? names[i] : "?";
}
}  // namespace CheatInfo
