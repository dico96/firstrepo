#include "KernelSystem.h"
#include "Process.h"
#include "KernelProcess.h"
#include <cmath>
#include "part.h"

#include <iostream>

using namespace std;


Partition* KernelSystem::_partition = 0;
PhysicalAddress KernelSystem::_VMSpaceAddress = 0;
PhysicalAddress KernelSystem::_pmtSpaceAddress=0;
PageNum KernelSystem::_numOfFrames = 0;
PageNum KernelSystem::_pmtSpaceSize = 0;
int KernelSystem::_numOfPmts = 0;
int KernelSystem::_pmtSize = 0;
int KernelSystem::_numOfFreeFrames = 0;
PMTEntry** KernelSystem::pmts = 0;
PmtTrackElem** KernelSystem::_pmtsTrack = 0;
map<ProcessId, PMTEntry*>KernelSystem::_mapPidPmt;



mutex KernelSystem::_mutex;

//------
unsigned KernelSystem::_blockBits=0;
//-------

FrameEntry** KernelSystem::_framesTrack=0;

ClusterNo KernelSystem::_numOfClusters = 0;
ClusterNo KernelSystem::_clusterCount = 0;
ClusterTrackElem** KernelSystem::_clustersTrack = 0;

unsigned KernelSystem::_victim =0;

//------------------

unsigned KernelSystem::lastID=0;



KernelSystem::KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
	PhysicalAddress pmtSpace, PageNum pmtSpaceSize, Partition* partition) {
	_partition = partition;
	_VMSpaceAddress = processVMSpace;
	_numOfFrames = processVMSpaceSize;
	_pmtSpaceAddress = pmtSpace;
	_pmtSpaceSize = pmtSpaceSize;

	calcBlockBits(processVMSpaceSize, _blockBits);

	_numOfClusters =_partition->getNumOfClusters();
	_clustersTrack = new ClusterTrackElem*[_numOfClusters];
	for (unsigned cluster = 0; cluster< _numOfClusters; cluster++)
		_clustersTrack[cluster] = new ClusterTrackElem();

	_numOfFreeFrames = _numOfFrames;
	_framesTrack = new FrameEntry*[_numOfFrames];
	for (unsigned frame = 0; frame < _numOfFrames; frame++)
		_framesTrack[frame] = new FrameEntry();

	_pmtSize = pow(2, pageBits) * sizeof(PMTEntry);  //number of entries in the PMT * size of 1 entry
	_numOfPmts = (_pmtSpaceSize * 1024) / _pmtSize;
	_pmtsTrack = new PmtTrackElem*[_numOfPmts];
	for (int pmt= 0; pmt< _numOfPmts; pmt++)
		_pmtsTrack[pmt] = new PmtTrackElem();

	alocatePmts();


}


void KernelSystem::alocatePmts() {
	pmts = new PMTEntry*[_numOfPmts];
	for (int i = 0; i < _numOfPmts; i++) {
		pmts[i] = (PMTEntry*)((char*)_pmtSpaceAddress + i*_pmtSize);
	}
}


void KernelSystem::calcBlockBits(PageNum blockNum, unsigned& bbits) {
	while (blockNum != 0) {
		blockNum /= 2;
		bbits++;
	}
	
}


KernelSystem::~KernelSystem() {
	delete pmts;

	for (int pmt = 0; pmt < _numOfPmts; pmt++)
		delete _pmtsTrack[pmt];
	delete[] _pmtsTrack;
	
	for (int frame = 0; frame < _numOfFrames; frame++)
		delete _framesTrack[frame];
	delete[] _framesTrack;
	
	for (int cluster = 0; cluster < _numOfFrames; cluster++)
		delete _clustersTrack[cluster];
	delete[] _clustersTrack;
}


int KernelSystem::numOfFreeFrames() {
	return _numOfFreeFrames;
}


