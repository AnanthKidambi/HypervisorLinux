#include "vmx.h"
#include "msr.h"
#include "memory.h"
#include "ept.h"
#include "asm.h"

#include <linux/cpu.h>
#include <linux/slab.h>

extern int AsmVmxOffAndRestoreState(void);
extern void AsmVmexitHandler(void);
extern uint64_t g_VirtualGuestMemoryAddress;

bool ClearVmcsState(PVIRTUAL_MACHINE_STATE GuestState) {
	int status = vmx_vmclear(GuestState->VmcsRegion);

    printk(KERN_DEBUG "[HPV] VMCS VMCLEAR Status is: %d\n", status);

	if (status) {
		printk(KERN_DEBUG "[HPV][ERR] VMCS failed to clear with status %d\n", status);
		return false;
	}

	return true;
}

bool LoadVmcs(PVIRTUAL_MACHINE_STATE GuestState) {
	vmx_vmptrld(&GuestState->VmcsRegion);
    return true;
}

bool GetSegmentDescriptor(PSEGMENT_SELECTOR SegmentSelector, uint16_t Selector, uint8_t* GdtBase) {
	PSEGMENT_DESCRIPTOR SegDesc;

	if (!SegmentSelector) {
		return false;
	}

	if (Selector & 0x4) {	// bits 15-3 are useful, bit 2 is the Table Indicator
		return false;
	}

	SegDesc = (PSEGMENT_DESCRIPTOR)(GdtBase + (Selector & ~0x7)); // segment descriptors are 8 bytes long
	
	SegmentSelector->SEL = Selector;
	SegmentSelector->BASE = SegDesc->BASE0 | SegDesc->BASE1 << 16 | SegDesc->BASE2 << 24;
	SegmentSelector->LIMIT = SegDesc->LIMIT0 | ((SegDesc->LIMIT1ATTR1 & 0xf) << 16);
	SegmentSelector->ATTRIBUTES.UCHARs = SegDesc->ATTR0 | ((SegDesc->LIMIT1ATTR1 & 0xf0) << 4);

	if (!(SegDesc->ATTR0 & 0x10)) {	// system segment e.g. TSS
		uint64_t Tmp;
		Tmp = (*(uint64_t*)((uint8_t*)SegDesc + 8));
		SegmentSelector->BASE = (SegmentSelector->BASE & 0xffffffff) | (Tmp << 32);	// base low + base high
	}

	if (SegmentSelector->ATTRIBUTES.Fields.G) {	// granularity of limit is 4 KB
		SegmentSelector->LIMIT = (SegmentSelector->LIMIT << 12) + 0xfff;
	}

	return true;
}

void FillGuestSelectorData(void* GdtBase, uint32_t Segreg, uint16_t Selector) {
	SEGMENT_SELECTOR SegmentSelector = { 0 };
	uint32_t AccessRights;

	GetSegmentDescriptor(&SegmentSelector, Selector, (uint8_t*)GdtBase);
	AccessRights = ((uint8_t*)&SegmentSelector.ATTRIBUTES)[0] | ((uint32_t)(((uint8_t*)&SegmentSelector.ATTRIBUTES)[1]) << 12); 

	if (!Selector) {
		AccessRights |= 0x10000;
	}

	vmx_vmwrite(GUEST_ES_SELECTOR + (Segreg << 1), Selector); // 16 bit wide
	vmx_vmwrite(GUEST_ES_LIMIT + (Segreg << 1), SegmentSelector.LIMIT); // 32 bit wide
	vmx_vmwrite(GUEST_ES_AR_BYTES + (Segreg << 1), AccessRights); // 32 bit wide
	vmx_vmwrite(GUEST_ES_BASE + (Segreg << 1), SegmentSelector.BASE); // 64 bit wide (natural width)
}

uint32_t AdjustControls(uint32_t Ctl, uint32_t Msr) {
	MSR MsrValue = { 0 };

	MsrValue.Content = readmsr(Msr);
	Ctl &= MsrValue.Fields.High; /* bit == 0 in high word ==> must be zero */
	Ctl |= MsrValue.Fields.Low;  /* bit == 1 in low word  ==> must be one  */
	return Ctl;
}

