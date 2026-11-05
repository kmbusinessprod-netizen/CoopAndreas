#include "stdafx.h"
#include "CModMenu.h"
#include "CCheatSync.h"
#include "CSkin.h"
#include "CSkinImport.h"
#include "CKamehameha.h"
#include "CGravityGun.h"
#include "CEmoteWheel.h"
#include "CFlight.h"
#include "CUnlockAll.h"
#include "CPrompt.h"
#include <unordered_map>

bool CModMenu::ms_bOpen = false;
std::vector<CModMenu::Page> CModMenu::ms_pages;
unsigned int CModMenu::ms_nLastInput = 0;
std::string CModMenu::ms_status;
unsigned int CModMenu::ms_nStatusUntil = 0;

static const int ROWS = 12;

// our own "was pressed last frame" memory: the game's OldKeyState is not reliable for us because
// other code (and the pause menu) resets it, which made one tap count as many
static std::unordered_map<size_t, bool> s_wasDown;
static bool KeyDown(short CKeyboardState::*key)
{
    return CPad::NewKeyState.*key != 0;
}
static bool KeyJustPressed(short CKeyboardState::*key)
{
    size_t id = (size_t)&(CPad::NewKeyState.*key);
    bool down = KeyDown(key);
    bool was = s_wasDown[id];
    s_wasDown[id] = down;
    return down && !was;
}

void CModMenu::SetStatus(const std::string& text)
{
    ms_status = text;
    ms_nStatusUntil = GetTickCount() + 4000;
}

void CModMenu::Open(const std::string& title, std::vector<Item> items)
{
    Page p;
    p.title = title;
    p.items = std::move(items);
    Push(std::move(p));
}

void CModMenu::Push(Page page)
{
    ms_pages.push_back(std::move(page));
}

void CModMenu::Toggle()
{
    ms_bOpen = !ms_bOpen;
    ms_pages.clear();
    if (ms_bOpen)
    {
        CModMenu m;
        BuildRoot(&m);
    }
}

// ------------------------------------------------------------------------------------------------
// pages

void CModMenu::BuildRoot(CModMenu* m)
{
    m->Open("MENU DEL COOP",
        {
            {"Trucos", nullptr, nullptr, BuildCheats},
            {"Vehiculos", nullptr, nullptr, BuildVehicles},
            {"Skin", [] { return std::string(CSkin::ModelName(CSkin::ms_nLocalSkin)); }, nullptr, BuildSkins},
            {"Jugador", nullptr, nullptr, BuildPlayer},
            {"Armas", nullptr, nullptr, BuildWeapons},
            {"Teletransporte", nullptr, nullptr, BuildTeleport},
            {"Misiones", nullptr, nullptr, BuildMissions},
            {"Guardar partida", nullptr,
                []
                {
                    ms_bOpen = false;
                    ms_pages.clear();
                    Command<Commands::ACTIVATE_SAVE_MENU>();
                }},
        });
}

struct CheatCategory
{
    const char* name;
    std::vector<int> cheats;
};
static const std::vector<CheatCategory>& CheatCategories()
{
    static const std::vector<CheatCategory> list = {
        {"Armas y salud", {0, 1, 2, 3, 61, 62, 72, 67, 63, 64, 29}},
        {"Cuerpo y habilidades", {36, 37, 38, 60, 49, 68, 71, 73, 79, 80, 81, 82, 83, 90}},
        {"Policia y busqueda", {4, 5, 15, 65, 66}},
        {"Clima y hora", {6, 7, 8, 9, 10, 57, 58, 55, 56, 11, 12, 13}},
        {"Trafico y autos", {26, 27, 28, 30, 31, 32, 33, 34, 35, 46, 47, 48, 52, 53, 54, 74, 75, 91}},
        {"Peatones y pandillas", {14, 16, 39, 40, 41, 42, 43, 44, 45, 69, 70, 76, 77, 78}},
    };
    return list;
}

void CModMenu::BuildCheats(CModMenu* m)
{
    std::vector<Item> items;
    for (size_t c = 0; c < CheatCategories().size(); c++)
        items.push_back({CheatCategories()[c].name, nullptr, nullptr, [c](CModMenu* mm) { BuildCheatCategory(mm, (int)c); }});
    m->Open("TRUCOS", std::move(items));
}

void CModMenu::BuildCheatCategory(CModMenu* m, int category)
{
    const CheatCategory& cat = CheatCategories()[category];
    std::vector<Item> items;
    for (int i : cat.cheats)
    {
        bool toggle = CCheat::m_aCheatFunctions[i] == nullptr;
        bool world = CheatInfo::IsWorld(i);
        std::string label = std::string(world ? "~y~[todos]~s~ " : "") + CheatInfo::Name(i);
        items.push_back({label,
            [i, toggle]() -> std::string
            {
                if (!toggle)
                    return "";
                return CCheat::m_aCheatsActive[i] ? "~g~ON" : "~r~OFF";
            },
            [i] { CCheatSync::ActivateFromMenu(i); }, nullptr});
    }
    m->Open(std::string("TRUCOS - ") + cat.name, std::move(items));
}

