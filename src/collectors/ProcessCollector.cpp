#include "collectors/ProcessCollector.h"
#include <spdlog/spdlog.h>
#pragma comment(lib, "pdh.lib")

static const char* SYSTEM_NAMES[] = {
	"_Total", "Idle", "System", "smss", "csrss",
	"wininit", "winlogon", "services", "lsass",
	"fontdrvhost", "dwm", "Memory Compression", nullptr
};

bool ProcessCollector::isSystemProcess(const std::string& name) const {
	for (int i = 0; SYSTEM_NAMES[i]; ++i)
		if (name == SYSTEM_NAMES[i]) return true;
	return false;
}


ProcessCollector::ProcessCollector(int topN) : currentCritrion(SortCriterion::CPU), topN(topN) {
	PDH_STATUS statusMiddle = PdhOpenQuery(NULL, 0, &queryMiddle);
	if (statusMiddle != ERROR_SUCCESS) {
		spdlog::error("ProcessCollector: PdhOpenQuery(middle) Error 0x{:X}", statusMiddle);
	}

	PDH_STATUS statusSlow = PdhOpenQuery(NULL, 0, &querySlow);
	if (statusSlow != ERROR_SUCCESS) {
		spdlog::error("ProcessCollector: PdhOpenQuery(slow) Error 0x{:X}", statusSlow);
	}

	initInfo(queryMiddle);


	//초기 수집
	PdhCollectQueryData(queryMiddle);

	/*if (!etwDisk.start()) {
		spdlog::warn("ETWDisk Start Error");
	}*/
	//Sleep(3000);

	if (!etwNet.start()) {
		spdlog::warn("ETWNetwork Start Error");
	}

	lastTime = std::chrono::steady_clock::now();

	Sleep(1000);
}
ProcessCollector::~ProcessCollector() {
	PdhCloseQuery(queryMiddle);
	PdhCloseQuery(querySlow);
}

void ProcessCollector::initInfo(PDH_HQUERY& query) {
	PdhAddEnglishCounter(	//PID
		queryMiddle,
		"\\Process(*)\\ID Process",
		0,
		&procIDMiddle
	);
	PdhAddEnglishCounter(	//CPU 사용량(%)
		queryMiddle,
		"\\Process(*)\\% Processor Time",
		0,
		&procCpuUsase
	);
	PdhAddEnglishCounter(	//실질적인 MEM
		queryMiddle,
		"\\Process(*)\\Working Set - Private",
		0,
		&procMem
	);

	PdhAddEnglishCounter(	//Private MEM 메모리 누수 감지용
		queryMiddle,
		"\\Process(*)\\Private Bytes",
		0,
		&procPrivateMem
	);


	// ----------------------------------------
	PdhAddEnglishCounter(queryMiddle,
		"\\Process(*)\\IO Read Bytes/sec", 0, &procDiskR);
	PdhAddEnglishCounter(queryMiddle,
		"\\Process(*)\\IO Write Bytes/sec", 0, &procDiskW);
	// ----------------------------------------
}

std::vector<char> ProcessCollector::getCountArray(PDH_HCOUNTER hCounter, DWORD& count) {
	DWORD bufferSize = 0;
	PdhGetFormattedCounterArray(hCounter, PDH_FMT_DOUBLE, &bufferSize, &count, NULL);

	std::vector<char> buffer;
	if (*&count > 0) {
		buffer.resize(bufferSize);
		PdhGetFormattedCounterArray(
			hCounter,
			PDH_FMT_DOUBLE,
			&bufferSize,
			&count,
			reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buffer.data()));
	}
	return buffer;
}

