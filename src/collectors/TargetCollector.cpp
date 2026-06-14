#include "collectors/TargetCollector.h"


TargetCollector::TargetCollector(ETWNetwork& sharedEtwNet) : etwNet(sharedEtwNet) {
	cores = std::thread::hardware_concurrency();
	if (cores == 0) cores = 1;

	PDH_STATUS s = PdhOpenQuery(nullptr, 0, &query);
	if (s != ERROR_SUCCESS) {
		spdlog::error("TargetCollector: PdhOpenQuery error 0x{:X}", s);
	}

	lastTime = std::chrono::steady_clock::now();
}
TargetCollector::~TargetCollector() {
	clearRegistered();
	if (query)
		PdhCloseQuery(query);
}

// -------------------------------------------------------
// 타겟 등록 — 실행 여부 무관
// -------------------------------------------------------
void TargetCollector::registerTarget(const std::string& name) {
	// 중복확인
	auto it = std::find(registeredNames.begin(), registeredNames.end(), name);
	if (it != registeredNames.end()) {
		spdlog::warn("TargetCollector: '{}' already registered", name);
		return;
	}

	registeredNames.push_back(name);
	spdlog::info("TargetCollector: '{}' register target", name);
}

// -------------------------------------------------------
// 타겟 해제
// -------------------------------------------------------
void TargetCollector::unregisterTarget(const std::string& name) {
	// registeredNames에서 제거
	registeredNames.erase(std::remove(registeredNames.begin(), registeredNames.end(), name), registeredNames.end());

	// 실행 중이면 deactivate
	deactivateTarget(name);

	// 마지막 세션 제거
	lastSessions.erase(name);
	spdlog::info("TargetCollector: '{}' released", name);
}

void TargetCollector::clearRegistered() {
	for (auto& e : activeTargets)
	{
		releaseProcessCounters(e.main);
		for (auto& c : e.children)
			releaseProcessCounters(c);
	}

	activeTargets.clear();
	registeredNames.clear();
	lastSessions.clear();
}

// -------------------------------------------------------
// 타겟 상태 조회
// -------------------------------------------------------
TargetStatus TargetCollector::getStatus(const std::string& name) const {
	// 실행 중인지 확인
	auto it = std::find_if(activeTargets.begin(), activeTargets.end(),
		[&](const TargetEntry& e) {return e.targetName == name; });
	if (it != activeTargets.end()) return TargetStatus::RUNNING;

	// 마지막 세션 데이터 확인
	auto sit = lastSessions.find(name);
	if (sit != lastSessions.end() && sit->second.hasData()) return TargetStatus::NOT_RUNNING;

	return TargetStatus::NEVER_SEEN;
}

// -------------------------------------------------------
// 마지막 세션 업데이트
// -------------------------------------------------------
void TargetCollector::updateLastSession(const std::string& name, const SessionTargetSummary& session) {
	lastSessions[name] = session;
}

// -------------------------------------------------------
// PDH 카운터 초기화
// -------------------------------------------------------
bool TargetCollector::initProcessCounters(ProcessCounters& counters) {
	if (!query || counters.name.empty()) return false;

	auto add = [&](const std::string& path, PDH_HCOUNTER& counter, bool required = false) -> bool {
		std::string full = "\\Process(" + counters.name + ")\\" + path;
		PDH_STATUS s = PdhAddEnglishCounterA(query, full.c_str(), 0, &counter);
		if (s != ERROR_SUCCESS) {
			if (required)
			{
				spdlog::warn("TargetCollector: Essential counter error '{}' 0x{:X}", path, s);
				return false;
			}
			else {
				spdlog::warn("TargetCollector: Selective counter error '{}' 0x{:X}", path, s);
				return true;
			}
		}
		return true;
		};

	// 필수 카운터
	if (!add("% Processor Time", counters.cpu, true)) return false;
	if (!add("Working Set - Private", counters.mem, true)) return false;
	if (!add("Private Bytes", counters.privateMem, true)) return false;
	if (!add("IO Read Bytes/sec", counters.diskR, true)) return false;
	if (!add("IO Write Bytes/sec", counters.diskW, true)) return false;

	// 선택 카운터
	add("Virtual Bytes", counters.virtualMem);
	add("Thread Count", counters.threads);
	add("Page Faults/sec", counters.pageFault);
	add("Elapsed Time", counters.elapsed);

	// 기준점 수집
	PdhCollectQueryData(query);

	return true;
}

