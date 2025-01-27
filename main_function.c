#include "main_function.h"

extern PVIRTUAL_MACHINE_STATE g_GuestState;

int noinline start(void){
    printk(KERN_INFO "\n[HPV] Driver called\n");

    int x = num_present_cpus();
    printk(KERN_INFO "[HPV] Number of logical processors: %d\n", x);
    
    if (!InitiateVmx()) {
        printk(KERN_INFO "[HPV] InitiateVmx failed\n");
        return -1;
    }

    int LogicalProcessorCount = num_present_cpus();

    for (int ProcessorID = 0; ProcessorID < LogicalProcessorCount; ProcessorID++) {
        if (!AllocateVmmStack(&g_GuestState[ProcessorID])){
            printk(KERN_INFO "[HPV] AllocateVmmStack failed\n");
            return -1;
        }
        if (!AllocateMsrBitmap(&g_GuestState[ProcessorID])){
            printk(KERN_INFO "[HPV] AllocateMsrBitmap failed\n");
            return -1;
        }
    }
    
    VirtualizeCurrentSystem();

    return 0;
}

void noinline end(void){
    printk(KERN_INFO "[HPV] Driver exiting\n");
    TerminateVmx();
}