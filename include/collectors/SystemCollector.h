#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <nlohmann/json.hpp>
#include "collectors/CpuCollector.h"
#include "collectors/MemCollector.h"
#include "collectors/DiskCollector.h"
#include "collectors/NetCollector.h"

class SystemCollector {
private:
	// 1초 간격
	PDH_HQUERY queryMiddle;
	//2초 간격
	PDH_HQUERY querySlow;

	CpuCollector cpu;
	MemCollector mem;
	NetCollector net;
	DiskCollector disk;

public:
	SystemCollector();
	~SystemCollector();
	void saveSystemScan();
	void collectMiddle();
	void collectSlow();
	nlohmann::json snapshot();
};
