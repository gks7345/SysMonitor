#pragma once
#include <iostream>
#include <chrono>

// ---------------------------------------------------------------------------
// 시스템
// ---------------------------------------------------------------------------
struct SnapShotSysCpu {
	double cpuTotal = 0.0;
	double cpuFredMHz = 0.0;

	double cpuUser = 0.0;
	double cpuKernel = 0.0;
	double cpuQueueLength = 0.0;
};

struct SnapShotSysMem {
	double memTotalMB = 0.0;
	double memUsagePercent = 0.0;
	double memUsedMB = 0.0;
	double memAvailMB = 0.0;

	double commitMemPercent = 0.0;
	double committedMemGB = 0.0;
	double commitLimitGB = 0.0;
};

struct SnapShotSysDisk {
	std::chrono::system_clock::time_point lastTime;
	double diskReadKBs = 0.0;
	double diskWriteKBs = 0.0;
};

struct SnapShotSysNet {
	double netSentKbps = 0.0;
	double netRecvKbps = 0.0;
};

struct SnapShotSysData {
	std::chrono::system_clock::time_point	timestamp;

	SnapShotSysCpu cpu;
	SnapShotSysMem mem;
	SnapShotSysDisk disk;
	SnapShotSysNet net;
};

// ---------------------------------------------------------------------------
// 프로세스
// ---------------------------------------------------------------------------
struct SnapShotProc {
	int			procID = 0;
	std::string procName;

	double		procCpuUsage = 0.0;

	double		procMemoryMB = 0.0;
	double		procPrivateMemoryMB = 0.0;

	double		procDiskReadMBs = 0.0;
	double		procDiskWriteMBs = 0.0;

	double		procNetSentMbps = 0.0;
	double		procNetRecvMbps = 0.0;
};

struct SnapShotProcData {
	std::chrono::system_clock::time_point	timestamp;
	int										topN;
	std::string								sortCriterion;
	std::vector<SnapShotProc>				procs;
};