// -------------------------------------------------------
// PDH 카운터 해제
// -------------------------------------------------------
void TargetCollector::releaseProcessCounters(ProcessCounters& counters) {
	auto remove = [](PDH_HCOUNTER& c) {
		if (c) { PdhRemoveCounter(c); c = nullptr; }
		};
	remove(counters.cpu);
	remove(counters.mem);
	remove(counters.privateMem);
	remove(counters.virtualMem);
	remove(counters.diskR);
	remove(counters.diskW);
	remove(counters.threads);
	remove(counters.pageFault);
	remove(counters.elapsed);
}

// -------------------------------------------------------
// 자식 프로세스 수집
// -------------------------------------------------------
SnapshotChildProcess TargetCollector::collectChild(ProcessCounters& counters, double elapsedSec) {
	SnapshotChildProcess c;
	c.pid = counters.pid;
	c.processName = getProcessNameFromPid(counters.pid);

	c.cpuUsage = getPdhDouble(counters.cpu) / cores;
	c.memoryMB = getPdhDouble(counters.mem) / (1024.0 * 1024.0);
	c.privateMemoryMB = getPdhDouble(counters.privateMem) / (1024.0 * 1024.0);
	c.diskReadMBs = getPdhDouble(counters.diskR) / (1024.0 * 1024.0);
	c.diskWriteMBs = getPdhDouble(counters.diskW) / (1024.0 * 1024.0);
	c.threadCount = static_cast<uint32_t>(getPdhDouble(counters.threads));
	c.pageFaultRate = getPdhDouble(counters.pageFault);

	HANDLE hChild = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, counters.pid);
	if (hChild) {
		DWORD handleCount = 0;
		GetProcessHandleCount(hChild, &handleCount);
		c.handleCount = static_cast<uint32_t>(handleCount);
		c.gdiObjectCount = static_cast<uint32_t>(GetGuiResources(hChild, GR_GDIOBJECTS));

		CloseHandle(hChild);
	}

	NetMbps net = etwNet.getAndResetForTarget(counters.pid, elapsedSec);
	c.netSentMbps = net.sentMbps;
	c.netRecvMbps = net.recvMbps;
	c.connections = getConnections(counters.pid);

	return c;
}

// -------------------------------------------------------
// 단일 타겟 수집
// -------------------------------------------------------
SnapshotTarget TargetCollector::collectTarget(TargetEntry& entry, double elapsedSec) {
	SnapshotTarget s;
	s.pid = entry.main.pid;
	s.targetName = entry.targetName;
	s.exePath = getExePath(entry.main.pid);

	// 메인 프로세스 수집
	s.cpuUsage = getPdhDouble(entry.main.cpu) / cores;
	s.memoryMB = getPdhDouble(entry.main.mem) / (1024.0 * 1024.0);
	s.privateMemoryMB = getPdhDouble(entry.main.privateMem) / (1024.0 * 1024.0);
	s.virtualMemoryMB = getPdhDouble(entry.main.virtualMem) / (1024.0 * 1024.0);
	s.diskReadMBs = getPdhDouble(entry.main.diskR) / (1024.0 * 1024.0);
	s.diskWriteMBs = getPdhDouble(entry.main.diskW) / (1024.0 * 1024.0);
	s.threadCount = static_cast<uint32_t>(getPdhDouble(entry.main.threads));
	s.pageFaultRate = getPdhDouble(entry.main.pageFault);
	s.elapsedSec = getPdhDouble(entry.main.elapsed);

	HANDLE hMain = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, entry.main.pid);
	// 핸들 수 / GDI 오브젝트
	if (hMain) {
		DWORD handleCount = 0;
		GetProcessHandleCount(hMain, &handleCount);
		s.handleCount = static_cast<uint32_t>(handleCount);
		s.gdiObjectCount = static_cast<uint32_t>(GetGuiResources(hMain, GR_GDIOBJECTS));
		CloseHandle(hMain);
	}

	// ETW 네트워크
	NetMbps net = etwNet.getAndResetForTarget(entry.main.pid, elapsedSec);
	s.netSentMbps = net.sentMbps;
	s.netRecvMbps = net.recvMbps;
	s.connections = getConnections(entry.main.pid);

	// 자식 프로세스 수집
	for (auto& c : entry.children)
		s.children.push_back(collectChild(c, elapsedSec));
	return s;
}

