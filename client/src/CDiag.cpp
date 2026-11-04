#include "stdafx.h"
#include "CDiag.h"
#include <cstdio>
#include "CTheScripts.h"
#include "CRunningScript.h"
#include "CCamera.h"
#include "CDraw.h"
#include "CGame.h"
#include "CCutsceneMgr.h"

static FILE* gDiagFile = nullptr;
static DWORD gLastDump = 0;
static DWORD gStart = 0;

void CDiag::Init()
{
    gDiagFile = fopen("CoopAndreas\\diag.log", "w");
    gStart = GetTickCount();
    if (gDiagFile)
    {
        fprintf(gDiagFile, "diag started\n");
        fflush(gDiagFile);
    }
    Events::gameProcessEvent += CDiag::Process;
}

void CDiag::Process()
{
    if (!gDiagFile)
        return;

    DWORD now = GetTickCount();
    if (now - gLastDump < 2000)
        return;
    gLastDump = now;

    CPlayerPed* ped = FindPlayerPed(0);
    CVector pos = ped ? ped->GetPosition() : CVector();

    fprintf(gDiagFile,
        "\n[t=%lus] connected=%d auth=%d host=%d | fadeAlpha=%.1f fadeFlag=%d drawFade=%d | area=%d | "
        "cut: load=%u running=%d | onMission=%d | pos=%.1f,%.1f,%.1f\n",
        (now - gStart) / 1000, (int)CNetwork::m_bConnected, (int)CNetwork::m_bAuthenticated,
        (int)CLocalPlayer::m_bIsHost, TheCamera.m_fFadeAlpha, (int)TheCamera.m_nFadeInOutFlag, (int)CDraw::FadeValue,
        CGame::currArea, CCutsceneMgr::ms_cutsceneLoadStatus, (int)CCutsceneMgr::ms_running,
        CTheScripts::OnAMissionFlag ? (int)CTheScripts::ScriptSpace[CTheScripts::OnAMissionFlag] : -1, pos.x, pos.y,
        pos.z);

    int n = 0;
    for (CRunningScript* s = CTheScripts::pActiveScripts; s != nullptr && n < 120; s = s->m_pNext, ++n)
    {
        char name[9]{};
        memcpy(name, s->m_szName, 8);
        long relBase = (long)(s->m_pCurrentIP - s->m_pBaseIP);
        long relSpace = (long)(s->m_pCurrentIP - (unsigned char*)CTheScripts::ScriptSpace);
        fprintf(gDiagFile, "  %-8s mission=%d ip_rel_base=%ld ip_abs=%ld wake=%d\n", name, (int)s->m_bIsMission,
            relBase, relSpace, s->m_nWakeTime);
    }
    fflush(gDiagFile);
}
