#include "stdafx.h"
#include "CTrace.h"
#include <cstdarg>

static FILE* s_file = nullptr;
static unsigned int s_until = 0;

bool CTrace::IsArmed()
{
    return s_file != nullptr && GetTickCount() < s_until;
}

void CTrace::Arm(const char* reason, unsigned int ms)
{
    if (s_file == nullptr)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash)
            strcpy_s(slash + 1, MAX_PATH - (slash + 1 - path), "CoopAndreas_trace.log");
        fopen_s(&s_file, path, "w");
    }
    s_until = GetTickCount() + ms;
    Mark("==== %s ====", reason);
}

void CTrace::Mark(const char* fmt, ...)
{
    if (!IsArmed())
        return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    fprintf(s_file, "[%u f%u] %s\n", GetTickCount(), CTimer::m_FrameCounter, buf);
    fflush(s_file);
}
