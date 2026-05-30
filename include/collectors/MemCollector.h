#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <iostream>

class MemCollector {
private:
	PDH_HCOUNTER availableMem;
	PDH_HCOUNTER commitMemPercent;
	PDH_HCOUNTER committedMem;
	PDH_HCOUNTER commitLimit;
	MEMORYSTATUSEX mem;

	float	memTotalMB;
	float	memUsedPercent;
	float	memUsedMB ;
	float	memAvailMB;
	
public:
	MemCollector();
	void init(PDH_HQUERY& query);
	
	double getAvailMemMB() const;
	double getMemUsedMB() const;
	double getMemUsagedPercent() const;
	double getMemTotalMB() const;

	double getCommitMemPercent() const;
	double getCommittedMemGB() const;
	double getCommitLimitGB() const;

	void collectMemInfo();
};
