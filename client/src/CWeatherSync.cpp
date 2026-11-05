#include "CWeatherSync.h"

void CWeatherSync::SyncCurrentState()
{
	if (!CLocalPlayer::m_bIsHost)
	{
		return;
	}

	Packets::World::GameWeatherTime packet{};
	packet.newWeather = static_cast<eWeatherType>(CWeather::NewWeatherType);
	packet.oldWeather = static_cast<eWeatherType>(CWeather::OldWeatherType);
	packet.forcedWeather = CWeather::ForcedWeatherType;
	packet.interpolation = (int)(CWeather::InterpolationValue * 1000.0f);
	packet.timeScale = (int)(CTimer::ms_fTimeScale * 1000.0f);
	packet.currentMonth = CClock::ms_nGameClockMonth;
	packet.currentDay = CClock::CurrentDay;
	packet.currentHour = CClock::ms_nGameClockHours;
	packet.currentMinute = CClock::ms_nGameClockMinutes;
	GetPacketFactory().Send(packet);
}

void CWeatherSync::HandlePacket(Packets::World::GameWeatherTime* pGameWeatherTime)
{
	CWeather::OldWeatherType = pGameWeatherTime->oldWeather;
	CWeather::NewWeatherType = pGameWeatherTime->newWeather;
	CWeather::ForcedWeatherType = (short)pGameWeatherTime->forcedWeather;
	// only snap the blend when we are clearly out of sync, otherwise it flickers
	float interp = pGameWeatherTime->interpolation / 1000.0f;
	if (fabsf(CWeather::InterpolationValue - interp) > 0.15f)
		CWeather::InterpolationValue = interp;
	float timeScale = pGameWeatherTime->timeScale / 1000.0f;
	if (fabsf(CTimer::ms_fTimeScale - timeScale) > 0.01f)
		CTimer::ms_fTimeScale = timeScale;
	CClock::ms_nGameClockMonth = pGameWeatherTime->currentMonth;
	CClock::CurrentDay = pGameWeatherTime->currentDay;
	CClock::ms_nGameClockHours = pGameWeatherTime->currentHour;
	CClock::ms_nGameClockMinutes = pGameWeatherTime->currentMinute;
}

void CWeatherSync::ServerTimeRecalculated(server_time_t serverTime)
{
	CWaterLevel::m_nWaterTimeOffset = CTimer::m_snTimeInMilliseconds - serverTime;
}
