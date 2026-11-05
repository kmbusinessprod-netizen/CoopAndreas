#include "stdafx.h"
#include "CUnlockAll.h"
#include "CEntryExitManager.h"

bool CUnlockAll::ms_bEnabled = true;
bool CUnlockAll::ms_bApplied = false;
unsigned int CUnlockAll::ms_nLastCheck = 0;

// entrances the story switches on one by one (shops, gyms, bars, Madd Dogg's crib, the casinos' offices...)
static const char* s_enex[] = {"ABATOIR", "AMMUN1", "AMMUN2", "AMMUN3", "AMMUN4", "AMMUN5", "BAR1", "BAR2", "BARBER2",
    "BARBER3", "BARBERS", "BURHOUS", "CARLS", "CLOTHGP", "CSCHP", "CSDESGN", "CSEXL", "CSSPRT", "DAMIN", "DAMOUT",
    "DESHOUS", "DINER1", "DINER2", "FDBURG", "FDCHICK", "FDPIZA", "FDREST1", "GANG", "GENOTB", "GYM1", "GYM2", "GYM3",
    "JUMP1", "JUMP2", "LACRAK", "LACS1", "LAHS1A", "MADDOGS", "MAFCAS", "MAFCAS2", "MDDOGS", "MOTEL1", "PAPER",
    "PDOMES", "PDOMES2", "RCPLAY", "REST2", "TATTOO", "TSDINER", "UFOBAR"};

// roads the intro switches off until the cities open (INTRO.txt)
static const float s_roads[][6] = {
    {-100.7515, -927.8298, 18.0, -68.3752, -891.9871, 14.0},
    {-1092.429, 1286.505, 30.0, -1077.038, 1319.495, 35.0},
    {-12.7067, -1522.455, 1.0, 80.8463, -1517.111, 5.0},
    {-16.3392, -1532.882, 0.0394, 69.3401, -1523.771, 5.922},
    {-1689.229, 513.0995, 30.2597, -1679.124, 524.8383, 40.25},
    {-1690.705, 539.6102, 30.3278, -1100.567, 1140.569, 50.735},
    {-1737.333, 455.9431, 30.3573, -1710.363, 500.6261, 40.4891},
    {-1742.906, 500.7302, 30.4679, -1650.312, 551.8201, 40.7455},
    {-1799.541, 379.7155, 16.0, -1780.199, 392.2779, 18.0},
    {-1860.133, 314.7891, 38.0, -1638.563, 557.4354, 40.0},
    {-205.8387, 250.7443, 7.2472, -131.0039, 481.8496, 15.9152},
    {-2670.173, 1203.331, 50.4297, -2662.66, 1237.707, 60.5781},
    {-2695.584, 1237.981, 40.7328, -2664.417, 1454.767, 60.8126},
    {-2696.464, 1239.866, 40.7599, -2665.359, 2190.96, 70.8125},
    {-2740.694, 2233.618, 40.8431, -2720.91, 2338.224, 80.4822},
    {-33.4208, -1341.84, 9.0, 35.3764, -1303.948, 13.0},
    {-41.2393, -1385.87, 8.0, -3.5883, -1368.856, 10.5},
    {-995.0013, -416.2032, 30.4207, -940.5399, -251.8564, 40.6762},
    {1643.536, 227.3723, 27.4457, 1673.062, 295.5788, 30.0815},
    {1673.765, 388.1013, 40.2331, 1815.862, 804.9291, 10.0},
    {1690.819, 376.5103, 28.1103, 1730.223, 445.2955, 30.8414},
    {1705.156, 308.3448, 20.0, 1710.948, 316.4094, 23.5612},
    {210.7811, 939.2068, 10.9062, 249.4799, 959.1111, 30.2141},
    {230.4545, 946.0961, 20.6674, 255.9772, 969.2755, 30.4776},
    {249.4279, 899.7975, 10.5871, 268.6826, 933.5995, 30.3975},
    {254.9982, 837.029, 10.1731, 270.7453, 929.2404, 30.2553},
    {289.2904, 636.3991, 7.8675, 409.4943, 702.3849, 20.0345},
    {300.3153, 718.7909, 7.7846, 316.7906, 781.0926, 14.0795},
    {312.1081, 694.1089, 6.0, 324.0811, 733.0005, 10.0},
    {317.1688, 707.7672, 7.0, 437.5726, 709.0657, 20.5578},
    {324.4526, 804.9198, 9.6186, 332.8747, 814.356, 14.3925},
    {391.1194, 640.015, 7.0, 402.2627, 664.798, 18.5098},
    {609.7595, 327.3437, 15.8783, 429.8884, 616.0168, 20.289},
    {618.7253, -1189.606, 18.0, 623.5441, -1161.981, 22.0},
};

// (21 is FAT and 23 is MUSCLE: never touch those, they change CJ's body)
static const int s_floatStats[] = {STAT_STAMINA, STAT_MAX_HEALTH, STAT_LUNG_CAPACITY, 69, 70, 71, 72, 73, 74, 75, 76, 77,
    78, 79 /*weapon skills*/, STAT_DRIVING_SKILL, STAT_FLYING_SKILL, STAT_BIKE_SKILL, STAT_CYCLING_SKILL};

void CUnlockAll::ApplyNow()
{
    if (!CTheScripts::ScriptSpace || FindPlayerPed(0) == nullptr)
        return;

    // cities
    Command<Commands::SET_INT_STAT>(181, 4);
    Command<Commands::REMOVE_IPL>("BARRIERS1");
    Command<Commands::REMOVE_IPL>("BARRIERS2");
    for (auto& r : s_roads)
        Command<Commands::SWITCH_ROADS_ON>(r[0], r[1], r[2], r[3], r[4], r[5]);
    Command<Commands::SET_MAX_WANTED_LEVEL>(6);

    // entrances
    for (const char* name : s_enex)
        Command<Commands::SWITCH_ENTRY_EXIT>(name, true);

    // body
    for (int stat : s_floatStats)
        Command<Commands::SET_FLOAT_STAT>(stat, 1000.0f);
    // undo what the first build of this feature did (it set FAT to 1000 by mistake)
    if (CStats::GetStatValue(STAT_FAT) >= 999.0f)
    {
        Command<Commands::SET_FLOAT_STAT>(STAT_FAT, 0.0f);
        if (CPlayerPed* ped = FindPlayerPed(0))
            if (ped->m_nModelIndex == 0)
                CClothes::RebuildPlayer(ped, false);
    }
    CWorld::Players[0].m_nMaxHealth = 176;
    CWorld::Players[0].m_nMaxArmour = 100;

    ms_bApplied = true;
    ms_nLastCheck = GetTickCount();
}

void CUnlockAll::Process()
{
    if (!ms_bEnabled || !CNetwork::m_bAuthenticated)
    {
        ms_bApplied = false;
        return;
    }
    if (!ms_bApplied)
    {
        // wait until the game is really running (player exists, scripts loaded)
        if (FindPlayerPed(0) == nullptr || !CTheScripts::ScriptSpace)
            return;
        ApplyNow();
        CChat::AddMessage("{a0f0a0}[coop] {d6dce8}todo desbloqueado: ciudades, tiendas, salud y habilidades al maximo");
        return;
    }
    // the intro script resets the cities stat, keep it
    if (GetTickCount() - ms_nLastCheck > 15000)
    {
        ms_nLastCheck = GetTickCount();
        if (CStats::GetStatValue(181) < 4.0f)
            Command<Commands::SET_INT_STAT>(181, 4);
        for (const char* name : s_enex)
            Command<Commands::SWITCH_ENTRY_EXIT>(name, true);
    }
}