bool SetupVmcs(PVIRTUAL_MACHINE_STATE GuestState, PEPTP EPTP) {
	uint64_t GdtBase = NULL;
	SEGMENT_SELECTOR SegmentSelector = { 0 };

	vmx_vmwrite(HOST_ES_SELECTOR, GetEs() & 0xf8);
	vmx_vmwrite(HOST_CS_SELECTOR, GetCs() & 0xf8);
	vmx_vmwrite(HOST_SS_SELECTOR, GetSs() & 0xf8);
	vmx_vmwrite(HOST_DS_SELECTOR, GetDs() & 0xf8);
	vmx_vmwrite(HOST_FS_SELECTOR, GetFs() & 0xf8);
	vmx_vmwrite(HOST_GS_SELECTOR, GetGs() & 0xf8);
	vmx_vmwrite(HOST_TR_SELECTOR, GetTr() & 0xf8);	// last 3 bits must be 0, else vmlaunch gives an error

	vmx_vmwrite(VMCS_LINK_POINTER, ~(0ULL));

	vmx_vmwrite(GUEST_IA32_DEBUGCTL, readmsr(MSR_IA32_DEBUGCTL) & 0xffffffff);
	vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, readmsr(MSR_IA32_DEBUGCTL) >> 32);

	vmx_vmwrite(TSC_OFFSET, 0);
	vmx_vmwrite(TSC_OFFSET_HIGH, 0);

	vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

	vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);

	// catch all exceptions
	vmx_vmwrite(EXCEPTION_BITMAP, 0xffffffff);

	GdtBase = GetGdtBase();

	FillGuestSelectorData((void*)GdtBase, ES, GetEs());
	FillGuestSelectorData((void*)GdtBase, CS, GetCs());
	FillGuestSelectorData((void*)GdtBase, SS, GetSs());
	FillGuestSelectorData((void*)GdtBase, DS, GetDs());
	FillGuestSelectorData((void*)GdtBase, FS, GetFs());
	FillGuestSelectorData((void*)GdtBase, GS, GetGs());
	FillGuestSelectorData((void*)GdtBase, LDTR, GetLdtr());
	FillGuestSelectorData((void*)GdtBase, TR, GetTr());

	vmx_vmwrite(GUEST_FS_BASE, readmsr(MSR_FS_BASE));
	vmx_vmwrite(GUEST_GS_BASE, readmsr(MSR_GS_BASE));	// what about limits and ARs?

	vmx_vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);
	vmx_vmwrite(GUEST_ACTIVITY_STATE, 0); // Active state

	vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, AdjustControls((uint32_t)CPU_BASED_HLT_EXITING | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, MSR_IA32_VMX_PROCBASED_CTLS));
	vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, AdjustControls(CPU_BASED_CTL2_RDTSCP, MSR_IA32_VMX_PROCBASED_CTLS2));

	vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, AdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
	vmx_vmwrite(VM_EXIT_CONTROLS, AdjustControls(VM_EXIT_IA32E_MODE /*| VM_EXIT_ACK_INTR_ON_EXIT*/	, MSR_IA32_VMX_EXIT_CTLS));
	vmx_vmwrite(VM_ENTRY_CONTROLS, AdjustControls(VM_ENTRY_IA32E_MODE, MSR_IA32_VMX_ENTRY_CTLS));

	vmx_vmwrite(CR3_TARGET_COUNT, 0);
	vmx_vmwrite(CR3_TARGET_VALUE0, 0);
	vmx_vmwrite(CR3_TARGET_VALUE1, 0);
	vmx_vmwrite(CR3_TARGET_VALUE2, 0);
	vmx_vmwrite(CR3_TARGET_VALUE3, 0);

	vmx_vmwrite(GUEST_CR0, readcr0());
	vmx_vmwrite(GUEST_CR3, readcr3());
	vmx_vmwrite(GUEST_CR4, readcr4());

	vmx_vmwrite(GUEST_DR7, 0x400);

	vmx_vmwrite(HOST_CR0, readcr0());
	vmx_vmwrite(HOST_CR3, readcr3());
	vmx_vmwrite(HOST_CR4, readcr4());

	vmx_vmwrite(GUEST_GDTR_BASE, GdtBase);
	vmx_vmwrite(GUEST_GDTR_LIMIT, GetGdtLimit());
	vmx_vmwrite(GUEST_IDTR_BASE, GetIdtBase());
	vmx_vmwrite(GUEST_IDTR_LIMIT, GetIdtLimit());

	vmx_vmwrite(GUEST_RFLAGS, GetRflags());

	vmx_vmwrite(GUEST_SYSENTER_CS, readmsr(MSR_IA32_SYSENTER_CS));
	vmx_vmwrite(GUEST_SYSENTER_ESP, readmsr(MSR_IA32_SYSENTER_ESP));
	vmx_vmwrite(GUEST_SYSENTER_EIP, readmsr(MSR_IA32_SYSENTER_EIP));

	GetSegmentDescriptor(&SegmentSelector, GetTr(), (uint8_t*)GdtBase);

	vmx_vmwrite(HOST_TR_BASE, SegmentSelector.BASE);

	vmx_vmwrite(HOST_FS_BASE, readmsr(MSR_FS_BASE));
	vmx_vmwrite(HOST_GS_BASE, readmsr(MSR_GS_BASE));

	vmx_vmwrite(HOST_GDTR_BASE, GdtBase);
	vmx_vmwrite(HOST_IDTR_BASE, GetIdtBase());

	vmx_vmwrite(HOST_IA32_SYSENTER_CS, readmsr(MSR_IA32_SYSENTER_CS));
	vmx_vmwrite(HOST_IA32_SYSENTER_ESP, readmsr(MSR_IA32_SYSENTER_ESP));
	vmx_vmwrite(HOST_IA32_SYSENTER_EIP, readmsr(MSR_IA32_SYSENTER_EIP));

	printk(KERN_DEBUG "[HPV] g_VirtualGuestMemoryAddress = 0x%llx, char1: 0x%x\n", g_VirtualGuestMemoryAddress, (uint32_t)(*(uint8_t*)g_VirtualGuestMemoryAddress));
	vmx_vmwrite(HOST_RSP, ((uint64_t)GuestState->VmmStack + VMM_STACK_SIZE - 1));
	vmx_vmwrite(HOST_RIP, (uint64_t)AsmVmexitHandler);

	vmx_vmwrite(GUEST_RSP, (uint64_t)g_VirtualGuestMemoryAddress); // setup guest sp
	vmx_vmwrite(GUEST_RIP, (uint64_t)g_VirtualGuestMemoryAddress); // setup guest ip

	return true;
}