// vehicle categories
enum
{
    CAT_CARS = 0,
    CAT_BIKES,
    CAT_AIR,
    CAT_BOATS,
    CAT_HEAVY,
    CAT_COUNT
};
static const char* s_categoryNames[CAT_COUNT] = {"Autos", "Motos y bicis", "Aviones y helicopteros", "Barcos", "Pesados y otros"};

static int VehicleCategory(int modelId)
{
    auto* info = (CVehicleModelInfo*)CModelInfo::ms_modelInfoPtrs[modelId];
    if (!info)
        return -1;
    switch (info->m_nVehicleType)
    {
    case VEHICLE_BIKE:
    case VEHICLE_BMX:
    case VEHICLE_QUAD:
        return CAT_BIKES;
    case VEHICLE_HELI:
    case VEHICLE_PLANE:
    case VEHICLE_FHELI:
    case VEHICLE_FPLANE:
        return CAT_AIR;
    case VEHICLE_BOAT:
        return CAT_BOATS;
    case VEHICLE_TRAIN:
    case VEHICLE_TRAILER:
        return -1;  // not spawnable this way
    case VEHICLE_MTRUCK:
        return CAT_HEAVY;
    default:
        break;
    }
    // automobiles: trucks, industrial and service vehicles go to "heavy"
    static const int heavy[] = {403, 406, 407, 408, 414, 416, 423, 427, 428, 431, 432, 433, 437, 443, 444, 455, 456, 459,
        482, 486, 498, 499, 508, 514, 515, 524, 525, 528, 530, 531, 532, 544, 552, 556, 557, 573, 574, 578, 583, 588,
        601, 609, 441, 464, 465, 501, 564, 571, 572, 583, 594};
    for (int h : heavy)
        if (h == modelId)
            return CAT_HEAVY;
    return CAT_CARS;
}

static std::string VehicleDisplayName(int modelId)
{
    auto* info = (CVehicleModelInfo*)CModelInfo::ms_modelInfoPtrs[modelId];
    if (!info)
        return "?";
    char key[9];
    strncpy_s(key, info->m_szGameName, 8);
    key[8] = '\0';
    const char* text = TheText.Get(key);
    if (text && text[0] && strcmp(text, key) != 0)
        return text;
    return key;
}

void CModMenu::BuildVehicles(CModMenu* m)
{
    std::vector<Item> items;
    items.push_back({"~y~Tunear el vehiculo actual", nullptr, nullptr, BuildTuning});
    for (int c = 0; c < CAT_COUNT; c++)
        items.push_back({s_categoryNames[c], nullptr, nullptr, [c](CModMenu* mm) { BuildVehicleCategory(mm, c); }});
    m->Open("VEHICULOS", std::move(items));
}

void CModMenu::BuildVehicleCategory(CModMenu* m, int category)
{
    std::vector<std::pair<std::string, int>> list;
    for (int id = 400; id <= 611; id++)
    {
        if (VehicleCategory(id) != category)
            continue;
        list.emplace_back(VehicleDisplayName(id), id);
    }
    std::sort(list.begin(), list.end());

    std::vector<Item> items;
    for (auto& [name, id] : list)
    {
        items.push_back({name, nullptr,
            [id, name]
            {
                CVehicle* vehicle = CCheat::VehicleCheat(id);
                if (vehicle)
                {
                    vehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = true;
                    SetStatus(name + " listo");
                }
                else
                    SetStatus("no se pudo crear " + name + " aqui");
            },
            nullptr});
    }
    m->Open(std::string("VEHICULOS - ") + s_categoryNames[category], std::move(items));
}

void CModMenu::BuildSkins(CModMenu* m)
{
    std::vector<Item> items;
    auto add = [&](int id)
    {
        items.push_back({std::string(CSkinImport::IsImportedId(id) ? "~p~*~s~ " : "") + CSkin::ModelName(id) + (id < 1000 && id > 0 ? "  ~b~#" + std::to_string(id) : ""),
            [id]() -> std::string { return CSkin::ms_nLocalSkin == id ? "~g~actual" : ""; },
            [id]
            {
                CSkin::SetLocal((uint16_t)id);
                SetStatus(std::string("skin: ") + CSkin::ModelName(id));
            },
            nullptr});
    };
    add(0);
    for (const auto& imported : CSkinImport::GetAll())  // CoopAndreas\skins
        add(imported.id);
    for (int i = 0; i < CSkin::SpecialCount(); i++)  // story characters
        if (CSkin::IsValidPedModel(CSkin::SpecialSkin(i)))
            add(CSkin::SpecialSkin(i));
    for (int id = 1; id < 290; id++)
        if (CSkin::IsValidPedModel(id))
            add(id);
    m->Open("SKIN", std::move(items));
}

