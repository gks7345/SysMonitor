#include "collectors/ProcessCollector.h"
#include <spdlog/spdlog.h>
#pragma comment(lib, "pdh.lib")

ProcessCollector::ProcessCollector() : currentCritrion(SortCriterion::CPU) {
	PdhOpenQuery(NULL, 0, &queryMiddle);
	PdhOpenQuery(NULL, 0, &querySlow);

	ULONGLONG bootTime = GetTickCount64();
	DWORD delay = 1000 - (bootTime % 1000);
	Sleep(delay);

	PdhCollectQueryData(queryMiddle);
	PdhCollectQueryData(querySlow);


	initInfo(queryMiddle);
	

	//초기 수집
	PdhCollectQueryData(queryMiddle);
	//PdhCollectQueryData(querySlow);
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
		&procID
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
}

void ProcessCollector::collectProc() {
	procList.clear();
	PdhCollectQueryData(queryMiddle);

	DWORD pidBufferSize = 0, pidItemCount = 0;
	DWORD cpuBufferSize = 0, cpuItemCount = 0;
	DWORD memBufferSize = 0, memItemCount = 0;
	DWORD priMemBufferSize = 0, priMemItemCount = 0;

	//PID
	PdhGetFormattedCounterArray(procID, PDH_FMT_DOUBLE, &pidBufferSize, &pidItemCount, NULL);
	std::vector<char> pidbuffer;
	if (pidItemCount > 0) {
		pidbuffer.resize(pidBufferSize);
		PdhGetFormattedCounterArray(
			procID, 
			PDH_FMT_DOUBLE, 
			&pidBufferSize, 
			&pidItemCount, 
			(PDH_FMT_COUNTERVALUE_ITEM*)&pidbuffer[0]);
	}

	//CPU
	PdhGetFormattedCounterArray(procCpuUsase, PDH_FMT_DOUBLE, &cpuBufferSize, &cpuItemCount, NULL);
	std::vector<char> cpubuffer;
	if (cpuItemCount > 0) {
		DWORD marginSize = sizeof(PDH_FMT_COUNTERVALUE_ITEM) * 10;
		cpuBufferSize += marginSize;

		cpubuffer.resize(cpuBufferSize);
		PdhGetFormattedCounterArray(
			procCpuUsase, 
			PDH_FMT_DOUBLE, 
			&cpuBufferSize, 
			&cpuItemCount, 
			(PDH_FMT_COUNTERVALUE_ITEM*)&cpubuffer[0]);
	}

	//MEM
	PdhGetFormattedCounterArray(procMem, PDH_FMT_DOUBLE, &memBufferSize, &memItemCount, NULL);
	std::vector<char> membuffer;
	if (memItemCount > 0) {
		membuffer.resize(memBufferSize);

		PdhGetFormattedCounterArray(
			procMem, 
			PDH_FMT_DOUBLE, 
			&memBufferSize, 
			&memItemCount, 
			(PDH_FMT_COUNTERVALUE_ITEM*) &membuffer[0]);
	}

	//Private MEM
	PdhGetFormattedCounterArray(procPrivateMem, PDH_FMT_DOUBLE, &priMemBufferSize, &priMemItemCount, NULL);
	std::vector<char> primembuffer;
	if (priMemItemCount > 0) {
		primembuffer.resize(priMemBufferSize);

		PdhGetFormattedCounterArray(
			procMem,
			PDH_FMT_DOUBLE,
			&priMemBufferSize,
			&priMemItemCount,
			(PDH_FMT_COUNTERVALUE_ITEM*)&primembuffer[0]);
	}

	DWORD safeCount = (std::min)(pidItemCount, (std::min)(cpuItemCount, memItemCount));

	auto* pidItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(pidbuffer.data());
	auto* cpuItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(cpubuffer.data());
	auto* memItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(membuffer.data());
	auto* privateMemItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(primembuffer.data());

	
	for (DWORD i = 0; i < safeCount; ++i) {
		std::string name = pidItems[i].szName;
		if (name == "_Total" || name == "Idle") continue;
		if (name.find("#") != std::string::npos) continue;
		auto proc_ = std::make_unique<ProcList>();
		proc_->setProcID(static_cast<uint32_t>(pidItems[i].FmtValue.doubleValue));
		proc_->setProcName(name);

		procList.push_back(std::move(proc_));
	}
	

	for (DWORD i = 0; i < safeCount; ++i) {
		std::string name = pidItems[i].szName;

		size_t hashPos = name.find("#");
		if (hashPos != std::string::npos) {
			name = name.substr(0, hashPos);
		}

		ProcList* proc = findProc(name);

		if (proc != nullptr) {
			auto cpu_ = proc->getProcCpuUsage() + cpuItems[i].FmtValue.doubleValue;
			auto mem_ = proc->getProcMemoryMB() + memItems[i].FmtValue.doubleValue / (1024.0 * 1024.0);
			auto pri_mem_ = proc->getProcPrivateMemMB() + privateMemItems[i].FmtValue.doubleValue / (1024.0 * 1024.0);

			proc->setProcCpuUsage(cpu_);
			proc->setProcMemoryMB(mem_);
			proc->setProcPrivateMemoryMB(pri_mem_);
		}
		
	}
}

void ProcessCollector::setCritrion(SortCriterion setCritrion) {
	currentCritrion = setCritrion;
}

void ProcessCollector::sortProc() {
	topN = (std::min)(procList.size(), static_cast<size_t>(10));
	if (currentCritrion == SortCriterion::CPU) {
		std::nth_element(procList.begin(), procList.begin() + topN, procList.end(),
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcCpuUsage() > b->getProcCpuUsage(); });

		std::sort(procList.begin(), procList.begin() + topN,
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcCpuUsage() > b->getProcCpuUsage(); });
	}
	if (currentCritrion == SortCriterion::MEMORY) {
		std::nth_element(procList.begin(), procList.begin() + topN, procList.end(),
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcMemoryMB() > b->getProcMemoryMB(); });

		std::sort(procList.begin(), procList.begin() + topN,
			[](const std::unique_ptr<ProcList>& a, const std::unique_ptr<ProcList>& b) {return a->getProcMemoryMB() > b->getProcMemoryMB(); });
	}
}

ProcList* ProcessCollector::findProc(std::string name) {
	for (auto& elem : procList) {
		if (elem->getProcName() == name) {
			return elem.get();
		}
	}
	return nullptr;
}

nlohmann::json ProcessCollector::snapshot() {
	nlohmann::json snapshot;
	snapshot["Timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();

	snapshot["proc"] = nlohmann::json::array();
	for (size_t i = 0; i < topN; ++i) {
		nlohmann::json procAttr;

		procAttr["PID"] = procList[i]->getProcID();
		procAttr["NAME"] = procList[i]->getProcName();
		procAttr["CPU"] = procList[i]->getProcCpuUsage();
		procAttr["MEM"] = procList[i]->getProcMemoryMB();
		procAttr["PriMEM"] = procList[i]->getProcPrivateMemMB();

		snapshot["proc"].push_back(procAttr);
	}
	return snapshot;
}