// -------------------------------------------------------
// 실행 감지 → 수집 시작
// -------------------------------------------------------
bool TargetCollector::activateTarget(const std::string& name) {
	auto info = findPdhInstance(name, 0);
	if (!info.isValid()) {
		spdlog::warn("TargetCollector: '{}' No PDH instance ", name);
		return false;
	}

	// 이미 active인지 확인
	auto it = std::find_if(activeTargets.begin(), activeTargets.end(),
		[&](const TargetEntry& e) { return e.targetName == info.instanceName; });
	if (it != activeTargets.end()) return false;

	TargetEntry entry;
	entry.targetName = name;
	entry.main.pid = info.pid;
	entry.main.name = info.instanceName;

	if (entry.main.name.empty()) {
		spdlog::warn("TargetCollector: '{}' PDH instance is empty", name);
		return false;
	}

	if (!initProcessCounters(entry.main)) {
		spdlog::warn("TargetCollector: '{}' counter init fail", name);
		return false;
	}

	activeTargets.push_back(std::move(entry));
	spdlog::info("TargetCollector: '{}' (PID={}) collect start", name, info.pid);
	return true;
}

// -------------------------------------------------------
// 종료 감지 → 수집 중단
// -------------------------------------------------------
void TargetCollector::deactivateTarget(const std::string& name) {
	auto it = std::find_if(activeTargets.begin(), activeTargets.end(), [&](const TargetEntry& e) { return e.targetName == name; });
	if (it == activeTargets.end()) return;

	spdlog::info("TargetCollector: '{}' collect stop", it->targetName);
	releaseProcessCounters(it->main);
	for (auto& c : it->children)
		releaseProcessCounters(c);
	activeTargets.erase(it);

	pendingDeactivated.push_back(name);
}

// -------------------------------------------------------
// 자식 PID 목록
// -------------------------------------------------------
std::vector<DWORD> TargetCollector::getChildPids(DWORD pid) {
	std::vector<DWORD> result;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return result;

	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(entry);
	if (Process32First(snap, &entry)) {
		do {
			if (entry.th32ParentProcessID == pid && entry.th32ProcessID != pid)
				result.push_back(entry.th32ProcessID);
		} while (Process32Next(snap, &entry));
	}
	CloseHandle(snap);
	return result;
}

// -------------------------------------------------------
// 자식 프로세스 동기화
// -------------------------------------------------------
void TargetCollector::syncChildren(TargetEntry& entry) {
	auto currentChildPids = getChildPids(entry.main.pid);

	// 새 자식 추가
	for (DWORD cpid : currentChildPids) {
		bool exists = std::any_of(entry.children.begin(), entry.children.end(),
			[&](const ProcessCounters& c) { return c.pid == cpid; });
		if (exists) continue;


		//std::string childName = getProcessNameFromPid(cpid);
		auto info = findPdhInstance(entry.targetName, cpid);
		if (!info.isValid()) continue;
		if (info.pid != cpid) {
			spdlog::warn("syncChildren: PID Disagreement {} != {}", info.pid, cpid);
			continue;
		}

		ProcessCounters pc;
		pc.pid = cpid;

		pc.name = info.instanceName;
		if (pc.name.empty()) continue;

		if (initProcessCounters(pc)) {
			entry.children.push_back(std::move(pc));
			//spdlog::info("TargetCollector: '{}' add child PID={}", entry.targetName, cpid);
		}
	}

	// 종료된 자식 제거
	auto termination = [&](ProcessCounters& c) -> bool {
		bool alive = std::find(currentChildPids.begin(), currentChildPids.end(), c.pid) != currentChildPids.end();
		if (!alive) releaseProcessCounters(c);
		return !alive;
		};
	entry.children.erase(std::remove_if(entry.children.begin(), entry.children.end(),
		termination), entry.children.end());
}

