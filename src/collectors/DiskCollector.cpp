#include "collectors/DiskCollector.h"

void DiskCollector::init(PDH_HQUERY& query) {
	PdhAddEnglishCounter(
		query,
		"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec",
		0,
		&readCounter
	);
	PdhAddEnglishCounter(
		query,
		"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec",
		0,
		&writeCounter
	);
}

float DiskCollector::getReadBytes() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(readCounter, PDH_FMT_DOUBLE, NULL, &val);
	double readDisk = val.doubleValue / 1024.0;
	return (float)readDisk;
}

float DiskCollector::getWriteBytes() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(writeCounter, PDH_FMT_DOUBLE, NULL, &val);
	double writeDisk = val.doubleValue / 1024.0;
	return (float)writeDisk;
}