void CModMenu::BuildPlayer(CModMenu* m)
{
    m->Open("JUGADOR",
        {
            {"Curar (vida y chaleco al maximo)", nullptr,
                []
                {
                    CPlayerPed* ped = FindPlayerPed(0);
                    if (!ped)
                        return;
                    ped->m_fHealth = (float)CWorld::Players[0].m_nMaxHealth;
                    ped->m_fArmour = (float)CWorld::Players[0].m_nMaxArmour;
                    SetStatus("curado");
                }},
            {"Dinero +$100.000", nullptr,
                []
                {
                    CWorld::Players[0].m_nMoney += 100000;
                    CWorld::Players[0].m_nDisplayMoney += 100000;
                    SetStatus("+$100.000");
                }},
            {"Quitar estrellas", nullptr,
                []
                {
                    if (CPlayerPed* ped = FindPlayerPed(0))
                        ped->SetWantedLevel(0);
                    SetStatus("sin busqueda");
                }},
            {"Armas: juego 1", nullptr, [] { CCheatSync::ActivateFromMenu(0); }},
            {"Armas: juego 2", nullptr, [] { CCheatSync::ActivateFromMenu(1); }},
            {"Armas: juego 3", nullptr, [] { CCheatSync::ActivateFromMenu(2); }},
            {"Jetpack", nullptr, [] { CCheatSync::ActivateFromMenu(64); }},
            {"Paracaidas", nullptr, [] { CCheatSync::ActivateFromMenu(63); }},
            {"Volar (doble salto)", []() -> std::string { return CFlight::ms_bFlyEnabled ? "~g~ON" : "~r~OFF"; },
                []
                {
                    CFlight::ms_bFlyEnabled = !CFlight::ms_bFlyEnabled;
                    SetStatus(CFlight::ms_bFlyEnabled ? "salta y vuelve a saltar en el aire para volar" : "volar desactivado");
                }},
            {"Super velocidad", []() -> std::string { return CFlight::ms_bSpeedEnabled ? "~g~ON" : "~r~OFF"; },
                []
                {
                    CFlight::ms_bSpeedEnabled = !CFlight::ms_bSpeedEnabled;
                    SetStatus(CFlight::ms_bSpeedEnabled ? "Alt + rueda del mouse cambia la velocidad" : "velocidad normal");
                }},
            {"Cuerpo normal (quitar gordo / musculo)", nullptr,
                []
                {
                    Command<Commands::SET_FLOAT_STAT>(21, 0.0f);
                    Command<Commands::SET_FLOAT_STAT>(23, 0.0f);
                    if (CPlayerPed* ped = FindPlayerPed(0))
                        if (ped->m_nModelIndex == 0)
                            CClothes::RebuildPlayer(ped, false);
                    SetStatus("cuerpo normal");
                }},
            {"Volver a desbloquear todo", nullptr,
                []
                {
                    CUnlockAll::ApplyNow();
                    SetStatus("todo desbloqueado");
                }},
        });
}

void CModMenu::BuildTeleport(CModMenu* m)
{
    struct Place
    {
        const char* name;
        float x, y, z;
    };
    static const Place places[] = {
        {"Grove Street (casa de CJ)", 2495.0f, -1687.0f, 13.5f},
        {"Los Santos - centro", 1480.0f, -1740.0f, 13.5f},
        {"Los Santos - aeropuerto", 1680.0f, -2330.0f, 13.5f},
        {"San Fierro - garaje de CJ", -2027.0f, 156.0f, 29.0f},
        {"San Fierro - centro", -1750.0f, 900.0f, 25.0f},
        {"Las Venturas - Strip", 2027.0f, 1008.0f, 10.8f},
        {"Las Venturas - aeropuerto", 1580.0f, 1448.0f, 10.8f},
        {"Monte Chiliad", -2320.0f, -1640.0f, 483.0f},
        {"Area 69", 215.0f, 1860.0f, 17.6f},
        {"Aerodromo abandonado", 410.0f, 2500.0f, 16.5f},
    };
    std::vector<Item> items;
    items.push_back({"~y~Al marcador del mapa", nullptr,
        []
        {
            int handle = FrontEndMenuManager.m_nTargetBlipIndex;
            int index = handle & 0xFFFF;
            if (handle == 0 || index < 0 || index >= 175 || CRadar::ms_RadarTrace[index].m_nCounter != (handle >> 16) ||
                CRadar::ms_RadarTrace[index].m_nBlipDisplay == 0)
            {
                SetStatus("pone un marcador en el mapa primero (menu de pausa > mapa > clic derecho)");
                return;
            }
            CVector pos = CRadar::ms_RadarTrace[index].m_vecPos;
            CVector scenePos(pos.x, pos.y, 20.0f);
            CStreaming::LoadScene(&scenePos);
            float z = CWorld::FindGroundZForCoord(pos.x, pos.y);
            CUtil::TeleportLocalPlayer(CVector(pos.x, pos.y, z + 1.5f), 0);
            SetStatus("teletransportado al marcador");
        },
        nullptr});
    for (auto& p : places)
    {
        items.push_back({p.name, nullptr,
            [p]
            {
                CUtil::TeleportLocalPlayer(CVector(p.x, p.y, p.z), 0);
                SetStatus(std::string("teletransportado a ") + p.name);
            },
            nullptr});
    }
    m->Open("TELETRANSPORTE", std::move(items));
}