// ---------------------------------------------------------------------------
// 1초 주기 수집
// ---------------------------------------------------------------------------
void ProcessCollector::collectProc() {
	auto now = std::chrono::steady_clock::now();
	double elapsedSec = std::chrono::duration <double>(
		now - lastTime).count();
	lastTime = now;
	if (elapsedSec <= 0.0) {
		elapsedSec = 1.0;
	}


	procList.clear();
	aggregatedProcList.clear();
	PdhCollectQueryData(queryMiddle);

	DWORD pidItemCount = 0;
	DWORD cpuItemCount = 0;
	DWORD memItemCount = 0, priMemItemCount = 0;
	DWORD diskRCounter = 0, diskWCounter = 0;

	std::vector<char> pidbuffer = getCountArray(procIDMiddle, pidItemCount);
	std::vector<char> cpubuffer = getCountArray(procCpuUsase, cpuItemCount);
	
	std::vector<char> membuffer = getCountArray(procMem, memItemCount);
	std::vector<char> primembuffer = getCountArray(procPrivateMem, priMemItemCount);
	
	std::vector<char> diskRbuffer = getCountArray(procDiskR, diskRCounter);
	std::vector<char> diskWbuffer = getCountArray(procDiskW, diskWCounter);


	auto* pidItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(pidbuffer.data());
	auto* cpuItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(cpubuffer.data());
	
	auto* memItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(membuffer.data());
	auto* privateMemItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(primembuffer.data());

	auto* diskRItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(diskRbuffer.data());
	auto* diskWItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(diskWbuffer.data());
	
	DWORD safeCount = (std::min)({ pidItemCount , cpuItemCount , memItemCount , priMemItemCount, diskRCounter, diskWCounter });

	unsigned int cores = std::thread::hardware_concurrency();
	if (cores == 0) cores = 1;

	for (DWORD i = 0; i < safeCount; ++i) {
		std::string name = pidItems[i].szName;
		//if (name == "_Total" || name == "Idle") continue;
		if (isSystemProcess(name)) continue;

		auto proc_ = std::make_unique<ProcList>();

		proc_->setProcID(static_cast<uint32_t>(pidItems[i].FmtValue.doubleValue));
		proc_->setProcName(name);

		proc_->setProcCpuUsage(cpuItems[i].FmtValue.doubleValue / cores);
		proc_->setProcMemoryMB(memItems[i].FmtValue.doubleValue / (1024.0 * 1024.0));
		proc_->setProcPrivateMemoryMB(privateMemItems[i].FmtValue.doubleValue / (1024.0 * 1024.0));

		proc_->setDiskReadMBs(diskRItems[i].FmtValue.doubleValue / (1024.0 * 1024.0));
		proc_->setDiskWriteMBs(diskWItems[i].FmtValue.doubleValue / (1024.0 * 1024.0));

		aggregatedProcList.push_back(std::move(proc_));
		
	}

	for (auto& proc : aggregatedProcList) {
		DWORD pid = proc->getProcID();
		NetMbps net = etwNet.getAndReset(pid, elapsedSec);
		proc->setNetSentMbps(net.sentMbps);
		proc->setNetRecvMbps(net.recvMbps);
	}
}

// ---------------------------------------------------------------------------
// 자식 프로세스 값 -> 메인 프로세스에 합산
// ---------------------------------------------------------------------------
void ProcessCollector::aggregateToParents() {
	auto tree = buildProcTree();

	// PID -> ProcList
	std::unordered_map<DWORD, ProcList*> pidMap;
	for (auto& p : aggregatedProcList) {
		pidMap[p->getProcID()] = p.get();
	}

	// 자식 -> 부모 합산
	std::unordered_set<DWORD> toRemove; // 합산 후 제거할 자식 PID

	for (const auto& [pid, entry] : tree) {
		DWORD ppid = entry.ppid;
		
		// 부모와 자식 둘 다 aggreatedProcList에 있어야 합산
		auto childIt = pidMap.find(pid);
		auto parentIt = pidMap.find(ppid);
		if (childIt == pidMap.end()) continue;
		if (parentIt == pidMap.end()) continue;

		ProcList* child = childIt->second;
		ProcList* parent = parentIt->second;

		// 이름이 같은 경우만 합산
		if (child->getProcName() != parent->getProcName()) continue;
		if (pid == ppid) continue;	// 자기 자신 방지


		parent->setProcCpuUsage(parent->getProcCpuUsage() + child->getProcCpuUsage());
		
		parent->setProcMemoryMB(parent->getProcMemoryMB() + child->getProcMemoryMB());
		parent->setProcPrivateMemoryMB(parent->getProcPrivateMemMB() + child->getProcPrivateMemMB());

		parent->setDiskReadMBs(parent->getDiskReadMBs() + child->getDiskReadMBs());
		parent->setDiskWriteMBs(parent->getDiskWriteMBs() + child->getDiskWriteMBs());
		
		parent->setNetRecvMbps(parent->getNetRecvMbps() + child->getNetRecvMbps());
		parent->setNetSentMbps(parent->getNetSentMbps() + child->getNetSentMbps());

		toRemove.insert(pid);
	}

	// 자식 제거
	aggregatedProcList.erase(
		std::remove_if(aggregatedProcList.begin(), aggregatedProcList.end(),
			[&](const std::unique_ptr<ProcList>& p) {
				return toRemove.count(p->getProcID()) > 0;
			}),
		aggregatedProcList.end());
}

std::unordered_map<DWORD, ProcEntry> ProcessCollector::buildProcTree() {
	std::unordered_map<DWORD, ProcEntry> tree;

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return tree;

	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(entry);

	if (Process32First(snap, &entry)) {
		do {
			ProcEntry pe;
			pe.pid = entry.th32ProcessID;
			pe.ppid = entry.th32ParentProcessID;
			pe.name = entry.szExeFile;
			tree[pe.pid] = pe;
		} while (Process32Next(snap, &entry));
	}
	CloseHandle(snap);
	return tree;
}


void ProcessCollector::setCritrion(SortCriterion setCritrion) {
	currentCritrion = setCritrion;
}

