#include "collectors/ETWNetwork.h"
#include <spdlog/spdlog.h>

ETWNetwork* ETWNetwork::instance = nullptr;
constexpr char ETWNetwork::SESSION_NAME[];

ETWNetwork::ETWNetwork() { instance = this; }
ETWNetwork::~ETWNetwork() {
    stop();
    std::lock_guard<std::mutex> lock(accumMtx);
    for (auto& [pid, entry] : accumulator) delete entry;
    instance = nullptr;
}

// -------------------------------------------------------
// GUID 자동 조회 — TdhEnumerateProviders
// -------------------------------------------------------
GUID ETWNetwork::findProviderGuid(const std::wstring& providerName) {
    ULONG bufSize = 0;
    TdhEnumerateProviders(nullptr, &bufSize);

    if (bufSize == 0) return GUID{};

    std::vector<BYTE> buf(bufSize);
    auto* list = reinterpret_cast<PROVIDER_ENUMERATION_INFO*>(buf.data());

    if (TdhEnumerateProviders(list, &bufSize) != ERROR_SUCCESS)
        return GUID{};

    for (ULONG i = 0; i < list->NumberOfProviders; ++i) {
        auto& entry = list->TraceProviderInfoArray[i];
        auto* name = reinterpret_cast<const wchar_t*>(
            buf.data() + entry.ProviderNameOffset);

        if (providerName == name) {
            spdlog::info("ETWNetwork: Provider '{}' GUID Search Success",
                std::string(providerName.begin(), providerName.end()));
            return entry.ProviderGuid;
        }
    }
    return GUID{};
}

// -------------------------------------------------------
// 루프백 주소 자동 수집 — GetAdaptersAddresses
// -------------------------------------------------------
std::unordered_set<ULONG> ETWNetwork::getLoopbackAddresses() {
    std::unordered_set<ULONG> result;

    // 항상 127.0.0.1 포함
    result.insert(inet_addr("127.0.0.1"));

    ULONG bufSize = 0;
    GetAdaptersAddresses(AF_INET,
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, nullptr, &bufSize);

    if (bufSize == 0) return result;

    std::vector<BYTE> buf(bufSize);
    auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());

    if (GetAdaptersAddresses(AF_INET,
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, addrs, &bufSize) != ERROR_SUCCESS)
        return result;

    for (auto* a = addrs; a; a = a->Next) {
        // 루프백 어댑터만
        if (a->IfType != IF_TYPE_SOFTWARE_LOOPBACK) continue;

        for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
            auto* sa = reinterpret_cast<sockaddr_in*>(
                ua->Address.lpSockaddr);
            if (sa->sin_family == AF_INET) {
                result.insert(sa->sin_addr.s_addr);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
                spdlog::info("ETWNetwork: Loop Back Address Detect {}", ipStr);
            }
        }
    }
    return result;
}



bool ETWNetwork::start() {
    if (running.load())  return true;

    // GUID 자동 조회
    KERNEL_NETWORK_GUID = findProviderGuid(
        L"Microsoft-Windows-Kernel-Network");

    if (KERNEL_NETWORK_GUID == GUID{}) {
        spdlog::error("ETWNetwork: Kernel-Network Provider Not Found");
        return false;
    }

    // 루프백 주소 자동 수집
    loopbackAddrs = getLoopbackAddresses();
    spdlog::info("ETWNetwork: loopbackAddrs {} Detect", loopbackAddrs.size());


    if (!startSession()) return false;
    running.store(true);
    etwThread = std::thread(&ETWNetwork::processLoop, this);
    spdlog::info("ETWNetwork: START");
    return true;
}

void ETWNetwork::stop() {
    if (!running.load()) return;
    running.store(false);

    if (traceHandle != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(traceHandle);
        traceHandle = INVALID_PROCESSTRACE_HANDLE;
    }
    if (sessionHandle != INVALID_PROCESSTRACE_HANDLE) {
        auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(sessionProps.data());
        ControlTraceA(sessionHandle, SESSION_NAME, props, EVENT_TRACE_CONTROL_STOP);
        sessionHandle = INVALID_PROCESSTRACE_HANDLE;
    }
    if (etwThread.joinable()) etwThread.join();
    spdlog::info("ETWNetwork: STOP");
}

