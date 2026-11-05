#pragma once
// [private build] properties (safehouses, businesses) are shared: when one player buys one, everybody gets it.
// The script keeps the state in its globals; we watch them and mirror the purchase on the other machines.
class CPropertySync
{
public:
    static constexpr int COUNT = 32;

    static void Process();   // every few frames, host and clients: detect local purchases
    static void OnPacket(Packets::Skin::PropertyBought* packet);
    static void Reset();     // on connect/disconnect

private:
    static void MirrorPurchase(int index);
    static uint8_t ms_known[COUNT];
};
