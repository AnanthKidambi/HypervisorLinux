#pragma once

#include <linux/types.h>

typedef union _IA32_FEATURE_CONTROL_MSR {
	struct {
		uint64_t Lock : 1;
		uint64_t EnableSMX : 1;
		uint64_t EnableVmxon : 1;
		uint64_t Reserved2 : 5;
		uint64_t EnableLocalSENTER : 7;
		uint64_t EnableGlobalSENTER : 1;
		uint64_t Reserved3a : 16;
		uint64_t Reserved3b : 32;
	} Fields;
	uint64_t All;
} IA32_FEATURE_CONTROL_MSR, * PIA32_FEATURE_CONTROL_MSR;

typedef struct _CPUID {
	int eax;
	int ebx;
	int ecx;
	int edx;
} CPUID, * PCPUID;

typedef union _IA32_VMX_BASIC_MSR {
	struct {
		uint32_t RevisionIdentifier : 31; // shouldn't this be 32 bits [bits 0-31]?
		uint32_t Reserved1 : 1;
		uint32_t RegionSize : 12;
		uint32_t RegionClear : 1;
		uint32_t Reserved2 : 3;
		uint32_t SupportedIA64 : 1;
		uint32_t SupportedDualMonitor : 1;
		uint32_t MemoryType : 4;
		uint32_t VmExitReport : 1;
		uint32_t VmxCapabilityHint : 1;
		uint32_t Reserved3 : 8;
	} Fields;
	uint64_t All;
} IA32_VMX_BASIC_MSR, * PIA32_VMX_BASIC_MSR;

typedef union _MSR {
	struct {
		uint32_t Low;
		uint32_t High;
	} Fields;
	uint64_t Content;
} MSR, * PMSR;

#define MSR_APIC_BASE            	0x01B
#define MSR_IA32_FEATURE_CONTROL 	0x03A

#define MSR_IA32_DEBUGCTL     		0x1D9

#define MSR_SHADOW_GS_BASE 			0xC0000102
