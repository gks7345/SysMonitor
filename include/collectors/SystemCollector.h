#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <pdhmsg.h>
#include "collectors/CpuCollector.h"
#include "collectors/MemCollector.h"
#include "collectors/DiskCollector.h"
#include "collectors/NetCollector.h"
#include "datastore/SystemSample.h"

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

	float cpuUsage;
	float cpuPerCoreUsage;
	float cpuClock;
	float memUsage;
	float netUsage;
	float diskUsage;
public:
	SystemCollector();
	~SystemCollector();
	void saveSystemScan();
	void collectMiddle();
	void collectSlow();
	SystemSample snapshot();

	float getCpuUsage();
	float getMemUsage();
	float getDiskUsage();
	float getNetUsage();
};
