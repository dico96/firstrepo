#include "System.h"
#include "KernelSystem.h"

System::System(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
	PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition) {
	
	pSystem = new KernelSystem(processVMSpace, processVMSpaceSize, pmtSpace, pmtSpaceSize, partition);

}

System::~System() {
	delete pSystem;
}

Time System::periodicJob() {
	return (int)0;
}


Process* System::createProcess() {
	return pSystem->createProcess();
}


Status System::access(ProcessId pid, VirtualAddress address, AccessType type) {
	return pSystem->access(pid, address, type);
}