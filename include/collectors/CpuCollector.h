#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <pdhmsg.h>
#include <iostream>

class CpuCollector {
private:
	PDH_HCOUNTER cpuTotal;
	PDH_HCOUNTER cpuCores;
	PDH_HCOUNTER cpuFredMHz;

	PDH_HCOUNTER cpuUser;
	PDH_HCOUNTER cpuKernel;
	PDH_HCOUNTER cpuQueueLength;
public:
	void init(PDH_HQUERY& query);
	float getTotalUsage() const;
	float getCpuFredGHz() const;
};