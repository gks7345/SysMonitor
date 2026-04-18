#pragma once
#include <chrono>

struct SystemSample {
    // 타임스탬프
    std::chrono::system_clock::time_point timestamp;

    // CPU
    float         cpuTotal;        // 전체 사용률 %
    float         cpuFreqGHz;      // 현재 클럭

    // 메모리
    float     memTotalMB;
    float     memAvailMB;
    float     memUsedMB;
    float     memUsedPercent;

    // 네트워크
    float netSentKB;
    float netRecvKB;

    // 디스크
    float diskReadKB;
    float diskWriteKB;
};