// ------------------------------------------------------------------------------------------------
// input

// the player stands still while the menu is open. CGame::Process turns keyboard keys into pad input
// (walk, enter vehicle...) right at its start (0x53BEE6 call CPad::UpdatePads); we clear it there.
static uintptr_t s_UpdatePads_orig = 0;
static void __cdecl CPad__UpdatePads_Hook()
{
    plugin::CallDyn(s_UpdatePads_orig);
    if (CPad* pad = CPad::GetPad(0))
    {
        CKamehameha::ms_bLmb = pad->NewState.ButtonCircle != 0;
        CKamehameha::ms_bRmb = pad->NewState.RightShoulder1 != 0;
        CFlight::ms_bJump = pad->NewState.ButtonSquare != 0;
        CFlight::ms_bCrouch = pad->NewState.ShockButtonL != 0;
        if (CFlight::IsFlying())  // jump = go up, crouch = go down: CJ must not jump/crouch on the slab
            pad->NewState.ButtonSquare = pad->NewState.ShockButtonL = 0;
    }
    if (CFlight::WantsWheel())  // Alt + wheel = speed level, not the weapon switch
    {
        if (CPad::NewMouseControllerState.wheelUp)
            CFlight::ms_nWheel++;
        if (CPad::NewMouseControllerState.wheelDown)
            CFlight::ms_nWheel--;
        CPad::NewMouseControllerState.wheelUp = CPad::NewMouseControllerState.wheelDown = 0;
        if (CPad* pad = CPad::GetPad(0))
            pad->NewState.DPadLeft = pad->NewState.DPadRight = 0;
    }
    bool kame = CKamehameha::IsLocalActive();
    bool wheel = CEmoteWheel::IsOpen();
    if (CKamehameha::IsLocalFiring() || wheel)  // the mouse steers the beam / picks the emote, not the camera
    {
        CKamehameha::ms_fMouseX += CPad::NewMouseControllerState.x;
        CKamehameha::ms_fMouseY += CPad::NewMouseControllerState.y;
        CEmoteWheel::ms_fMouseX += CPad::NewMouseControllerState.x;
        CEmoteWheel::ms_fMouseY += CPad::NewMouseControllerState.y;
        CPad::NewMouseControllerState.x = CPad::NewMouseControllerState.y = 0.0f;
    }
    if (!CModMenu::IsOpen() && !kame)
        return;
    if (CPad* pad = CPad::GetPad(0))
    {
        for (CControllerState* st : {&pad->NewState, &pad->OldState})
        {
            st->LeftStickX = st->LeftStickY = 0;
            st->DPadUp = st->DPadDown = st->DPadLeft = st->DPadRight = 0;
            st->ButtonTriangle = st->ButtonCross = st->ButtonSquare = st->ButtonCircle = 0;
        }
    }
}

void CModMenu::Install()
{
    s_UpdatePads_orig = injector::GetBranchDestination(0x53BEE6).as_int();
    patch::RedirectCall(0x53BEE6, (void*)CPad__UpdatePads_Hook);
}