void ResumeToNextInstruction(void) {
	void* ResumeRIP = NULL;
	void* CurrentRIP = NULL;
	uint32_t ExitInstructionLength = 0;

	vmx_vmread(GUEST_RIP, (uint64_t*)&CurrentRIP);
	vmx_vmread(VM_EXIT_INSTRUCTION_LEN, (uint64_t*)&ExitInstructionLength);

	ResumeRIP = (void*)((uint64_t)CurrentRIP + ExitInstructionLength);

	vmx_vmwrite(GUEST_RIP, (uint64_t)ResumeRIP);
}

void VmResumeInstruction(void) {
	vmx_vmresume();

	uint64_t ErrorCode = 0;
	vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	printk(KERN_DEBUG "[HPV][ERR] VMRESUME failed with error code 0x%llx\n", ErrorCode);
}

void MainVmExitHandler(PGUEST_REGS GuestRegs) {
	uint64_t ExitReason = 0;
	vmx_vmread(VM_EXIT_REASON, (uint64_t*)&ExitReason);

	uint64_t ExitQualification = 0;
	vmx_vmread(EXIT_QUALIFICATION, (uint64_t*)&ExitQualification);

	ExitReason &= 0xffffffff;

	switch (ExitReason) {
		case EXIT_REASON_VMCLEAR:
		case EXIT_REASON_VMPTRLD:
		case EXIT_REASON_VMPTRST:
		case EXIT_REASON_VMREAD:
		case EXIT_REASON_VMRESUME:
		case EXIT_REASON_VMWRITE:
		case EXIT_REASON_VMXOFF:
		case EXIT_REASON_VMXON:
		case EXIT_REASON_VMLAUNCH:
		case EXIT_REASON_EXCEPTION_NMI:
		case EXIT_REASON_CPUID:
		case EXIT_REASON_INVD:
		case EXIT_REASON_VMCALL:
		case EXIT_REASON_CR_ACCESS:
		case EXIT_REASON_MSR_READ:
		case EXIT_REASON_MSR_WRITE:
		case EXIT_REASON_EPT_VIOLATION:
		case EXIT_REASON_HLT:
		default: {
			printk(KERN_DEBUG "[HPV] Execution stopped due to exit reason: 0x%x, exit qualification: 0x%llx \n", (uint8_t)ExitReason, ExitQualification);
			AsmVmxOffAndRestoreState();
			break;
		}
	}
}
