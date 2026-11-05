#include "stdafx.h"
#include "network/packets/skin.h"
#include <map>
#include <fstream>

// [private build] skins and bought properties: relay, and remember them for players that join later
std::vector<uint8_t> g_boughtProperties;

// skins by player name, kept in skins.txt ("name=skin") so they survive reconnects and server restarts
static std::map<std::string, uint16_t> s_savedSkins;
static bool s_skinsLoaded = false;

static std::string LowerName(const char* name)
{
	std::string s = name;
	for (char& c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

static void LoadSkins()
{
	if (s_skinsLoaded)
		return;
	s_skinsLoaded = true;
	std::ifstream file("skins.txt");
	std::string line;
	while (std::getline(file, line))
	{
		size_t eq = line.rfind('=');
		if (eq == std::string::npos)
			continue;
		s_savedSkins[line.substr(0, eq)] = (uint16_t)atoi(line.c_str() + eq + 1);
	}
}

static void SaveSkins()
{
	std::ofstream file("skins.txt", std::ios::trunc);
	for (auto& [name, skin] : s_savedSkins)
		file << name << "=" << skin << "\n";
}

uint16_t GetSavedSkin(const char* name)
{
	LoadSkins();
	auto it = s_savedSkins.find(LowerName(name));
	return it != s_savedSkins.end() ? it->second : 0;
}

PACKET_HANDLER(ePacketType::PLAYER_SKIN, Packets::Skin::PlayerSkin* pSkin, CNetworkPlayer* pNetworkPlayer)
{
	pSkin->senderPlayerid = pNetworkPlayer->m_playerId;
	pNetworkPlayer->m_nSkin = pSkin->modelId;
	LoadSkins();
	s_savedSkins[LowerName(pNetworkPlayer->m_Name)] = pSkin->modelId;
	SaveSkins();
	logger::info("player %s skin -> %d", to_string(pNetworkPlayer->m_playerId).c_str(), pSkin->modelId);
	GetPacketFactory().SendToAll(*pSkin, pNetworkPlayer);
}

PACKET_HANDLER(ePacketType::PROPERTY_BOUGHT, Packets::Skin::PropertyBought* pProp, CNetworkPlayer* pNetworkPlayer)
{
	pProp->senderPlayerid = pNetworkPlayer->m_playerId;
	if (std::find(g_boughtProperties.begin(), g_boughtProperties.end(), pProp->index) == g_boughtProperties.end())
		g_boughtProperties.push_back(pProp->index);
	logger::info("player %s bought property #%d", to_string(pNetworkPlayer->m_playerId).c_str(), pProp->index);
	GetPacketFactory().SendToAll(*pProp, pNetworkPlayer);
}

PACKET_HANDLER(ePacketType::PLAYER_HOLD, Packets::Skin::PlayerHold* pHold, CNetworkPlayer* pNetworkPlayer)
{
	pHold->senderPlayerid = pNetworkPlayer->m_playerId;
	GetPacketFactory().SendToAll(*pHold, pNetworkPlayer);
}
