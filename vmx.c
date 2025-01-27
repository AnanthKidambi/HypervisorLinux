#include "vmx.h"
#include "msr.h"
#include "memory.h"
#include "ept.h"
#include "asm.h"

#include <linux/cpu.h>
#include <linux/slab.h>

extern uint64_t AsmSaveRegsAndLaunchVM(void);
extern int AsmVmxSaveState(void*);

VIRTUAL_MACHINE_STATE* g_GuestState = NULL;

static int InitiateVmxProcessor(void* i){
    uint32_t ProcessorID = smp_processor_id();
    printk(KERN_DEBUG "\t\t[HPV] Current thread is executing in %d th logical processor, expected processor %lld\n", ProcessorID, (uint64_t)i);

    AsmEnableVmxOperation(); 

    if (!AllocateVMXonRegion(&g_GuestState[ProcessorID])) {
        printk(KERN_DEBUG "[HPV][ERR] AllocateVMXonRegion failed\n");
        return 1;
    }

	vmx_on(&(g_GuestState[ProcessorID].VmxonRegion));

    if (!AllocateVmcsRegion(&g_GuestState[ProcessorID])) {
        printk(KERN_DEBUG "[HPV][ERR] AllocateVmcsRegion failed\n");
        return 1;
    }

    printk(KERN_DEBUG "[HPV] VMCS Region is allocated at 0x%llx", g_GuestState[ProcessorID].VmcsRegion);
    printk(KERN_DEBUG "[HPV] VMXON Region is allocated at 0x%llx", g_GuestState[ProcessorID].VmxonRegion);
	return 0;
}

bool InitiateVmx(void) {
	printk(KERN_DEBUG "[HPV] Initializing VMX\n");
	
	int ProcessorCounts = num_present_cpus();
	g_GuestState = (PVIRTUAL_MACHINE_STATE)kmalloc_array(ProcessorCounts, sizeof(VIRTUAL_MACHINE_STATE), GFP_KERNEL);

	if (g_GuestState == NULL) {
		printk(KERN_DEBUG "[HPV][ERR] ExAllocatePool2 failed\n");
		return false;
	}

	for (int i = 0; i < ProcessorCounts; i++) {
		smp_call_on_cpu(i, InitiateVmxProcessor, (void*)(uint64_t)i, false);
	}

	printk(KERN_DEBUG "[HPV] VMX Operation turned on successfully\n");
	return true;
}
 
static int TerminateVmxProcessor(void* i){
    uint32_t ProcessorID = smp_processor_id();
    printk(KERN_DEBUG "\t\t[HPV] Current thread is executing in %d th logical processor, expected processor %lld\n", ProcessorID, (uint64_t)i);	

	uint32_t cpu_info_for_exit[4];
	cpuidex(cpu_info_for_exit, CPUID_RAX_FOR_VMEXIT, CPUID_RCX_FOR_VMEXIT);

    kfree((void*)PhysicalToVirtualAddress(g_GuestState[ProcessorID].VmxonRegion));
    kfree((void*)PhysicalToVirtualAddress(g_GuestState[ProcessorID].VmcsRegion));
	return 0;
}

void TerminateVmx(void) {
	printk(KERN_DEBUG "[HPV] Terminating VMX\n");

	int ProcessorCounts = num_present_cpus();
	for (int i = 0; i < ProcessorCounts; i++) {
		smp_call_on_cpu(i, TerminateVmxProcessor, (void*)(uint64_t)i, false);
	}

	if (g_GuestState) {
		kfree(g_GuestState);
	}

	printk(KERN_DEBUG "[HPV] VMX Operation turned off successfully\n");
}

int VirtualizeCurrentSystemProcessor(void* pvoid_data, void* GuestStack) {
	printk(KERN_DEBUG "[HPV] ===================================== Launching VM ====================================\n");
	LaunchVMProcessorData* data = (LaunchVMProcessorData*)pvoid_data;

	uint32_t processor_id = data -> processor_id;

	printk(KERN_DEBUG "[HPV]\t\tCurrent thread is executing in %d th logical processor\n", processor_id);

	SEGMENT_DESCRIPTOR* GdtBase = (SEGMENT_DESCRIPTOR*)GetGdtBase();
	printk(KERN_DEBUG "[HPV] GdtBase: 0x%llx\n", (uint64_t)GdtBase);

	if (!ClearVmcsState(&g_GuestState[processor_id])) {
		printk(KERN_DEBUG "[HPV][ERR] Failed to setup VMCS\n");
		return 1;
	}

	if (!LoadVmcs(&g_GuestState[processor_id])) {
		printk(KERN_DEBUG "[HPV][ERR] Failed to setup VMCS\n");
		return 1;
	}

	printk(KERN_DEBUG "[HPV] Setting up VMCS.\n");
	SetupVmcsAndVirtualizeMachine(&g_GuestState[processor_id], data->EPTP, GuestStack);

	vmx_vmlaunch();

	uint64_t ErrorCode = 0;
	vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	printk(KERN_DEBUG "[HPV][ERR] VMX Launch failed with error code 0x%llx\n", ErrorCode);

	return 1;
}

void VirtualizeCurrentSystem(){
	int ProcessorCounts = num_present_cpus();
	for (int i = 0; i < ProcessorCounts; i++) {
		LaunchVMProcessorData data = { i, NULL };
		uint64_t GdtBase = GetGdtBase();
		uint16_t GdtLimit = GetGdtLimit();
		uint64_t IdtBase = GetIdtBase();
		uint16_t IdtLimit = GetIdtLimit();
		smp_call_on_cpu(i, AsmVmxSaveState, (void*)&data, false);
	}
}
