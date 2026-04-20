// SysMonitor.cpp : 애플리케이션의 진입점을 정의합니다.
//

#include "SysMonitor.h"
#include <chrono>

using namespace std;

std::string TimeToString(const std::chrono::system_clock::time_point& tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &t);   // Windows 안전 버전
#else
    localtime_r(&t, &tm);   // Linux
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

void LogSystemSample(nlohmann::json& j) {
    spdlog::info(
        "CPU: {:.1f}% ({:.2f} GHz) | "
        "MEM: {:.2f}/{:.0f} GB ({:.1f}%) Committed {:.2f}/{:.0f} GB ({:.1f}%)| "
        "NET: Recv {:.2f} KB/s Sent {:.2f} KB/s | "
        "DISK: R {:.2f} KB/s | W {:.2f} KB/s",

        j["CPU"].value("Total",0.0),
        j["CPU"].value("FredGHZ",0.0),

        j["MEM"].value("UsedMB",0.0)/1024.0,
        j["MEM"].value("TotalMB",0.0)/1024.0,
        j["MEM"].value("UsedPercent", 0.0),
        j["MEM"].value("committedGB",0.0)/1024.0,
        j["MEM"].value("commitLimitGB",0.0)/1024.0,
        j["MEM"].value("commitPercent",0.0),

        j["NET"].value("netSentKB",0.0),
        j["NET"].value("netRecvKB",0.0),

        j["DISK"].value("diskReadKB",0.0),
        j["DISK"].value("diskWriteKB",0.0)

    );
}

int main()
{	
	SystemCollector sc;
    int tick = 0;

    while (true) {
        sc.collectMiddle();
        if (tick % 4 == 0) {
            sc.collectSlow();
        }
        nlohmann::json sample = sc.snapshot();
        LogSystemSample(sample);
        tick++;
        std::this_thread::sleep_for(1000ms);
    }

}