// -------------------------------------------------------
// 전체 수집
// -------------------------------------------------------
void TargetCollector::collect() {
	if (registeredNames.empty()) return;

	auto now = std::chrono::steady_clock::now();
	double elapsedSec = std::chrono::duration<double>(now - lastTime).count();
	lastTime = now;
	if (elapsedSec <= 0.0) elapsedSec = 1.0;

	// 등록된 타겟 중 새로 실행된 것 감지 -> activate
	for (const auto& name : registeredNames) {
		bool isActive = std::any_of(activeTargets.begin(), activeTargets.end(), [&](const TargetEntry& e) {return e.targetName == name; });

		if (!isActive) {
			// 실행 중인지 확인
			auto info = findPdhInstance(name, 0);
			if (info.isValid()) {
				spdlog::info("TargetCollector: '{}' (PID={}) running detect", name, info.pid);
				activateTarget(name);
			}
		}
	}

	//// 자식 프로세스 동기화 (매 수집마다)
	//for (auto& entry : activeTargets)
	//	syncChildren(entry);
	 // 자식 동기화 — 5초마다
	if (++cleanupTick >= 5) {
		for (auto& entry : activeTargets)
			syncChildren(entry);
		cleanupTick = 0;
	}

	// PDH 수집
	if (!activeTargets.empty()) PdhCollectQueryData(query);

	snapshot.timestamp = std::chrono::system_clock::now();
	snapshot.targets.clear();

	// 종료된 프로세스 감지
	std::vector<std::string> deadTargets;
	for (auto& entry : activeTargets) {
		HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.main.pid);

		if (!hProc) {
			spdlog::info("TargetCollector: '{}' shutdown detect", entry.targetName);
			deadTargets.push_back(entry.targetName);
			continue;
		}
		CloseHandle(hProc);
		SnapshotTarget s = collectTarget(entry, elapsedSec);
		s.calcTotals();
		snapshot.targets.push_back(std::move(s));
	}

	// 종료된 프로세스 deactivate
	for (const auto& name : deadTargets)
		deactivateTarget(name);
}

// -------------------------------------------------------
// PID → PDH 인스턴스명
// -------------------------------------------------------
PdhInstanceInfo TargetCollector::findPdhInstance(
	const std::string& name, DWORD targetPid) {

	// 와일드카드로 모든 인스턴스 PID 한 번에 조회
	std::string path = "\\Process(" + name + "*)\\ID Process";

	PDH_HQUERY   tq = nullptr;
	PDH_HCOUNTER tc = nullptr;

	if (PdhOpenQuery(nullptr, 0, &tq) != ERROR_SUCCESS)
		return {};

	if (PdhAddEnglishCounterA(tq, path.c_str(), 0, &tc) != ERROR_SUCCESS) {
		PdhCloseQuery(tq);
		return {};
	}

	PdhCollectQueryData(tq);

	// 배열로 전체 인스턴스 조회
	DWORD bufSize = 0, count = 0;
	PdhGetFormattedCounterArray(tc, PDH_FMT_DOUBLE, &bufSize, &count, nullptr);

	if (count == 0 || bufSize == 0) {
		PdhCloseQuery(tq);
		return {};
	}

	std::vector<char> buf(bufSize);
	auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buf.data());

	PdhInstanceInfo result;
	PDH_STATUS s = PdhGetFormattedCounterArray(tc, PDH_FMT_DOUBLE, &bufSize, &count, items);

	if (s != ERROR_SUCCESS) return {};

	for (DWORD i = 0; i < count; ++i) {
		DWORD pdhPid = static_cast<DWORD>(items[i].FmtValue.doubleValue);

		std::string szName = items[i].szName;  // "name#3" 등

		// szName에 이미 #N이 붙어있는지 확인
		bool hasIndex = (szName.size() > name.size() &&
			szName.substr(0, name.size()) == name &&
			szName[name.size()] == '#');

		std::string instName;

		if (hasIndex)
			instName = szName;
		else	// szName이 메인 프로세스가 아니고 #N이 없을 경우 #N 추가
			instName = (i == 0) ? name : name + "#" + std::to_string(i);



		//spdlog::info("findPdhInstance: '{}' PID={}",instName, pdhPid);

		if (targetPid == 0 || pdhPid == targetPid) {
			result.instanceName = instName;
			result.pid = pdhPid;
			PdhCloseQuery(tq);
			return result;
		}
	}

	PdhCloseQuery(tq);
	spdlog::warn("findPdhInstance: '{}' (PID={}) search failed", name, targetPid);
	return {};
}
// -------------------------------------------------------
// 프로세스 이름 조회
// -------------------------------------------------------
std::string TargetCollector::getProcessNameFromPid(DWORD pid) {
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return "";

	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(entry);

	if (Process32First(snap, &entry)) {
		do {
			if (entry.th32ProcessID == pid) {
				CloseHandle(snap);
				std::string name = entry.szExeFile;
				if (name.size() > 4 && name.substr(name.size() - 4) == ".exe")
					name = name.substr(0, name.size() - 4);
				return name;
			}
		} while (Process32Next(snap, &entry));
	}
	// 실패 시 로그 추가
	spdlog::warn("getProcessNameFromPid: pid={} failed search", pid);
	CloseHandle(snap);
	return "";
}

