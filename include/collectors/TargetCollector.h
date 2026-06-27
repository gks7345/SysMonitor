#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pdh.h>
#include <PdhMsg.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <thread>
#include <algorithm>
#include <spdlog/spdlog.h>
#include "collectors/ETWNetwork.h"
#include "models/SnapshotData.h"
#include "models/SessionSummary.h"


#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// 반환: {인스턴스명, PID} 쌍, 실패 시 {"", 0}
struct PdhInstanceInfo {
	std::string instanceName;
	DWORD       pid = 0;
	bool isValid() const { return !instanceName.empty() && pid != 0; }
};

// -------------------------------------------------------
// 단일 프로세스별 PDH 카운터 묶음
// -------------------------------------------------------
struct ProcessCounters {
	DWORD pid = 0;
	std::string name;	// PDH 인스턴스명
	bool         isChild = false;

	// 메인은 공유 query 사용
	// 자식은 개별 childQuery 사용
	PDH_HQUERY childQuery = nullptr;

	PDH_HCOUNTER cpu = nullptr;
	PDH_HCOUNTER mem = nullptr;
	PDH_HCOUNTER privateMem = nullptr;
	PDH_HCOUNTER virtualMem = nullptr;
	PDH_HCOUNTER diskR = nullptr;
	PDH_HCOUNTER diskW = nullptr;
	PDH_HCOUNTER threads = nullptr;
	PDH_HCOUNTER pageFault = nullptr;
	PDH_HCOUNTER elapsed = nullptr;		// 메인 프로세스만

	// 자식 전용 Win32 CPU delta 계산용
	ULONGLONG prevKernelTime = 0;
	ULONGLONG prevUserTime = 0;
	ULONGLONG prevMeasureTime = 0; // 100ns 단위 시스템 시간
};

struct TargetEntry {
	std::string targetName;		// 등록된 이름
	ProcessCounters main;		// 메인 프로세스
	std::vector<ProcessCounters> children;	// 자식 프로세스들
};

enum class TargetStatus {
	RUNNING,      // 실행 중 → 실시간 수집
	NOT_RUNNING,  // 꺼져 있음 → 마지막 데이터 표시
	NEVER_SEEN    // 한 번도 수집된 적 없음
};

// 타겟 프로세스
class TargetCollector {
private:
	int cleanupTick = 5;
	std::vector<TargetEntry> activeTargets;
	// 등록된 타겟 이름 목록 (실행 여부 무관)
	std::vector<std::string> registeredNames;

	// 마지막 세션 데이터 (이름 -> 마지막 세션)
	std::unordered_map<std::string, SessionTargetSummary> lastSessions;

	using ConnectionMap = std::unordered_map<DWORD, std::vector<TargetNetConnection>>;

	PDH_HQUERY query = nullptr;

	ETWNetwork& etwNet;

	std::chrono::steady_clock::time_point lastTime;

	unsigned int cores = 1;

	SnapshotTargetData snapshot;

	// 실행 중 프로세스 종료 감지 -> 안전하게 SessionSummary 전달
	std::vector<std::string> pendingDeactivated;

	// 헬퍼
	PdhInstanceInfo activateTarget(const std::string& name);	// 실행 중 감지 -> 수집 시작
	void deactivateTarget(const std::string& name);	// 종료 감지 -> 수집 중단

	bool initProcessCounters(ProcessCounters& counters);
	void releaseProcessCounters(ProcessCounters& counters);

	SnapshotTarget collectTarget(TargetEntry& entry, double elapsedSec, const ConnectionMap& connMap);
	SnapshotChildProcess collectChild(ProcessCounters& counters, double elapsedSec, const ConnectionMap& connMap);

	ConnectionMap getAllConnectionsGroupedByPid();
	std::vector<TargetNetConnection> lookupConnections(const ConnectionMap& map, DWORD pid);

	// 자식 프로세스 활성화
	void syncChildren(TargetEntry& entry);

	void printRunning(const SnapshotTarget& s) const;
	void printNotRunning(const std::string& name) const;

	static PdhInstanceInfo findChildPdhInstance(const std::string& name, DWORD targetPid);
	static PdhInstanceInfo findMainPdhInstance(const std::string& name);
	static std::vector<DWORD> getChildPids(DWORD pid);
	static std::string getExePath(DWORD pid);
	static std::string getProcessNameFromPid(DWORD pid);
	static double getPdhDouble(PDH_HCOUNTER counter);
	static std::string addrToString(DWORD ip, WORD port);
	static std::string tcpStateToString(DWORD state);
	static ULONGLONG filetimeToULL(const FILETIME& ft);

public:
	explicit TargetCollector(ETWNetwork& sharedEtwNet);
	~TargetCollector();

	// 타겟 등록/해제 (실행 중이 아니어도 등록 가능)
	void registerTarget(const std::string& name);
	void unregisterTarget(const std::string& name);
	void clearRegistered();

	// 등록된 타겟 목록 조회
	const std::vector<std::string>& getRegisteredNames() const {
		return registeredNames;
	}

	// 타겟 상태 조회
	TargetStatus getStatus(const std::string& name) const;

	// 실행 중 프로세스 종료 감지 -> 안전하게 SessionSummary 전달
	const std::vector<std::string>& getPendingDeactivated() const { return pendingDeactivated; }
	void clearPendingDeactivated() { pendingDeactivated.clear(); }


	void collect();

	// 결과 조회
	const SnapshotTargetData& getSnapshot() const { return snapshot; }
	void printToConsole() const;

	// 마지막 세션 업데이트 (DataStore에서 주입)
	void updateLastSession(const std::string& name, const SessionTargetSummary& session);
};