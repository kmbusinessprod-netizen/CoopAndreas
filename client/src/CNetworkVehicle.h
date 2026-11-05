#pragma once
class CNetworkVehicle
{
private:
	CNetworkVehicle() {}
public:
	vehicleid_t m_vehicleId{};
	CVehicle* m_pVehicle = nullptr;
	int m_nVehicleRef = -1;  // pool handle of m_pVehicle when it was attached; detects a reused slot
	int m_nModelId = 0;
	char m_nPaintJob = -1;
	bool m_bSyncing = false;
	unsigned char m_nCreatedBy;
	unsigned char m_nColor1 = 0, m_nColor2 = 0;  // [private build]
	unsigned int m_nLastRecreate = 0;
	Packets::Vehicles::VehicleDriverUpdate m_playerDriverSnapshot{};
	CDamageManager m_oldDamageState{};

	~CNetworkVehicle();
	CNetworkVehicle(vehicleid_t vehicleid, int modelid, CVector pos, float rotation, unsigned char color1, unsigned char color2, unsigned char createdBy);
	bool CreateVehicle(vehicleid_t vehicleid, int modelid, CVector pos, float rotation, unsigned char color1, unsigned char color2);
	bool HasDriver();
	
	static CNetworkVehicle* CreateHosted(CVehicle* vehicle);

	// true if m_pVehicle still points to the same live game vehicle (not freed, not a reused slot)
	bool IsGameVehicleAlive() const;

	// [private build] a remote vehicle whose local copy the game deleted is created again (the owner
	// still has it, so does everybody who joins later). Returns true if a live game vehicle exists.
	bool EnsureGameVehicle(const CVector& pos);
};

