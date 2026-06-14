#include "analysis/SessionReport.h"
#include "Config.h"


SessionReport::SessionReport(duckdb::Connection& con) :con(con) {}

std::string SessionReport::todayStr() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

double SessionReport::safeStod(const std::string& s) {
    try { return std::stod(s); }
    catch (...) { return 0.0; }
}

// -------------------------------------------------------
// 시스템 분석
// -------------------------------------------------------
SessionSysSummary SessionReport::analyzeSys() {
    SessionSysSummary result;
    result.date = todayStr();

    auto r = con.Query(R"(
        SELECT
            ROUND(AVG(cpuTotal),    2) AS avgCpu,
            ROUND(MAX(cpuTotal),    2) AS peakCpu,
            ROUND(AVG(memUsagePercent), 2) AS avgMem,
            ROUND(MAX(memUsagePercent), 2) AS peakMem,
            ROUND(AVG(diskReadKBs + diskWriteKBs), 2) AS avgDisk,
            ROUND(MAX(diskReadKBs + diskWriteKBs), 2) AS peakDisk,
            ROUND(AVG(netSentKbps + netRecvKbps),  2) AS avgNet,
            ROUND(MAX(netSentKbps + netRecvKbps),  2) AS peakNet,
            COUNT(*) AS samples
        FROM sys_snapshot
    )");

    if (r->HasError()) return result;
    auto chunk = r->Fetch();
    if (!chunk || chunk->size() == 0) return result;

    result.avgCpuTotal = safeStod(chunk->GetValue(0, 0).ToString());
    result.peakCpuTotal = safeStod(chunk->GetValue(1, 0).ToString());
    result.avgMemUsagePercent = safeStod(chunk->GetValue(2, 0).ToString());
    result.peakMemUsagePercent = safeStod(chunk->GetValue(3, 0).ToString());
    result.avgDiskKBs = safeStod(chunk->GetValue(4, 0).ToString());
    result.peakDiskKBs = safeStod(chunk->GetValue(5, 0).ToString());
    result.avgNetKbps = safeStod(chunk->GetValue(6, 0).ToString());
    result.peakNetKbps = safeStod(chunk->GetValue(7, 0).ToString());
    result.sampleCount = std::stoi(chunk->GetValue(8, 0).ToString());

    return result;
}

// -------------------------------------------------------
// 프로세스 분석
// -------------------------------------------------------
std::vector<SessionProcSummary> SessionReport::analyzeProcs(int topN) {
    std::vector<SessionProcSummary> result;
    std::string date = todayStr();

    std::string sql =
        "SELECT procName, "
        "  ROUND(AVG(procCpuUsage), 2) AS avgCpu, "
        "  ROUND(MAX(procCpuUsage), 2) AS peakCpu, "
        "  ROUND(AVG(procMemoryMB), 2) AS avgMem, "
        "  ROUND(MAX(procMemoryMB), 2) AS peakMem, "
        "  COUNT(*) AS samples "
        "FROM proc_snapshot "
        "GROUP BY procName "
        "ORDER BY avgCpu DESC "
        "LIMIT " + std::to_string(topN);

    auto r = con.Query(sql);
    if (r->HasError()) return result;

    auto chunk = r->Fetch();
    while (chunk) {
        for (size_t i = 0; i < chunk->size(); ++i) {
            SessionProcSummary p;
            p.date = date;
            p.procName = chunk->GetValue(0, i).ToString();
            p.avgCpuUsage = safeStod(chunk->GetValue(1, i).ToString());
            p.peakCpuUsage = safeStod(chunk->GetValue(2, i).ToString());
            p.avgMemoryMB = safeStod(chunk->GetValue(3, i).ToString());
            p.peakMemoryMB = safeStod(chunk->GetValue(4, i).ToString());
            p.sampleCount = std::stoi(chunk->GetValue(5, i).ToString());
            result.push_back(std::move(p));
        }
        chunk = r->Fetch();
    }

    return result;
}

