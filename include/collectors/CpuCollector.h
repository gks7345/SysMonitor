#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <iostream>

class CpuCollector {
private:
	PDH_HCOUNTER cpuTotal;
	PDH_HCOUNTER cpuFredMHz;

	PDH_HCOUNTER cpuUser;
	PDH_HCOUNTER cpuKernel;
	PDH_HCOUNTER cpuQueueLength;
public:
	void init(PDH_HQUERY& query);
	double getTotalUsage() const;
	double getCpuFredGHz() const;
	double getCpuQueueLength() const;
	double getCpuUser() const;
	double getCpuKernel() const;
};