void ProcessCollector::sortProc() {
	topNSampleSize = (std::min)(aggregatedProcList.size(), static_cast<size_t>(topN));

	if (currentCritrion == SortCriterion::CPU) {
		std::nth_element(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize, aggregatedProcList.end(),
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcCpuUsage() > b->getProcCpuUsage(); });

		std::sort(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize,
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcCpuUsage() > b->getProcCpuUsage(); });
		return;
	}
	
	if (currentCritrion == SortCriterion::MEMORY) {
		std::nth_element(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize, aggregatedProcList.end(),
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcMemoryMB() > b->getProcMemoryMB(); });

		std::sort(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize,
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcMemoryMB() > b->getProcMemoryMB(); });
		return;
	}
	
	if (currentCritrion == SortCriterion::DiSK) {
		std::nth_element(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize, aggregatedProcList.end(),
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {
				return a->getDiskReadMBs() + a->getDiskWriteMBs() > b->getDiskReadMBs() + b->getDiskWriteMBs(); });

		std::sort(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize,
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {
				return a->getDiskReadMBs() + a->getDiskWriteMBs() > b->getDiskReadMBs() + b->getDiskWriteMBs(); });
		return;
	}

	if (currentCritrion == SortCriterion::NET) {
		std::nth_element(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize, aggregatedProcList.end(),
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {
				return a->getNetSentMbps() + a->getNetRecvMbps() > b->getNetSentMbps() + b->getNetRecvMbps(); });

		std::sort(aggregatedProcList.begin(), aggregatedProcList.begin() + topNSampleSize,
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {
				return a->getNetSentMbps() + a->getNetRecvMbps() > b->getNetSentMbps() + b->getNetRecvMbps(); });
		return;
	}
}

ProcList* ProcessCollector::findProcByName(std::string& name, std::vector<std::unique_ptr<ProcList>>& procs) const {
	for (auto& elem : procs) {
		if (elem->getProcName() == name) {
			return elem.get();
		}
	}
	return nullptr;
}

ProcList* ProcessCollector::findProcByPid(DWORD pid, std::vector<std::unique_ptr<ProcList>>& procs) const {
	for (auto& elem : procs) {
		if (elem->getProcID() == pid) {
			return elem.get();
		}
	}
	return nullptr;
}

SnapShotProcData ProcessCollector::makeSnapShot() {
	//size_t count = aggregatedProcList.size();
	size_t count = (std::min)(topNSampleSize, aggregatedProcList.size());

	SnapShotProcData snapShot;
	snapShot.timestamp = std::chrono::system_clock::now();
	std::string sortKey;

	switch (currentCritrion) {
	case SortCriterion::CPU:
		sortKey = "CPU";
		break;
	case SortCriterion::MEMORY:
		sortKey = "MEMORY";
		break;
	case SortCriterion::DiSK:
		sortKey = "DISK";
		break;
	case SortCriterion::NET:
		sortKey = "NET";
		break;
	}

	snapShot.topN = topN;
	snapShot.sortCriterion = sortKey;

	for (size_t i = 0; i < count; ++i) {
		const auto& proc = aggregatedProcList[i];
		SnapShotProc sp;

		sp.procID = proc->getProcID();
		sp.procName = proc->getProcName();

		sp.procCpuUsage = proc->getProcCpuUsage();

		sp.procMemoryMB = proc->getProcMemoryMB();
		sp.procPrivateMemoryMB = proc->getProcPrivateMemMB();

		sp.procDiskReadMBs = proc->getDiskReadMBs();
		sp.procDiskWriteMBs = proc->getDiskWriteMBs();

		sp.procNetRecvMbps = proc->getNetRecvMbps();
		sp.procNetSentMbps = proc->getNetSentMbps();

		snapShot.procs.push_back(sp);
	}
	return snapShot;
}

void ProcessCollector::printToConsole() const {
	const char* criterionName[] = {
		"CPU", "MEMORY", "DISK", "NET",
	};
	printf("\n=== Process Top %d [%s] ===\n",
		topN, criterionName[static_cast<int>(currentCritrion)]);
	printf("%-8s %-22s %7s %9s %9s %9s\n",
		"PID", "Name", 
		"CPU%",
		"Mem MB",
		"Disk MB/s",
		"NetMbps");
	printf("%s\n", std::string(80, '-').c_str());

	size_t printCount = (std::min)(aggregatedProcList.size(), static_cast<size_t>(topN));
	
	for (size_t i = 0; i < printCount; ++i) {
		const auto& p = aggregatedProcList[i];

		std::string sname = p->getProcName();
		double disk = p->getDiskReadMBs() + p->getDiskWriteMBs();
		double net = p->getNetRecvMbps() + p->getNetSentMbps();
		
		if (sname.size() > 21) sname = sname.substr(0, 18) + "...";
		
		printf("%-8u %-22s %6.1f%% %8.1fMB %8.2f %8.2f\n",
			p->getProcID(),
			sname.c_str(),
			p->getProcCpuUsage(),
			p->getProcMemoryMB(),
			disk,
			net);
	}
}