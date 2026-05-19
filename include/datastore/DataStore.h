#pragma once
#include <iostream>
#include <Windows.h>
#include <Pdh.h>
#include <nlohmann/json.hpp>
#include "datastore/RingBuffer.h"
#include "collectors/SystemCollector.h"
#include "collectors/ProcessCollector.h"

class DataStore {
private:
	RingBuffer<nlohmann::json> Slow(300);
	RingBuffer<nlohmann::json> Middle(300);
public:
	DataStore();
	void appendSlowData();
	void appendMiddleData();
	RingBuffer<nlohmann::json> getSessionReportSlow();
	RingBuffer<nlohmann::json> getSessionReportMiddle();
};
