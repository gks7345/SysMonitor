#include "collectors/CpuCollector.h"

void CpuCollector::init(PDH_HQUERY& query) {
	PdhAddEnglishCounterW(	//전체 사용률
		query,
		L"\\Processor Information(_Total)\\% Processor Utility",
		0,
		&cpuTotal
	);
	PdhAddEnglishCounterW(	//현재 클럭 속도
		query,
		L"\\Processor Information(_Total)\\Actual Frequency",
		0,
		&cpuFredMHz
	);

	PdhAddEnglishCounterW(	// Processor Queue Length 병목 판단용 코어 수 이상 -> 병목 가능
		query,
		L"\\System\\Processor Queue Length",
		0,
		&cpuQueueLength
	);
	//CPU 사용 성격 분석
	//User 높음 -> 프로세스 원인
	//Kernel 높음 -> OS/드라이버 / I/O 문제
	PdhAddEnglishCounterW(
		query,
		L"\\Processor Information(_Total)\\% User Time",
		0,
		&cpuUser
	);
	//Kernel
	PdhAddEnglishCounterW(
		query,
		L"\\Processor Information(_Total)\\% Privileged Time",
		0,
		&cpuKernel
	);
}

double CpuCollector::getTotalUsage() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &val);
	return val.doubleValue;
}
double CpuCollector::getCpuFredGHz() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuFredMHz, PDH_FMT_DOUBLE, NULL, &val);
	double cpuFredGHZ = val.doubleValue / 1000.0;
	return cpuFredGHZ;
}
double CpuCollector::getCpuQueueLength() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuQueueLength, PDH_FMT_DOUBLE, NULL, &val);
	return val.doubleValue;
}
double CpuCollector::getCpuUser() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuUser, PDH_FMT_DOUBLE, NULL, &val);
	return val.doubleValue;
}
double CpuCollector::getCpuKernel() const {
	PDH_FMT_COUNTERVALUE val;
	PdhGetFormattedCounterValue(cpuKernel, PDH_FMT_DOUBLE, NULL, &val);

	return val.doubleValue;
}