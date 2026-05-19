#define NOMINMAX
#pragma once

#include <Windows.h>
#include <Pdh.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <vector>
#include <string>


#include "collectors/ProcList.h"

enum class SortCriterion { CPU, MEMORY };

class ProcessCollector {
private:
	std::vector<std::unique_ptr<ProcList>> procList;
	size_t topN;
	SortCriterion currentCritrion;

	// 1초 간격
	PDH_HQUERY queryMiddle;
	//2초 간격
	PDH_HQUERY querySlow;

	PDH_HCOUNTER procID;
	PDH_HCOUNTER procName;
	PDH_HCOUNTER procCpuUsase;
	PDH_HCOUNTER procPrivateMem;
	PDH_HCOUNTER procMem;
	PDH_HCOUNTER procNet;

public:
	ProcessCollector();
	~ProcessCollector();

	void initInfo(PDH_HQUERY& query);

	int getProcID() const;
	std::string getProcName() const;
	float getProcCpuUsage() const;
	float getProcPrivateMem() const;
	float getProcWorkingSet() const;


	void collectProc();
	void collectSlow();

	void setCritrion(SortCriterion setCritrion);
	void sortProc();

	ProcList* findProc(std::string);

	nlohmann::json snapshot();
};