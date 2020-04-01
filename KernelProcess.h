#ifndef _kernelprocess_h_
#define _kernelprocess_h_

#include "vm_declarations.h"
#include <map>
using namespace std;

class KernelProcess {
public:
	KernelProcess(ProcessId pid);

	~KernelProcess();

	ProcessId getProcessId() const;

	Status createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags);

	Status loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content);

	Status deleteSegment(VirtualAddress startAddress);

	Status pageFault(VirtualAddress address);

	PhysicalAddress getPhysicalAddress(VirtualAddress address);

private:
	friend class KernelSystem;
	
	static map<ProcessId, KernelProcess*> allProc;

	map<VirtualAddress, unsigned> _segmentsTrack;          //map<virtualAddress, numOfSegments>

	ProcessId _id;
	PMTEntry* _pmt;

	static void setPmt(ProcessId pid, PMTEntry* pmt);

	void deleteAllSegments();

	void initEntry(PageNum page, PhysicalAddress padr, AccessType flags);

	int isSegmentOverload(VirtualAddress vadr,PageNum numOfPages);

};



#endif
