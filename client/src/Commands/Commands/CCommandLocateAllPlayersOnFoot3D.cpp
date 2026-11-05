#include "stdafx.h"
#include "CCommandLocateAllPlayersOnFoot3D.h"

void CCommandLocateAllPlayersOnFoot3D::Process(CRunningScript* script)
{
	script->CollectParameters(7);

	if (!CLocalPlayer::m_bIsHost)
	{
		script->UpdateCompareFlag(false);
		return;
	}

	bool showMarker = ScriptParams[0] != 0;
	CVector* position = (CVector*)&ScriptParams[1];
	CVector* radius = (CVector*)&ScriptParams[4];
	
	// [private build] originally EVERY player had to stand inside a ~1 m marker at once (with 3+ players
	// that almost never happens, so missions like "Ryder" never started). Now one player is enough:
	// the host shows the marker; if another player steps in it, the host is brought there and the
	// mission starts (the mission itself brings the rest of the players).
	CVector r(std::max(radius->x, 1.5f), std::max(radius->y, 1.5f), std::max(radius->z, 2.0f));

	bool result = Command<Commands::LOCATE_CHAR_ON_FOOT_3D>(
		CPools::GetPedRef(FindPlayerPed(0)),
		position->x, position->y, position->z,
		r.x, r.y, r.z,
		showMarker);

	if (!result)
	{
		for (auto networkPlayer : CNetworkPlayerManager::m_pPlayers)
		{
			if (networkPlayer->m_pPed && Command<Commands::LOCATE_CHAR_ON_FOOT_3D>(
				CPools::GetPedRef(networkPlayer->m_pPed),
				position->x, position->y, position->z,
				r.x, r.y, r.z,
				false))
			{
				result = true;
				if (!CTheScripts::IsPlayerOnAMission() && !FindPlayerPed(0)->m_nPedFlags.bInVehicle)
				{
					CUtil::TeleportLocalPlayer(*position + CVector(1.0f, 0.0f, 0.0f), 0);
					CChat::AddMessage("{a0f0a0}[coop] {d6dce8}%s inicio la mision, te traemos al marcador",
						networkPlayer->GetName().c_str());
				}
				break;
			}
		}
	}

	script->UpdateCompareFlag(result);
}