void CModMenu::Process()
{
    if (CChat::m_bInputActive || FrontEndMenuManager.m_bMenuActive)
        return;

    static bool f2Was = false;
    bool f2 = CPad::NewKeyState.FKeys[1] != 0;
    bool f2Pressed = f2 && !f2Was;
    f2Was = f2;
    if (f2Pressed)  // F2
    {
        Toggle();
        return;
    }
    if (!ms_bOpen || ms_pages.empty() || CPrompt::IsActive())
        return;

    Page& page = ms_pages.back();
    int count = (int)page.items.size();

    unsigned int now = GetTickCount();
    static unsigned int holdStart = 0;
    auto step = [&](short CKeyboardState::*key) -> bool
    {
        if (KeyJustPressed(key))
        {
            holdStart = now;
            ms_nLastInput = now;
            return true;
        }
        if (KeyDown(key) && now - holdStart > 350 && now - ms_nLastInput > 110)
        {
            ms_nLastInput = now;
            return true;
        }
        return false;
    };
    int move = 0;
    if (step(&CKeyboardState::up))
        move = -1;
    else if (step(&CKeyboardState::down))
        move = 1;
    else if (step(&CKeyboardState::pgup) || step(&CKeyboardState::left))
        move = -ROWS;
    else if (step(&CKeyboardState::pgdn) || step(&CKeyboardState::right))
        move = ROWS;

    if (move != 0 && count > 0)
    {
        page.selected += move;
        if (move == -1 && page.selected < 0)
            page.selected = count - 1;
        else if (move == 1 && page.selected >= count)
            page.selected = 0;
        page.selected = std::max(0, std::min(count - 1, page.selected));
        if (page.selected < page.scroll)
            page.scroll = page.selected;
        if (page.selected >= page.scroll + ROWS)
            page.scroll = page.selected - ROWS + 1;
    }

    bool select = KeyJustPressed(&CKeyboardState::enter) || KeyJustPressed(&CKeyboardState::extenter);
    bool back = KeyJustPressed(&CKeyboardState::back) || KeyJustPressed(&CKeyboardState::esc);

    if (select && count > 0)
    {
        Item& item = page.items[page.selected];
        if (item.onSelect)
            item.onSelect();
        else if (item.submenu)
        {
            CModMenu m;
            item.submenu(&m);
        }
    }
    else if (back)
    {
        if (ms_pages.size() > 1)
            ms_pages.pop_back();
        else
            ms_bOpen = false;
    }

    // keep Esc away from the pause menu while we are open
    CPad::NewKeyState.esc = 0;
}

// ------------------------------------------------------------------------------------------------
// drawing (same look as the prompt box: black translucent panel, game font)

void CModMenu::Draw()
{
    if (!ms_bOpen || ms_pages.empty() || !FrontEndMenuManager.m_bPrefsShowHud)
        return;

    const Page& page = ms_pages.back();
    const float boxW = 330.0f;
    const float rowH = 15.0f;
    const float headH = 24.0f;
    const float footH = 20.0f;
    int rows = std::min(ROWS, (int)page.items.size());
    const float boxH = headH + rows * rowH + footH + 8.0f;
    const float x = 30.0f;
    const float y = 100.0f;

    CSprite2d::DrawRect(CRect(CUtil::HUD_X(x), CUtil::HUD_Y(y), CUtil::HUD_X(x + boxW), CUtil::HUD_Y(y + boxH)),
        CRGBA(0, 0, 0, 190));
    CSprite2d::DrawRect(CRect(CUtil::HUD_X(x), CUtil::HUD_Y(y), CUtil::HUD_X(x + boxW), CUtil::HUD_Y(y + headH)),
        CRGBA(54, 104, 44, 230));  // Grove green

    CFont::SetBackground(false, false);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetProportional(true);
    CFont::SetEdge(0);
    CFont::SetDropShadowPosition(0);
    CFont::SetWrapx(CUtil::HUD_X(x + boxW + 200.0f));

    // title
    CFont::SetFontStyle(2);
    CFont::SetScale(CUtil::HUD_X(0.3f), CUtil::HUD_Y(0.7f));
    CFont::SetColor(CRGBA(255, 255, 255, 255));
    CFont::PrintString(CUtil::HUD_X(x + 8.0f), CUtil::HUD_Y(y + 5.0f), page.title.c_str());

    // rows
    CFont::SetFontStyle(1);
    for (int r = 0; r < rows; r++)
    {
        int i = page.scroll + r;
        if (i >= (int)page.items.size())
            break;
        const Item& item = page.items[i];
        float ry = y + headH + 4.0f + r * rowH;
        bool sel = i == page.selected;
        if (sel)
            CSprite2d::DrawRect(CRect(CUtil::HUD_X(x + 4.0f), CUtil::HUD_Y(ry), CUtil::HUD_X(x + boxW - 4.0f),
                                    CUtil::HUD_Y(ry + rowH)),
                CRGBA(255, 255, 255, 40));

        CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
        CFont::SetScale(CUtil::HUD_X(0.27f), CUtil::HUD_Y(0.6f));
        CFont::SetColor(sel ? CRGBA(255, 220, 90, 255) : CRGBA(225, 225, 225, 255));
        std::string label = (sel ? "> " : "  ") + item.label + (item.onSelect ? "" : "  ~w~>");
        CFont::PrintString(CUtil::HUD_X(x + 8.0f), CUtil::HUD_Y(ry + 1.0f), label.c_str());

        if (item.right)
        {
            std::string right = item.right();
            if (!right.empty())
            {
                CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
                CFont::PrintString(CUtil::HUD_X(x + boxW - 10.0f), CUtil::HUD_Y(ry + 1.0f), right.c_str());
            }
        }
    }

    // footer
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetScale(CUtil::HUD_X(0.22f), CUtil::HUD_Y(0.5f));
    CFont::SetColor(CRGBA(170, 170, 170, 255));
    std::string foot;
    if (GetTickCount() < ms_nStatusUntil && !ms_status.empty())
        foot = "~g~" + ms_status;
    else
    {
        foot = "~y~Flechas~s~ mover  ~y~Enter~s~ elegir  ~y~Retroceso~s~ volver";
        if ((int)page.items.size() > ROWS)
            foot += "   " + std::to_string(page.selected + 1) + "/" + std::to_string(page.items.size());
    }
    CFont::PrintString(CUtil::HUD_X(x + 8.0f), CUtil::HUD_Y(y + boxH - footH + 1.0f), foot.c_str());
}

