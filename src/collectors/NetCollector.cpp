#include "collectors/NetCollector.h"

void NetCollector::init(PDH_HQUERY& query) {
	PdhAddEnglishCounter(
		query,
		"\\Network Interface(*)\\Bytes Sent/sec",
		0,
		&sentCounter);
	PdhAddEnglishCounter(
		query,
		"\\Network Interface(*)\\Bytes Received/sec",
		0,
		&recvCounter);
}

float NetCollector::getSentKB() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(sentCounter, PDH_FMT_DOUBLE, NULL, &val);
	double sentNet = val.doubleValue / 1024.0;
	return (float)sentNet;
}

float NetCollector::getRecvKB() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(recvCounter, PDH_FMT_DOUBLE, NULL, &val);
	double recvNet = val.doubleValue / 1024.0;
	return (float)recvNet;
}