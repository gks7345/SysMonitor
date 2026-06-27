// SysMonitor.cpp : 애플리케이션의 진입점을 정의합니다.
//
#include "SysMonitor.h"


int main()
{
    ULONGLONG bootTime = GetTickCount64();
    DWORD delay = 1000 - (bootTime % 1000);
    Sleep(delay);

    SystemCollector sc;
    ProcessCollector proc(Config::DEFAULT_TOP_N);
    TargetCollector target(proc.getEtwNet());
    SummaryStore summary;
    DataStore dataStore(Config::PROC_BUFFER_CAPACITY, Config::SYS_BUFFER_CAPACITY);
    SessionReport report(dataStore);

    target.registerTarget("whale");
    target.registerTarget("chrome");

    // DataStore에서 마지막 세션 주입
    for (const auto& name : target.getRegisteredNames()) {
        auto last = summary.getTargetLastSession(name);
        target.updateLastSession(name, last);
    }

    int tick = 0;
    auto lastFlush = std::chrono::steady_clock::now();
    proc.setCriterion(SortCriterion::CPU);

    bool keep_running = true;

    while (keep_running) {
        auto loopStart = std::chrono::steady_clock::now();

        //1초 주기 수집
        sc.collectMiddle();
        proc.collectProc();
        target.collect();

        for (const auto& name : target.getPendingDeactivated()) {
            dataStore.flushTargetToDB();

            auto last = report.getSessionTargetSummaryOne(name);
            target.updateLastSession(name, last);
        }
        target.clearPendingDeactivated();

        if (tick % Config::SLOW_COLLECT_INTERVAL == 0) {
            //2초 주기 수집
            sc.collectSlow();
        }

        sc.printToConsole();
        printf("%s\n", std::string(Config::CONSOLE_WIDTH, '-').c_str());
        proc.sortProc();
        proc.printToConsole();

        target.printToConsole();

        SnapshotSysData sysSnap = sc.makeSnapshot();
        SnapshotProcData procSnap = proc.makeSnapshot();
        SnapshotTargetData targetSnap = target.getSnapshot();
        dataStore.pushSysData(sysSnap);
        dataStore.pushProcsData(procSnap);
        dataStore.pushTargetData(targetSnap);
        spdlog::info("ringbuffer size={}", dataStore.getProcsSize());

        // 60초마다 DuckDB flush
        if (tick > 0 && tick % Config::FLUSH_INTERVAL_TICKS == 0) {
            dataStore.flushProcsToDB();
            dataStore.flushSysToDB();
            dataStore.flushTargetToDB();
        }

        tick++;

        // 1. 키보드 입력이 있었는지 루프를 멈추지 않고 확인
        if (_kbhit()) {
            char ch = _getch(); // 누른 키 값 가져오기
            if (ch == 'q' || ch == 'Q') {
                std::cout << "\n['q' key detected] SysMonitor Quit.\n";
                keep_running = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }


    dataStore.flushProcsToDB();
    dataStore.flushSysToDB();
    dataStore.flushTargetToDB();

    std::string resultProc = dataStore.queryReport(
        "SELECT * FROM proc_snapshot LIMIT 10");
    printf("%s\n", resultProc.c_str());

    std::string resultSys = dataStore.queryReport(
        "SELECT * FROM sys_snapshot LIMIT 10");
    printf("%s\n", resultSys.c_str());

    std::string resultTarget = dataStore.queryReport(
        "SELECT * FROM target_snapshot JOIN target_child_snapshot ON target_snapshot.timestamp = target_child_snapshot.timestamp LIMIT 10");
    printf("%s\n", resultTarget.c_str());


    summary.flushProcSummaries(report.analyzeProcs());
    summary.flushSysSummary(report.analyzeSys());
    summary.flushTargetSummaries(report.analyzeTargets());
    std::string resultSummaryTarget = summary.queryReport(
        "SELECT * FROM target_summary LIMIT 10");
    printf("%s\n", resultSummaryTarget.c_str());

    // SessionReport 분석 + 출력
    report.printReport();
}
