#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>


#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

#pragma comment(lib, "iphlpapi.lib")




struct NetAccumEntry {
    std::atomic<ULONGLONG> sentBytes{ 0 };
    std::atomic<ULONGLONG> recvBytes{ 0 };
};

struct NetMbps {
    double sentMbps = 0.0;
    double recvMbps = 0.0;
};

class ETWNetwork {
public:
    ETWNetwork();
    ~ETWNetwork();

    bool    start();
    void    stop();
    bool    isRunning() const { return running.load(); }
    NetMbps getAndReset(DWORD pid, double elapsedSec);

private:
    GUID KERNEL_NETWORK_GUID = {};

    // 이벤트 Id
    // 10 = TcpIp Send,  11 = TcpIp Recv
    // 26 = UdpIp Send,  27 = UdpIp Recv
    static bool isSendEvent(USHORT id) {
        return id == 10 || id == 26;
    }
    static bool isRecvEvent(USHORT id) {
        return id == 11 || id == 27;
    }

    static constexpr char SESSION_NAME[] = "SysPulse_NetMonitor";

    TRACEHANDLE       sessionHandle = INVALID_PROCESSTRACE_HANDLE;
    TRACEHANDLE       traceHandle = INVALID_PROCESSTRACE_HANDLE;
    std::vector<BYTE> sessionProps;

    std::thread       etwThread;
    std::atomic<bool> running{ false };

    std::mutex accumMtx;
    std::unordered_map<DWORD, NetAccumEntry*> accumulator;

    // 루프백 주소 목록 (자동 감지)
    std::unordered_set<ULONG> loopbackAddrs;

    bool startSession();
    void processLoop();

    // GUID 자동 조회
    static GUID findProviderGuid(const std::wstring& providerName);

    // 루프백 주소 자동 수집
    static std::unordered_set<ULONG> getLoopbackAddresses();

    static VOID WINAPI onEvent(PEVENT_RECORD pEvent);
    NetAccumEntry& getOrCreateEntry(DWORD pid);

    static ETWNetwork* instance;

};
