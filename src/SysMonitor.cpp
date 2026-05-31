// SysMonitor.cpp : 애플리케이션의 진입점을 정의합니다.
//
#include "SysMonitor.h"

// 1. 원자적(Atomic) 플래그 선언 (여러 스레드에서 안전하게 접근 가능)
std::atomic<bool> keep_running(true);




int main()
{

    ULONGLONG bootTime = GetTickCount64();
    DWORD delay = 1000 - (bootTime % 1000);
    Sleep(delay);

    SystemCollector sc;
    ProcessCollector proc;
    DataStore dataStore;

    int tick = 0;
    auto lastFlush = std::chrono::steady_clock::now();
    proc.setCritrion(SortCriterion::NET);


    while (keep_running) {
        auto loopStart = std::chrono::steady_clock::now();

        //1초 주기 수집
        sc.collectMiddle();
        proc.collectProc();


        if (tick % 2 == 0) {
            //2초 주기 수집
            sc.collectSlow();
            //proc.collectSlow();
        }

        sc.printToConsole();

        proc.aggregateToParents();
        proc.sortProc();

        proc.printToConsole();

        SnapShotSysData sysSnap = sc.makeSnapShot();
        SnapShotProcData procSnap = proc.makeSnapShot();
        dataStore.pushSysData(sysSnap);
        dataStore.pushProcsData(procSnap);
        spdlog::info("ringbuffer size={}", dataStore.getProcsSize());

        // 60초마다 DuckDB flush
        if (tick % 60 == 0) {
            dataStore.flushProcsToDB();
            dataStore.flushSysToDB();
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

    std::string resultProc = dataStore.queryReport(
        "SELECT * FROM proc_snapshot LIMIT 10");
    printf("%s\n", resultProc.c_str());

    std::string resultSys = dataStore.queryReport(
        "SELECT * FROM sys_snapshot LIMIT 10");
    printf("%s\n", resultSys.c_str());
}
