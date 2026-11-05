#include "stdafx.h"
#include "CHoldSync.h"

// CTaskSimpleHoldEntity layout (ctor 0x6913A0)
static constexpr int HOLD_ENTITY = 8, HOLD_POS = 0xC, HOLD_BONE = 0x18, HOLD_FLAGS = 0x19, HOLD_ANIM = 0x20,
                     HOLD_GROUP = 0x24;

struct RemoteHold
{
    CPlayerPed* ped;
    int objectHandle;
    Packets::Skin::PlayerHold data;
    unsigned int pendingSince;
    bool attached;
    std::vector<int> parts;
};
static std::vector<RemoteHold> s_remote;
static uint16_t s_lastLocalModel = 0;

static void SendLocal(uint16_t model, void* task)
{
    Packets::Skin::PlayerHold packet{};
    packet.modelId = model;
    if (task)
    {
        packet.offset = *(CVector*)((uintptr_t)task + HOLD_POS);
        packet.bone = *(uint8_t*)((uintptr_t)task + HOLD_BONE);
        packet.boneFlags = *(uint8_t*)((uintptr_t)task + HOLD_FLAGS);
        packet.animId = *(uint32_t*)((uintptr_t)task + HOLD_ANIM);
        packet.animGroup = *(uint32_t*)((uintptr_t)task + HOLD_GROUP);
    }
    GetPacketFactory().Send(packet);
}

static void ReleaseRemote(RemoteHold& h)
{
    for (int part : h.parts)
        if (CPools::GetObject(part))
            Command<Commands::DELETE_OBJECT>(part);
    h.parts.clear();
    if (CPoolGuard::IsPedAlive(h.ped) && h.attached)
        h.ped->DropEntityThatThisPedIsHolding(1);
    if (h.objectHandle >= 0)
    {
        if (CPools::GetObject(h.objectHandle))
            Command<Commands::DELETE_OBJECT>(h.objectHandle);
        h.objectHandle = -1;
    }
    h.attached = false;
}

static bool AttachRemote(RemoteHold& h)
{
    int model = h.data.modelId;
    if (CStreaming::ms_aInfoForModel[model].m_nLoadState != LOADSTATE_LOADED)
    {
        CStreaming::RequestModel(model, GAME_REQUIRED);
        return false;
    }
    CVector pos = h.ped->GetPosition();
    int handle = -1;
    Command<Commands::CREATE_OBJECT>(model, pos.x, pos.y, pos.z + 1.0f, &handle);
    CObject* object = CPools::GetObject(handle);
    if (object == nullptr)
        return true;  // give up
    Command<Commands::SET_OBJECT_COLLISION>(handle, false);
    h.objectHandle = handle;

    void* task = plugin::CallAndReturn<void*, 0x61A5A0, unsigned int>(0x3C);  // CTask::operator new
    if (task == nullptr)
        return true;
    CVector offset = h.data.offset;
    plugin::CallMethod<0x6913A0, void*, CEntity*, CVector*, char, unsigned char, int, int, bool>(task, object, &offset,
        (char)h.data.bone, h.data.boneFlags, (int)h.data.animId, (int)h.data.animGroup, false);
    h.ped->m_pIntelligence->m_TaskMgr.SetTaskSecondary((CTask*)task, 4 /*TASK_SECONDARY_PARTIAL_ANIM*/);
    h.attached = true;

    // the gravity gun of another player: add its parts so it looks like the real thing
    if (model == 348 && h.data.bone == 6 && h.data.boneFlags == 16)
    {
        float rot[4][3] = {{0.0f, 82.5f, 0.0f}, {0.0f, 15.0f, 0.0f}, {120.0f, 15.0f, 0.0f}, {240.0f, 15.0f, 0.0f}};
        float off[4][3] = {{0.1f, 0.025f, 0.1f}, {0.5f, 0.025f, 0.2f}, {0.5f, -0.025f, 0.125f}, {0.5f, 0.025f, 0.125f}};
        int models[4] = {2976, 2956, 2956, 2956};
        float scale[4] = {0.75f, 0.025f, 0.025f, 0.025f};
        for (int i = 0; i < 4; i++)
        {
            if (CStreaming::ms_aInfoForModel[models[i]].m_nLoadState != LOADSTATE_LOADED)
            {
                CStreaming::RequestModel(models[i], GAME_REQUIRED);
                CStreaming::LoadAllRequestedModels(false);
            }
            int part = -1;
            Command<Commands::CREATE_OBJECT>(models[i], pos.x, pos.y, pos.z + 1.0f, &part);
            if (part < 0)
                continue;
            Command<Commands::SET_OBJECT_COLLISION>(part, false);
            Command<Commands::SET_OBJECT_SCALE>(part, scale[i]);
            Command<Commands::ATTACH_OBJECT_TO_OBJECT>(part, handle, off[i][0], off[i][1], off[i][2], rot[i][0], rot[i][1], rot[i][2]);
            h.parts.push_back(part);
        }
    }
    return true;
}

void CHoldSync::HoldLocally(CObject* object, const CVector& offset, int bone, int boneFlags)
{
    CPlayerPed* me = FindPlayerPed(0);
    if (!me || !object)
        return;
    void* task = plugin::CallAndReturn<void*, 0x61A5A0, unsigned int>(0x3C);
    if (!task)
        return;
    CVector off = offset;
    plugin::CallMethod<0x6913A0, void*, CEntity*, CVector*, char, unsigned char, int, int, bool>(task, object, &off, (char)bone,
        (unsigned char)boneFlags, 0, 0, false);
    me->m_pIntelligence->m_TaskMgr.SetTaskSecondary((CTask*)task, 4);
}

void CHoldSync::Process()
{
    if (!CNetwork::m_bAuthenticated)
        return;

    // local
    if (CPlayerPed* me = FindPlayerPed(0))
    {
        void* task = me->GetHoldingTask();
        CEntity* held = task ? *(CEntity**)((uintptr_t)task + HOLD_ENTITY) : nullptr;
        uint16_t model = held ? held->m_nModelIndex : 0;
        if (model != s_lastLocalModel)
        {
            s_lastLocalModel = model;
            SendLocal(model, held ? task : nullptr);
        }
    }

    // remote: create the object once its model is loaded
    for (auto it = s_remote.begin(); it != s_remote.end();)
    {
        if (!CPoolGuard::IsPedAlive(it->ped))
        {
            it->attached = false;
            ReleaseRemote(*it);
            it = s_remote.erase(it);
            continue;
        }
        if (!it->attached && it->objectHandle < 0)
        {
            if (!AttachRemote(*it) && GetTickCount() - it->pendingSince > 8000)
            {
                it = s_remote.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void CHoldSync::OnPacket(Packets::Skin::PlayerHold* packet)
{
    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(packet->senderPlayerid);
    if (player == nullptr || player->m_pPed == nullptr)
        return;

    for (auto it = s_remote.begin(); it != s_remote.end(); ++it)
    {
        if (it->ped == player->m_pPed)
        {
            ReleaseRemote(*it);
            s_remote.erase(it);
            break;
        }
    }
    if (packet->modelId == 0)
        return;
    s_remote.push_back({player->m_pPed, -1, *packet, GetTickCount(), false, {}});
}

void CHoldSync::OnPlayerRemoved(CPlayerPed* ped)
{
    for (auto it = s_remote.begin(); it != s_remote.end(); ++it)
    {
        if (it->ped == ped)
        {
            ReleaseRemote(*it);
            s_remote.erase(it);
            return;
        }
    }
}
