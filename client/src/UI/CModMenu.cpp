#include "stdafx.h"
#include "CModMenu.h"
#include "CCheatSync.h"
#include "CSkin.h"
#include "CUnlockAll.h"
#include "CPrompt.h"

bool CModMenu::ms_bOpen = false;
std::vector<CModMenu::Page> CModMenu::ms_pages;
unsigned int CModMenu::ms_nLastInput = 0;
std::string CModMenu::ms_status;
unsigned int CModMenu::ms_nStatusUntil = 0;

static const int ROWS = 12;

static bool KeyDown(short CKeyboardState::*key)
{
    return CPad::NewKeyState.*key != 0;
}
static bool KeyJustPressed(short CKeyboardState::*key)
{
    return CPad::NewKeyState.*key != 0 && CPad::OldKeyState.*key == 0;
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
            {"Teletransporte", nullptr, nullptr, BuildTeleport},
            {"Guardar partida", nullptr,
                []
                {
                    ms_bOpen = false;
                    ms_pages.clear();
                    Command<Commands::ACTIVATE_SAVE_MENU>();
                }},
        });
}

static bool IsVehicleCheat(int i)
{
    static const int vehicleCheats[] = {17, 18, 19, 20, 21, 22, 23, 24, 25, 50, 51, 84, 85, 86, 87, 88, 89};
    for (int v : vehicleCheats)
        if (v == i)
            return true;
    return false;
}

void CModMenu::BuildCheats(CModMenu* m)
{
    std::vector<Item> items;
    for (int i = 0; i < CheatInfo::COUNT; i++)
    {
        if (IsVehicleCheat(i) || i == 59)
            continue;
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
    m->Open("TRUCOS  (amarillo = necesita que todos acepten)", std::move(items));
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
        items.push_back({std::string(CSkin::ModelName(id)) + "  ~b~#" + std::to_string(id),
            [id]() -> std::string { return CSkin::ms_nLocalSkin == id ? "~g~actual" : ""; },
            [id]
            {
                CSkin::SetLocal((uint16_t)id);
                SetStatus(std::string("skin: ") + CSkin::ModelName(id));
            },
            nullptr});
    };
    add(0);
    // story characters first, then the rest
    static const int story[] = {270, 271, 269, 292, 294, 295, 293, 297, 296, 298, 265, 266, 267, 290, 291, 1, 2, 3, 4, 5, 6,
        42, 65, 268, 272, 299, 273, 8};
    for (int id : story)
        if (CSkin::IsValidPedModel(id))
            add(id);
    for (int id = 1; id < 300; id++)
    {
        bool isStory = false;
        for (int s : story)
            if (s == id)
                isStory = true;
        if (!isStory && CSkin::IsValidPedModel(id))
            add(id);
    }
    m->Open("SKIN  (solo cambia el modelo, CJ se mueve igual)", std::move(items));
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
    m->Open("TELETRANSPORTE  (a jugadores: Tab)", std::move(items));
}

// ------------------------------------------------------------------------------------------------
// input

void CModMenu::Process()
{
    if (CChat::m_bInputActive || FrontEndMenuManager.m_bMenuActive)
        return;

    if (CPad::NewKeyState.FKeys[1] != 0 && CPad::OldKeyState.FKeys[1] == 0)  // F2
    {
        Toggle();
        return;
    }
    if (!ms_bOpen || ms_pages.empty() || CPrompt::IsActive())
        return;

    Page& page = ms_pages.back();
    int count = (int)page.items.size();

    // keep the keys away from the game (Enter = enter vehicle, Esc = pause)
    auto consume = [](short CKeyboardState::*key)
    {
        CPad::NewKeyState.*key = 0;
        CPad::OldKeyState.*key = 0;
    };

    unsigned int now = GetTickCount();
    bool repeat = now - ms_nLastInput > 120;
    int move = 0;
    if ((KeyJustPressed(&CKeyboardState::up) || (KeyDown(&CKeyboardState::up) && repeat)))
        move = -1;
    else if ((KeyJustPressed(&CKeyboardState::down) || (KeyDown(&CKeyboardState::down) && repeat)))
        move = 1;
    else if (KeyJustPressed(&CKeyboardState::pgup) || (KeyDown(&CKeyboardState::pgup) && repeat))
        move = -ROWS;
    else if (KeyJustPressed(&CKeyboardState::pgdn) || (KeyDown(&CKeyboardState::pgdn) && repeat))
        move = ROWS;

    if (move != 0 && count > 0)
    {
        ms_nLastInput = now;
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

    bool select = KeyJustPressed(&CKeyboardState::enter) || KeyJustPressed(&CKeyboardState::extenter) ||
                  KeyJustPressed(&CKeyboardState::right);
    bool back = KeyJustPressed(&CKeyboardState::back) || KeyJustPressed(&CKeyboardState::left) ||
                KeyJustPressed(&CKeyboardState::esc);

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

    consume(&CKeyboardState::enter);
    consume(&CKeyboardState::extenter);
    consume(&CKeyboardState::esc);
    consume(&CKeyboardState::back);
    consume(&CKeyboardState::up);
    consume(&CKeyboardState::down);
    consume(&CKeyboardState::left);
    consume(&CKeyboardState::right);
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
        foot = "~y~Flechas~s~ mover  ~y~Enter~s~ elegir  ~y~Retroceso~s~ volver  ~y~F2~s~ cerrar";
        if ((int)page.items.size() > ROWS)
            foot += "   " + std::to_string(page.selected + 1) + "/" + std::to_string(page.items.size());
    }
    CFont::PrintString(CUtil::HUD_X(x + 8.0f), CUtil::HUD_Y(y + boxH - footH + 1.0f), foot.c_str());
}
