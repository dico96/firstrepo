#include "KernelProcess.h"
#include "KernelSystem.h"
#include <cmath>

#include <iostream>
#include <stdio.h>

using namespace std;

map<ProcessId, KernelProcess*> KernelProcess:: allProc;


KernelProcess::KernelProcess(ProcessId pid) {
	_id = pid;
	allProc[pid] = this;
}
KernelProcess::~KernelProcess() {
	deleteAllSegments();
	KernelSystem::releasePmt(this->_id);
	allProc.erase(this->_id);
}


ProcessId KernelProcess::getProcessId() const {
	return _id;
}


void KernelProcess::setPmt(ProcessId pid, PMTEntry* pmt) {
	map<ProcessId, KernelProcess*>::iterator it = allProc.find(pid);
	KernelProcess* proc = it->second;
	proc->_pmt = pmt;
}





Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags) {
	if (startAddress&maskAligned) return TRAP;

	if (isSegmentOverload(startAddress,segmentSize)) return TRAP;

	_segmentsTrack[startAddress] = segmentSize;

	for (int i = 0; i < segmentSize; i++) {
		PageNum page = (startAddress+i*pageSize) >> wordBits;
		PMTEntry* pmtEntry = _pmt + page;
		PhysicalAddress padr = KernelSystem::allocatePage(this->_id, pmtEntry, page);
		initEntry(page , padr, flags);
	}
	
	return OK;
	
}


Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content) {
	if (startAddress&maskAligned) return TRAP;

	if (isSegmentOverload(startAddress, segmentSize)) return TRAP;

	_segmentsTrack[startAddress] = segmentSize;

	for (int segmentNo = 0; segmentNo < segmentSize; segmentNo++) {
		PageNum page = (startAddress + segmentNo*pageSize) >> wordBits;
		PMTEntry* pmtEntry = _pmt + page;
		PhysicalAddress padr = KernelSystem::allocatePage(this->_id, pmtEntry, page);
		KernelSystem::initSpace(padr, (char*)content, segmentNo);
		initEntry(page, padr, flags);
	}

	return OK;
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress) {
	if (_segmentsTrack.find(startAddress) == _segmentsTrack.end()) return TRAP;  //there is no such segment
	map<VirtualAddress, unsigned>::iterator it = _segmentsTrack.find(startAddress);
	PageNum numOfPages = it->second;
	PMTEntry* pmtEntry = _pmt + (startAddress >> wordBits);
	for (int i = 0; i < numOfPages; i++, pmtEntry++) {
		if ((*pmtEntry & maskV) == 1 && ((*pmtEntry & maskD) == 1) || ((*pmtEntry & maskD) == 0))
			KernelSystem::releaseFrame(this->_id, startAddress >> wordBits);	//releaseFrame(processID, page)
		else if ((*pmtEntry & maskV) == 0 && (*pmtEntry & maskD) == 1)
			KernelSystem::releaseCluster(this->_id, startAddress >> wordBits);
		*pmtEntry = 0;   //  reset entry : V=0, D=0 ...
	}
	_segmentsTrack.erase(startAddress);
}


void KernelProcess::deleteAllSegments() {
	KernelSystem::releaseAllFrames(this->_id);
	KernelSystem::releaseAllClusters(this->_id);
	map<VirtualAddress, unsigned>::iterator it = _segmentsTrack.begin();
	while (it != _segmentsTrack.end()) {
		_segmentsTrack.erase(it++);
	}
}

void KernelProcess::initEntry(PageNum page, PhysicalAddress padr, AccessType flags) {
	PMTEntry * pmtEntry = _pmt + page;

	unsigned long long entry = 0x8000000000000000;  //V is set to 1
	unsigned long long temp = flags;
	entry |= temp << 60; // V and ACCflags are set
	temp = reinterpret_cast<unsigned long long>(padr) >> wordBits;  //block
	entry |= temp << 32;  // size(pmt[blockAddress]) = 28 bits;

	*pmtEntry = entry;

}


PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address) {
	PageNum pageNum = address >> wordBits;
	unsigned long offset =address&maskOffset;
	PMTEntry* pmtEntry = _pmt + pageNum;
	unsigned long long block=((*pmtEntry & 0x0FFFFFFFFFFFFFFF) >> 32);
	PhysicalAddress pa = reinterpret_cast<PhysicalAddress>((block << wordBits) | offset);
	return pa;
}


Status KernelProcess::pageFault(VirtualAddress address) {
	VirtualAddress alignedAddress = (address >> wordBits) << wordBits;
	PageNum page = address >> wordBits;
	PMTEntry* pmtEntry = _pmt + page;
	ClusterNo cluster = (ClusterNo)((*pmtEntry)&maskCluster);
	PhysicalAddress padr = KernelSystem::allocatePage(this->_id, pmtEntry, page);
	char* content =(char*) padr;
	KernelSystem::loadFromSwapSpace(cluster,content);
	//KernelSystem::initSpace(padr, content, 0);
	
	//init entry after swap in
	
	unsigned long long block = (reinterpret_cast<unsigned long long>(padr) >> wordBits)<<32;  
	//*pmtEntry |= block;  //set block
	*pmtEntry |= maskV;  //set V
	*pmtEntry &= resetD; //reset D
	
	return OK;

}


int KernelProcess::isSegmentOverload(VirtualAddress vadr, PageNum numOfPages) {
	for (map<VirtualAddress, unsigned>::iterator it = _segmentsTrack.begin(); it != _segmentsTrack.end(); it++) {
		VirtualAddress currAdr = it->first;    //startAddress of the segment
		PageNum currNumOfPages = it->second;
		if (currAdr > vadr) {
			if (currAdr < (vadr + numOfPages*pageSize)) return true;     //overlap
		}
		else if (currAdr<vadr){
			if ((currAdr + currNumOfPages*pageSize) > vadr) return true;  //overlap
		}
		else return true;   // currAddr=vadr - overlap
	}
	return false;
}

