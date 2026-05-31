#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <chrono>

#include "collectors/CpuCollector.h"
#include "collectors/MemCollector.h"
#include "collectors/DiskCollector.h"
#include "collectors/NetCollector.h"
#include "models/SnapShotData.h"

class SystemCollector {
private:
	// 1ÃÊ °£°Ý
	PDH_HQUERY queryMiddle;
	//2ÃÊ °£°Ý
	PDH_HQUERY querySlow;

	CpuCollector cpu;
	MemCollector mem;
	NetCollector net;
	DiskCollector disk;

	std::chrono::system_clock::time_point lastTime;
public:
	SystemCollector();
	~SystemCollector();
	void saveSystemScan();
	void collectMiddle();
	void collectSlow();

	SnapShotSysData makeSnapShot();
	void printToConsole() const;
};
