#include "stdafx.h"
#include "CEmoteWheel.h"
#include "UI/CModMenu.h"
#include "CKamehameha.h"

bool CEmoteWheel::ms_bOpen = false;
int CEmoteWheel::ms_nSelected = -1;
int CEmoteWheel::ms_nPlaying = 0;
unsigned int CEmoteWheel::ms_nStartedAt = 0;
float CEmoteWheel::ms_fMouseX = 0.0f;
float CEmoteWheel::ms_fMouseY = 0.0f;

struct Emote
{
    const char* label;
    const char* block;  // IFP
    const char* anim;
    bool loop;
};
static const Emote s_emotes[] = {
    {"Bailar", "DANCING", "dance_loop", true},
    {"Bailar 2", "DANCING", "DAN_Up_A", true},
    {"Bailar 3", "DANCING", "dnce_M_b", true},
    {"Fumar", "SMOKING", "M_smkstnd_loop", true},
    {"Beber", "BAR", "dnk_stndM_loop", true},
    {"Mear", "PAULNMAC", "Piss_loop", true},
    {"Saludar", "ON_LOOKERS", "wave_loop", true},
    {"Reir", "RAPPING", "Laugh_01", true},
    {"Brazos cruzados", "COP_AMBIENT", "Coplook_loop", true},
    {"Sentarse", "BEACH", "ParkSit_M_loop", true},
    {"Manos arriba", "PED", "handsup", false},
    {"Grosero", "PED", "fucku", false},
};
static constexpr int COUNT = sizeof(s_emotes) / sizeof(s_emotes[0]);

static bool LoadBlock(const char* block)
{
    if (strcmp(block, "PED") == 0)
        return true;
    if (Command<Commands::HAS_ANIMATION_LOADED>(block))
        return true;
    Command<Commands::REQUEST_ANIMATION>(block);
    return false;
}

static void PlayOn(CPed* ped, int index)
{
    if (!ped || index < 0 || index >= COUNT)
        return;
    const Emote& e = s_emotes[index];
    Command<Commands::TASK_PLAY_ANIM>(CPools::GetPedRef(ped), e.anim, e.block, 4.0f, e.loop, 0, 0, !e.loop, -1);
}

static void StopOn(CPed* ped)
{
    if (ped && CPoolGuard::IsPedAlive(ped))
        Command<Commands::CLEAR_CHAR_TASKS>(CPools::GetPedRef(ped));
}

static void Send(int playing)
{
    Packets::Skin::PlayerPower p{};
    p.power = 1;
    p.phase = (uint8_t)playing;
    GetPacketFactory().Send(p);
}

// remote emotes waiting for their animation block
struct PendingRemote
{
    CPed* ped;
    int index;
    unsigned int since;
};
static std::vector<PendingRemote> s_pendingRemote;
static int s_pendingLocal = -1;
static unsigned int s_pendingLocalSince = 0;

void CEmoteWheel::Process()
{
    CPlayerPed* me = FindPlayerPed(0);
    if (!me)
        return;

    bool b = CPad::NewKeyState.standardKeys['B'] != 0 || CPad::NewKeyState.standardKeys['b'] != 0;
    bool canUse = !me->m_nPedFlags.bInVehicle && me->m_fHealth > 0.0f && !CChat::m_bInputActive &&
                  !FrontEndMenuManager.m_bMenuActive && !CModMenu::IsOpen() && !CKamehameha::IsLocalActive();

    if (!ms_bOpen && b && canUse)
    {
        ms_bOpen = true;
        ms_nSelected = -1;
        ms_fMouseX = ms_fMouseY = 0.0f;
    }
    else if (ms_bOpen && (!b || !canUse))
    {
        ms_bOpen = false;
        if (canUse && ms_nSelected >= 0)
        {
            s_pendingLocal = ms_nSelected;
            s_pendingLocalSince = GetTickCount();
        }
    }

    if (ms_bOpen)
    {
        // pick by the direction of the accumulated mouse movement (no click needed)
        float len = sqrtf(ms_fMouseX * ms_fMouseX + ms_fMouseY * ms_fMouseY);
        if (len > 25.0f)
        {
            float angle = atan2f(ms_fMouseX, -ms_fMouseY);  // 0 = up, clockwise
            if (angle < 0)
                angle += 6.2831853f;
            ms_nSelected = (int)((angle + 3.14159265f / COUNT) / (6.2831853f / COUNT)) % COUNT;
            if (len > 120.0f)  // keep the "stick" inside a ring
            {
                ms_fMouseX *= 120.0f / len;
                ms_fMouseY *= 120.0f / len;
            }
        }
    }

    // start the chosen emote once its animations are in memory
    if (s_pendingLocal >= 0)
    {
        if (LoadBlock(s_emotes[s_pendingLocal].block))
        {
            PlayOn(me, s_pendingLocal);
            ms_nPlaying = s_pendingLocal + 1;
            ms_nStartedAt = GetTickCount();
            Send(ms_nPlaying);
            s_pendingLocal = -1;
        }
        else if (GetTickCount() - s_pendingLocalSince > 5000)
            s_pendingLocal = -1;
    }

    // moving, jumping, aiming or getting hurt cancels it
    if (ms_nPlaying)
    {
        CPad* pad = CPad::GetPad(0);
        bool moving = pad && (abs(pad->NewState.LeftStickX) > 20 || abs(pad->NewState.LeftStickY) > 20 ||
                                 pad->NewState.ButtonSquare || pad->NewState.RightShoulder1 || pad->NewState.ButtonCircle ||
                                 pad->NewState.ButtonCross || pad->NewState.ButtonTriangle);
        if (GetTickCount() - ms_nStartedAt > 300 && (moving || me->m_nPedFlags.bInVehicle || me->m_fHealth <= 0.0f))
        {
            StopOn(me);
            ms_nPlaying = 0;
            Send(0);
        }
    }

    for (auto it = s_pendingRemote.begin(); it != s_pendingRemote.end();)
    {
        if (!CPoolGuard::IsPedAlive(it->ped) || GetTickCount() - it->since > 5000)
        {
            it = s_pendingRemote.erase(it);
            continue;
        }
        if (LoadBlock(s_emotes[it->index].block))
        {
            PlayOn(it->ped, it->index);
            it = s_pendingRemote.erase(it);
            continue;
        }
        ++it;
    }
}

