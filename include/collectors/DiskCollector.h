#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <iostream>

class DiskCollector {
private:
	PDH_HCOUNTER readCounter;
	PDH_HCOUNTER writeCounter;

public:
	void init(PDH_HQUERY& query);
	float getReadKB() const;
	float getWriteKB() const;
};