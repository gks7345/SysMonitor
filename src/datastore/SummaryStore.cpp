#include "datastore/SummaryStore.h"


SummaryStore::SummaryStore()
    : db(ensurePath("reports/summary/SysMonitor_summary.db"))
    , con(db) {
    initDB();
    spdlog::info("SummaryStore: reports/summary/SysMonitor_summary.db");
}

std::string SummaryStore::ensurePath(const std::string& path) {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());
    return path;
}

void SummaryStore::initDB() {
    // 시스템 요약 (날짜별 1행)
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS system_summary (
            date                VARCHAR PRIMARY KEY,
            avgCpuTotal         DOUBLE,
            peakCpuTotal        DOUBLE,
            avgMemUsagePercent  DOUBLE,
            peakMemUsagePercent DOUBLE,
            avgDiskKBs          DOUBLE,
            peakDiskKBs         DOUBLE,
            avgNetKbps          DOUBLE,
            peakNetKbps         DOUBLE,
            sampleCount         INTEGER
        )
    )");

    // 프로세스 요약 (날짜 + 이름별)
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS proc_summary (
            date         VARCHAR,
            procName     VARCHAR,
            avgCpuUsage  DOUBLE,
            peakCpuUsage DOUBLE,
            avgMemoryMB  DOUBLE,
            peakMemoryMB DOUBLE,
            sampleCount  INTEGER,
            PRIMARY KEY (date, procName)
        )
    )");

    // 타겟 요약 (날짜 + 이름별)
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS target_summary (
            date             VARCHAR,
            targetName       VARCHAR,
            exePath          VARCHAR,
            avgCpuUsage      DOUBLE,
            peakCpuUsage     DOUBLE,
            avgMemoryMB      DOUBLE,
            peakMemoryMB     DOUBLE,
            avgNetMbps       DOUBLE,
            peakNetMbps      DOUBLE,
            peakThreadCount  BIGINT,
            peakHandleCount  BIGINT,
            totalRuntime      DOUBLE,
            sampleCount      INTEGER,
            PRIMARY KEY (date, targetName)
        )
    )");
}

std::string SummaryStore::todayStr() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = {};
    localtime_s(&tm, &time);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

double SummaryStore::safeStod(const std::string& s) {
    try { return std::stod(s); }
    catch (...) { return 0.0; }
}

int SummaryStore::safeStoi(const std::string& s) {
    try { return std::stoi(s); }
    catch (...) { return 0; }
}

uint32_t SummaryStore::safeStoul(const std::string& s) {
    try { return static_cast<uint32_t>(std::stoul(s)); }
    catch (...) { return 0; }
}

SessionTargetSummary SummaryStore::parseTargetRow(duckdb::DataChunk* chunk, size_t row) {
    SessionTargetSummary t;
    t.date = chunk->GetValue(0, row).ToString();
    t.targetName = chunk->GetValue(1, row).ToString();
    t.exePath = chunk->GetValue(2, row).ToString();
    t.avgCpuUsage = safeStod(chunk->GetValue(3, row).ToString());
    t.peakCpuUsage = safeStod(chunk->GetValue(4, row).ToString());
    t.avgMemoryMB = safeStod(chunk->GetValue(5, row).ToString());
    t.peakMemoryMB = safeStod(chunk->GetValue(6, row).ToString());
    t.avgNetMbps = safeStod(chunk->GetValue(7, row).ToString());
    t.peakNetMbps = safeStod(chunk->GetValue(8, row).ToString());
    t.peakThreadCount = safeStoul(chunk->GetValue(9, row).ToString());
    t.peakHandleCount = safeStoul(chunk->GetValue(10, row).ToString());
    t.totalRuntime = safeStod(chunk->GetValue(11, row).ToString());
    t.sampleCount = safeStoi(chunk->GetValue(12, row).ToString());
    return t;
}

