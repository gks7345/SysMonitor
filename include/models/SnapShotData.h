#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint> 

// ---------------------------------------------------------------------------
// 시스템
// ---------------------------------------------------------------------------
struct SnapshotSysCpu {
	double cpuTotal = 0.0;
	double cpuFreqGHz = 0.0;

	double cpuUser = 0.0;
	double cpuKernel = 0.0;
	double cpuQueueLength = 0.0;
};

struct SnapshotSysMem {
	double memTotalMB = 0.0;
	double memUsagePercent = 0.0;
	double memUsedMB = 0.0;
	double memAvailMB = 0.0;

	double commitMemPercent = 0.0;
	double committedMemGB = 0.0;
	double commitLimitGB = 0.0;
};

struct SnapshotSysDisk {
	std::chrono::system_clock::time_point lastTime;
	double diskReadKBs = 0.0;
	double diskWriteKBs = 0.0;
};

struct SnapshotSysNet {
	double netSentKbps = 0.0;
	double netRecvKbps = 0.0;
};

struct SnapshotSysData {
	std::chrono::system_clock::time_point	timestamp;

	SnapshotSysCpu cpu;
	SnapshotSysMem mem;
	SnapshotSysDisk disk;
	SnapshotSysNet net;
};

// ---------------------------------------------------------------------------
// 프로세스
// ---------------------------------------------------------------------------
struct SnapshotProc {
	uint32_t	procID = 0;
	std::string procName;

	double		procCpuUsage = 0.0;

	double		procMemoryMB = 0.0;
	double		procPrivateMemoryMB = 0.0;

	double		procDiskReadMBs = 0.0;
	double		procDiskWriteMBs = 0.0;

	double		procNetSentMbps = 0.0;
	double		procNetRecvMbps = 0.0;
};

struct SnapshotProcData {
	std::chrono::system_clock::time_point	timestamp;
	int										topN = 0;
	std::string								sortCriterion;
	std::vector<SnapshotProc>				procs;
};