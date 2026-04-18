#include "collectors/SystemCollector.h"
#pragma comment(lib, "pdh.lib")


SystemCollector::SystemCollector() {
	PdhOpenQuery(NULL, 0, &queryMiddle);
	PdhOpenQuery(NULL, 0, &querySlow);

	
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
}
void SystemCollector::collectSlow() {
	PdhCollectQueryData(querySlow);
}
SystemSample SystemCollector::snapshot() {
	return {
		.cpuTotal = cpu.getTotalUsage(),
		.cpuFreqGHz = cpu.getCpuFredGHz(),

		.memTotalMB = mem.getMemTotalMB(),
		.memAvailMB = mem.getAvailMemMB(),
		.memUsedMB = mem.getMemUsedMB(),
		.memUsedPercent = mem.getMemUsagedPercent(),

		.netSentKB = net.getSentKB(),
		.netRecvKB = net.getRecvKB(),

		.diskReadKB = disk.getReadBytes(),
		.diskWriteKB = disk.getWriteBytes()
	};
}