// ------------------------------------------------------------------------------------------------
// missions (for testing): the host launches any story mission directly, without its marker

void CModMenu::BuildMissions(CModMenu* m)
{
    struct Mission
    {
        int id;
        const char* name;
    };
    static const Mission missions[] = {
        {11, "Big Smoke"},
        {12, "Ryder"},
        {13, "Tagging Up Turf"},
        {14, "Cleaning The Hood"},
        {15, "Drive-Thru"},
        {16, "Nines And AK's"},
        {17, "Drive-By"},
        {18, "Sweet's Girl"},
        {19, "Cesar Vialpando"},
        {20, "Los Sepulcros"},
        {21, "Doberman"},
        {22, "Burning Desire"},
        {23, "Gray Imports"},
        {24, "Home Invasion"},
        {25, "Catalyst"},
        {26, "Robbing Uncle Sam"},
        {27, "OG Loc"},
        {28, "Running Dog"},
        {29, "Wrong Side of the Tracks"},
        {30, "Just Business"},
        {31, "Life's a Beach"},
        {32, "Madd Dogg's Rhymes"},
        {33, "Management Issues"},
        {34, "House Party"},
        {35, "Race Tournament / 8-track / Dirt Track"},
        {36, "Lowrider (High Stakes)"},
        {37, "Reuniting The Families"},
        {38, "The Green Sabre"},
        {39, "Badlands"},
        {40, "First Date"},
        {41, "Local Liquor Store"},
        {42, "Small Town Bank"},
        {43, "Tanker Commander"},
        {44, "Against All Odds"},
        {45, "King in Exile"},
        {46, "Body Harvest"},
        {47, "Are you going to San Fierro?"},
        {48, "Wu Zi Mu / Farewell, My Love..."},
        {49, "Wear Flowers In Your Hair"},
        {50, "Deconstruction"},
        {51, "555 WE TIP"},
        {52, "Snail Trail"},
        {53, "Mountain Cloud Boys"},
        {54, "Ran Fa Li"},
        {55, "Lure"},
        {56, "Amphibious Assault"},
        {57, "The Da Nang Thang"},
        {58, "Photo Opportunity"},
        {59, "Jizzy"},
        {60, "Outrider"},
        {61, "Ice Cold Killa"},
        {62, "Toreno's Last Flight"},
        {63, "Yay Ka-Boom-Boom"},
        {64, "Pier 69"},
        {65, "T-Bone Mendez"},
        {66, "Mike Toreno"},
        {67, "Zeroing In"},
        {68, "Test Drive"},
        {69, "Customs Fast Track"},
        {70, "Puncture Wounds"},
        {71, "Back to School"},
        {72, "Air Raid"},
        {73, "Supply Lines..."},
        {74, "New Model Army"},
        {75, "Monster"},
        {76, "Highjack"},
        {77, "Interdiction"},
        {78, "Verdant Meadows"},
        {79, "N.O.E."},
        {80, "Stowaway"},
        {81, "Black Project"},
        {82, "Green Goo"},
        {83, "Learning to Fly"},
        {84, "Fender Ketchup"},
        {85, "Explosive Situation"},
        {86, "You've Had Your Chips"},
        {87, "Fish in a Barrel"},
        {88, "Don Peyote"},
        {89, "Intensive Care"},
        {90, "The Meat Business"},
        {91, "Freefall"},
        {92, "Saint Mark's Bistro"},
        {93, "Misappropriation"},
        {94, "High Noon"},
        {95, "Madd Dogg"},
        {96, "Architectural Espionage"},
        {97, "Key To Her Heart"},
        {98, "Dam And Blast"},
        {99, "Cop Wheels"},
        {100, "Up, Up and Away!"},
        {101, "Breaking the Bank at Caligula's"},
        {102, "A Home In The Hills"},
        {103, "Vertical Bird"},
        {104, "Home Coming"},
        {105, "Cut Throat Business"},
        {106, "Beat Down on B Dup"},
        {107, "Grove 4 Life"},
        {108, "Riot"},
        {109, "Los Desperados"},
        {110, "End Of The Line (1)"},
        {111, "End Of The Line (2)"},
        {112, "End Of The Line (3)"}
    };

    std::vector<Item> items;
    items.push_back({"~r~Fallar / cancelar la mision actual", nullptr,
        []
        {
            if (!CLocalPlayer::m_bIsHost && CNetwork::m_bAuthenticated)
            {
                SetStatus("solo el host puede hacerlo");
                return;
            }
            if (!CTheScripts::IsPlayerOnAMission())
            {
                SetStatus("no hay ninguna mision en curso");
                return;
            }
            Command<Commands::FAIL_CURRENT_MISSION>();
            SetStatus("mision cancelada");
        },
        nullptr});
    for (const Mission& mi : missions)
    {
        int id = mi.id;
        std::string name = mi.name;
        items.push_back({"#" + std::to_string(id) + "  " + name, nullptr,
            [id, name]
            {
                if (!CLocalPlayer::m_bIsHost && CNetwork::m_bAuthenticated)
                {
                    SetStatus("solo el host puede iniciar misiones");
                    return;
                }
                if (CTheScripts::IsPlayerOnAMission())
                {
                    SetStatus("ya hay una mision en curso (cancelala primero)");
                    return;
                }
                // everybody plays it: bring every player next to the host first
                if (CPlayerPed* me = FindPlayerPed(0))
                {
                    int n = 0;
                    for (auto player : CNetworkPlayerManager::m_pPlayers)
                    {
                        Packets::Admin::AdminAction bring{};
                        bring.targetPlayerid = player->m_playerId;
                        bring.action = Packets::Admin::ADMIN_BRING;
                        float angle = (float)(n++) * 1.2566f;  // around the host, 2 m away
                        bring.pos = me->GetPosition() + CVector(2.0f * cosf(angle), 2.0f * sinf(angle), 0.0f);
                        bring.areaCode = (uint8_t)me->m_nAreaCode;
                        GetPacketFactory().Send(bring);
                    }
                }
                *(int*)(CTheScripts::ScriptSpace + CTheScripts::OnAMissionFlag) = 1;
                Command<Commands::LOAD_AND_LAUNCH_MISSION_INTERNAL>(id);
                ms_bOpen = false;
                ms_pages.clear();
                CChat::AddMessage("{a0f0a0}[coop] {d6dce8}mision iniciada desde el menu: %s", name.c_str());
            },
            nullptr});
    }
    m->Open("MISIONES", std::move(items));
}

