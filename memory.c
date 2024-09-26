#include "memory.h"
#include "vmx.h"
#include "msr.h"
#include "asm.h"

#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <asm/io.h>
#include <linux/set_memory.h>

uint64_t g_VirtualGuestMemoryAddress = 0;

uint64_t VirtualToPhysicalAddress(void* va) {
	return virt_to_phys(va);
}

uint64_t PhysicalToVirtualAddress(uint64_t pa) {
	return phys_to_virt(pa);
}

bool AllocateVMXonRegion(PVIRTUAL_MACHINE_STATE GuestState) {
	int VMXONSize = 2*VMXON_SIZE;
	char* Buffer = kvzalloc(VMXONSize + ALIGNMENT_PAGE_SIZE, GFP_KERNEL);

	if (Buffer == NULL) {
		printk(KERN_DEBUG "[HPV][ERR] kvzalloc failed\n");
		return false;	
	}
	// zeroing memory is necessary since all bits except the revision identifier [bits 0-31] should be zero in the vmxon region
	char* AlignedVirtualBuffer = (char*)((uint64_t)(Buffer + ALIGNMENT_PAGE_SIZE - 1) & ~(uint64_t)(ALIGNMENT_PAGE_SIZE - 1));

	uint64_t PhysicalBuffer = VirtualToPhysicalAddress(Buffer);
	uint64_t AlignedPhysicalBuffer = (uint64_t)(PhysicalBuffer + ALIGNMENT_PAGE_SIZE - 1) & ~(uint64_t)(ALIGNMENT_PAGE_SIZE - 1);
	uint64_t AlignedPhysicalBuffer1 = VirtualToPhysicalAddress(AlignedVirtualBuffer);

	printk(KERN_DEBUG "[HPV] Virtual allocated buffer for VMXON at 0x%llx\n", (uint64_t)Buffer);
	printk(KERN_DEBUG "[HPV] Virtual aligned allocated buffer for VMXON at 0x%llx\n", (uint64_t)AlignedVirtualBuffer);
	printk(KERN_DEBUG "[HPV] Physical aligned allocated buffer for VMXON at 0x%llx\n", AlignedPhysicalBuffer);
	printk(KERN_DEBUG "[HPV] AlignedPhysicalBuffer1: 0x%llx\n", AlignedPhysicalBuffer1);

	IA32_VMX_BASIC_MSR basic = { 0 };

	basic.All = readmsr(MSR_IA32_VMX_BASIC);

	printk(KERN_DEBUG "[HPV] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier 0x%x\n", basic.Fields.RevisionIdentifier);

	*(uint64_t*)AlignedVirtualBuffer = basic.Fields.RevisionIdentifier;

	GuestState->VmxonRegion = AlignedPhysicalBuffer;

	return true;
}

bool AllocateVmcsRegion(PVIRTUAL_MACHINE_STATE GuestState) {
	int VMCSSize = 2*VMCS_SIZE;
	char* Buffer = (char*)kvzalloc(VMCSSize + ALIGNMENT_PAGE_SIZE, GFP_KERNEL);
	
	if (Buffer == NULL) {
		printk(KERN_DEBUG "[HPV][ERR] MmAllocateContiguousMemory failed\n");
		return false;
	}

	char* AlignedVirtualBuffer = (char*)((uint64_t)(Buffer + ALIGNMENT_PAGE_SIZE - 1) & ~(uint64_t)(ALIGNMENT_PAGE_SIZE - 1));

	uint64_t PhysicalBuffer = VirtualToPhysicalAddress(Buffer);
	uint64_t AlignedPhysicalBuffer = (uint64_t)(PhysicalBuffer + ALIGNMENT_PAGE_SIZE - 1) & ~(uint64_t)(ALIGNMENT_PAGE_SIZE - 1);
	uint64_t AlignedPhysicalBuffer1 = VirtualToPhysicalAddress(AlignedVirtualBuffer);

	printk(KERN_DEBUG "[HPV] Virtual allocated buffer for VMCS at 0x%llx\n", (uint64_t)Buffer);
	printk(KERN_DEBUG "[HPV] Virtual aligned allocated buffer for VMCS at 0x%llx\n", (uint64_t)AlignedVirtualBuffer);
	printk(KERN_DEBUG "[HPV] Physical aligned allocated buffer for VMCS at 0x%llx\n", AlignedPhysicalBuffer);
	printk(KERN_DEBUG "[HPV] AlignedPhysicalBuffer1: 0x%llx\n", AlignedPhysicalBuffer1);

	IA32_VMX_BASIC_MSR basic = { 0 };

	basic.All = readmsr(MSR_IA32_VMX_BASIC);

	printk(KERN_DEBUG "[HPV] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier 0x%x\n", basic.Fields.RevisionIdentifier);

	*(uint64_t*)AlignedVirtualBuffer = basic.Fields.RevisionIdentifier;

	GuestState->VmcsRegion = AlignedPhysicalBuffer;

	return true;
}

bool AllocateVmmStack(PVIRTUAL_MACHINE_STATE GuestState) {
	uint64_t VMM_STACK_VA = (uint64_t)kvzalloc(VMM_STACK_SIZE, GFP_KERNEL);
	GuestState->VmmStack = VMM_STACK_VA;

	printk(KERN_DEBUG "[HPV] VMM Stack is allocated at 0x%llx, current stack: 0x%llx\n", (uint64_t)GuestState->VmmStack, (uint64_t)&VMM_STACK_VA);

	if (GuestState->VmmStack == NULL) {
		printk(KERN_DEBUG "[HPV][ERR] kvzalloc failed for VMM Stack\n");
		return false;
	}

	return true;
}

bool AllocateMsrBitmap(PVIRTUAL_MACHINE_STATE GuestState) {
	GuestState->MsrBitmap = (uint64_t)kvzalloc(PAGE_SIZE, GFP_KERNEL);

	if (GuestState->MsrBitmap == NULL) {
		printk(KERN_DEBUG "[HPV][ERR] Error in allocating MSRBitmap\n");
		return false;
	}
	GuestState->MsrBitmapPhysical = VirtualToPhysicalAddress((void*)GuestState->MsrBitmap);

	return true;
}

bool AllocateGuestMemory(void){
	const int PagesToAllocate = 10;
	g_VirtualGuestMemoryAddress = (uint64_t)vmalloc_exec(PagesToAllocate); // change this to allocate executable memory

	if(g_VirtualGuestMemoryAddress == NULL){
		printk(KERN_DEBUG "[HPV][ERR] Error in allocating guest memory\n");
		return false;
	}

	return true;
}

void* vmalloc_exec(uint32_t n_pages) {
	void* addr = kvzalloc(n_pages * PAGE_SIZE, GFP_KERNEL);
	if (addr == NULL) {
		printk(KERN_DEBUG "[HPV][ERR] Error in allocating memory\n");
		return NULL;
	}
	// set_memory_x((unsigned long)addr, n_pages);
	return addr;
}
