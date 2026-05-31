#include "collectors/DiskCollector.h"

void DiskCollector::init(PDH_HQUERY& query) {
	PDH_STATUS readStatus = PdhAddEnglishCounterW(
		query,
		L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec",
		0,
		&readCounter
	);
	if (readStatus != ERROR_SUCCESS) {
		spdlog::error("Counter Error 0x{:X}", readStatus);
	}


	PDH_STATUS writeStatus = PdhAddEnglishCounterW(
		query,
		L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec",
		0,
		&writeCounter
	);
	if (writeStatus != ERROR_SUCCESS) {
		spdlog::error("Counter Error 0x{:X}", writeStatus);
	}
}

double DiskCollector::getPdhDoubleValue(PDH_HCOUNTER counter) {
	PDH_FMT_COUNTERVALUE val = {}; // √ ±‚»≠

	PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &val);

	if (status != ERROR_SUCCESS) return 0.0;

	if (val.CStatus != PDH_CSTATUS_VALID_DATA &&
		val.CStatus != PDH_CSTATUS_NEW_DATA) {
		return 0.0;
	}

	return val.doubleValue;
}

double DiskCollector::getReadKB() const {
	return getPdhDoubleValue(readCounter) / 1024.0;
}

double DiskCollector::getWriteKB() const {
	return getPdhDoubleValue(writeCounter) / 1024.0;
}