// -------------------------------------------------------
// 실행 파일 경로
// -------------------------------------------------------
std::string TargetCollector::getExePath(DWORD pid) {
	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProc) return "";
	char path[MAX_PATH] = {};
	DWORD size = MAX_PATH;
	QueryFullProcessImageNameA(hProc, 0, path, &size);
	CloseHandle(hProc);
	return std::string(path);
}

// -------------------------------------------------------
// PDH 값 double 반환
// -------------------------------------------------------
double TargetCollector::getPdhDouble(PDH_HCOUNTER counter) {
	if (!counter) return 0.0;
	PDH_FMT_COUNTERVALUE val = {};
	PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val);
	if (status != ERROR_SUCCESS)
		return 0.0;
	if (val.CStatus != PDH_CSTATUS_VALID_DATA && val.CStatus != PDH_CSTATUS_NEW_DATA)
		return 0.0;
	if (std::isnan(val.doubleValue) || std::isinf(val.doubleValue))
		return 0.0;
	return val.doubleValue;
}

// -------------------------------------------------------
// 네트워크 연결 목록
// -------------------------------------------------------
std::vector<TargetNetConnection> TargetCollector::getConnections(DWORD pid) {
	std::vector<TargetNetConnection> result;

	// TCP
	DWORD size = 0;
	GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
	if (size > 0) {
		std::vector<BYTE> tcpBuf(size);
		auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcpBuf.data());
		if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
			for (DWORD i = 0; i < table->dwNumEntries; ++i) {
				auto& row = table->table[i];
				if (row.dwOwningPid != pid) continue;
				TargetNetConnection conn;
				conn.protocol = "TCP";
				conn.localAddr = addrToString(row.dwLocalAddr, ntohs(static_cast<u_short>(row.dwLocalPort)));
				conn.remoteAddr = addrToString(row.dwRemoteAddr, ntohs(static_cast<u_short>(row.dwRemotePort)));
				conn.state = tcpStateToString(row.dwState);
				result.push_back(std::move(conn));
			}
		}
	}

	// UDP
	size = 0;
	GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
	if (size > 0) {
		std::vector<BYTE> udpBuf(size);
		auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(udpBuf.data());
		if (GetExtendedUdpTable(table, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
			for (DWORD i = 0; i < table->dwNumEntries; ++i) {
				auto& row = table->table[i];

				if (row.dwOwningPid != pid) continue;
				// 0.0.0.0 LISTEN 제외
				// (실제 통신이 없는 단순 바인딩)
				if (row.dwLocalAddr == 0) continue;  // 0.0.0.0

				TargetNetConnection conn;
				conn.protocol = "UDP";
				conn.localAddr = addrToString(row.dwLocalAddr, ntohs(static_cast<u_short>(row.dwLocalPort)));
				conn.remoteAddr = "*:*";
				conn.state = "LISTEN";
				result.push_back(std::move(conn));
			}
		}
	}
	return result;
}

// -------------------------------------------------------
// IP:Port 문자열
// -------------------------------------------------------
std::string TargetCollector::addrToString(DWORD ip, WORD port) {
	in_addr addr;
	addr.s_addr = ip;
	char buf[INET_ADDRSTRLEN] = {};
	inet_ntop(AF_INET, &addr, buf, sizeof(buf));
	return std::string(buf) + ":" + std::to_string(port);

}

