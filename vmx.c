#include "vmx.h"
#include "msr.h"
#include "memory.h"
#include "ept.h"
#include "asm.h"

#include <linux/cpu.h>
#include <linux/slab.h>

extern uint64_t AsmSaveRegsAndLaunchVM(void);
extern uint64_t g_VirtualGuestMemoryAddress;	

VIRTUAL_MACHINE_STATE* g_GuestState = NULL;

uint64_t g_StackPointerForReturning = 0;
uint64_t g_BasePointerForReturning = 0;

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
		// local_irq_disable();
		smp_call_on_cpu(i, InitiateVmxProcessor, (void*)(uint64_t)i, false);
		// local_irq_enable();
	}

	printk(KERN_DEBUG "[HPV] VMX Operation turned on successfully\n");
	return true;
}
 
static int TerminateVmxProcessor(void* i){
    uint32_t ProcessorID = smp_processor_id();
    printk(KERN_DEBUG "\t\t[HPV] Current thread is executing in %d th logical processor, expected processor %lld\n", ProcessorID, (uint64_t)i);	

    vmx_off();

    kfree((void*)PhysicalToVirtualAddress(g_GuestState[ProcessorID].VmxonRegion));
    kfree((void*)PhysicalToVirtualAddress(g_GuestState[ProcessorID].VmcsRegion));
	return 0;
}

void TerminateVmx(void) {
	printk(KERN_DEBUG "[HPV] Terminating VMX\n");

	int ProcessorCounts = num_present_cpus();
	for (int i = 0; i < ProcessorCounts; i++) {
		local_irq_disable();
		smp_call_on_cpu(i, TerminateVmxProcessor, (void*)(uint64_t)i, false);
		local_irq_enable();
	}

	if (g_GuestState) {
		kfree(g_GuestState);
	}

	printk(KERN_DEBUG "[HPV] VMX Operation turned off successfully\n");
}

static int LaunchVmProcessor(void* pvoid_data) {
	printk(KERN_DEBUG "[HPV] ===================================== Launching VM ====================================\n");

	PLaunchVMProcessorData data = (PLaunchVMProcessorData) pvoid_data;

	uint32_t processor_id = data->processor_id;	
	uint32_t ProcessorID = smp_processor_id();

	printk(KERN_DEBUG "[HPV]\t\tCurrent thread is executing in %d th logical processor, expected processor %d\n", ProcessorID, processor_id);

	g_GuestState[ProcessorID].MsrBitmapPhysical = VirtualToPhysicalAddress((void*)g_GuestState[ProcessorID].MsrBitmap);

	if (!ClearVmcsState(&g_GuestState[ProcessorID])) {
		printk(KERN_DEBUG "[HPV][ERR] Failed to setup VMCS\n");
		return 1;
	}

	if (!LoadVmcs(&g_GuestState[ProcessorID])) {
		printk(KERN_DEBUG "[HPV][ERR] Failed to setup VMCS\n");
		return 1;
	}

	printk(KERN_DEBUG "[HPV] Setting up VMCS.\n");
	SetupVmcs(&g_GuestState[ProcessorID], data->EPTP);

	uint64_t status = AsmSaveRegsAndLaunchVM();
	// int status = 0;
	if (status == 0) {
		printk(KERN_DEBUG "[HPV] VM exited successfully");
		return 0;
	}
	else {
		uint64_t ErrorCode = 0;
		vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
		printk(KERN_DEBUG "[HPV][ERR] VMX Launch failed with error code 0x%llx\n", ErrorCode);
		return 1;
	}
	return 0;
}

bool LaunchVm(PEPTP EPTP) {
	int n_cpus = num_present_cpus();
	for (int i = 0; i < n_cpus; i++){
		AllocateVmmStack(&g_GuestState[i]);
		AllocateMsrBitmap(&g_GuestState[i]);
		LaunchVMProcessorData data = { i, EPTP };
		smp_call_on_cpu(i, LaunchVmProcessor, &data, false);
	}
	return true;
}        