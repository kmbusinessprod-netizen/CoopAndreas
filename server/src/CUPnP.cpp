#include "stdafx.h"
#include "CUPnP.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <natupnp.h>
#include <oleauto.h>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ws2_32.lib")

static unsigned short s_port = 0;
static std::atomic<bool> s_mapped = false;

static std::string GetLocalIPv4()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::string result;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s != INVALID_SOCKET)
    {
        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        inet_pton(AF_INET, "1.1.1.1", &remote.sin_addr);
        // UDP connect sends nothing, it only picks the outgoing interface
        if (connect(s, (sockaddr*)&remote, sizeof(remote)) == 0)
        {
            sockaddr_in local{};
            int len = sizeof(local);
            if (getsockname(s, (sockaddr*)&local, &len) == 0)
            {
                char buf[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
                result = buf;
            }
        }
        closesocket(s);
    }
    WSACleanup();
    return result;
}

static bool IsPrivateOrCgnat(const std::string& ip)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return true;
    if (a == 10 || a == 127 || a == 0)
        return true;
    if (a == 192 && b == 168)
        return true;
    if (a == 172 && b >= 16 && b <= 31)
        return true;
    if (a == 100 && b >= 64 && b <= 127)  // carrier-grade NAT
        return true;
    if (a == 169 && b == 254)
        return true;
    return false;
}

static std::string FromBSTR(BSTR b)
{
    if (!b)
        return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, b, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    if (len > 1)
        WideCharToMultiByte(CP_UTF8, 0, b, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static IStaticPortMappingCollection* GetCollection(IUPnPNAT* nat)
{
    // the router is discovered in the background; the collection can be null for a few seconds
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        IStaticPortMappingCollection* coll = nullptr;
        if (SUCCEEDED(nat->get_StaticPortMappingCollection(&coll)) && coll)
            return coll;
        Sleep(1500);
    }
    return nullptr;
}

static void PrintManualHelp(const std::string& localIp)
{
    printf("[UPnP] : Tus amigos no podran conectarse directo hasta que el puerto %u UDP este abierto.\n", s_port);
    printf("[UPnP] : Opciones: activar UPnP en el router, abrir el puerto %u UDP a mano hacia %s,\n", s_port,
        localIp.empty() ? "esta PC" : localIp.c_str());
    printf("[UPnP] : o usar un tunel (playit.gg) / Radmin VPN.\n");
    printf("[UPnP] : Para probar en esta misma PC usa la IP 127.0.0.1\n\n");
}

static void Worker()
{
    std::string localIp = GetLocalIPv4();

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
        printf("[UPnP] : No se pudo iniciar COM.\n");
        return;
    }

    printf("[UPnP] : Buscando el router para abrir el puerto %u UDP automaticamente...\n", s_port);

    IUPnPNAT* nat = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(UPnPNAT), nullptr, CLSCTX_ALL, __uuidof(IUPnPNAT), (void**)&nat);
    if (FAILED(hr) || !nat)
    {
        printf("[UPnP] : UPnP no esta disponible en este Windows.\n");
        PrintManualHelp(localIp);
        CoUninitialize();
        return;
    }

    IStaticPortMappingCollection* coll = GetCollection(nat);
    if (!coll)
    {
        printf("[UPnP] : El router no respondio a UPnP (puede estar desactivado en el router).\n");
        PrintManualHelp(localIp);
        nat->Release();
        CoUninitialize();
        return;
    }

    if (localIp.empty())
    {
        printf("[UPnP] : No pude averiguar la IP local de esta PC.\n");
        PrintManualHelp(localIp);
        coll->Release();
        nat->Release();
        CoUninitialize();
        return;
    }

    BSTR proto = SysAllocString(L"UDP");
    BSTR desc = SysAllocString(L"CoopAndreas Server");
    std::wstring wLocal(localIp.begin(), localIp.end());
    BSTR client = SysAllocString(wLocal.c_str());

    // remove a stale mapping from a previous run, then create ours
    coll->Remove(s_port, proto);

    IStaticPortMapping* mapping = nullptr;
    hr = coll->Add(s_port, proto, s_port, client, VARIANT_TRUE, desc, &mapping);

    if (SUCCEEDED(hr) && mapping)
    {
        s_mapped = true;

        BSTR ext = nullptr;
        std::string externalIp;
        if (SUCCEEDED(mapping->get_ExternalIPAddress(&ext)))
            externalIp = FromBSTR(ext);
        if (ext)
            SysFreeString(ext);

        printf("[UPnP] : Puerto %u UDP abierto en el router -> %s\n", s_port, localIp.c_str());

        if (externalIp.empty())
        {
            printf("[UPnP] : No pude leer tu IP publica; buscala en una pagina como cualesmiip.com\n");
        }
        else if (IsPrivateOrCgnat(externalIp))
        {
            printf("[UPnP] : ATENCION: el router tiene la IP %s, que NO es publica.\n", externalIp.c_str());
            printf("[UPnP] : Tu proveedor usa CGNAT o hay otro router delante: tus amigos no podran entrar\n");
            printf("[UPnP] : directo. Usa un tunel (playit.gg) o Radmin VPN.\n\n");
        }
        else
        {
            printf("\n");
            printf("  ==========================================================\n");
            printf("   Pasale a tus amigos:   IP %s   Puerto %u\n", externalIp.c_str(), s_port);
            printf("   (tu, en esta PC, conectate a 127.0.0.1)\n");
            printf("  ==========================================================\n\n");
        }
        mapping->Release();
    }
    else
    {
        printf("[UPnP] : El router rechazo abrir el puerto (error 0x%08lX).\n", (unsigned long)hr);
        PrintManualHelp(localIp);
    }

    SysFreeString(proto);
    SysFreeString(desc);
    SysFreeString(client);
    coll->Release();
    nat->Release();
    CoUninitialize();
}

static BOOL WINAPI ConsoleCtrlHandler(DWORD type)
{
    if (type == CTRL_CLOSE_EVENT || type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_LOGOFF_EVENT ||
        type == CTRL_SHUTDOWN_EVENT)
    {
        CUPnP::Cleanup();
    }
    return FALSE;  // let the default handler terminate the process
}

void CUPnP::StartAsync(unsigned short port)
{
    s_port = port;
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    std::thread(Worker).detach();
}

void CUPnP::Cleanup()
{
    if (!s_mapped.exchange(false))
        return;

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return;

    IUPnPNAT* nat = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(UPnPNAT), nullptr, CLSCTX_ALL, __uuidof(IUPnPNAT), (void**)&nat)) && nat)
    {
        IStaticPortMappingCollection* coll = nullptr;
        if (SUCCEEDED(nat->get_StaticPortMappingCollection(&coll)) && coll)
        {
            BSTR proto = SysAllocString(L"UDP");
            coll->Remove(s_port, proto);
            SysFreeString(proto);
            coll->Release();
        }
        nat->Release();
    }
    CoUninitialize();
}

#else
void CUPnP::StartAsync(unsigned short) {}
void CUPnP::Cleanup() {}
#endif
