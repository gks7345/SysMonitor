#include "collectors/MemCollector.h"

MemCollector::MemCollector() {
	mem.dwLength = sizeof(mem);
	GlobalMemoryStatusEx(&mem);
	float totalRAM = (float)mem.ullTotalPhys;
	memTotalMB = totalRAM / (1024 * 1024);
}

//Available Memory↓ commit ↑ 위험
//Available 매우 작음 Commit % 90%+ 위험
void MemCollector::init(PDH_HQUERY& qurey) {
	PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\Available MBytes",
		0,
		&availableMem
	);
	PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\% Committed Bytes In Use",
		0,
		&commitMemPercent
	);
	PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\Committed Bytes",
		0,
		&committedMem
	);
	PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\Commit Limit",
		0,
		&commitLimit
	);
}
double MemCollector::getAvailMemMB() const {
	return memAvailMB;
}
double MemCollector::getMemUsedMB() const {
	return memUsedMB;
}
double MemCollector::getMemUsagedPercent() const {
	return memUsedPercent;
}
double MemCollector::getMemTotalMB() const {
	return memTotalMB;
}

double MemCollector::getCommitMemPercent() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(commitMemPercent, PDH_FMT_DOUBLE, NULL, &val);
	return val.doubleValue;
}
double MemCollector::getCommittedMemGB() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(committedMem, PDH_FMT_DOUBLE, NULL, &val);
	double committedMemMB = val.doubleValue / (1024.0 * 1024.0);
	return committedMemMB/1024.0;
}
double MemCollector::getCommitLimitGB() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(commitLimit, PDH_FMT_DOUBLE, NULL, &val);
	double commitLimitMB = val.doubleValue /(1024.0*1024.0);
	return commitLimitMB/1024.0;
}


void MemCollector::collectMemInfo() {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(availableMem, PDH_FMT_DOUBLE, NULL, &val);
	memAvailMB = val.doubleValue;
	memUsedMB = memTotalMB - memAvailMB;
	memUsedPercent = (memUsedMB / memTotalMB) * 100.0;
}