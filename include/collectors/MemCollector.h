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
	
	float getAvailMemMB();
	float getMemUsedMB();
	float getMemUsagedPercent();
	float getMemTotalMB();

	float getCommitMemPercent();
	float getCommittedMemGB();
	float getCommitLimitGB();

	void collectMemInfo();
};
