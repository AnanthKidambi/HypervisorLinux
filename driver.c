#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/cpu.h>

#include "ept.h"
#include "vmx.h"
#include "memory.h"

MODULE_LICENSE("GPL");

extern PVIRTUAL_MACHINE_STATE g_GuestState;
extern uint64_t g_VirtualGuestMemoryAddress;

static int __init hello_start(void){
    printk(KERN_INFO "\n[HPV] Driver called\n");

    // PEPTP EPTP = InitializeEPTP();
    // if (EPTP == NULL) {
    //     printk(KERN_DEBUG "[HPV][ERR] InitializeEPTP failed\n");
    //     return 1;
    // }
    // else {
    //     DbgPrint("[HPV] InitializeEPTP succeeded\n");
    // }
    AllocateGuestMemory();
    InitiateVmx();

    for (int i = 0; i < 10 * PAGE_SIZE - 1; i++) {
        uint8_t TempAsm = 0xf4;	// hlt instruction
        memcpy((void*)(g_VirtualGuestMemoryAddress + i), &TempAsm, 1);
    }
    
    // int LogicalProcessorCount = num_present_cpus();

    // for (int i = 0; i < LogicalProcessorCount; i++) {
    //     int ProcessorID = i;
    //     VirtualizeCurrentSystem(ProcessorID, NULL, NULL);
    // }

    LaunchVm(NULL);

    return 0;
}

static void __exit hello_end(void){
    printk(KERN_INFO "[HPV] Driver exiting\n");
    TerminateVmx();
}

module_init(hello_start);
module_exit(hello_end);