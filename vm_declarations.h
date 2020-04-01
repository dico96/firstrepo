#ifndef _vm_declarations_
#define _vm_declarations_


#define PAGE_SIZE 1024


typedef unsigned long PageNum;
typedef unsigned long VirtualAddress;
typedef void* PhysicalAddress;
typedef unsigned long Time;

enum Status {OK, PAGE_FAULT, TRAP};

enum AccessType {READ, WRITE, READ_WRITE, EXECUTE};

typedef unsigned ProcessId;


//----------------------- moje definicije -----------------

const unsigned pageSize = 1024;
const unsigned clusterSize = 1024;
const  unsigned pageBits=14;
const unsigned wordBits = 10;
const unsigned maskAligned = 0x3FF;
const unsigned long maskOffset = 0x3FF;
const unsigned long long maskV = 0x8000000000000000;
const unsigned long long maskD = 0x4000000000000000;
const unsigned long long maskCluster = 0x00000000FFFFFFFF;
const unsigned long long resetD = 0xBFFFFFFFFFFFFFFF;
const unsigned long long resetV = 0x7FFFFFFFFFFFFFFF;

typedef unsigned long long PMTEntry;
typedef unsigned Frame;
typedef unsigned long ClusterNo;


#endif