// -------------------------------------------------------
// 시스템 요약 저장 — 같은 날짜면 누적
// -------------------------------------------------------
void SummaryStore::flushSysSummary(const SessionSysSummary& s) {
    std::lock_guard<std::mutex> lock(mtx);

    // analyzeSys()가 이미 오늘 전체를 집계한 값이므로
    // INSERT OR REPLACE로 그대로 덮어쓰기
    auto stmt = con.Prepare(
        "INSERT OR REPLACE INTO system_summary VALUES ("
        "?, "   // s.date
        "?, "   // s.avgCpuTotal
        "?, "   // s.peakCpuTotal
        "?, "   // s.avgMemUsagePercent
        "?, "   // s.peakMemUsagePercent
        "?, "   // s.avgDiskKBs
        "?, "   // s.peakDiskKBs
        "?, "   // s.avgNetKbps
        "?, "   // s.peakNetKbps
        "?)");  // s.sampleCount

    if (stmt->HasError()) {
        spdlog::error("SummaryStore: flushSysSummaries prepare {}", stmt->GetError());
        return;
    }

    try {
        auto r = stmt->Execute(s.date, s.avgCpuTotal, s.peakCpuTotal,
            s.avgMemUsagePercent, s.peakMemUsagePercent, s.avgDiskKBs,
            s.peakDiskKBs, s.avgNetKbps, s.peakNetKbps, s.sampleCount);
        if (r->HasError())
            spdlog::error("SummaryStore: flushSysSummary {}", r->GetError());
    }
    catch (const std::exception& e) {
        spdlog::error("SummaryStore: flushSysSummary exception: {}", e.what());
    }



    spdlog::info("SummaryStore: system_summary save ({})", s.date);
    con.Query("CHECKPOINT");
    spdlog::info("SummaryStore: system_summary flush clear");
}

// -------------------------------------------------------
// 프로세스 요약 저장
// -------------------------------------------------------
void SummaryStore::flushProcSummaries(const std::vector<SessionProcSummary>& procs) {
    std::lock_guard<std::mutex> lock(mtx);

    // analyzeProcs()가 이미 오늘 전체를 집계한 값이므로
        // INSERT OR REPLACE로 그대로 덮어쓰기
    auto stmt = con.Prepare(
        "INSERT OR REPLACE INTO proc_summary VALUES ("
        "?, "
        "?, "   // procName
        "?, "   // p.avgCpuUsage
        "?, "   // p.peakCpuUsage
        "?, "   // p.avgMemoryMB
        "?, "   // p.peakMemoryMB
        "?)");  // p.sampleCount

    if (stmt->HasError()) {
        spdlog::error("SummaryStore: flushProcSummaries prepare {}", stmt->GetError());
        return;  // 한 프로세스 실패해도 나머지 계속 처리
    }

    for (const auto& p : procs) {
        try {
            auto r = stmt->Execute(p.date, p.procName, p.avgCpuUsage, p.peakCpuUsage, p.avgMemoryMB, p.peakMemoryMB, p.sampleCount);
            if (r->HasError())
                spdlog::error("SummaryStore: flushProcSummaries execute {}", r->GetError());
        }
        catch (const std::exception& e) {
            spdlog::error("SummaryStore: flushProcSummaries exception: {}", e.what());
        }

    }

    spdlog::info("SummaryStore: proc_summary save {}", procs.size());
    con.Query("CHECKPOINT");
    spdlog::info("SummaryStore: proc_summary flush clear");
}

