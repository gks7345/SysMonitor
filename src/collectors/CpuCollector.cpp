#include "collectors/CpuCollector.h"

void CpuCollector::init(PDH_HQUERY& query) {
	PdhAddEnglishCounter(	//전체 사용률
		query,
		"\\Processor(_Total)\\% Processor Time",
		0,
		&cpuTotal
	);
	PdhAddEnglishCounter(	//현재 클럭 속도
		query,
		"\\Processor Information(_Total)\\Processor Frequency",
		0,
		&cpuFredMHz
	);

	PdhAddEnglishCounter(	// Processor Queue Length 병목 판단용 코어 수 이상 -> 병목 가능
		query,
		"\\System\\Processor Queue Length",
		0,
		&cpuQueueLength
	);
	//CPU 사용 성격 분석
	//User 높음 -> 프로세스 원인
	//Kernel 높음 -> OS/드라이버 / I/O 문제
	PdhAddEnglishCounter(
		query,
		"\\Processor(_Total)\\% User Time",
		0,
		&cpuUser
	);
	//Kernel
	PdhAddEnglishCounter(
		query,
		"\\Processor(_Total)\\% Privileged  Time",
		0,
		&cpuKernel
	);
}

float CpuCollector::getTotalUsage() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &val);
	return (float)val.doubleValue;
}
float CpuCollector::getCpuFredGHz() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuFredMHz, PDH_FMT_DOUBLE, NULL, &val);
	double cpuFredGHZ = val.doubleValue / 1000.0;
	return (float)cpuFredGHZ;
}