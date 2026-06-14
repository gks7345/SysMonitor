#pragma once
#include <string>
#include <chrono>
#include <spdlog/spdlog.h>
#include <cmath>
#include <filesystem>
#include "duckdb.hpp"
#include "models/SnapshotData.h"
#include "models/SessionSummary.h"


// -------------------------------------------------------
// SummaryStore
// 날짜별 누적 요약 저장 + 이전 세션 조회
// SessionReport 결과를 받아 DB 저장
// TargetCollector에 마지막 세션 제공
// -------------------------------------------------------
class SummaryStore {
private:
    duckdb::DuckDB     db;
    duckdb::Connection con;
    std::mutex mtx;

    void initDB();
    static std::string todayStr();
    static double safeStod(const std::string& s);
    static uint32_t safeStoul(const std::string& s);
    static std::string ensurePath(const std::string& path);
    static SessionTargetSummary parseTargetRow(duckdb::DataChunk* chunk, size_t row);

public:
    SummaryStore();
    ~SummaryStore() = default;

    // SessionReport 결과 저장 (세션 종료 시 호출)
    void flushSysSummary(const SessionSysSummary& s);
    void flushProcSummaries(const std::vector<SessionProcSummary>& procs);
    void flushTargetSummaries(const std::vector<SessionTargetSummary>& targets);

    // 마지막 세션 조회 (TargetCollector에 전달)
    SessionTargetSummary getTargetLastSession(const std::string& name);

    void flushSummary();
    std::string queryReport(const std::string& sql);
};