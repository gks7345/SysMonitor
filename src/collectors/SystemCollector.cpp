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
//CPU, MEM, NET
nlohmann::json SystemCollector::snapshotMiddle() {
	nlohmann::json snapshot;
	snapshot["Timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();

	snapshot["system"]["CPU"]["Total"] = cpu.getTotalUsage();
	snapshot["system"]["CPU"]["FredGHZ"] = cpu.getCpuFredGHz();
	snapshot["system"]["CPU"]["QueueLength"] = cpu.getCpuQueueLength();
	snapshot["system"]["CPU"]["User"] = cpu.getCpuUser();
	snapshot["system"]["CPU"]["Kernel"] = cpu.getCpuKernel();


	snapshot["system"]["MEM"]["TotalMB"] = mem.getMemTotalMB();
	snapshot["system"]["MEM"]["AvailMB"] = mem.getAvailMemMB();
	snapshot["system"]["MEM"]["UsedPercent"] = mem.getMemUsagedPercent();
	snapshot["system"]["MEM"]["UsedMB"] = mem.getMemUsedMB();
	snapshot["system"]["MEM"]["commitPercent"] = mem.getCommitMemPercent();
	snapshot["system"]["MEM"]["committedGB"] = mem.getCommittedMemGB();
	snapshot["system"]["MEM"]["commitLimitGB"] = mem.getCommitLimitGB();


	snapshot["system"]["NET"]["netSentKB"] = net.getSentKB();
	snapshot["system"]["NET"]["netRecvKB"] = net.getRecvKB();
	
	return snapshot;
}

//DISK
nlohmann::json SystemCollector::snapshotSlow() {
	nlohmann::json snapshot;
	snapshot["Timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();

	snapshot["system"]["DISK"]["diskReadKB"] = disk.getReadKB();
	snapshot["system"]["DISK"]["diskWriteKB"] = disk.getWriteKB();

	return snapshot;
}

nlohmann::json SystemCollector::snapshot() {
	nlohmann::json snapshot;
	snapshot["Timestamp"] = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();

	snapshot["system"]["CPU"]["Total"] = cpu.getTotalUsage();
	snapshot["system"]["CPU"]["FredGHZ"] = cpu.getCpuFredGHz();
	snapshot["system"]["CPU"]["QueueLength"] = cpu.getCpuQueueLength();
	snapshot["system"]["CPU"]["User"] = cpu.getCpuUser();
	snapshot["system"]["CPU"]["Kernel"] = cpu.getCpuKernel();


	snapshot["system"]["MEM"]["TotalMB"] = mem.getMemTotalMB();
	snapshot["system"]["MEM"]["AvailMB"] = mem.getAvailMemMB();
	snapshot["system"]["MEM"]["UsedPercent"] = mem.getMemUsagedPercent();
	snapshot["system"]["MEM"]["UsedMB"] = mem.getMemUsedMB();
	snapshot["system"]["MEM"]["commitPercent"] = mem.getCommitMemPercent();
	snapshot["system"]["MEM"]["committedGB"] = mem.getCommittedMemGB();
	snapshot["system"]["MEM"]["commitLimitGB"] = mem.getCommitLimitGB();


	snapshot["system"]["NET"]["netSentKB"] = net.getSentKB();
	snapshot["system"]["NET"]["netRecvKB"] = net.getRecvKB();

	snapshot["system"]["DISK"]["diskReadKB"] = disk.getReadKB();
	snapshot["system"]["DISK"]["diskWriteKB"] = disk.getWriteKB();

	return snapshot;
}

