#include "stdafx.h"
#include "CPropertySync.h"
#include "CPickups.h"

// script globals (see sdk/Sanny Builder 4/.../CustomVariables.ini), index = property 0..31
static constexpr int GV_ALREADY_BOUGHT = 728;   // $already_bought_house[32]
static constexpr int GV_PROPERTY_X = 1524;      // $propertyX[32]
static constexpr int GV_PROPERTY_Y = 1556;      // $propertyY[32]
static constexpr int GV_PICKUP = 1685;          // $save_housepickup[32]

// what the "buy property" mission does for each property besides blips/stats (BUYPRO1.txt)
static const char* s_garages[CPropertySync::COUNT][2] = {
    {nullptr, nullptr}, {nullptr, nullptr}, {"CN2GAR2", "DHANGAR"}, {"BEACSV", nullptr}, {"VESVGRG", nullptr},
    {"CN2GAR1", nullptr}, {"BLOB69", nullptr}, {"BLOB7", nullptr}, {"BURBDOO", nullptr}, {"BLOB6", nullptr},
    {"CARLAS1", nullptr}, {"SAV1SFE", nullptr}, {"CESAFE1", nullptr}, {"SAV1SFW", nullptr}, {"SVGSFS1", nullptr},
    {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr},
    {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr},
    {"BURBDO2", nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr},
    {nullptr, nullptr}, {nullptr, nullptr},
};

uint8_t CPropertySync::ms_known[CPropertySync::COUNT] = {};

static int& Global(int index)
{
    return *(int*)(CTheScripts::ScriptSpace + index * 4);
}
static float& GlobalF(int index)
{
    return *(float*)(CTheScripts::ScriptSpace + index * 4);
}

void CPropertySync::Reset()
{
    for (int i = 0; i < COUNT; i++)
        ms_known[i] = (uint8_t)(Global(GV_ALREADY_BOUGHT + i) != 0);
}

void CPropertySync::Process()
{
    if (!CNetwork::m_bAuthenticated || !CTheScripts::ScriptSpace)
        return;

    static unsigned int last = 0;
    if (GetTickCount() - last < 500)
        return;
    last = GetTickCount();

    for (int i = 0; i < COUNT; i++)
    {
        bool bought = Global(GV_ALREADY_BOUGHT + i) != 0;
        if (bought && !ms_known[i])
        {
            ms_known[i] = 1;
            Packets::Skin::PropertyBought packet{};
            packet.index = (uint8_t)i;
            GetPacketFactory().Send(packet);
        }
    }
}

void CPropertySync::MirrorPurchase(int index)
{
    if (index < 0 || index >= COUNT || !CTheScripts::ScriptSpace)
        return;
    if (Global(GV_ALREADY_BOUGHT + index) != 0)
        return;

    ms_known[index] = 1;

    int pickup = Global(GV_PICKUP + index);
    float x = GlobalF(GV_PROPERTY_X + index);
    float y = GlobalF(GV_PROPERTY_Y + index);

    if (CLocalPlayer::m_bIsHost)
    {
        // the host runs the real purchase flow (BUY_PRO sees the pickup as collected and launches the
        // "buy property" mission): blips, entrances and garages then reach everybody through the usual sync
        if (pickup != 0)
        {
            CPickups::AddToCollectedPickupsArray(pickup);
            Command<Commands::REMOVE_PICKUP>(pickup);
        }
        return;
    }

    // a client only mirrors the local state
    Global(GV_ALREADY_BOUGHT + index) = 1;
    if (pickup != 0)
        Command<Commands::REMOVE_PICKUP>(pickup);
    Command<Commands::SET_CLOSEST_ENTRY_EXIT_FLAG>(x, y, 10.0f, 16384 /*ENABLED*/, true);
    for (const char* garage : s_garages[index])
        if (garage)
            Command<Commands::ACTIVATE_GARAGE>(garage);
    if (index == 1)
        Command<Commands::SWITCH_ENTRY_EXIT>("RCPLAY", true);
}

void CPropertySync::OnPacket(Packets::Skin::PropertyBought* packet)
{
    if (Global(GV_ALREADY_BOUGHT + packet->index) != 0)
        return;

    std::string name = "?";
    if (CNetworkPlayer* p = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid.value))
        name = p->GetName();
    else if (!packet->senderPlayerid.value.valid())
        name.clear();

    if (!name.empty())
        CChat::AddMessage("{a0f0a0}[propiedad] {d6dce8}%s compro una propiedad, ahora es de todos", name.c_str());

    MirrorPurchase(packet->index);
}
