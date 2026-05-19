#pragma once
#include <string>
#include <iostream>
#include <Windows.h>

class ProcList {
private:
	uint32_t pid;
	std::string name;
	double cpuUsage;
	double memoryMB;
	double privateMemMB;
public:
	ProcList() = default;
	ProcList(std::string name, uint32_t pid,	 double cpuUsage, double memoryMB);

	void setProcName(std::string name);
	void setProcID(uint32_t pid);
	void setProcCpuUsage(double cpuUsage);
	void setProcMemoryMB(double memoryMB);
	void setProcPrivateMemoryMB(double privateMemMB);


	uint32_t getProcID() const;
	double getProcCpuUsage() const;
	double getProcMemoryMB() const;
	std::string getProcName() const;
	double getProcPrivateMemMB() const;

	ProcList* find(std::string name);
};