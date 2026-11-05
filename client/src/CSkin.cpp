#include "stdafx.h"
#include "CSkin.h"
#include "CTrace.h"
#include <map>

// Skin ids (what travels over the network):
//   0          CJ with his clothes
//   1..299     a normal ped model (only ids that really have a model in gta3.img)
//   1000 + i   story character s_special[i]: those have NO model id of their own (Cesar, Sweet...). The
//              game loads them by name into a "special character" slot (models 290..299), exactly what
//              CLEO skin selectors do with 023C + 09C7. Requesting e.g. model 292 directly froze the game.
uint16_t CSkin::ms_nLocalSkin = 0;

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

struct SpecialChar
{
    const char* file;
    const char* name;
};
static const SpecialChar s_special[] = {
    {"sweet", "Sweet"}, {"ryder", "Ryder"}, {"smoke", "Big Smoke"}, {"cesar", "Cesar"}, {"kendl", "Kendl"},
    {"ogloc", "OG Loc"}, {"wuzimu", "Wu Zi Mu"}, {"torino", "Toreno"}, {"maddogg", "Madd Dogg"},
    {"jizzy", "Jizzy"}, {"cat", "Catalina"}, {"tenpen", "Tenpenny"}, {"pulaski", "Pulaski"},
    {"hern", "Hernandez"}, {"rose", "Rosenberg"}, {"paul", "Paul"}, {"maccer", "Maccer"}, {"truth", "The Truth"},
    {"claude", "Claude"}, {"zero", "Zero"}, {"jethro", "Jethro"}, {"dwayne", "Dwayne"}, {"emmet", "Emmet"},
    {"andre", "Andre"}, {"bbthin", "Big Bear (flaco)"}, {"bb", "Big Bear"}, {"forelli", "Forelli"},
    {"janitor", "Conserje"}, {"sindaco", "Mediador"}, {"suzie", "Suzie"}, {"wmosci", "Woozie (cientifico)"},
};
static constexpr int SPECIAL_BASE = 1000;
static constexpr int SPECIAL_COUNT = sizeof(s_special) / sizeof(s_special[0]);

static bool IsSpecial(int skin)
{
    return skin >= SPECIAL_BASE && skin < SPECIAL_BASE + SPECIAL_COUNT;
}

static bool SpecialExists(int i)
{
    return CStreaming::ms_pExtraObjectsDir && CStreaming::ms_pExtraObjectsDir->FindItem(s_special[i].file) != nullptr;
}

int CSkin::SpecialCount()
{
    return SPECIAL_COUNT;
}

uint16_t CSkin::SpecialSkin(int i)
{
    return (uint16_t)(SPECIAL_BASE + i);
}

bool CSkin::IsValidPedModel(int skin)
{
    if (skin == 0)
        return true;
    if (IsSpecial(skin))
        return SpecialExists(skin - SPECIAL_BASE);
    if (skin < 1 || skin >= 290 || s_pedNames[skin][0] == 0)
        return false;
    CBaseModelInfo* info = CModelInfo::ms_modelInfoPtrs[skin];
    return info != nullptr && info->GetModelType() == MODEL_INFO_PED && CStreaming::ms_aInfoForModel[skin].m_nCdSize != 0;
}

const char* CSkin::ModelName(int skin)
{
    if (IsSpecial(skin))
        return s_special[skin - SPECIAL_BASE].name;
    if (skin < 0 || skin >= 300 || s_pedNames[skin][0] == 0)
        return "?";
    return s_pedNames[skin];
}

// special slots: each ped owns two and alternates, so a story character can replace another one
// (loading into the slot the ped is wearing right now does nothing). Me: 10/9, the others: 8/7, 6/5.
struct SlotPair
{
    int a, b;
    bool useA;
};
static std::map<CPlayerPed*, SlotPair> s_slots;
static int NextSlotFor(CPlayerPed* ped)
{
    auto it = s_slots.find(ped);
    if (it == s_slots.end())
    {
        SlotPair pair{10, 9, true};
        if (ped != FindPlayerPed(0))
        {
            static int nextRemote = 0;
            pair = (nextRemote++ % 2 == 0) ? SlotPair{8, 7, true} : SlotPair{6, 5, true};
        }
        it = s_slots.emplace(ped, pair).first;
    }
    int slot = it->second.useA ? it->second.a : it->second.b;
    it->second.useA = !it->second.useA;
    return slot;
}

struct PendingSkin
{
    CPlayerPed* ped;
    int model;  // real model id to wait for
    unsigned int since;
};
static std::vector<PendingSkin> s_pending;

