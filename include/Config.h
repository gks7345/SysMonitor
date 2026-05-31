// SysMonitor.h 또는 별도 Config.h
#pragma once

namespace Config {
    // RingBuffer 용량 (초 단위 = 보관 시간)
    constexpr size_t PROC_BUFFER_CAPACITY = 120;  // 120초
    constexpr size_t SYS_BUFFER_CAPACITY = 120;  // 120초

    // flush 주기 (tick 단위 = 초)
    constexpr int FLUSH_INTERVAL_TICKS = 60;      // 60초

    // 콘솔 출력 폭
    constexpr int CONSOLE_WIDTH = 80;

    // 프로세스 상위 N개
    constexpr int DEFAULT_TOP_N = 10;

    // 수집 주기
    constexpr int SLOW_COLLECT_INTERVAL = 2;      // 2초마다 collectSlow
}