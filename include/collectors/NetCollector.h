#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <iostream>
#include <string>
#include <format>
#include <vector>

class NetCollector {
private:
	PDH_HCOUNTER sentCounter;
	PDH_HCOUNTER recvCounter;

	
public:
	void init(PDH_HQUERY& query);
	double getSentKbps() const;
	double getRecvKbps() const;
	static std::string formatNetSpeed(double kbps);
};

