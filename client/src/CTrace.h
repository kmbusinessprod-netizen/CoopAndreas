#pragma once
// [private build] step-by-step trace written to CoopAndreas_trace.log (next to gta_sa.exe), flushed on
// every line. Only active for a few seconds after Arm(): used to find where the game freezes.
class CTrace
{
public:
    static void Arm(const char* reason, unsigned int ms = 15000);
    static void Mark(const char* fmt, ...);
    static bool IsArmed();
};
