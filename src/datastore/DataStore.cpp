// DataStore.cpp
#include "datastore/DataStore.h"
#include <spdlog/spdlog.h>

DataStore::DataStore(size_t procCap, size_t sysCap)
    : procsData(procCap)
    , sysData(sysCap)
    , currentDbPath(makeDailyDbPath())
    , db(currentDbPath)  // 날짜 기반 파일명
    , con(db)

{
    initDB();
    spdlog::info("DB file location: {}", currentDbPath);  // ← 실제 경로 출력
}


// 오늘 날짜로 파일명 생성
std::string DataStore::makeDailyDbPath() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);

    char buf[64];
    strftime(buf, sizeof(buf), "reports/SysMonitor_%Y-%m-%d.db", &tm);

    // reports 폴더 생성
    std::filesystem::create_directories("reports");


    return std::string(buf);
}

void DataStore::checkDailyRotation() {
    std::string newPath = makeDailyDbPath();

    if (newPath == currentDbPath) return;

    // 날짜 바뀌면 새 파일로 교체
    flushProcsToDB();
    flushSysToDB();

    // 새 DB 연결
    db = duckdb::DuckDB(newPath);
    con = duckdb::Connection(db);
    initDB();
    currentDbPath = newPath;

    spdlog::info("DataStore: create new DBfile {}", newPath);
}

void DataStore::initDB() {
    // 프로세스 테이블
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS proc_snapshot (
            timestamp           BIGINT,
            topN                INTEGER,
            sortCriterion       VARCHAR,
            procID              INTEGER,
            procName            VARCHAR,
            procCpuUsage        DOUBLE,
            procMemoryMB        DOUBLE,
            procPrivateMemoryMB DOUBLE,
            procDiskReadMBs     DOUBLE,
            procDiskWriteMBs    DOUBLE,
            procNetSentMbps     DOUBLE,
            procNetRecvMbps     DOUBLE
        )
    )");

    // 시스템 테이블 — SnapShotSysData 구조 반영
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS sys_snapshot (
            timestamp           BIGINT,

            cpuTotal            DOUBLE,
            cpuFreqGHz          DOUBLE,
            cpuUser             DOUBLE,
            cpuKernel           DOUBLE,
            cpuQueueLength      DOUBLE,

            memTotalMB          DOUBLE,
            memUsagePercent     DOUBLE,
            memUsedMB           DOUBLE,
            memAvailMB          DOUBLE,
            commitMemPercent    DOUBLE,
            committedMemGB      DOUBLE,
            commitLimitGB       DOUBLE,

            diskReadKBs         DOUBLE,
            diskWriteKBs        DOUBLE,

            netSentKbps         DOUBLE,
            netRecvKbps         DOUBLE
        )
    )");
}

void DataStore::pushProcsData(const SnapshotProcData& data) {
    std::lock_guard<std::mutex> lock(procMtx);
    procsData.enQueue(data);
}

void DataStore::pushSysData(const SnapshotSysData& data) {
    std::lock_guard<std::mutex> lock(sysMtx);
    sysData.enQueue(data);
}

// SnapShotProcData → DuckDB
// SnapShotProcData.procs는 vector<SnapShotProc>이므로
// 하나의 스냅샷에서 여러 행 INSERT
void DataStore::flushProcsToDB() {
    spdlog::info("flush start ringbuffer size={}", procsData.getSize());
    std::vector<SnapshotProcData> items;
    {
        std::lock_guard<std::mutex> lock(procMtx);
        items = procsData.deQueue();
    }
    spdlog::info("deQueue after items.size()={}", items.size());

    if (items.empty()) return;

    std::lock_guard<std::mutex> lock(dbMtx);
    duckdb::Appender appender(con, "proc_snapshot");

    int rowCount = 0;

    for (const auto& snap : items) {
        int64_t ts = toTimestamp(snap.timestamp);

        for (const auto& p : snap.procs) {
            appender.BeginRow();
            appender.Append(ts);
            appender.Append(static_cast<int32_t>(snap.topN));
            appender.Append(duckdb::string_t(snap.sortCriterion));
            appender.Append(p.procID);
            appender.Append(duckdb::string_t(p.procName));
            appender.Append(p.procCpuUsage);
            appender.Append(p.procMemoryMB);
            appender.Append(p.procPrivateMemoryMB);
            appender.Append(p.procDiskReadMBs);
            appender.Append(p.procDiskWriteMBs);
            appender.Append(p.procNetSentMbps);
            appender.Append(p.procNetRecvMbps);
            appender.EndRow();
        }
    }
    appender.Close();
    spdlog::info("DataStore: proc_snapshot {}th snapshot flush", items.size());
    spdlog::info("INSERT Complete {} row", rowCount);

    // 즉시 디스크에 반영
    con.Query("CHECKPOINT");
}

// SnapShotSysData → DuckDB
void DataStore::flushSysToDB() {

    std::vector<SnapshotSysData> items;
    {
        std::lock_guard<std::mutex> lock(sysMtx);
        items = sysData.deQueue();
    }
    if (items.empty()) return;

    std::lock_guard<std::mutex> lock(dbMtx);
    duckdb::Appender appender(con, "sys_snapshot");
    for (const auto& s : items) {
        appender.BeginRow();
        appender.Append(toTimestamp(s.timestamp));

        // CPU
        appender.Append(s.cpu.cpuTotal);
        appender.Append(s.cpu.cpuFredGHz);
        appender.Append(s.cpu.cpuUser);
        appender.Append(s.cpu.cpuKernel);
        appender.Append(s.cpu.cpuQueueLength);

        // MEM
        appender.Append(s.mem.memTotalMB);
        appender.Append(s.mem.memUsagePercent);
        appender.Append(s.mem.memUsedMB);
        appender.Append(s.mem.memAvailMB);
        appender.Append(s.mem.commitMemPercent);
        appender.Append(s.mem.committedMemGB);
        appender.Append(s.mem.commitLimitGB);

        // DISK
        appender.Append(s.disk.diskReadKBs);
        appender.Append(s.disk.diskWriteKBs);

        // NET
        appender.Append(s.net.netSentKbps);
        appender.Append(s.net.netRecvKbps);

        appender.EndRow();
    }
    appender.Close();
    spdlog::info("DataStore: sys_snapshot {}th flush", items.size());
}

std::string DataStore::queryReport(const std::string& sql) {
    std::lock_guard<std::mutex> lock(dbMtx);
    auto result = con.Query(sql);
    if (result->HasError()) {
        spdlog::error("DataStore: query error {}", result->GetError());
        return "";
    }
    return result->ToString();
}