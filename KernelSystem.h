#ifndef _kernelsystem_h_
#define _kernelsystem_h_

#include "vm_declarations.h"
#include <map>
#include <list>
#include <mutex>
using namespace std;

class Process;
class Partition;


struct ClusterTrackElem {
	unsigned _free;
	ProcessId _pid;
	PageNum _page;

	ClusterTrackElem() {
		_free = 1;
		_pid = -1;
		_page = -1;
	}

	void reset() {
		_free = 1;
		_pid = -1;
		_page = -1;
	}
};


struct PmtTrackElem {
	unsigned _free;
	ProcessId _pid;
	
	PmtTrackElem() {
		_free = 1;
		_pid = -1;
	}

	void reset() {
		_free = 1;
		_pid = -1;
	}
};

struct FrameEntry {
	PMTEntry * _pmtEntry;
	ProcessId _pid;
	PageNum _page;

	FrameEntry() {
		_pmtEntry = 0;
		_pid = -1;
		_page = -1;
	}

	void reset() {
		_pmtEntry = 0;
		_pid = -1;
		_page = -1;
	}

};

class KernelSystem {
public:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
		PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition);

	~KernelSystem();

	Process* createProcess();

	//Time periodicJob();

	//Hardware job
	Status access(ProcessId pid, VirtualAddress address, AccessType type);

public:
	static Partition* _partition;
	static PhysicalAddress _VMSpaceAddress, _pmtSpaceAddress;
	static PageNum _numOfFrames, _pmtSpaceSize;
	static int _numOfPmts;
	static int _pmtSize;
	static int _numOfFreeFrames;
	static PMTEntry**pmts;
	static map<ProcessId, PMTEntry*> _mapPidPmt;
	static PmtTrackElem**_pmtsTrack;

	static mutex _mutex;

	//------
	static unsigned _blockBits;
	//-------

	static FrameEntry** _framesTrack;

	static ClusterNo _numOfClusters, _clusterCount;
	static ClusterTrackElem** _clustersTrack;

	static unsigned _victim;
	
	//------------------
	
	static unsigned lastID;
	
	//----------methods---------

	static void calcBlockBits(PageNum blockNum, unsigned& blockBits);
	static void alocatePmts();
	static int numOfFreeFrames();

	static void getVictim(unsigned& victim);

	static	PhysicalAddress allocatePage(ProcessId pid, PMTEntry* pmtEntry, PageNum page);
	//static void deallocatePage(ProcessId pid, PhysicalAddress padr);
	
	static void loadFromSwapSpace(ClusterNo cluster, char* content);

	static void initSpace(PhysicalAddress padr, char* content, unsigned segmentNo);

	static void releaseAllFrames(ProcessId pid);

	static void releaseAllClusters(ProcessId pid);

	static void releaseFrame(ProcessId pid, PageNum page);
	static void releaseCluster(ProcessId pid, PageNum page);
	static void releasePmt(ProcessId pid);

	friend class KernelProcess;

};


#endif