// -------------------------------------------------------
// 타겟 분석
// -------------------------------------------------------
std::vector<SessionTargetSummary> SessionReport::analyzeTargets() {
    std::vector<SessionTargetSummary> result;
    std::string date = todayStr();

    auto r = con.Query(
        "SELECT "
        "    targetName, "
        "    MAX(exePath)                     AS exePath, "
        "    ROUND(AVG(totalCpuUsage), 2)     AS avgCpu, "
        "    ROUND(MAX(totalCpuUsage), 2)     AS peakCpu, "
        "    ROUND(AVG(totalMemoryMB), 2)     AS avgMem, "
        "    ROUND(MAX(totalMemoryMB), 2)     AS peakMem, "
        "    ROUND(AVG(totalNetMbps), 2)      AS avgNet, "
        "    ROUND(MAX(totalNetMbps), 2)      AS peakNet, "
        "    MAX(totalThreadCount)            AS peakThread, "
        "    MAX(totalHandleCount)            AS peakHandle, "
        "    ROUND(COUNT(*) * " + std::to_string(Config::MIDDLE_COLLECT_INTERVAL) + ", 1) AS runTime, "
        "    COUNT(*)                         AS samples "
        "FROM target_snapshot "
        "GROUP BY targetName"
    );

    if (r->HasError()) return result;

    auto chunk = r->Fetch();
    while (chunk) {
        for (size_t i = 0; i < chunk->size(); ++i)
        {
            SessionTargetSummary t;
            t.date = date;
            t.targetName = chunk->GetValue(0, i).ToString();
            t.exePath = chunk->GetValue(1, i).ToString();
            t.avgCpuUsage = safeStod(chunk->GetValue(2, i).ToString());
            t.peakCpuUsage = safeStod(chunk->GetValue(3, i).ToString());
            t.avgMemoryMB = safeStod(chunk->GetValue(4, i).ToString());
            t.peakMemoryMB = safeStod(chunk->GetValue(5, i).ToString());
            t.avgNetMbps = safeStod(chunk->GetValue(6, i).ToString());
            t.peakNetMbps = safeStod(chunk->GetValue(7, i).ToString());
            t.peakThreadCount = static_cast<uint32_t>(std::stoul(chunk->GetValue(8, i).ToString()));
            t.peakHandleCount = static_cast<uint32_t>(std::stoul(chunk->GetValue(9, i).ToString()));
            t.runTime = safeStod(chunk->GetValue(10, i).ToString());
            t.sampleCount = std::stoi(chunk->GetValue(11, i).ToString());
            result.push_back(std::move(t));
        }
        chunk = r->Fetch();
    }

    return result;
}

// -------------------------------------------------------
// 전체 출력
// -------------------------------------------------------
void SessionReport::printReport() {
    printf("\n+%s+\n", std::string(66, '=').c_str());
    printf("|%-66s|\n", "  SysMonitor Session Report");
    printf("+%s+\n", std::string(66, '=').c_str());

    auto sys = analyzeSys();
    auto procs = analyzeProcs(Config::DEFAULT_TOP_N);
    auto targets = analyzeTargets();

    printSysReport(sys);
    printProcReport(procs);
    printTargetReport(targets);
    printAnomalies();

    printf("+%s+\n", std::string(66, '=').c_str());
}

void SessionReport::printSysReport(const SessionSysSummary& s) {
    printf("\n[ System Summary ] (%d samples)\n", s.sampleCount);
    printf("  %-20s %8s %8s\n", "", "AVG", "PEAK");
    printf("  %-20s %7.1f%% %7.1f%%\n",
        "CPU", s.avgCpuTotal, s.peakCpuTotal);
    printf("  %-20s %7.1f%% %7.1f%%\n",
        "Memory", s.avgMemUsagePercent, s.peakMemUsagePercent);
    printf("  %-20s %6.1fKB/s %6.1fKB/s\n",
        "Disk", s.avgDiskKBs, s.peakDiskKBs);
    printf("  %-20s %5.1fKbps %5.1fKbps\n",
        "Network", s.avgNetKbps, s.peakNetKbps);
}

void SessionReport::printProcReport(
    const std::vector<SessionProcSummary>& procs) {
    printf("\n[ Process Top %zu ]\n", procs.size());
    printf("  %-20s %8s %8s %8s %8s\n",
        "Name", "avgCPU%", "peakCPU", "avgMem", "peakMem");
    printf("  %s\n", std::string(56, '-').c_str());
    for (const auto& p : procs) {
        std::string name = p.procName;
        if (name.size() > 19) name = name.substr(0, 16) + "...";
        printf("  %-20s %7.1f%% %7.1f%% %7.1fMB %7.1fMB\n",
            name.c_str(),
            p.avgCpuUsage, p.peakCpuUsage,
            p.avgMemoryMB, p.peakMemoryMB);
    }
}

