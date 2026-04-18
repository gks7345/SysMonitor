#pragma once
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
	PdhAddEnglishCounter(
		qurey,
		"\\Memory\\Available MBytes",
		0,
		&availableMem
	);
	PdhAddEnglishCounter(
		qurey,
		"\\Memory\\% Committde Bytes In Use",
		0,
		&commitMemPercent
	);
	PdhAddEnglishCounter(
		qurey,
		"\\Memory\\Committded Bytes",
		0,
		&committedMem
	);
	PdhAddEnglishCounter(
		qurey,
		"\\Memory\\Commit Limit",
		0,
		&commitLimit
	);
}
float MemCollector::getAvailMemMB() {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(availableMem, PDH_FMT_DOUBLE, NULL, &val);
	memAvailMB = (float)val.doubleValue;
	return memAvailMB;
}
float MemCollector::getMemUsedMB() {
	memUsedMB = memTotalMB - memAvailMB;
	return memUsedMB;
}
float MemCollector::getMemUsagedPercent(){
	memUsedPercent = (memUsedMB / memTotalMB) * 100.0;
	return memUsedPercent;
}
float MemCollector::getMemTotalMB() {
	return memTotalMB;
}