void CEmoteWheel::OnPacket(Packets::Skin::PlayerPower* packet)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    if (!player || !player->m_pPed)
        return;
    for (auto it = s_pendingRemote.begin(); it != s_pendingRemote.end();)
        it = it->ped == player->m_pPed ? s_pendingRemote.erase(it) : it + 1;
    if (packet->phase == 0 || packet->phase > COUNT)
    {
        StopOn(player->m_pPed);
        return;
    }
    s_pendingRemote.push_back({player->m_pPed, packet->phase - 1, GetTickCount()});
}

// ---- drawing: a ring of dark segments in the middle of the screen, San Andreas colours/font
void CEmoteWheel::Draw()
{
    if (!ms_bOpen || !FrontEndMenuManager.m_bPrefsShowHud)
        return;

    float cx = SCREEN_WIDTH * 0.5f, cy = SCREEN_HEIGHT * 0.5f;
    float rOut = CUtil::HUD_Y(150.0f), rIn = CUtil::HUD_Y(55.0f);
    const float step = 6.2831853f / COUNT;
    const int sub = 6;

    for (int i = 0; i < COUNT; i++)
    {
        float a0 = i * step - step * 0.5f + 0.012f, a1 = a0 + step - 0.024f;
        bool sel = i == ms_nSelected;
        CRGBA col = sel ? CRGBA(54, 104, 44, 230) : CRGBA(0, 0, 0, 170);
        float ro = sel ? rOut + CUtil::HUD_Y(8.0f) : rOut;
        for (int k = 0; k < sub; k++)
        {
            float b0 = a0 + (a1 - a0) * k / sub, b1 = a0 + (a1 - a0) * (k + 1) / sub;
            // angle 0 = up, clockwise
            float x1 = cx + sinf(b0) * rIn, y1 = cy - cosf(b0) * rIn;
            float x2 = cx + sinf(b1) * rIn, y2 = cy - cosf(b1) * rIn;
            float x3 = cx + sinf(b0) * ro, y3 = cy - cosf(b0) * ro;
            float x4 = cx + sinf(b1) * ro, y4 = cy - cosf(b1) * ro;
            CSprite2d::Draw2DPolygon(x1, y1, x2, y2, x3, y3, x4, y4, col);
        }
    }

    CFont::SetBackground(false, false);
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_WIDTH);
    CFont::SetFontStyle(1);
    CFont::SetDropShadowPosition(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));
    CFont::SetEdge(0);
    for (int i = 0; i < COUNT; i++)
    {
        float a = i * step, r = (rIn + rOut) * 0.5f;
        bool sel = i == ms_nSelected;
        CFont::SetScale(CUtil::HUD_X(sel ? 0.36f : 0.3f), CUtil::HUD_Y(sel ? 0.8f : 0.68f));
        CFont::SetColor(sel ? CRGBA(255, 255, 255, 255) : CRGBA(200, 200, 200, 255));
        CFont::PrintString(cx + sinf(a) * r, cy - cosf(a) * r - CUtil::HUD_Y(6.0f), s_emotes[i].label);
    }

    // centre: title and pointer
    CFont::SetFontStyle(2);
    CFont::SetScale(CUtil::HUD_X(0.32f), CUtil::HUD_Y(0.75f));
    CFont::SetColor(CRGBA(255, 255, 255, 255));
    CFont::PrintString(cx, cy - CUtil::HUD_Y(9.0f), ms_nSelected >= 0 ? s_emotes[ms_nSelected].label : "EMOTES");
    float px = cx + ms_fMouseX / 120.0f * rIn * 0.8f, py = cy + ms_fMouseY / 120.0f * rIn * 0.8f;
    float d = CUtil::HUD_Y(3.0f);
    CSprite2d::DrawRect(CRect(px - d, py - d, px + d, py + d), CRGBA(255, 220, 90, 255));
}
