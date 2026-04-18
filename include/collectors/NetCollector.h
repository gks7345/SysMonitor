#pragma once
#include <Windows.h>
#include <Pdh.h>
#include <pdhmsg.h>
#include <iostream>

class NetCollector {
private:
	PDH_HCOUNTER sentCounter;
	PDH_HCOUNTER recvCounter;

public:
	void init(PDH_HQUERY& query);
	float getSentKB() const;
	float getRecvKB() const;
};

