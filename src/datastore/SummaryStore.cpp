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
            peakThreadCount  INTEGER,
            peakHandleCount  INTEGER,
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
    catch (...) { return 0.0; }
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

    auto r = con.Query(
        "SELECT avgCpuTotal, peakCpuTotal, "
        "       avgMemUsagePercent, peakMemUsagePercent, "
        "       avgDiskKBs, peakDiskKBs, "
        "       avgNetKbps, peakNetKbps, sampleCount "
        "FROM system_summary WHERE date = '" + s.date + "'");

    auto chunk = (r->HasError()) ? nullptr : r->Fetch();

    if (!chunk || chunk->size() == 0) {
        // 신규 삽입
        con.Query(
            "INSERT INTO system_summary VALUES ("
            "'" + s.date + "', " +
            std::to_string(s.avgCpuTotal) + ", " +
            std::to_string(s.peakCpuTotal) + ", " +
            std::to_string(s.avgMemUsagePercent) + ", " +
            std::to_string(s.peakMemUsagePercent) + ", " +
            std::to_string(s.avgDiskKBs) + ", " +
            std::to_string(s.peakDiskKBs) + ", " +
            std::to_string(s.avgNetKbps) + ", " +
            std::to_string(s.peakNetKbps) + ", " +
            std::to_string(s.sampleCount) + ")");
    }
    else {
        // 기존 행 누적 — 이동 평균
        double pa = safeStod(chunk->GetValue(0, 0).ToString());
        double pk = safeStod(chunk->GetValue(1, 0).ToString());
        double ma = safeStod(chunk->GetValue(2, 0).ToString());
        double mk = safeStod(chunk->GetValue(3, 0).ToString());
        double da = safeStod(chunk->GetValue(4, 0).ToString());
        double dk = safeStod(chunk->GetValue(5, 0).ToString());
        double na = safeStod(chunk->GetValue(6, 0).ToString());
        double nk = safeStod(chunk->GetValue(7, 0).ToString());
        int    n = safeStoi(chunk->GetValue(8, 0).ToString());
        int    ns = s.sampleCount;
        int    total = n + ns;

        // 가중 평균
        auto wavg = [&](double a, double b) {
            return (a * n + b * ns) / total; };

        con.Query(
            "UPDATE system_summary SET "
            "avgCpuTotal = " + std::to_string(wavg(pa, s.avgCpuTotal)) + ", "
            "peakCpuTotal = " + std::to_string(std::max(pk, s.peakCpuTotal)) + ", "
            "avgMemUsagePercent = " + std::to_string(wavg(ma, s.avgMemUsagePercent)) + ", "
            "peakMemUsagePercent = " + std::to_string(std::max(mk, s.peakMemUsagePercent)) + ", "
            "avgDiskKBs = " + std::to_string(wavg(da, s.avgDiskKBs)) + ", "
            "peakDiskKBs = " + std::to_string(std::max(dk, s.peakDiskKBs)) + ", "
            "avgNetKbps = " + std::to_string(wavg(na, s.avgNetKbps)) + ", "
            "peakNetKbps = " + std::to_string(std::max(nk, s.peakNetKbps)) + ", "
            "sampleCount = " + std::to_string(total) + " "
            "WHERE date = '" + s.date + "'");
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

    for (const auto& p : procs) {
        auto stmt = con.Prepare(
            "SELECT avgCpuUsage, peakCpuUsage, "
            "       avgMemoryMB, peakMemoryMB, sampleCount "
            "FROM proc_summary "
            "WHERE date = '" + p.date + "' "
            "  AND procName = ? ");
        if (stmt->HasError()) {
            spdlog::error("SummaryStore: flushProcSummaries prepare {}", stmt->GetError());
            return;
        }

        auto r = stmt->Execute(p.procName);
        auto chunk = (r->HasError()) ? nullptr : r->Fetch();

        if (!chunk || chunk->size() == 0) {
            auto stmt = con.Prepare(
                "INSERT INTO proc_summary VALUES ("
                "'" + p.date + "', "
                " ?, " +
                std::to_string(p.avgCpuUsage) + ", " +
                std::to_string(p.peakCpuUsage) + ", " +
                std::to_string(p.avgMemoryMB) + ", " +
                std::to_string(p.peakMemoryMB) + ", " +
                std::to_string(p.sampleCount) + ")");

            if (stmt->HasError()) {
                spdlog::error("SummaryStore: flushProcSummaries INSERT prepare {}", stmt->GetError());
                return;
            }
            auto r = stmt->Execute(p.procName);

        }
        else {
            double ca = safeStod(chunk->GetValue(0, 0).ToString());
            double ck = safeStod(chunk->GetValue(1, 0).ToString());
            double ma = safeStod(chunk->GetValue(2, 0).ToString());
            double mk = safeStod(chunk->GetValue(3, 0).ToString());
            int    n = safeStoi(chunk->GetValue(4, 0).ToString());
            int    ns = p.sampleCount;
            int    total = n + ns;

            auto wavg = [&](double a, double b) {
                return (a * n + b * ns) / total; };

            auto stmt = con.Prepare(
                "UPDATE proc_summary SET "
                "avgCpuUsage = " + std::to_string(wavg(ca, p.avgCpuUsage)) + ", "
                "peakCpuUsage = " + std::to_string(std::max(ck, p.peakCpuUsage)) + ", "
                "avgMemoryMB = " + std::to_string(wavg(ma, p.avgMemoryMB)) + ", "
                "peakMemoryMB = " + std::to_string(std::max(mk, p.peakMemoryMB)) + ", "
                "sampleCount = " + std::to_string(total) + " "
                "WHERE date = '" + p.date + "' "
                "  AND procName = ? ");

            if (stmt->HasError()) {
                spdlog::error("SummaryStore: flushProcSummaries prepare {}", stmt->GetError());
                return;
            }

            auto r = stmt->Execute(p.procName);
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

    for (const auto& t : targets) {
        auto stmt = con.Prepare(
            "SELECT avgCpuUsage, peakCpuUsage, "
            "       avgMemoryMB, peakMemoryMB, "
            "       avgNetMbps, peakNetMbps, "
            "       peakThreadCount, peakHandleCount, "
            "       totalRuntime, sampleCount "
            "FROM target_summary "
            "WHERE date = '" + t.date + "' "
            "  AND targetName = ? ");
        if (stmt->HasError()) {
            spdlog::error("SummaryStore: flushTargetSummaries prepare {}", stmt->GetError());
            return;
        }

        auto r = stmt->Execute(t.targetName);
        auto chunk = (r->HasError()) ? nullptr : r->Fetch();

        if (!chunk || chunk->size() == 0) {
            // 신규 삽입
            auto stmt = con.Prepare(
                "INSERT OR REPLACE INTO target_summary VALUES ("
                "'" + t.date + "', "
                "?, "
                "?, " +
                std::to_string(t.avgCpuUsage) + ", " +
                std::to_string(t.peakCpuUsage) + ", " +
                std::to_string(t.avgMemoryMB) + ", " +
                std::to_string(t.peakMemoryMB) + ", " +
                std::to_string(t.avgNetMbps) + ", " +
                std::to_string(t.peakNetMbps) + ", " +
                std::to_string(t.peakThreadCount) + ", " +
                std::to_string(t.peakHandleCount) + ", " +
                std::to_string(t.runTime) + ", " +
                std::to_string(t.sampleCount) + ")");
            if (stmt->HasError())
            {
                spdlog::error("SummaryStore: flushTargetSummaries prepare {}", stmt->GetError());
                return;
            }
            auto r = stmt->Execute(t.targetName, t.exePath);
        }
        else {
            // 같은 날짜
            double ca = safeStod(chunk->GetValue(0, 0).ToString());
            double ck = safeStod(chunk->GetValue(1, 0).ToString());
            double ma = safeStod(chunk->GetValue(2, 0).ToString());
            double mk = safeStod(chunk->GetValue(3, 0).ToString());
            double na = safeStod(chunk->GetValue(4, 0).ToString());
            double nk = safeStod(chunk->GetValue(5, 0).ToString());
            uint32_t pt = safeStoul(chunk->GetValue(6, 0).ToString());
            uint32_t ph = safeStoul(chunk->GetValue(7, 0).ToString());
            double el = safeStod(chunk->GetValue(8, 0).ToString());
            int    n = safeStoi(chunk->GetValue(9, 0).ToString());
            int    ns = t.sampleCount;
            int    total = n + ns;

            auto wavg = [&](double a, double b) {
                return (a * n + b * ns) / total; };

            auto stmt = con.Prepare(
                "UPDATE target_summary SET "
                "exePath = , "
                "avgCpuUsage = " + std::to_string(wavg(ca, t.avgCpuUsage)) + ", "
                "peakCpuUsage = " + std::to_string(std::max(ck, t.peakCpuUsage)) + ", "
                "avgMemoryMB = " + std::to_string(wavg(ma, t.avgMemoryMB)) + ", "
                "peakMemoryMB = " + std::to_string(std::max(mk, t.peakMemoryMB)) + ", "
                "avgNetMbps = " + std::to_string(wavg(na, t.avgNetMbps)) + ", "
                "peakNetMbps = " + std::to_string(std::max(nk, t.peakNetMbps)) + ", "
                "peakThreadCount = " + std::to_string(std::max(pt, t.peakThreadCount)) + ", "
                "peakHandleCount = " + std::to_string(std::max(ph, t.peakHandleCount)) + ", "
                "totalRuntime = " + std::to_string(std::max(el, t.runTime)) + ", "
                "sampleCount = " + std::to_string(total) + " "
                "WHERE date = '" + t.date + "' "
                "  AND targetName = ? ");
            if (stmt->HasError())
            {
                spdlog::error("SummaryStore: flushTargetSummaries prepare {}", stmt->GetError());
                return;
            }

            auto r = stmt->Execute(t.exePath, t.targetName);
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
