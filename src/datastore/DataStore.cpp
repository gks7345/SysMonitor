// DataStore.cpp
#include "datastore/DataStore.h"
#include <spdlog/spdlog.h>

DataStore::DataStore(size_t procCap, size_t sysCap, size_t targetCap)
    : procsData(procCap)
    , sysData(sysCap)
    , targetData(targetCap)
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
    strftime(buf, sizeof(buf), "reports/data/SysMonitor_data_%Y-%m-%d.db", &tm);

    // reports 폴더 생성
    std::filesystem::create_directories("reports/data");


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
    // 시스템 테이블 — SnapShotSysData 구조 반영
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS sys_snapshot (
            timestamp           BIGINT,
            diskTimestamp       BIGINT,
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

    // 타겟 프로세스 테이블
    // 메인 + 합계 테이블
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS target_snapshot (
            timestamp           BIGINT,
            targetName          VARCHAR,
            exePath             VARCHAR,
            pid                 INTEGER,
            cpuUsage            DOUBLE,
            memoryMB            DOUBLE,
            privateMemoryMB     DOUBLE,
            virtualMemoryMB     DOUBLE,
            diskReadMBs         DOUBLE,
            diskWriteMBs        DOUBLE,
            netSentMbps         DOUBLE,
            netRecvMbps         DOUBLE,
            threadCount         INTEGER,
            handleCount         INTEGER,
            gdiObjectCount      INTEGER,
            pageFaultRate       DOUBLE,
            elapsedSec          DOUBLE,
            totalCpuUsage       DOUBLE,
            totalMemoryMB       DOUBLE,
            totalDiskMBs        DOUBLE,
            totalNetMbps        DOUBLE,
            totalThreadCount    INTEGER,
            totalHandleCount    INTEGER,
            totalProcessCount   INTEGER
        )
    )");

    // 자식 프로세스 테이블
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS target_child_snapshot (
            timestamp           BIGINT,
            targetName          VARCHAR,
            pid                 INTEGER,
            processName         VARCHAR,
            cpuUsage            DOUBLE,
            memoryMB            DOUBLE,
            privateMemoryMB     DOUBLE,
            virtualMemoryMB     DOUBLE,
            diskReadMBs         DOUBLE,
            diskWriteMBs        DOUBLE,
            netSentMbps         DOUBLE,
            netRecvMbps         DOUBLE,
            threadCount         INTEGER,
            handleCount         INTEGER,
            gdiObjectCount      INTEGER,
            pageFaultRate       DOUBLE
        )
    )");

    // 네트워크 연결 테이블
    con.Query(R"(
        CREATE TABLE IF NOT EXISTS target_connection_snapshot (
            timestamp           BIGINT,
            targetName          VARCHAR,
            pid                 INTEGER,
            protocol            VARCHAR,
            localAddr           VARCHAR,
            remoteAddr          VARCHAR,
            state               VARCHAR
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

void DataStore::pushTargetData(const SnapshotTargetData& data) {
    std::lock_guard<std::mutex> lock(targetMtx);
    targetData.enQueue(data);
}

double DataStore::safeDouble(double v) {
    if (std::isnan(v) || std::isinf(v)) return 0.0;
    return v;
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
    try {
        for (const auto& snap : items) {
            int64_t ts = toTimestamp(snap.timestamp);

            for (const auto& p : snap.procs) {
                appender.BeginRow();
                appender.Append(ts);
                appender.Append(static_cast<int32_t>(snap.topN));
                appender.Append(duckdb::string_t(snap.sortCriterion));
                appender.Append(static_cast<int32_t>(p.procID));
                appender.Append(duckdb::string_t(p.procName));
                appender.Append(safeDouble(p.procCpuUsage));
                appender.Append(safeDouble(p.procMemoryMB));
                appender.Append(safeDouble(p.procPrivateMemoryMB));
                appender.Append(safeDouble(p.procDiskReadMBs));
                appender.Append(safeDouble(p.procDiskWriteMBs));
                appender.Append(safeDouble(p.procNetSentMbps));
                appender.Append(safeDouble(p.procNetRecvMbps));
                appender.EndRow();
                rowCount++;
            }
        }
        appender.Close();
        spdlog::info("DataStore: proc_snapshot {}th snapshot flush", items.size());
        spdlog::info("INSERT Complete {} row", rowCount);
    }
    catch (const duckdb::InvalidInputException& e) {
        spdlog::error("flushProcsToDB InvalidInput: {}", e.what());
    }
    catch (const std::exception& e) {
        spdlog::error("flushProcsToDB 예외: {}", e.what());
    }

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
    try {
        for (const auto& s : items) {
            appender.BeginRow();
            appender.Append(toTimestamp(s.timestamp));
            appender.Append(toTimestamp(s.disk.lastTime));

            // CPU
            appender.Append(safeDouble(s.cpu.cpuTotal));
            appender.Append(safeDouble(s.cpu.cpuFreqGHz));
            appender.Append(safeDouble(s.cpu.cpuUser));
            appender.Append(safeDouble(s.cpu.cpuKernel));
            appender.Append(safeDouble(s.cpu.cpuQueueLength));

            // MEM
            appender.Append(safeDouble(s.mem.memTotalMB));
            appender.Append(safeDouble(s.mem.memUsagePercent));
            appender.Append(safeDouble(s.mem.memUsedMB));
            appender.Append(safeDouble(s.mem.memAvailMB));
            appender.Append(safeDouble(s.mem.commitMemPercent));
            appender.Append(safeDouble(s.mem.committedMemGB));
            appender.Append(safeDouble(s.mem.commitLimitGB));

            // DISK
            appender.Append(safeDouble(s.disk.diskReadKBs));
            appender.Append(safeDouble(s.disk.diskWriteKBs));

            // NET
            appender.Append(safeDouble(s.net.netSentKbps));
            appender.Append(safeDouble(s.net.netRecvKbps));

            appender.EndRow();
        }
        appender.Close();
        spdlog::info("DataStore: sys_snapshot {}th flush", items.size());
    }
    catch (const duckdb::InvalidInputException& e) {
        spdlog::error("flushSysToDB InvalidInput: {}", e.what());
    }
    catch (const std::exception& e) {
        spdlog::error("flushSysToDB 예외: {}", e.what());
    }
}

void DataStore::flushTargetToDB() {
    std::vector<SnapshotTargetData> items;
    {
        std::lock_guard<std::mutex> lock(targetMtx);
        items = targetData.deQueue();
    }
    if (items.empty()) return;

    std::lock_guard<std::mutex> dbLock(dbMtx);
    try {
        duckdb::Appender mainApp(con, "target_snapshot");
        duckdb::Appender childApp(con, "target_child_snapshot");
        duckdb::Appender connApp(con, "target_connection_snapshot");
        for (const auto& snap : items) {
            int64_t ts = toTimestamp(snap.timestamp);
            for (const auto& t : snap.targets) {
                mainApp.BeginRow();
                mainApp.Append(ts);
                mainApp.Append(duckdb::string_t(t.targetName));
                mainApp.Append(duckdb::string_t(t.exePath));
                mainApp.Append(static_cast<int32_t>(t.pid));
                mainApp.Append(safeDouble(t.cpuUsage));
                mainApp.Append(safeDouble(t.memoryMB));
                mainApp.Append(safeDouble(t.privateMemoryMB));
                mainApp.Append(safeDouble(t.virtualMemoryMB));
                mainApp.Append(safeDouble(t.diskReadMBs));
                mainApp.Append(safeDouble(t.diskWriteMBs));
                mainApp.Append(safeDouble(t.netSentMbps));
                mainApp.Append(safeDouble(t.netRecvMbps));
                mainApp.Append(static_cast<int32_t>(t.threadCount));
                mainApp.Append(static_cast<int32_t>(t.handleCount));
                mainApp.Append(static_cast<int32_t>(t.gdiObjectCount));
                mainApp.Append(safeDouble(t.pageFaultRate));
                mainApp.Append(safeDouble(t.elapsedSec));
                mainApp.Append(safeDouble(t.totalCpuUsage));
                mainApp.Append(safeDouble(t.totalMemoryMB));
                mainApp.Append(safeDouble(t.totalDiskMBs));
                mainApp.Append(safeDouble(t.totalNetMbps));
                mainApp.Append(static_cast<int32_t>(t.totalThreadCount));
                mainApp.Append(static_cast<int32_t>(t.totalHandleCount));
                mainApp.Append(static_cast<int32_t>(t.totalProcessCount));
                mainApp.EndRow();

                // 메인 네트워크 연결
                for (const auto& conn : t.connections) {
                    connApp.BeginRow();
                    connApp.Append(ts);
                    connApp.Append(duckdb::string_t(t.targetName));
                    connApp.Append(static_cast<int32_t>(t.pid));
                    connApp.Append(duckdb::string_t(conn.protocol));
                    connApp.Append(duckdb::string_t(conn.localAddr));
                    connApp.Append(duckdb::string_t(conn.remoteAddr));
                    connApp.Append(duckdb::string_t(conn.state));
                    connApp.EndRow();
                }

                // 자식 프로세스
                for (const auto& c : t.children) {
                    childApp.BeginRow();
                    childApp.Append(ts);
                    childApp.Append(duckdb::string_t(t.targetName));
                    childApp.Append(static_cast<int32_t>(c.pid));
                    childApp.Append(duckdb::string_t(c.processName));
                    childApp.Append(safeDouble(c.cpuUsage));
                    childApp.Append(safeDouble(c.memoryMB));
                    childApp.Append(safeDouble(c.privateMemoryMB));
                    childApp.Append(safeDouble(c.virtualMemoryMB));
                    childApp.Append(safeDouble(c.diskReadMBs));
                    childApp.Append(safeDouble(c.diskWriteMBs));
                    childApp.Append(safeDouble(c.netSentMbps));
                    childApp.Append(safeDouble(c.netRecvMbps));
                    childApp.Append(static_cast<int32_t>(c.threadCount));
                    childApp.Append(static_cast<int32_t>(c.handleCount));
                    childApp.Append(static_cast<int32_t>(c.gdiObjectCount));
                    childApp.Append(safeDouble(c.pageFaultRate));
                    childApp.EndRow();

                    // 자식 네트워크 연결
                    for (const auto& conn : c.connections) {
                        connApp.BeginRow();
                        connApp.Append(ts);
                        connApp.Append(duckdb::string_t(t.targetName));
                        connApp.Append(static_cast<int32_t>(c.pid));
                        connApp.Append(duckdb::string_t(conn.protocol));
                        connApp.Append(duckdb::string_t(conn.localAddr));
                        connApp.Append(duckdb::string_t(conn.remoteAddr));
                        connApp.Append(duckdb::string_t(conn.state));
                        connApp.EndRow();
                    }
                }
            }
        }

        mainApp.Close();
        childApp.Close();
        connApp.Close();
        con.Query("CHECKPOINT");
        spdlog::info("DataStore: target flush clear ({} snapshot)", items.size());
    }
    catch (const std::exception& e) {
        spdlog::error("flushTargetToDB exception: {}", e.what());
    }
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

duckdb::Connection& DataStore::getConnection() {
    std::lock_guard<std::mutex> lock(dbMtx);
    return con;
};

