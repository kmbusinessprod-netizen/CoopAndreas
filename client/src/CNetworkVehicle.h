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
	Packets::Vehicles::VehicleDriverUpdate m_playerDriverSnapshot{};
	CDamageManager m_oldDamageState{};

	~CNetworkVehicle();
	CNetworkVehicle(vehicleid_t vehicleid, int modelid, CVector pos, float rotation, unsigned char color1, unsigned char color2, unsigned char createdBy);
	bool CreateVehicle(vehicleid_t vehicleid, int modelid, CVector pos, float rotation, unsigned char color1, unsigned char color2);
	bool HasDriver();
	
	static CNetworkVehicle* CreateHosted(CVehicle* vehicle);

	// true if m_pVehicle still points to the same live game vehicle (not freed, not a reused slot)
	bool IsGameVehicleAlive() const;
};