// -------------------------------------------------------
// TCP 상태 문자열
// -------------------------------------------------------
std::string TargetCollector::tcpStateToString(DWORD state) {
	switch (state) {
	case MIB_TCP_STATE_CLOSED:     return "CLOSED";
	case MIB_TCP_STATE_LISTEN:     return "LISTEN";
	case MIB_TCP_STATE_SYN_SENT:   return "SYN_SENT";
	case MIB_TCP_STATE_SYN_RCVD:   return "SYN_RCVD";
	case MIB_TCP_STATE_ESTAB:      return "ESTABLISHED";
	case MIB_TCP_STATE_FIN_WAIT1:  return "FIN_WAIT1";
	case MIB_TCP_STATE_FIN_WAIT2:  return "FIN_WAIT2";
	case MIB_TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
	case MIB_TCP_STATE_CLOSING:    return "CLOSING";
	case MIB_TCP_STATE_LAST_ACK:   return "LAST_ACK";
	case MIB_TCP_STATE_TIME_WAIT:  return "TIME_WAIT";
	case MIB_TCP_STATE_DELETE_TCB: return "DELETE_TCB";
	default:                       return "UNKNOWN";
	}
}

// -------------------------------------------------------
// 콘솔 출력
// -------------------------------------------------------
void TargetCollector::printSeparator(int width) {
	printf("%s\n", std::string(width, '-').c_str());
}

void TargetCollector::printToConsole() const {
	for (const auto& name : registeredNames) {
		TargetStatus status = getStatus(name);

		if (status == TargetStatus::RUNNING) {
			// 실행 중 -> 실시간 데이터
			auto it = std::find_if(snapshot.targets.begin(), snapshot.targets.end(),
				[&](const SnapshotTarget& s) { return s.targetName == name; });
			if (it != snapshot.targets.end())
				printRunning(*it);
		}
		else {
			// 꺼져 있거나 미수집 -> 마지막세션
			printNotRunning(name);
		}
	}
}

void TargetCollector::printRunning(const SnapshotTarget& s) const {
	printf("\n");

	// +-- Target: [name] --+  전체 68자
	std::string title = s.targetName + " (PID=" +
		std::to_string(s.pid) + ") [Running]";
	printf("+-- Target: %-53s--+\n", title.c_str());

	std::string path = s.exePath;
	if (path.size() > 58) path = path.substr(0, 55) + "...";
	printf("|  Path: %-58s|\n", path.c_str());
	printf("|  Running Time: %.0fsec (%.1fmin)%-34s|\n",
		s.elapsedSec, s.elapsedSec / 60.0, "");

	// 합계 섹션
	std::string totalTitle = "-- Total (" +
		std::to_string(s.totalProcessCount) +
		" Process)";
	printf("+%s%s+\n", totalTitle.c_str(),
		std::string(66 - totalTitle.size(), '-').c_str());

	printf("|  %-14s %8.1f%%    %-14s %8.1f MB          |\n",
		"CPU(Total)", s.totalCpuUsage,
		"MEM(Total)", s.totalMemoryMB);
	printf("|  %-14s %7.2f MB/s   %-13s %7.2f Mbps        |\n",
		"Disk(Total)", s.totalDiskMBs,
		"Net(Total)", s.totalNetMbps);
	printf("|  %-14s %8u    %-14s %8u              |\n",
		"Thread(Total)", s.totalThreadCount,
		"Handle(Total)", s.totalHandleCount);

	// 메인 프로세스
	std::string mainTitle = "-- Main Process (PID=" +
		std::to_string(s.pid) + ")";
	printf("+%s%s+\n", mainTitle.c_str(),
		std::string(66 - mainTitle.size(), '-').c_str());

	printf("|  %-14s %8.1f%%    %-14s %8.1f MB          |\n",
		"CPU", s.cpuUsage,
		"Memory", s.memoryMB);
	printf("|  %-14s %7.2f MB/s   %-13s %7.2f Mbps        |\n",
		"Disk Read", s.diskReadMBs,
		"Net Sent", s.netSentMbps);
	printf("|  %-14s %7.2f MB/s   %-13s %7.2f Mbps        |\n",
		"Disk Write", s.diskWriteMBs,
		"Net Recv", s.netRecvMbps);
	printf("|  %-14s %8u    %-14s %8u              |\n",
		"Threads", s.threadCount,
		"Handles", s.handleCount);
	printf("|  %-14s %8u    %-14s %7.1f/s             |\n",
		"GDI Objs", s.gdiObjectCount,
		"PageFault", s.pageFaultRate);

	// 메인 네트워크 연결
	if (!s.connections.empty()) {
		printf("|  -- Connections (%zu)%-46s|\n",
			s.connections.size(), "");
		for (const auto& c : s.connections) {
			std::string local = c.localAddr;
			std::string remote = c.remoteAddr;
			if (local.size() > 20) local = local.substr(0, 17) + "...";
			if (remote.size() > 19) remote = remote.substr(0, 16) + "...";
			printf("|    %-4s %-20s -> %-19s %-9s |\n",
				c.protocol.c_str(), local.c_str(),
				remote.c_str(), c.state.c_str());
		}
	}

	// 자식 프로세스
	if (!s.children.empty()) {
		std::string childTitle = "-- Child Processes (" +
			std::to_string(s.children.size()) + ")";
		printf("+%s%s+\n", childTitle.c_str(),
			std::string(66 - childTitle.size(), '-').c_str());

		printf("|  %-7s %-12s %6s %7s %8s %8s           |\n",
			"PID", "Name", "CPU%", "MemMB", "DiskMBs", "NetMbps");
		printf("|  %-64s|\n", std::string(64, '-').c_str());

		for (const auto& c : s.children) {
			std::string name = c.processName;
			if (name.size() > 12) name = name.substr(0, 9) + "...";
			printf("|  %-7u %-12s %5.1f%% %7.1f %8.2f %8.2f           |\n",
				c.pid, name.c_str(),
				c.cpuUsage, c.memoryMB,
				c.diskReadMBs + c.diskWriteMBs,
				c.netSentMbps + c.netRecvMbps);

			for (const auto& conn : c.connections) {
				std::string cl = conn.localAddr;
				std::string cr = conn.remoteAddr;
				if (cl.size() > 19) cl = cl.substr(0, 16) + "...";
				if (cr.size() > 20) cr = cr.substr(0, 17) + "...";
				printf("|    +- %-4s %-19s -> %-20s%-10s|\n",
					conn.protocol.c_str(), cl.c_str(),
					cr.c_str(), conn.state.c_str());
			}
		}
	}
	printf("+%s+\n", std::string(66, '=').c_str());
}

