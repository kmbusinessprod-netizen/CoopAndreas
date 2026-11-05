#include "stdafx.h"
#include "CSkin.h"

uint16_t CSkin::ms_nLocalSkin = 0;

// names of the 300 ped models (peds.ide order), story characters by their name
static const char* s_pedNames[300] = {
    "CJ", "The Truth", "Maccer", "Andre", "Big Bear (flaco)", "Big Bear",
    "Emmet", "male01", "Conserje", "bfori", "bfost", "vbfycrp",
    "bfyri", "bfyst", "bmori", "bmost", "bmyap", "bmybu",
    "bmybe", "bmydj", "bmyri", "bmycr", "bmyst", "wmybmx",
    "wbdyg1", "wbdyg2", "wmybp", "wmycon", "bmydrug", "wmydrug",
    "hmydrug", "dwfolc", "dwmolc1", "dwmolc2", "dwmylc1", "hmogar",
    "wmygol1", "wmygol2", "hfori", "hfost", "hfyri", "hfyst",
    "Jethro", "hmori", "hmost", "hmybe", "hmyri", "hmycr",
    "hmyst", "omokung", "wmymech", "bmymoun", "wmymoun", "ofori",
    "ofost", "ofyri", "ofyst", "omori", "omost", "omyri",
    "omyst", "wmyplt", "wmopj", "bfypro", "hfypro", "Kendl",
    "bmypol1", "bmypol2", "wmoprea", "sbfyst", "wmosci", "wmysgrd",
    "swmyhp1", "swmyhp2", "", "swfopro", "wfystew", "swmotr1",
    "wmotr1", "bmotr1", "vbmybox", "vwmybox", "vhmyelv", "vbmyelv",
    "vimyelv", "vwfypro", "", "vwfyst1", "wfori", "wfost",
    "wfyjg", "wfyri", "wfyro", "wfyst", "wmori", "wmost",
    "wmyjg", "wmylg", "wmyri", "wmyro", "wmycr", "wmyst",
    "ballas1", "ballas2", "ballas3", "fam1", "fam2", "fam3",
    "lsv1", "lsv2", "lsv3", "maffa", "maffb", "mafboss",
    "vla1", "vla2", "vla3", "triada", "triadb", "",
    "triboss", "dnb1", "dnb2", "dnb3", "vmaff1", "vmaff2",
    "vmaff3", "vmaff4", "dnmylc", "dnfolc1", "dnfolc2", "dnfylc",
    "dnmolc1", "dnmolc2", "sbmotr2", "swmotr2", "sbmytr3", "swmotr3",
    "wfybe", "bfybe", "hfybe", "sofybu", "sbmyst", "sbmycr",
    "bmycg", "wfycrk", "hmycm", "wmybu", "bfybu", "",
    "wfybu", "dwfylc1", "wfypro", "wmyconb", "wmybe", "wmypizz",
    "bmobar", "cwfyhb", "cwmofr", "cwmohb1", "cwmohb2", "cwmyfr",
    "cwmyhb1", "bmyboun", "wmyboun", "wmomib", "bmymib", "wmybell",
    "bmochil", "sofyri", "somyst", "vwmybjd", "vwfycrp", "sfr1",
    "sfr2", "sfr3", "bmybar", "wmybar", "wfysex", "wmyammo",
    "bmytatt", "vwmycr", "vbmocd", "vbmycr", "vhmycr", "sbmyri",
    "somyri", "somybu", "swmyst", "wmyva", "copgrl3", "gungrl3",
    "mecgrl3", "nurgrl3", "crogrl3", "gangrl3", "cwfofr", "cwfohb",
    "cwfyfr1", "cwfyfr2", "cwmyhb2", "dwfylc2", "dwmylc2", "omykara",
    "wmykara", "wfyburg", "vwmycd", "vhfypro", "", "omonood",
    "omoboat", "wfyclot", "vwmotr1", "vwmotr2", "vwfywai", "sbfori",
    "swfyri", "wmyclot", "sbfost", "sbfyri", "sbmocd", "sbmori",
    "sbmost", "shmycr", "sofori", "sofost", "sofyst", "somobu",
    "somori", "somost", "swmotr5", "swfori", "swfost", "swfyst",
    "swmocd", "swmori", "swmost", "shfypro", "sbfypro", "swmotr4",
    "swmyri", "smyst", "smyst2", "sfypro", "vbfyst2", "vbfypro",
    "vhfyst3", "bikera", "bikerb", "bmypimp", "swmycr", "wfylg",
    "wmyva2", "bmosec", "bikdrug", "wmych", "sbfystr", "swfystr",
    "heck1", "heck2", "bmycon", "wmycd1", "bmocd", "vwfywa2",
    "wmoice", "Tenpenny", "Pulaski", "Hernandez", "Dwayne", "Big Smoke",
    "Sweet", "Ryder", "Forelli", "Mediador", "Paramedico LS", "Paramedico LV",
    "Paramedico SF", "Bombero LS", "Bombero LV", "Bombero SF", "Policia LS", "Policia SF",
    "Policia LV", "Sheriff", "Policia moto", "SWAT", "FBI", "Ejercito",
    "Sheriff desierto", "", "Rosenberg", "Paul", "Cesar", "OG Loc",
    "Wu Zi Mu", "Toreno", "Jizzy", "Madd Dogg", "Catalina", "Claude"
};


bool CSkin::IsValidPedModel(int modelId)
{
    if (modelId == 0)
        return true;
    if (modelId < 1 || modelId >= 300 || s_pedNames[modelId][0] == 0)
        return false;
    CBaseModelInfo* info = CModelInfo::ms_modelInfoPtrs[modelId];
    return info != nullptr && info->GetModelType() == MODEL_INFO_PED;
}


const char* CSkin::ModelName(int modelId)
{
    if (modelId < 0 || modelId >= 300 || s_pedNames[modelId][0] == 0)
        return "?";
    return s_pedNames[modelId];
}

void CSkin::ApplyToPed(CPlayerPed* ped, uint16_t modelId)
{
    if (ped == nullptr || !IsValidPedModel(modelId))
        return;

    if (modelId != 0)
    {
        CStreaming::RequestModel(modelId, GAME_REQUIRED);
        CStreaming::LoadAllRequestedModels(false);
        if (!CStreaming::ms_aInfoForModel[modelId].IsLoaded())
            return;
    }

    if (ped->m_nModelIndex != modelId)
    {
        ped->DeleteRwObject();
        ped->SetModelIndex(modelId);
    }
    if (modelId == 0)
        CClothes::RebuildPlayer(ped, ped != FindPlayerPed(0));
}

void CSkin::SetLocal(uint16_t modelId)
{
    ms_nLocalSkin = modelId;
    ApplyToPed(FindPlayerPed(0), modelId);
    SendLocal();
}

void CSkin::SendLocal()
{
    if (!CNetwork::m_bAuthenticated)
        return;
    Packets::Skin::PlayerSkin packet{};
    packet.modelId = ms_nLocalSkin;
    GetPacketFactory().Send(packet);
}

void CSkin::OnPacket(Packets::Skin::PlayerSkin* packet)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    if (player == nullptr)
        return;
    player->m_nSkin = packet->modelId;
    ApplyToPed(player->m_pPed, packet->modelId);
}
