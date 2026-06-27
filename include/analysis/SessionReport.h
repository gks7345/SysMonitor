#pragma once
#include <string>
#include <vector>
#include "duckdb.hpp"
#include "models/SnapshotData.h"
#include "models/SessionSummary.h"
#include "datastore/SummaryStore.h"
#include "datastore/DataStore.h"
#include <spdlog/spdlog.h>


class SessionReport {
private:
	DataStore& dataStore;

	void printSysReport(const SessionSysSummary& s);
	void printProcReport(const std::vector<SessionProcSummary>& procs);
	void printTargetReport(const std::vector<SessionTargetSummary>& targets);
	void printAnomalies();

	static double safeStod(const std::string& s);
	static int safeStoi(const std::string& s);
	static std::string todayStr();
public:
	explicit SessionReport(DataStore& dataStore);

	// 전체 리포트 출력 및 결과 반환
	SessionSysSummary analyzeSys();
	std::vector<SessionProcSummary>   analyzeProcs(int topN = 10);
	std::vector<SessionTargetSummary> analyzeTargets();
	SessionTargetSummary getSessionTargetSummaryOne(const std::string& name);

	// 전체 출력 (분석 + 콘솔)
	void printReport();
};