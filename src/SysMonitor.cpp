// SysMonitor.cpp : 애플리케이션의 진입점을 정의합니다.
//
#include "SysMonitor.h"
#include <chrono>

using namespace std;

void LogSystemSample(nlohmann::json& sys, nlohmann::json& proc) {
    spdlog::info(
        "CPU: {:.1f}% ({:.2f} GHz) | "
        "MEM: {:.2f}/{:.0f} GB ({:.1f}%) Committed {:.2f}/{:.0f} GB ({:.1f}%)| "
        "NET: Recv {:.2f} KB/s Sent {:.2f} KB/s | "
        "DISK: R {:.2f} KB/s | W {:.2f} KB/s",

        sys["system"]["CPU"].value("Total", 0.0),
        sys["system"]["CPU"].value("FredGHZ",0.0),

        sys["system"]["MEM"].value("UsedMB",0.0)/1024.0,
        sys["system"]["MEM"].value("TotalMB",0.0)/1024.0,
        sys["system"]["MEM"].value("UsedPercent", 0.0),
        sys["system"]["MEM"].value("committedGB",0.0),
        sys["system"]["MEM"].value("commitLimitGB",0.0),
        sys["system"]["MEM"].value("commitPercent",0.0),

        sys["system"]["NET"].value("netRecvKB", 0.0),
        sys["system"]["NET"].value("netSentKB",0.0),

        sys["system"]["DISK"].value("diskReadKB",0.0),
        sys["system"]["DISK"].value("diskWriteKB",0.0)
    ); 
    std::cout << "---------------------------------------------------------------------------------------\n";
    for (int i = 0; i < 10; ++i) {
        std::printf(
            "|\tPID : %.0f \t| NAME : %s \t| CPU : %.1f \t| MEM : %.2f \n",
            proc["proc"][i].value("PID", 0.0),
            proc["proc"][i].value("NAME", std::string("")).c_str(),
            proc["proc"][i].value("CPU", 0.0),
            proc["proc"][i].value("MEM", 0.0)
        );       
    }
    std::cout << "---------------------------------------------------------------------------------------\n";

}

int main()
{	
	SystemCollector sc;
    ProcessCollector proc;
    int tick = 0;
    proc.setCritrion(SortCriterion::CPU);

    while (true) {
        sc.collectMiddle();
        proc.collectProc();
        proc.sortProc();

        if (tick % 4 == 0) {
            sc.collectSlow();
        }
        nlohmann::json systemSample = sc.snapshot();
        nlohmann::json procSample = proc.snapshot();
        LogSystemSample(systemSample, procSample);
        tick++;
        std::this_thread::sleep_for(1000ms);
    }

}
