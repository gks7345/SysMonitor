#include "collectors/ProcList.h"



ProcList::ProcList(std::string name, uint32_t pid,  double cpuUsage, double memoryMB)
	: pid(pid), name(name), cpuUsage(cpuUsage), memoryMB(memoryMB) {

}

void ProcList::setProcName(std::string name) {
	ProcList::name = name;
}
void ProcList::setProcID(uint32_t pid) {
	ProcList::pid = pid;
}
void ProcList::setProcCpuUsage(double cpuUsage) {
	ProcList::cpuUsage = cpuUsage;
}
void ProcList::setProcMemoryMB(double memoryMB) {
	ProcList::memoryMB = memoryMB;
}
void ProcList::setProcPrivateMemoryMB(double privateMemMB) {
	ProcList::privateMemMB = privateMemMB;
}


uint32_t ProcList::getProcID() const {
	return pid;
}
double ProcList::getProcCpuUsage() const {
	return cpuUsage;
}

double ProcList::getProcMemoryMB() const {
	return memoryMB;
}

std::string ProcList::getProcName() const {
	return name;
}

double ProcList::getProcPrivateMemMB() const {
	return privateMemMB;
}

ProcList* ProcList::find(std::string name) {
	if(ProcList::name == name)
		return this;
	return nullptr;
}