void SessionReport::printTargetReport(
    const std::vector<SessionTargetSummary>& targets) {
    if (targets.empty()) return;

    printf("\n[ Target Process Summary ]\n");
    for (const auto& t : targets) {
        printf("  +-- %s\n", t.targetName.c_str());
        printf("  |  Path: %s\n", t.exePath.c_str());
        printf("  |  %-14s %7.1f%% (peak %.1f%%)\n",
            "CPU(Total)", t.avgCpuUsage, t.peakCpuUsage);
        printf("  |  %-14s %7.1fMB (peak %.1fMB)\n",
            "Memory", t.avgMemoryMB, t.peakMemoryMB);
        printf("  |  %-14s %7.2fMbps (peak %.2fMbps)\n",
            "Network", t.avgNetMbps, t.peakNetMbps);
        printf("  |  %-14s %7u\n",
            "Peak Threads", t.peakThreadCount);
        printf("  |  %-14s %7u\n",
            "Peak Handles", t.peakHandleCount);
        printf("  |  %-14s %7.0fsec (%.1fmin)\n",
            "Run Time", t.runTime,
            t.runTime / 60.0);
        printf("  +%s\n", std::string(44, '-').c_str());
    }
}

void SessionReport::printAnomalies() {
    printf("\n[ Anomaly Detection ]\n");

    // CPU 90% 이상 지속
    auto r1 = con.Query(
        "SELECT procName, COUNT(*) AS cnt "
        "FROM proc_snapshot WHERE procCpuUsage > 90 "
        "GROUP BY procName HAVING cnt >= 10 "
        "ORDER BY cnt DESC LIMIT 5");
    if (!r1->HasError()) {
        auto chunk = r1->Fetch();
        bool found = false;
        while (chunk) {
            for (size_t i = 0; i < chunk->size(); ++i) {
                printf("  [!] CPU 90%% over: %s (%s sec)\n",
                    chunk->GetValue(0, i).ToString().c_str(),
                    chunk->GetValue(1, i).ToString().c_str());
                found = true;
            }
            chunk = r1->Fetch();
        }
        if (!found) printf("  [OK] CPU fine \n");
    }

    // 메모리 500MB 이상 증가
    auto r2 = con.Query(
        "SELECT procName, "
        "  ROUND(MIN(procMemoryMB),1) AS s, "
        "  ROUND(MAX(procMemoryMB),1) AS e, "
        "  ROUND(MAX(procMemoryMB)-MIN(procMemoryMB),1) AS g "
        "FROM proc_snapshot "
        "GROUP BY procName HAVING g > 500 "
        "ORDER BY g DESC LIMIT 5");
    if (!r2->HasError()) {
        auto chunk = r2->Fetch();
        bool found = false;
        while (chunk) {
            for (size_t i = 0; i < chunk->size(); ++i) {
                printf("  [!] Suspected memory leak: %s "
                    "(%.1f→%.1fMB, +%.1fMB)\n",
                    chunk->GetValue(0, i).ToString().c_str(),
                    safeStod(chunk->GetValue(1, i).ToString()),
                    safeStod(chunk->GetValue(2, i).ToString()),
                    safeStod(chunk->GetValue(3, i).ToString()));
                found = true;
            }
            chunk = r2->Fetch();
        }
        if (!found) printf("  [OK] No memory leaks\n");
    }

    // 타겟 핸들 1000 초과 -> 브라우저는 일반적으로 5000개
    if (con.Query("SELECT * FROM target_snapshot LIMIT 1")
        ->RowCount() > 0) {
        auto r3 = con.Query(
            "SELECT targetName, MAX(totalHandleCount) AS maxH "
            "FROM target_snapshot "
            "GROUP BY targetName HAVING maxH > 5000 "
            "ORDER BY maxH DESC");
        if (!r3->HasError()) {
            auto chunk = r3->Fetch();
            while (chunk) {
                for (size_t i = 0; i < chunk->size(); ++i)
                    printf("  [!] Suspected handle leak: %s (MAX %s)\n",
                        chunk->GetValue(0, i).ToString().c_str(),
                        chunk->GetValue(1, i).ToString().c_str());
                chunk = r3->Fetch();
            }
        }
    }
}