static void ApplyNow(CPlayerPed* ped, int model)
{
    CTrace::Mark("skin: apply model %d to %p local=%d curModel=%d", model, ped, ped == FindPlayerPed(0), ped->m_nModelIndex);
    if (ped->m_nModelIndex != model)
    {
        if (ped == FindPlayerPed(0))
        {
            Command<Commands::SET_PLAYER_MODEL>(0, model);  // 09C7, like the CLEO skin selectors
        }
        else
        {
            // 09C7 only works on player 0: same steps by hand for a remote ped
            using DeleteRwObject_t = void(__thiscall*)(CEntity*);
            using SetModelIndex_t = void(__thiscall*)(CEntity*, unsigned int);
            void** vtable = *(void***)ped;
            int savedAnimGroup = *(int*)((uintptr_t)ped + 0x4D4);
            ((DeleteRwObject_t)vtable[8])(ped);
            ped->m_nModelIndex = 0xFFFF;
            ((SetModelIndex_t)vtable[5])(ped, model);
            *(int*)((uintptr_t)ped + 0x4D4) = savedAnimGroup;
        }
    }
    if (model == 0)
        CClothes::RebuildPlayer(ped, ped != FindPlayerPed(0));
    CTrace::Mark("skin: done, model now %d", ped->m_nModelIndex);
}

void CSkin::ApplyToPed(CPlayerPed* ped, uint16_t skin)
{
    if (ped == nullptr || !IsValidPedModel(skin))
        return;

    for (auto it = s_pending.begin(); it != s_pending.end();)
        it = (it->ped == ped) ? s_pending.erase(it) : it + 1;

    int model = skin;
    if (IsSpecial(skin))
    {
        int slot = NextSlotFor(ped);
        model = 289 + slot;
        // the slot may still hold this character from before
        Command<Commands::LOAD_SPECIAL_CHARACTER>(slot, s_special[skin - SPECIAL_BASE].file);
        CTrace::Mark("skin: special '%s' requested in slot %d (model %d)", s_special[skin - SPECIAL_BASE].file, slot, model);
    }
    else if (model != 0 && CStreaming::ms_aInfoForModel[model].m_nLoadState != LOADSTATE_LOADED)
    {
        CStreaming::RequestModel(model, GAME_REQUIRED);
    }

    if (model == 0 || (!IsSpecial(skin) && CStreaming::ms_aInfoForModel[model].m_nLoadState == LOADSTATE_LOADED))
    {
        ApplyNow(ped, model);
        return;
    }
    s_pending.push_back({ped, model, GetTickCount()});
}

void CSkin::Process()
{
    Process2();
    for (auto it = s_pending.begin(); it != s_pending.end();)
    {
        bool alive = CPoolGuard::IsPedAlive(it->ped);
        bool loaded = alive && CStreaming::ms_aInfoForModel[it->model].m_nLoadState == LOADSTATE_LOADED;
        if (loaded)
            ApplyNow(it->ped, it->model);
        else if (alive && GetTickCount() - it->since < 8000)
        {
            ++it;
            continue;
        }
        else if (alive && it->ped == FindPlayerPed(0))
            CChat::AddMessage("{f07878}[skin] {d6dce8}no se pudo cargar la skin");
        it = s_pending.erase(it);
    }
}

void CSkin::SetLocal(uint16_t modelId)
{
    CPlayerPed* me = FindPlayerPed(0);
    if (me && me->m_nPedFlags.bInVehicle)  // (m_pVehicle keeps the last car after getting out)
    {
        CChat::AddMessage("{f07878}[skin] {d6dce8}bajate del vehiculo para cambiar la skin");
        return;
    }
    ms_nLocalSkin = modelId;
    CTrace::Arm("skin change requested");
    CTrace::Mark("skin: SetLocal %d (%s)", modelId, ModelName(modelId));
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

static int s_restoreLocal = -1;  // skin the server remembered for me, applied once I am on foot

void CSkin::Process2()
{
    if (s_restoreLocal < 0)
        return;
    CPlayerPed* me = FindPlayerPed(0);
    if (me == nullptr || me->m_nPedFlags.bInVehicle || !CNetwork::m_bAuthenticated)
        return;
    ms_nLocalSkin = (uint16_t)s_restoreLocal;
    s_restoreLocal = -1;
    ApplyToPed(me, ms_nLocalSkin);
    CChat::AddMessage("{a0f0a0}[skin] {d6dce8}skin recuperada: %s", ModelName(ms_nLocalSkin));
}

void CSkin::OnPacket(Packets::Skin::PlayerSkin* packet)
{
    if (packet->senderPlayerid.value == CNetworkPlayerManager::m_localPlayerId)
    {
        s_restoreLocal = packet->modelId;
        return;
    }
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    if (player == nullptr)
        return;
    player->m_nSkin = packet->modelId;
    ApplyToPed(player->m_pPed, packet->modelId);
}