void TargetCollector::printNotRunning(const std::string& name) const {
	printf("\n");
	std::string title = name + " [Not Running]";
	printf("+-- Target: %-53s--+\n", title.c_str());

	auto it = lastSessions.find(name);
	if (it == lastSessions.end() || !it->second.hasData()) {
		printf("|  %-64s|\n", "No data collected");
		printf("+%s+\n", std::string(66, '=').c_str());
		return;
	}

	const auto& last = it->second;
	std::string path = last.exePath;
	if (path.size() > 58) path = path.substr(0, 55) + "...";

	// 날짜 표시 — 오늘이면 "Today", 아니면 날짜 출력
	std::string dateLabel = last.date.empty()
		? "(Last Session)"
		: "(Last Session: " + last.date + ")";

	printf("|  Path: %-58s|\n", path.c_str());
	printf("|  %-64s|\n", dateLabel.c_str());
	printf("+%s+\n", std::string(66, '=').c_str());

	printf("|  %-16s %7.1f%%  (MAX %6.1f%%)%-24s|\n",
		"CPU", last.avgCpuUsage, last.peakCpuUsage, "");
	printf("|  %-16s %7.1f MB (MAX %7.1f MB)%-18s|\n",
		"Memory", last.avgMemoryMB, last.peakMemoryMB, "");
	printf("|  %-16s %7.2f    (MAX %7.2f Mbps)%-15s|\n",
		"Network Mbps", last.avgNetMbps, last.peakNetMbps, "");
	printf("|  %-16s %8u%-38s|\n",
		"Peak Threads", last.peakThreadCount, "");
	printf("|  %-16s %8u%-38s|\n",
		"Peak Handles", last.peakHandleCount, "");
	printf("|  %-16s %8.0fsec (%.1fmin)%-28s|\n",
		"Total Run Time", last.totalRuntime,
		last.totalRuntime / 60.0, "");
	printf("+%s+\n", std::string(66, '=').c_str());
}
