#include "collectors/SystemCollector.h"
#pragma comment(lib, "pdh.lib")


SystemCollector::SystemCollector() {
	PdhOpenQuery(NULL, 0, &queryMiddle);
	PdhOpenQuery(NULL, 0, &querySlow);

	ULONGLONG bootTime = GetTickCount64();
	DWORD delay = 1000 - (bootTime % 1000);
	Sleep(delay);
	
	PdhCollectQueryData(queryMiddle);
	PdhCollectQueryData(querySlow);
	
	cpu.init(queryMiddle);
	mem.init(queryMiddle);
	net.init(queryMiddle);
	disk.init(querySlow);

	//초기 수집
	PdhCollectQueryData(queryMiddle);
	PdhCollectQueryData(querySlow);
	Sleep(1000);
}
SystemCollector::~SystemCollector() {
	PdhCloseQuery(queryMiddle);
	PdhCloseQuery(querySlow);
}
void SystemCollector::collectMiddle() {
	PdhCollectQueryData(queryMiddle);
	mem.collectMemInfo();
}
void SystemCollector::collectSlow() {
	PdhCollectQueryData(querySlow);
}
nlohmann::json SystemCollector::snapshot() {
	nlohmann::json snapshot;
	snapshot["Timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
	snapshot["CPU"] = {
		{"Total", cpu.getTotalUsage()},
		{"FredGHZ", cpu.getCpuFredGHz()},
		{"QueueLength", cpu.getCpuQueueLength()},
		{"User", cpu.getCpuUser()},
		{"Kernel", cpu.getCpuKernel()}
	};
	snapshot["MEM"] = {
		{"TotalMB", mem.getMemTotalMB()},
		{"AvailMB", mem.getAvailMemMB()},
		{"UsedPercent", mem.getMemUsagedPercent()},
		{"UsedMB", mem.getMemUsedMB()},

		{"commitPercent", mem.getCommitMemPercent()},
		{"committedGB", mem.getCommittedMemGB()},
		{"commitLimitGB", mem.getCommitLimitGB()}
	};
	snapshot["NET"] = {
		{"netSentKB", net.getSentKB()},
		{"netRecvKB", net.getRecvKB()}
	};
	snapshot["DISK"] = {
		{"diskReadKB", disk.getReadKB()},
		{"diskWriteKB", disk.getWriteKB()}
	};

	return snapshot;
}