// ------------------------------------------------------------------------------------------------
// weapons: everything, including the ones no cheat gives (they add to what you carry)

void CModMenu::BuildWeapons(CModMenu* m)
{
    struct W
    {
        int id;
        const char* name;
        int ammo;
    };
    static const W weapons[] = {
        {38, "Minigun", 5000}, {17, "Granada de humo", 30}, {16, "Granada", 30}, {18, "Molotov", 30}, {39, "Carga explosiva (satchel)", 30},
        {35, "Lanzacohetes", 50}, {36, "Lanzacohetes guiado", 50}, {37, "Lanzallamas", 5000}, {8, "Katana", 1}, {9, "Motosierra", 1},
        {24, "Desert Eagle", 500}, {23, "Pistola silenciada", 500}, {22, "Pistola", 500}, {27, "Escopeta de combate (SPAS)", 500},
        {26, "Escopeta recortada", 500}, {25, "Escopeta", 500}, {29, "MP5", 1000}, {28, "Uzi", 1000}, {32, "Tec-9", 1000},
        {31, "M4", 1000}, {30, "AK-47", 1000}, {33, "Rifle", 500}, {34, "Francotirador", 500}, {4, "Cuchillo", 1},
        {5, "Bate", 1}, {6, "Pala", 1}, {7, "Taco de billar", 1}, {3, "Porra", 1}, {2, "Palo de golf", 1}, {1, "Manopla", 1},
        {15, "Baston", 1}, {14, "Flores", 1}, {10, "Consolador", 1}, {11, "Consolador 2", 1}, {12, "Vibrador", 1}, {13, "Vibrador 2", 1},
        {41, "Spray", 5000}, {42, "Extintor", 5000}, {43, "Camara", 500}, {44, "Vision nocturna", 1}, {45, "Vision termica", 1},
        {46, "Paracaidas", 1}, {40, "Detonador", 1},
    };
    std::vector<Item> items;
    items.push_back({"~y~Gravity gun", []() -> std::string { return CGravityGun::IsActive() ? "~g~ON" : "~r~OFF"; },
        [] { CGravityGun::Toggle(); }, nullptr});
    items.push_back({"~y~Kamehameha", []() -> std::string { return CKamehameha::IsWeaponEnabled() ? "~g~ON" : "~r~OFF"; },
        [] { CKamehameha::ToggleWeapon(); }, nullptr});
    for (const W& w : weapons)
    {
        int id = w.id, ammo = w.ammo;
        std::string name = w.name;
        items.push_back({name, nullptr,
            [id, ammo, name]
            {
                CPlayerPed* me = FindPlayerPed(0);
                if (!me)
                    return;
                CWeaponInfo* info = CWeaponInfo::GetWeaponInfo((eWeaponType)id, 1);
                if (info && info->m_nModelId1 > 0)
                {
                    CStreaming::RequestModel(info->m_nModelId1, GAME_REQUIRED);
                    if (info->m_nModelId2 > 0)
                        CStreaming::RequestModel(info->m_nModelId2, GAME_REQUIRED);
                    CStreaming::LoadAllRequestedModels(false);
                }
                me->GiveWeapon((eWeaponType)id, ammo, true);
                SetStatus(name + " lista");
            },
            nullptr});
    }
    m->Open("ARMAS", std::move(items));
}