Process* KernelSystem::createProcess() {
	lock_guard<mutex> guard(_mutex);
	for (int i = 0; i < _numOfPmts; i++) {
		if (_pmtsTrack[i]->_free) {
			Process* newProc = new Process(lastID);
			_mapPidPmt[lastID] = pmts[i];
			_pmtsTrack[i]->_free = 0;
			_pmtsTrack[i]->_pid = lastID;
			KernelProcess::setPmt(lastID, pmts[i]);
			lastID++;
			return newProc;
		}
	}
	//no more available PMTs - error!
	exit(1);
}

void KernelSystem::getVictim(unsigned& victim) {
	//lock_guard<mutex> guard(_mutex);
	victim = _victim;
	_victim = (_victim + 1) % _numOfFrames;
}



PhysicalAddress KernelSystem::allocatePage(ProcessId pid, PMTEntry* pmtEntryArg, PageNum page) {
	//lock_guard<mutex> guard(_mutex);
	unsigned victim=0;
	victim = _victim;
	_victim = (_victim + 1) % _numOfFrames;
	unsigned long block=0;
	//getVictim(victim);
	if (_framesTrack[victim]->_pmtEntry) {
		PMTEntry * pmtEntry = _framesTrack[victim]->_pmtEntry;
		if (*pmtEntry&maskV) {
			*pmtEntry &= resetV;  //redundantno, jer i na kraju azuriram pmt ulaz, ali ovo radim umesto neke sinhronizacije sa procesom-zrtvom(nije 100% sigurno)
			//if (*pmtEntry&maskD) {    // if(true) ==> the page is "dirty"  ==> return page to the disk and update pmtTable
				// 1) returning page to the disk
				ProcessId vicProcId = _framesTrack[victim]->_pid;
				//if(_victim>=1000)
				//printf("%d ", _victim);
				PageNum vicPage = _framesTrack[victim]->_page;	
				
				//_mutex.lock();
				int cluster = 0;
				if (_numOfClusters == _clusterCount++) {
					for (; cluster < _numOfClusters; cluster++) {
						if (!(_clustersTrack[cluster]->_free)) continue;
						_clustersTrack[cluster]->_free = 0;
						_clustersTrack[cluster]->_pid = vicProcId;
						_clustersTrack[cluster]->_page = vicPage;

						//extract startAddress of the page (which is to be swapped) from the victim's pmtEntry
						block = (unsigned long)((*pmtEntry & 0x0FFFFFFFFFFFFFFF) >> 32);
						PhysicalAddress padr = reinterpret_cast<PhysicalAddress>(block << wordBits);
						//save it on disk
						_partition->writeCluster(cluster, (char*)padr);
						break;
					}
				}
				else {
					ClusterNo cluster = *pmtEntry & 0x00000000FFFFFFFF;
					block = (unsigned long)((*pmtEntry & 0x0FFFFFFFFFFFFFFF) >> 32);
					PhysicalAddress padr = reinterpret_cast<PhysicalAddress>(block << wordBits);
					//save it on disk
					_partition->writeCluster(cluster, (char*)padr);
				}
				//_mutex.unlock();
				
				if (cluster == _numOfClusters) 
					exit(1);   // no more swap space - error!
				
				// 2) update pmtTable of the victim process : pmtEntry[diskAddress]=cluster
				*pmtEntry |= cluster;
				*pmtEntry &= 0x3FFFFFFFFFFFFFFF;
		}
	}
	
	_framesTrack[victim]->_pid = pid;
	ClusterNo cluster = *pmtEntryArg&0x00000000FFFFFFFF;
	*pmtEntryArg = (*pmtEntryArg >> 32);
	*pmtEntryArg &=0xF0000000;
	*pmtEntryArg |= block;
	*pmtEntryArg = (*pmtEntryArg << 32) | cluster;
	_framesTrack[victim]->_pmtEntry = pmtEntryArg;
	_framesTrack[victim]->_page = page;

	//now return PAGE ADDRESS to a caller-process
	return (PhysicalAddress)((char*)_VMSpaceAddress + victim*pageSize);
}


