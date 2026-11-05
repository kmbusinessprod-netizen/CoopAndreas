#include "stdafx.h"
#include "CPrompt.h"

std::deque<CPrompt::SPrompt> CPrompt::ms_queue;

static bool CharJustPressed(char c)
{
    unsigned char up = (unsigned char)c, lo = (unsigned char)tolower(c);
    return (CPad::NewKeyState.standardKeys[up] || CPad::NewKeyState.standardKeys[lo]) &&
           !CPad::OldKeyState.standardKeys[up] && !CPad::OldKeyState.standardKeys[lo];
}

void CPrompt::Ask(const std::string& text, std::function<void(bool)> onAnswer, unsigned int timeoutMs)
{
    SPrompt p{text, onAnswer, timeoutMs, 0};
    ms_queue.push_back(p);
    if (ms_queue.size() == 1)
        ms_queue.front().startedAt = GetTickCount();
}

bool CPrompt::IsActive()
{
    return !ms_queue.empty();
}

void CPrompt::Clear()
{
    ms_queue.clear();
}

void CPrompt::Answer(bool accept)
{
    if (ms_queue.empty())
        return;
    SPrompt p = ms_queue.front();
    ms_queue.pop_front();
    if (!ms_queue.empty())
        ms_queue.front().startedAt = GetTickCount();
    if (p.onAnswer)
        p.onAnswer(accept);
}

void CPrompt::Process()
{
    if (ms_queue.empty())
        return;

    SPrompt& p = ms_queue.front();
    if (p.startedAt == 0)
        p.startedAt = GetTickCount();

    if (GetTickCount() - p.startedAt > p.timeoutMs)
    {
        Answer(false);
        return;
    }

    if (CChat::m_bInputActive)
        return;

    if (CharJustPressed('Y'))
        Answer(true);
    else if (CharJustPressed('N'))
        Answer(false);
}

void CPrompt::Draw()
{
    if (ms_queue.empty() || !FrontEndMenuManager.m_bPrefsShowHud)
        return;

    const SPrompt& p = ms_queue.front();

    // box in the top centre, like the game's help box but centred
    const float boxW = 300.0f;
    const float boxH = 62.0f;
    const float x = (CUtil::SCREEN_BASE_WIDTH - boxW) * 0.5f;
    const float y = 26.0f;

    CRect rect(CUtil::HUD_X(x), CUtil::HUD_Y(y), CUtil::HUD_X(x + boxW), CUtil::HUD_Y(y + boxH));
    CSprite2d::DrawRect(rect, CRGBA(0, 0, 0, 200));

    // remaining time bar at the bottom of the box
    float left = 1.0f - (float)(GetTickCount() - p.startedAt) / (float)p.timeoutMs;
    if (left < 0.0f)
        left = 0.0f;
    CSprite2d::DrawRect(CRect(CUtil::HUD_X(x + 6.0f), CUtil::HUD_Y(y + boxH - 5.0f),
                            CUtil::HUD_X(x + 6.0f + (boxW - 12.0f) * left), CUtil::HUD_Y(y + boxH - 2.0f)),
        CRGBA(200, 200, 200, 180));

    CFont::SetBackground(false, false);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetFontStyle(1);
    CFont::SetProportional(true);
    CFont::SetEdge(0);
    CFont::SetDropShadowPosition(0);
    CFont::SetWrapx(CUtil::HUD_X(x + boxW - 8.0f));
    CFont::SetScale(CUtil::HUD_X(0.26f), CUtil::HUD_Y(0.62f));
    CFont::SetColor(CRGBA(225, 225, 225, 255));
    CFont::PrintString(CUtil::HUD_X(x + 8.0f), CUtil::HUD_Y(y + 5.0f), p.text.c_str());

    CFont::SetScale(CUtil::HUD_X(0.24f), CUtil::HUD_Y(0.58f));
    CFont::SetColor(CRGBA(180, 180, 180, 255));
    CFont::PrintString(CUtil::HUD_X(x + 8.0f), CUtil::HUD_Y(y + boxH - 22.0f), "~y~Y~s~  aceptar          ~y~N~s~  rechazar");
}
