#include "collectors/CpuCollector.h"


void CpuCollector::init(PDH_HQUERY& query) {
	PDH_STATUS cpuStatus = PdhAddEnglishCounterW(	//전체 사용률
		query,
		L"\\Processor Information(_Total)\\% Processor Utility",
		0,
		&cpuTotal
	);
	if (cpuStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", cpuStatus);
	}

	PDH_STATUS fredStatus = PdhAddEnglishCounterW(	// 클럭 속도
		query,
		L"\\Processor Information(_Total)\\Actual Frequency",
		0,
		&cpuFreqMHz
	);
	if (fredStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", fredStatus);
	}



	// Processor Queue Length 병목 판단용 코어 수 이상 -> 병목 가능
	PDH_STATUS queueLengthStatus = PdhAddEnglishCounterW(
		query,
		L"\\System\\Processor Queue Length",
		0,
		&cpuQueueLength
	);
	if (queueLengthStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", queueLengthStatus);
	}

	//CPU 사용 성격 분석
	//User 높음 -> 프로세스 원인
	//Kernel 높음 -> OS/드라이버 / I/O 문제
	PDH_STATUS userStatus = PdhAddEnglishCounterW(
		query,
		L"\\Processor Information(_Total)\\% User Time",
		0,
		&cpuUser
	);
	if (userStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", userStatus);
	}
	//Kernel
	PDH_STATUS kernelStatus = PdhAddEnglishCounterW(
		query,
		L"\\Processor Information(_Total)\\% Privileged Time",
		0,
		&cpuKernel
	);
	if (kernelStatus != ERROR_SUCCESS) {
		spdlog::error("Counter ERROR 0x{:X}", kernelStatus);
	}
}

double CpuCollector::getPdhDoubleValue(PDH_HCOUNTER counter) {
	PDH_FMT_COUNTERVALUE val = {}; // 초기화

	PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &val);

	if (status != ERROR_SUCCESS) {
		spdlog::error("val Status ERROR 0x{:X}", status);
		return 0.0;
	}

	if (val.CStatus != PDH_CSTATUS_VALID_DATA && val.CStatus != PDH_CSTATUS_NEW_DATA) {
		spdlog::error("CStatus ERROR 0x{:X}", val.CStatus);
		return 0.0;
	}

	return val.doubleValue;
}

double CpuCollector::getTotalUsage() const {
	return getPdhDoubleValue(cpuTotal);
}
double CpuCollector::getCpuFreqGHz() const {
	double val = getPdhDoubleValue(cpuFreqMHz);

	if (val > 1'000'000'000)  // Hz (10억 이상)
		return val / 1'000'000'000.0;
	else if (val > 1'000'000) // KHz (100만 이상)
		return val / 1'000'000.0;
	else if (val > 100)       // MHz (100 이상)
		return val / 1'000.0;
	else                      // 이미 GHz (1~6 범위)
		return val;
}
double CpuCollector::getCpuQueueLength() const {
	return getPdhDoubleValue(cpuQueueLength);
}
double CpuCollector::getCpuUser() const {
	return getPdhDoubleValue(cpuUser);
}
double CpuCollector::getCpuKernel() const {
	return getPdhDoubleValue(cpuKernel);
}