//da li je address podrazumevano deo segmenta ; treba li to proveravati???

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType accessType) {
	if (_mapPidPmt.count(pid) > 0) {
		PMTEntry* pmt = _mapPidPmt[pid];
		PageNum pageNum = address >> wordBits;
		PMTEntry* pmtEntry = pmt + pageNum;
		unsigned accessBits = (unsigned)((*pmtEntry & 0x3000000000000000) >> 60);

		//check if wanted access is forbidden for this page

		if (accessType == EXECUTE && accessBits < 3) return TRAP;
		if (accessType == READ && (accessBits == 1 || accessBits == 3)) return TRAP;
		if (accessType == WRITE && (accessBits == 0 || accessBits == 3)) return TRAP;
		if (accessType == READ_WRITE && accessBits == 3) return TRAP;
		//access allowed

		//check V bit

		if ((*pmtEntry & maskV )== 0) return PAGE_FAULT;  //the page is not valid : not loaded at all or in the swap space;

		//everything is OK : V=1 and ACCESS ALLOWED

		return OK;
	}
	else
		return TRAP;
}


void KernelSystem::loadFromSwapSpace(ClusterNo cluster, char* content) {
		//load content from the swap space
		_partition->readCluster(cluster, content);  //ERROR : can't read from the partition
		double a = (*content);

		/*_mutex.lock();
		_clustersTrack[cluster]->reset();
		_mutex.unlock();*/

}


void KernelSystem::initSpace(PhysicalAddress padr, char* content, unsigned segmentNo) {
	char* buffer = content+segmentNo*pageSize;
	for (int byteNo = 0; byteNo < pageSize; byteNo++, buffer++)
		*((char*)padr + byteNo) = *buffer;
}




void KernelSystem::releaseAllFrames(ProcessId pid) {
	FrameEntry* frameEntry = 0;
	for (int frame = 0; frame < _numOfFrames; frame++) {
		frameEntry = _framesTrack[frame];
		if (frameEntry->_pid != pid) continue;
		_mutex.lock();
		frameEntry->reset();
		_mutex.unlock();
	}
}


void KernelSystem::releaseAllClusters(ProcessId pid) {
	ClusterTrackElem* clusterElement = 0;
	for (int cluster = 0; cluster < _numOfClusters; cluster++) {
		clusterElement = _clustersTrack[cluster];
		if (clusterElement->_pid != pid) continue;
		_mutex.lock();
		clusterElement->reset();
		_mutex.unlock();
	}
}


void KernelSystem::releaseFrame(ProcessId pid, PageNum page) {
	FrameEntry* frameEntry = 0;
	for (int frame = 0; frame < _numOfFrames; frame++) {
		frameEntry = _framesTrack[frame];
		if (frameEntry->_pid != pid) continue;
		if (frameEntry->_page != page) continue;
		_mutex.lock();
		frameEntry->reset();
		_mutex.unlock();
	}
}

void KernelSystem::releaseCluster(ProcessId pid, PageNum page) {
	ClusterTrackElem* clusterElement = 0;
	for (int cluster = 0; cluster < _numOfClusters; cluster++) {
		clusterElement= _clustersTrack[cluster];
		if (clusterElement->_pid != pid) continue;
		if (clusterElement->_page != page) continue;
		_mutex.lock();
		clusterElement->reset();
		_mutex.unlock();
	}
}


void KernelSystem::releasePmt(ProcessId pid) {
	PmtTrackElem* pmtElem = 0;
	for (int pmt = 0; pmt < _numOfPmts; pmt++) {
		pmtElem = _pmtsTrack[pmt];
		if (pmtElem->_pid != pid) continue;
		_mutex.lock();
		pmtElem->reset();
		_mutex.unlock();
	}
}