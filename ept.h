#pragma once

#include <linux/types.h>

/*
Note - We assume that the the physical address width is 48 bits for defining these structs, but it doesn't matter 
	(in fact it is 39 in most cases), since the upper bits are reserved and should be zeroed out.
*/

typedef union _EPTP {
	uint64_t All;
	struct {
		uint64_t MemoryType : 3;
		uint64_t PageWalkLength : 3;
		uint64_t DirtyAndAccessEnabled : 1;
		uint64_t EnableEnforcement : 1;	// extra addition
		uint64_t Reserved1 : 4;
		uint64_t PageFrameNumber : 36;
		uint64_t Reserved2 : 16;
	} Fields;
} EPTP, *PEPTP;

typedef union _EPT_PML4E {
	uint64_t All;
	struct {
		uint64_t Read : 1;
		uint64_t Write : 1;
		uint64_t Execute : 1;
		uint64_t Reserved1 : 5;
		uint64_t Accessed : 1;
		uint64_t Ignored1 : 1;
		uint64_t ExecuteForUserMode : 1;
		uint64_t Ignored2 : 1;
		uint64_t PhysicalAddress : 36;
		uint64_t Reserved2 : 4;
		uint64_t Ignored3 : 11;
		uint64_t SuppressVE : 1; // extra addition
	} Fields;
} EPT_PML4E, *PEPT_PML4E;

typedef union _EPT_PDPTE {
	uint64_t All;
	struct {
		uint64_t Read : 1;
		uint64_t Write : 1;
		uint64_t Execute : 1;
		uint64_t Reserved1 : 4;
		uint64_t Zero : 1;	// extra addition
		uint64_t Accessed : 1;
		uint64_t Ignored1 : 1;
		uint64_t ExecuteForUserMode : 1;
		uint64_t Ignored2 : 1;
		uint64_t PhysicalAddress : 36;
		uint64_t Reserved2 : 4;
		uint64_t Ignored3 : 11;
		uint64_t SuppressVE : 1; // extra addition
	} Fields;
} EPT_PDPTE, * PEPT_PDPTE;

typedef union _EPT_PDE {
	uint64_t All;
	struct {
		uint64_t Read : 1;
		uint64_t Write : 1;
		uint64_t Execute : 1;
		uint64_t Reserved1 : 4;
		uint64_t Zero : 1;	// extra addition
		uint64_t Accessed : 1;
		uint64_t Ignored1 : 1;
		uint64_t ExecuteForUserMode : 1;
		uint64_t Ignored2 : 1;
		uint64_t PhysicalAddress : 36;
		uint64_t Reserved2 : 4;
		uint64_t Ignored3 : 11;
		uint64_t SuppressVE : 1; // extra addition
	} Fields;
} EPT_PDE, * PEPT_PDE;

typedef union _EPT_PTE {
	uint64_t All;
	struct {
		uint64_t Read : 1;
		uint64_t Write : 1;
		uint64_t Execute : 1;
		uint64_t EPTMemoryType : 3;
		uint64_t IgnorePAT : 1;
		uint64_t Ignored1 : 1;
		uint64_t AccessedFlag : 1;
		uint64_t DirtyFlag : 1;
		uint64_t ExecuteForUserMode : 1;
		uint64_t Ignored2 : 1;
		uint64_t PhysicalAddress : 36;
		uint64_t Reserved1 : 4;
		uint64_t Ignored3 : 11;
		uint64_t SuppressVE : 1;
	} Fields;
} EPT_PTE, * PEPT_PTE;

PEPTP InitializeEPTP(void);