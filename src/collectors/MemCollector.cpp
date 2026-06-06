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
	PDH_STATUS memStatus = PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\Available MBytes",
		0,
		&availableMem
	);
	if (memStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", memStatus);
	}

	PDH_STATUS commitStatus = PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\% Committed Bytes In Use",
		0,
		&commitMemPercent
	);
	if (commitStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", commitStatus);
	}

	PDH_STATUS commitMemStatus = PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\Committed Bytes",
		0,
		&committedMem
	);
	if (commitMemStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", commitMemStatus);
	}


	PDH_STATUS commitLimitStatus = PdhAddEnglishCounterW(
		qurey,
		L"\\Memory\\Commit Limit",
		0,
		&commitLimit
	);
	if (commitLimitStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", commitLimitStatus);
	}
}

double MemCollector::getPdhDoubleValue(PDH_HCOUNTER counter) {
	PDH_FMT_COUNTERVALUE val = {}; // 초기화

	PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &val);

	if (status != ERROR_SUCCESS) return 0.0;

	if (val.CStatus != PDH_CSTATUS_VALID_DATA &&
		val.CStatus != PDH_CSTATUS_NEW_DATA) {
		return 0.0;
	}

	return val.doubleValue;
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
	return getPdhDoubleValue(commitMemPercent);
}
double MemCollector::getCommittedMemGB() const {
	double committedMemMB = getPdhDoubleValue(committedMem) / (1024.0 * 1024.0);
	return committedMemMB / 1024.0;
}
double MemCollector::getCommitLimitGB() const {
	double commitLimitMB = getPdhDoubleValue(commitLimit) / (1024.0 * 1024.0);
	return commitLimitMB / 1024.0;
}


void MemCollector::collectMemInfo() {
	double val = getPdhDoubleValue(availableMem);
	memAvailMB = val;
	memUsedMB = memTotalMB - memAvailMB;
	memUsedPercent = (memUsedMB / memTotalMB) * 100.0;
}