// -------------------------------------------------------
// 타겟 요약 저장
// -------------------------------------------------------
void SummaryStore::flushTargetSummaries(const std::vector<SessionTargetSummary>& targets) {
    std::lock_guard<std::mutex> lock(mtx);

    auto stmt = con.Prepare(
        "INSERT OR REPLACE INTO target_summary VALUES ("
        "?, "    // t.date
        "?, "    // t.targetName
        "?, "    // t.exePath
        "?, "    // t.avgCpuUsage
        "?, "    // t.peakCpuUsage 
        "?, "    // t.avgMemoryMB
        "?, "    // t.peakMemoryMB
        "?, "    // t.avgNetMbps
        "?, "    // t.peakNetMbps
        "?, "    // t.peakThreadCount
        "?, "    // t.peakHandleCount
        "?, "    // t.runTime
        "?)");   // t.sampleCount
    if (stmt->HasError()) {
        spdlog::error("SummaryStore: flushTargetSummaries prepare {}", stmt->GetError());
        return;
    }

    for (const auto& t : targets) {
        // SELECT/UPDATE/INSERT 분기 없이
        // analyzeTargets()가 이미 오늘 전체를 집계한 값이므로
        // 그대로 INSERT OR REPLACE로 덮어쓰기
        std::string safePath = t.exePath;   // \를 DuckDB가 exePath를 std::string인데도 BLOB으로 오인
        for (char& c : safePath)
            if (c == '\\') c = '/';  // \ → /

        try {
            auto r = stmt->Execute(t.date, t.targetName,
                std::string(safePath), t.avgCpuUsage,
                t.peakCpuUsage, t.avgMemoryMB, t.peakMemoryMB,
                t.avgNetMbps, t.peakNetMbps,
                static_cast<int64_t>(t.peakThreadCount),
                static_cast<int64_t>(t.peakHandleCount),
                t.runTime, t.sampleCount);
            if (r->HasError()) {
                spdlog::error("SummaryStore: flushTargetSummaries execute {}", r->GetError());
            }
        }
        catch (const std::exception& e) {
            spdlog::error("SummaryStore: flushTargetSummaries exception: {}", e.what());
        }

    }
    spdlog::info("SummaryStore: target_summary save {}", targets.size());
    con.Query("CHECKPOINT");
    spdlog::info("SummaryStore: target_summary flush clear");
}

// -------------------------------------------------------
// 마지막 세션 조회 (날짜 무관 최신)
// -------------------------------------------------------
SessionTargetSummary SummaryStore::getTargetLastSession(const std::string& name) {

    std::lock_guard<std::mutex> lock(mtx);
    std::string today = todayStr();

    // 1순위 당일
    auto stmt = con.Prepare(
        "SELECT date, targetName, exePath, "
        "       avgCpuUsage, peakCpuUsage, "
        "       avgMemoryMB, peakMemoryMB, "
        "       avgNetMbps, peakNetMbps, "
        "       peakThreadCount, peakHandleCount, "
        "       totalRuntime, sampleCount "
        "FROM target_summary "
        "WHERE targetName = ? "
        "  AND date = '" + today + "'");

    auto r = stmt->Execute(name);
    if (!r->HasError() && !stmt->HasError()) {
        auto chunk = r->Fetch();
        if (chunk && chunk->size() > 0) {
            auto parsed = parseTargetRow(chunk.get(), 0);
            spdlog::info("SummaryStore: '{}' Session return Today({})", name, today);
            return parsed;
        }
    }

    // 2순위 — 가장 최근 날짜
    auto stmt2 = con.Prepare(
        "SELECT date, targetName, exePath, "
        "       avgCpuUsage, peakCpuUsage, "
        "       avgMemoryMB, peakMemoryMB, "
        "       avgNetMbps, peakNetMbps, "
        "       peakThreadCount, peakHandleCount, "
        "       totalRuntime, sampleCount "
        "FROM target_summary "
        "WHERE targetName = ? "
        "ORDER BY date DESC LIMIT 1");

    auto r2 = stmt2->Execute(name);
    if (!r2->HasError() && !stmt2->HasError()) {
        auto chunk = r2->Fetch();
        if (chunk && chunk->size() > 0) {
            auto parsed = parseTargetRow(chunk.get(), 0);
            spdlog::info("SummaryStore: '{}' Session return Last day ({})", name, parsed.date);
            return parsed;
        }
    }

    // 데이터 없음
    SessionTargetSummary empty;
    empty.targetName = name;
    return empty;
}

// -------------------------------------------------------
// flush
// -------------------------------------------------------
void SummaryStore::flushSummary() {
    std::lock_guard<std::mutex> lock(mtx);
    con.Query("CHECKPOINT");
    spdlog::info("SummaryStore: flush 완료");
}

std::string SummaryStore::queryReport(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx);
    auto result = con.Query(sql);
    if (result->HasError()) {
        spdlog::error("SummaryStore: query error {}", result->GetError());
        return "";
    }
    return result->ToString();
}
