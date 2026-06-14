#pragma once
#include <string>
#include <vector>
#include "duckdb.hpp"
#include "models/SnapshotData.h"
#include "models/SessionSummary.h"
#include "datastore/SummaryStore.h"
#include <spdlog/spdlog.h>


class SessionReport {
private:
	duckdb::Connection& con;

	void printSysReport(const SessionSysSummary& s);
	void printProcReport(const std::vector<SessionProcSummary>& procs);
	void printTargetReport(const std::vector<SessionTargetSummary>& targets);
	void printAnomalies();

	static double safeStod(const std::string& s);
	static std::string todayStr();
public:
	explicit SessionReport(duckdb::Connection& con);

	// 전체 리포트 출력 및 결과 반환
	SessionSysSummary analyzeSys();
	std::vector<SessionProcSummary>   analyzeProcs(int topN = 10);
	std::vector<SessionTargetSummary> analyzeTargets();

	// 전체 출력 (분석 + 콘솔)
	void printReport();
};