// ------------------------------------------------------------------------------------------------
// tuning of the vehicle you are in. Upgrades go through the game's own ADD_VEHICLE_MOD, which the coop
// already syncs to the other players.

static CVehicle* MyVehicle()
{
    CPlayerPed* me = FindPlayerPed(0);
    return me && me->m_nPedFlags.bInVehicle ? me->m_pVehicle : nullptr;
}

static bool AddMod(int mod)
{
    CVehicle* v = MyVehicle();
    if (!v)
        return false;
    int ref = CPools::GetVehicleRef(v);
    Command<Commands::REQUEST_VEHICLE_MOD>(mod);
    CStreaming::LoadAllRequestedModels(false);
    if (!Command<Commands::HAS_VEHICLE_MOD_LOADED>(mod))
        return false;
    int slot = -1;
    Command<Commands::ADD_VEHICLE_MOD>(ref, mod, &slot);
    return true;
}

void CModMenu::BuildTuning(CModMenu* m)
{
    CVehicle* v = MyVehicle();
    if (!v)
    {
        SetStatus("subete a un vehiculo primero");
        return;
    }
    bool car = v->m_nVehicleSubType == VEHICLE_AUTOMOBILE || v->m_nVehicleSubType == VEHICLE_MTRUCK ||
               v->m_nVehicleSubType == VEHICLE_QUAD;
    std::vector<Item> items;
    items.push_back({"Reparar", nullptr,
        []
        {
            if (CVehicle* v = MyVehicle())
            {
                Command<Commands::FIX_CAR>(CPools::GetVehicleRef(v));
                v->m_fHealth = 1000.0f;
                SetStatus("reparado");
            }
        },
        nullptr});
    items.push_back({"Indestructible", []() -> std::string
        {
            CVehicle* v = MyVehicle();
            return v && v->m_nPhysicalFlags.bBulletProof && v->m_nPhysicalFlags.bExplosionProof ? "~g~ON" : "~r~OFF";
        },
        []
        {
            if (CVehicle* v = MyVehicle())
            {
                bool on = !(v->m_nPhysicalFlags.bBulletProof && v->m_nPhysicalFlags.bExplosionProof);
                Command<Commands::SET_CAR_PROOFS>(CPools::GetVehicleRef(v), on, on, on, on, on);
                SetStatus(on ? "indestructible" : "normal");
            }
        },
        nullptr});
    if (car)
    {
        struct Mod
        {
            int id;
            const char* name;
        };
        static const Mod mods[] = {
            {1010, "Nitro x10"}, {1008, "Nitro x5"}, {1009, "Nitro x2"}, {1087, "Hidraulica"}, {1086, "Estereo (bajos)"},
            {1025, "Ruedas Offroad"}, {1073, "Ruedas Shadow"}, {1074, "Ruedas Mega"}, {1075, "Ruedas Rimshine"},
            {1076, "Ruedas Wires"}, {1077, "Ruedas Classic"}, {1078, "Ruedas Twist"}, {1079, "Ruedas Cutter"},
            {1080, "Ruedas Switch"}, {1081, "Ruedas Grove"}, {1082, "Ruedas Import"}, {1083, "Ruedas Dollar"},
            {1084, "Ruedas Trance"}, {1085, "Ruedas Atomic"}, {1096, "Ruedas Ahab"}, {1097, "Ruedas Virtual"},
            {1098, "Ruedas Access"},
        };
        for (const Mod& md : mods)
        {
            int id = md.id;
            std::string name = md.name;
            items.push_back({name, nullptr,
                [id, name]
                {
                    if (AddMod(id))
                        SetStatus(name + " puesto");
                    else
                        SetStatus("este vehiculo no acepta " + name);
                },
                nullptr});
        }
        for (int pj = 0; pj < 3; pj++)
        {
            items.push_back({"Pintura especial " + std::to_string(pj + 1), nullptr,
                [pj]
                {
                    if (CVehicle* v = MyVehicle())
                    {
                        Command<Commands::GIVE_VEHICLE_PAINTJOB>(CPools::GetVehicleRef(v), pj);
                        SetStatus("si el modelo la tiene, ya esta puesta");
                    }
                },
                nullptr});
        }
    }
    m->Open("TUNING", std::move(items));
}
