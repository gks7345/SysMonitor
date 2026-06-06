#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <PdhMsg.h>
#include <iostream>
#include <spdlog/spdlog.h>

class DiskCollector {
private:
	PDH_HCOUNTER readCounter;
	PDH_HCOUNTER writeCounter;

	static double getPdhDoubleValue(PDH_HCOUNTER);
public:
	void init(PDH_HQUERY& query);
	double getReadKB() const;
	double getWriteKB() const;
};