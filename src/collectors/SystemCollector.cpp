#include "collectors/SystemCollector.h"
#include <spdlog/spdlog.h>
#pragma comment(lib, "pdh.lib")

SystemCollector::SystemCollector() : queryMiddle(nullptr), querySlow(nullptr)
{
	PDH_STATUS statusMiddle = PdhOpenQuery(nullptr, 0, &queryMiddle);
	if (statusMiddle != ERROR_SUCCESS) {
		spdlog::error("ProcessCollector: PdhOpenQuery(middle) ½ÇÆÐ 0x{:X}", statusMiddle);
	}
	PDH_STATUS statusSlow = PdhOpenQuery(nullptr, 0, &querySlow);
	if (statusSlow != ERROR_SUCCESS) {
		spdlog::error("ProcessCollector: PdhOpenQuery(slow) ½ÇÆÐ 0x{:X}", statusSlow);
	}

	
	PdhCollectQueryData(queryMiddle);
	PdhCollectQueryData(querySlow);
	
	cpu.init(queryMiddle);
	mem.init(queryMiddle);
	net.init(queryMiddle);
	disk.init(querySlow);

	//ÃÊ±â ¼öÁý
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
	lastTime = std::chrono::system_clock::now();
}

void SystemCollector::printToConsole() const {
	printf("\n=== System ===\n");
	printf(
		"CPU : %2.1f%%  (%2.2f GHz) | "
		"MEM : %2.1f / %2.1f GB (%2.1f%%) "
		" Commit : %2.1f / %2.1f GB | "
		"DISK : R %4.2f KB/s  W %4.2f KB/s | "
		"NET : Recv %-12s  Send %-12s\n",
		cpu.getTotalUsage(),
		cpu.getCpuFredGHz(),
		mem.getMemUsedMB() / 1024.0,
		mem.getMemTotalMB() / 1024.0,
		mem.getMemUsagedPercent(),
		mem.getCommittedMemGB(),
		mem.getCommitLimitGB(),
		disk.getReadKB(),
		disk.getWriteKB(),
		net.formatNetSpeed(net.getRecvKbps()).c_str(),
		net.formatNetSpeed(net.getSentKbps()).c_str()
	);
	printf("%s\n", std::string(80, '-').c_str());
}

SnapShotSysData SystemCollector::makeSnapShot() {
	SnapShotSysData snapShot;
	snapShot.timestamp = std::chrono::system_clock::now();
	
	const auto& cpu_ = cpu;
	const auto& mem_ = mem;
	const auto& disk_ = disk;
	const auto& net_ = net;

	snapShot.cpu.cpuTotal = cpu_.getTotalUsage();
	snapShot.cpu.cpuFredMHz = cpu_.getCpuFredGHz();
	snapShot.cpu.cpuQueueLength = cpu_.getCpuQueueLength();
	snapShot.cpu.cpuUser = cpu_.getCpuUser();
	snapShot.cpu.cpuKernel = cpu_.getCpuKernel();

	snapShot.mem.memTotalMB = mem_.getMemTotalMB();
	snapShot.mem.memAvailMB = mem_.getAvailMemMB();
	snapShot.mem.memUsagePercent = mem_.getMemUsagedPercent();
	snapShot.mem.memUsedMB = mem_.getMemUsedMB();
	snapShot.mem.commitMemPercent = mem_.getCommitMemPercent();
	snapShot.mem.committedMemGB = mem_.getCommittedMemGB();
	snapShot.mem.commitLimitGB = mem_.getCommitLimitGB();

	snapShot.disk.lastTime = lastTime;
	snapShot.disk.diskReadKBs = disk_.getReadKB();
	snapShot.disk.diskWriteKBs = disk_.getWriteKB();

	snapShot.net.netRecvKbps = net_.getRecvKbps();
	snapShot.net.netSentKbps = net_.getSentKbps();
	return snapShot;
}