bool ETWNetwork::startSession() {    
    // 기존 세션 정리
    {
        size_t sz = sizeof(EVENT_TRACE_PROPERTIES)
            + (strlen(SESSION_NAME) + 1);
        std::vector<BYTE> tmp(sz, 0);
        auto* p = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(tmp.data());
        p->Wnode.BufferSize = static_cast<ULONG>(sz);
        ControlTraceA(0, SESSION_NAME, p, EVENT_TRACE_CONTROL_STOP);
    }

    size_t sz = sizeof(EVENT_TRACE_PROPERTIES)
        + (strlen(SESSION_NAME) + 1);
    sessionProps.assign(sz, 0);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(sessionProps.data());

    props->Wnode.BufferSize = static_cast<ULONG>(sz);
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->Wnode.ClientContext = 1;
    props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG status = StartTraceA(&sessionHandle, SESSION_NAME, props);
    if (status != ERROR_SUCCESS && status != ERROR_ALREADY_EXISTS) {
        spdlog::error("ETWNetwork: StartTraceA Error 0x{:X}", status);
        return false;
    }

    

    ENABLE_TRACE_PARAMETERS params = {};
    params.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;

    status = EnableTraceEx2(
        sessionHandle, &KERNEL_NETWORK_GUID,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        0, 0, 0, &params);

    if (status != ERROR_SUCCESS) {
        spdlog::error("ETWNetwork: EnableTraceEx2 Error 0x{:X}", status);
        return false;
    }
    return true;
}

void ETWNetwork::processLoop() {
    EVENT_TRACE_LOGFILEA logFile = {};
    logFile.LoggerName = const_cast<LPSTR>(SESSION_NAME);
    logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME
        | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = onEvent;

    traceHandle = OpenTraceA(&logFile);
    if (traceHandle == INVALID_PROCESSTRACE_HANDLE) {
        spdlog::error("ETWNetwork: OpenTrace Error 0x{:X}", GetLastError());
        running.store(false);
        return;
    }

    spdlog::info("ETWNetwork: ProcessTrace Start");
    ULONG status = ProcessTrace(&traceHandle, 1, nullptr, nullptr);
    if (status != ERROR_SUCCESS && status != ERROR_CANCELLED)
        spdlog::warn("ETWNetwork: ProcessTrace Close 0x{:X}", status);
}

VOID WINAPI ETWNetwork::onEvent(PEVENT_RECORD pEvent) {
    if (!instance || !instance->running.load()) return;
    if (!pEvent) return;

    if (!IsEqualGUID(pEvent->EventHeader.ProviderId, instance->KERNEL_NETWORK_GUID))
        return;


    USHORT id = pEvent->EventHeader.EventDescriptor.Id;
    bool isSend;
    if (isSendEvent(id)) isSend = true;
    else if (isRecvEvent(id)) isSend = false;
    else return;

    if (!pEvent->UserData || pEvent->UserDataLength < 8) return;

    const BYTE* p = reinterpret_cast<const BYTE*>(pEvent->UserData);

    // TcpIp/UdpIp 페이로드
    // offset 0 : PID (ULONG)
    // offset 4 : size (ULONG) ← 송수신 bytes
    ULONG bytes = *reinterpret_cast<const ULONG*>(p + 4);
    if (bytes == 0) return;

    // EventHeader.ProcessId로 PID 직접 사용
    //DWORD pid = pEvent->EventHeader.ProcessId;
    DWORD pid = *reinterpret_cast<const ULONG*>(p + 0);
    if (pid == 0 || pid == 4 || pid == 0xFFFFFFFF) return;

    
    // 로컬호스트 트래픽 제외 (len >= 12이면 목적지 IP 있음)
    if (pEvent->UserDataLength >= 12) {
        ULONG daddr = *reinterpret_cast<const ULONG*>(p + 8);

        // 127.x.x.x 전체 제외 (루프백)
        //if ((daddr & 0xFF) == 127) return;
        if (instance->loopbackAddrs.count(daddr)) return;  // 루프백 제외
    }

    auto& entry = instance->getOrCreateEntry(pid);
    if (isSend)
        entry.sentBytes.fetch_add(bytes, std::memory_order_relaxed);
    else
        entry.recvBytes.fetch_add(bytes, std::memory_order_relaxed);
}

NetAccumEntry& ETWNetwork::getOrCreateEntry(DWORD pid) {
    std::lock_guard<std::mutex> lock(accumMtx);
    auto it = accumulator.find(pid);
    if (it != accumulator.end()) return *it->second;
    auto* entry = new NetAccumEntry();
    accumulator[pid] = entry;
    return *entry;
}

NetMbps ETWNetwork::getAndReset(DWORD pid, double elapsedSec) {
    NetMbps result;
    if (elapsedSec <= 0.0) return result;

    std::lock_guard<std::mutex> lock(accumMtx);
    auto it = accumulator.find(pid);
    if (it == accumulator.end()) return result;

    ULONGLONG s = it->second->sentBytes.exchange(0, std::memory_order_relaxed);
    ULONGLONG r = it->second->recvBytes.exchange(0, std::memory_order_relaxed);



    // bytes → Mbps (1 Mbps = 125,000 bytes/sec)
    constexpr double BYTES_TO_MBPS = 8.0 / 1'000'000.0;
    result.sentMbps = static_cast<double>(s) / elapsedSec * BYTES_TO_MBPS;
    result.recvMbps = static_cast<double>(r) / elapsedSec * BYTES_TO_MBPS;
    return result;
}
