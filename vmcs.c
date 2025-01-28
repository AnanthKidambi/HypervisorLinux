#include "vmx.h"
#include "msr.h"
#include "memory.h"
#include "ept.h"
#include "asm.h"

#include <linux/cpu.h>
#include <linux/slab.h>

extern int AsmVmxOffAndRestoreState(void);
extern void AsmVmexitHandler(void);
extern void AsmGuestCode(void);
extern int AsmVmxRestoreState(void);

uint32_t g_Cr3TargetCount = 0;
void* g_VmxoffGuestRSP = 0;
void* g_VmxoffGuestRIP = 0;
uint64_t g_VmxoffGuestRflags = 0;
uint64_t g_VmxoffGuestCs = 0;
uint64_t g_VmxoffGuestSs = 0;
uint8_t g_VmxoffGuestPL = 0;

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

	MsrValue.Content = read_msr(Msr);
	Ctl &= MsrValue.Fields.High; /* bit == 0 in high word ==> must be zero */
	Ctl |= MsrValue.Fields.Low;  /* bit == 1 in low word  ==> must be one  */
	return Ctl;
}

static void SetHostStateFromGuest(void) {
	uint64_t cr0 = 0;
	vmx_vmread(GUEST_CR0, &cr0);
	writecr0(cr0);

	uint64_t cr3 = 0;
	vmx_vmread(GUEST_CR3, &cr3);
	writecr3(cr3);

	uint64_t cr4 = 0;
	vmx_vmread(GUEST_CR4, &cr4);
	writecr4(cr4);

	uint64_t dr7 = 0;
	vmx_vmread(GUEST_DR7, &dr7);
	writedr7(dr7);

	// TODO: Set MSRs from guest state

	uint64_t GdtBase = 0;
	vmx_vmread(GUEST_GDTR_BASE, &GdtBase);
	uint64_t GdtLimit = 0;
	vmx_vmread(GUEST_GDTR_LIMIT, &GdtLimit);
	char Gdtr[10] = { 0 };
	*(uint16_t*)(&Gdtr[0]) = GdtLimit & 0xffff;
	*(uint64_t*)(&Gdtr[2]) = GdtBase;
	SetGdtr(Gdtr);

	uint64_t IdtBase = 0;
	vmx_vmread(GUEST_IDTR_BASE, &IdtBase);
	uint64_t IdtLimit = 0;
	vmx_vmread(GUEST_IDTR_LIMIT, &IdtLimit);
	char Idtr[10] = { 0 };
	*(uint16_t*)(&Idtr[0]) = IdtLimit & 0xffff;
	*(uint64_t*)(&Idtr[2]) = IdtBase;
	SetIdtr(Idtr);
}

bool SetupVmcsAndVirtualizeMachine(PVIRTUAL_MACHINE_STATE GuestState, PEPTP EPTP, void* GuestStack) {
	uint64_t GdtBase = 0;
	SEGMENT_SELECTOR SegmentSelector = { 0 };

	vmx_vmwrite(HOST_ES_SELECTOR, GetEs() & 0xf8);
	vmx_vmwrite(HOST_CS_SELECTOR, GetCs() & 0xf8);
	vmx_vmwrite(HOST_SS_SELECTOR, GetSs() & 0xf8);
	vmx_vmwrite(HOST_DS_SELECTOR, GetDs() & 0xf8);
	vmx_vmwrite(HOST_FS_SELECTOR, GetFs() & 0xf8);
	vmx_vmwrite(HOST_GS_SELECTOR, GetGs() & 0xf8);
	vmx_vmwrite(HOST_TR_SELECTOR, GetTr() & 0xf8);	// last 3 bits must be 0, else vmlaunch gives an error

	vmx_vmwrite(VMCS_LINK_POINTER, ~(0ULL));

	vmx_vmwrite(GUEST_IA32_DEBUGCTL, read_msr(MSR_IA32_DEBUGCTL) & 0xffffffff);
	vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, read_msr(MSR_IA32_DEBUGCTL) >> 32);

	vmx_vmwrite(TSC_OFFSET, 0);
	vmx_vmwrite(TSC_OFFSET_HIGH, 0);

	vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

	vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);

	// catch all exceptions
	// vmx_vmwrite(EXCEPTION_BITMAP, 0xffffffff);

	GdtBase = GetGdtBase();

	FillGuestSelectorData((void*)GdtBase, ES, GetEs());
	FillGuestSelectorData((void*)GdtBase, CS, GetCs());
	FillGuestSelectorData((void*)GdtBase, SS, GetSs());
	FillGuestSelectorData((void*)GdtBase, DS, GetDs());
	FillGuestSelectorData((void*)GdtBase, FS, GetFs());
	FillGuestSelectorData((void*)GdtBase, GS, GetGs());
	FillGuestSelectorData((void*)GdtBase, LDTR, GetLdtr());
	FillGuestSelectorData((void*)GdtBase, TR, GetTr());

	vmx_vmwrite(GUEST_FS_BASE, read_msr(MSR_FS_BASE));
	vmx_vmwrite(GUEST_GS_BASE, read_msr(MSR_GS_BASE));	// what about limits and ARs?

	// vmx_vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);
	// vmx_vmwrite(GUEST_ACTIVITY_STATE, 0); // Active state

	vmx_vmwrite(
		CPU_BASED_VM_EXEC_CONTROL, 
		AdjustControls(
			(uint32_t)CPU_BASED_ACTIVATE_MSR_BITMAP | 
			// (uint32_t)CPU_BASED_RDTSC_EXITING | 
			(uint32_t)CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, 
			MSR_IA32_VMX_PROCBASED_CTLS
		)
	);
	vmx_vmwrite(
		SECONDARY_VM_EXEC_CONTROL, 
		AdjustControls(
			(uint32_t)CPU_BASED_CTL2_RDTSCP | 
			(uint32_t)CPU_BASED_CTL2_ENABLE_INVPCID | 
			// (uint32_t)CPU_BASED_CTL2_DESC_TABLE_EXITING |
			(uint32_t)CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS,
			MSR_IA32_VMX_PROCBASED_CTLS2)
	);

	vmx_vmwrite(
		PIN_BASED_VM_EXEC_CONTROL, 
		AdjustControls(
			0, 
			MSR_IA32_VMX_PINBASED_CTLS
		)
	);

	vmx_vmwrite(
		VM_EXIT_CONTROLS, 
		AdjustControls(
			VM_EXIT_IA32E_MODE | 
			VM_EXIT_ACK_INTR_ON_EXIT, 
			MSR_IA32_VMX_EXIT_CTLS
		)
	);	
	
	vmx_vmwrite(
		VM_ENTRY_CONTROLS, 
		AdjustControls(
			VM_ENTRY_IA32E_MODE, 
			MSR_IA32_VMX_ENTRY_CTLS
		)
	);

	vmx_vmwrite(CR3_TARGET_COUNT, 0);
	vmx_vmwrite(CR3_TARGET_VALUE0, 0);
	vmx_vmwrite(CR3_TARGET_VALUE1, 0);
	vmx_vmwrite(CR3_TARGET_VALUE2, 0);
	vmx_vmwrite(CR3_TARGET_VALUE3, 0);

	vmx_vmwrite(CR0_GUEST_HOST_MASK, 0);
	vmx_vmwrite(CR4_GUEST_HOST_MASK, 0);
	vmx_vmwrite(CR0_READ_SHADOW, 0);
	vmx_vmwrite(CR4_READ_SHADOW, 0);

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

	vmx_vmwrite(GUEST_SYSENTER_CS, read_msr(MSR_IA32_SYSENTER_CS));
	vmx_vmwrite(GUEST_SYSENTER_ESP, read_msr(MSR_IA32_SYSENTER_ESP));
	vmx_vmwrite(GUEST_SYSENTER_EIP, read_msr(MSR_IA32_SYSENTER_EIP));

	GetSegmentDescriptor(&SegmentSelector, GetTr(), (uint8_t*)GdtBase);

	vmx_vmwrite(HOST_TR_BASE, SegmentSelector.BASE);

	vmx_vmwrite(HOST_FS_BASE, read_msr(MSR_FS_BASE));
	vmx_vmwrite(HOST_GS_BASE, read_msr(MSR_GS_BASE));

	vmx_vmwrite(HOST_GDTR_BASE, GdtBase);
	vmx_vmwrite(HOST_IDTR_BASE, GetIdtBase());

	vmx_vmwrite(HOST_IA32_SYSENTER_CS, read_msr(MSR_IA32_SYSENTER_CS));
	vmx_vmwrite(HOST_IA32_SYSENTER_ESP, read_msr(MSR_IA32_SYSENTER_ESP));
	vmx_vmwrite(HOST_IA32_SYSENTER_EIP, read_msr(MSR_IA32_SYSENTER_EIP));

	vmx_vmwrite(MSR_BITMAP, GuestState->MsrBitmapPhysical);

	vmx_vmwrite(HOST_RSP, ((uint64_t)GuestState->VmmStack + VMM_STACK_SIZE - 1));
	vmx_vmwrite(HOST_RIP, (uint64_t)AsmVmexitHandler);

	vmx_vmwrite(GUEST_RSP, (uint64_t)GuestStack);
	vmx_vmwrite(GUEST_RIP, (uint64_t)AsmVmxRestoreState);

	{
		// sanity check
		uint64_t GuestRIP = 0;
		uint64_t GuestRSP = 0;
		uint64_t CR3 = 0;

		vmx_vmread(GUEST_RIP, &GuestRIP);
		vmx_vmread(GUEST_RSP, &GuestRSP);
		vmx_vmread(GUEST_CR3, &CR3);

		printk(KERN_DEBUG "[HPV] Guest RIP: 0x%llx, Expected: 0x%llx\n", GuestRIP, (uint64_t)AsmVmxRestoreState);
		printk(KERN_DEBUG "[HPV] Guest RSP: 0x%llx, Expected: 0x%llx\n", GuestRSP, (uint64_t)GuestStack);
		printk(KERN_DEBUG "[HPV] Guest CR3: 0x%llx, Expected: 0x%llx\n", CR3, readcr3());
	}

	return true;
}

bool SetTargetControls(uint64_t CR3, uint64_t Index) {
	if (Index >= 4) {
		return false;
	}
	if (CR3 == 0 && g_Cr3TargetCount <= 0) {
		return false;
	}
	if (Index == 0) {
		vmx_vmwrite(CR3_TARGET_VALUE0, CR3);
	}
	else if (Index == 1) {
		vmx_vmwrite(CR3_TARGET_VALUE1, CR3);
	}
	else if (Index == 2) {
		vmx_vmwrite(CR3_TARGET_VALUE2, CR3);
	}
	else if (Index == 3) {
		vmx_vmwrite(CR3_TARGET_VALUE3, CR3);
	}
	g_Cr3TargetCount += CR3 ? 1 : -1;

	vmx_vmwrite(CR3_TARGET_COUNT, g_Cr3TargetCount);
	return true;
}

static int8_t HandleCPUID(PGUEST_REGS state) {
	int32_t CpuInfo[4];
	uint64_t Mode = 0;

	vmx_vmread(GUEST_CS_SELECTOR, &Mode);
	Mode = Mode & RPL_MASK;	// ring value - check if it is running as ring 0 to issue VMXOFF indication

	if ((state->RAX == CPUID_RAX_FOR_VMEXIT) && (state->RCX == CPUID_RCX_FOR_VMEXIT) && (Mode == DPL_SYSTEM)) {
		return true; // turn off VMX
	}

	cpuidex(CpuInfo, (int32_t)state->RAX, (int32_t)state->RCX);	// Implement cpuid function

	if (state->RAX == 1) {
		CpuInfo[2] |= HYPERV_HYPERVISOR_PRESENT_BIT;
	}
	else if (state->RAX == HYPERV_CPUID_INTERFACE) {
		char* cpu_info = (char*)CpuInfo;
		cpu_info[0] = 'H';
		cpu_info[1] = 'V';
		cpu_info[2] = 'F';
		cpu_info[3] = 'S';
	}

	state->RAX = CpuInfo[0];
	state->RBX = CpuInfo[1];
	state->RCX = CpuInfo[2];
	state->RDX = CpuInfo[3];

	return 0; // don't have to turn off VMX
}

static uint8_t HandleControlRegisterAccess(PGUEST_REGS GuestState) {
	uint64_t ExitQualification = 0;
	vmx_vmread(EXIT_QUALIFICATION, &ExitQualification);

	PMOV_CR_QUALIFICATION data = (PMOV_CR_QUALIFICATION)&ExitQualification;
	uint64_t* RegPtr = (uint64_t*)&GuestState->RAX + data->Fields.Register;

	bool IsRsp = (data->Fields.Register == 4);
	if (IsRsp) {	// Reg is RSP
		uint64_t RSP = 0;
		vmx_vmread(GUEST_RSP, &RSP);
		*RegPtr = RSP;	// rsp stored in GuestState is wrong, get it from VMCS directly
	}

	switch (data->Fields.AccessType) {
		case TYPE_MOV_TO_CR: {
			switch (data->Fields.ControlRegister){
				case 0: {
					vmx_vmwrite(GUEST_CR0, *RegPtr);
					vmx_vmwrite(CR0_READ_SHADOW, *RegPtr);
					break;
				}
				case 3: {
					uint64_t guest_cr3 = (*RegPtr & ~(1ULL << 63));
					vmx_vmwrite(GUEST_CR3, guest_cr3);	// vmcs requires CR3[63] to be 0
					// update the host cr3 as well so that it can read the relevent userspace addresses
					write_cr3(guest_cr3);
					vmx_vmwrite(HOST_CR3, guest_cr3);
					break;
				}
				case 4: {
					vmx_vmwrite(GUEST_CR4, *RegPtr);
					vmx_vmwrite(CR4_READ_SHADOW, *RegPtr);
					break;
				}
				default: {
					printk(KERN_DEBUG "[HPV][ERR] Invalid Control Register\n");
					return 1;
				}
			}
			break;
		}
		case TYPE_MOV_FROM_CR: {
			switch (data->Fields.ControlRegister) {
				case 0: {
					vmx_vmread(GUEST_CR0, RegPtr);
					break;
				}
				case 3: {
					vmx_vmread(GUEST_CR3, RegPtr);
					break;
				}
				case 4: {
					vmx_vmread(GUEST_CR4, RegPtr);
					break;
				}
				default: {
					printk(KERN_DEBUG "[HPV][ERR] Invalid Control Register\n");
					return 1;
				}
			}
			if (IsRsp){
				vmx_vmwrite(GUEST_RSP, *RegPtr);
			}
			break;
		}
		default: {
			printk(KERN_DEBUG "[HPV][ERR] Invalid Control Register Operation\n");
			return 1;
		}
	}
	return 0;
}

static void HandleMSRRead(PGUEST_REGS GuestRegs) {
	MSR msr = { 0 };
	uint64_t rcx = GuestRegs->RCX;
	if ((rcx <= 0x00001FFF) || ((rcx >= 0xC0000000) && (rcx <= 0xC0001FFF))) {// value of rcx is valid (either low or high msr) 
		msr.Content = read_msr((uint32_t)rcx);
	}
	else {
		msr.Content = 0;
	}

	GuestRegs->RAX = msr.Fields.Low;
	GuestRegs->RDX = msr.Fields.High;
}

static void HandleMSRWrite(PGUEST_REGS GuestRegs) {
	MSR msr = { 0 };
	uint64_t rcx = GuestRegs->RCX;

	if ((rcx <= 0x00001FFF) || ((rcx >= 0xC0000000) && (rcx <= 0xC0001FFF))) {// value of rcx is valid (either low or high msr) 
		msr.Fields.Low = (uint32_t)GuestRegs->RAX;
		msr.Fields.High = (uint32_t)GuestRegs->RDX;
		write_msr((uint32_t)rcx, msr.Content);
	}
}

// add code for setting msr bitmap

static void ResumeToNextInstruction(void) {
	uint64_t ResumeRIP = 0;
	uint64_t CurrentRIP = 0;
	uint64_t ExitInstructionLength = 0;

	vmx_vmread(GUEST_RIP, &CurrentRIP);
	vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &ExitInstructionLength);

	ResumeRIP = CurrentRIP + ExitInstructionLength;

	vmx_vmwrite(GUEST_RIP, ResumeRIP);
}

void VmResumeInstruction(void) {
	vmx_vmresume();

	uint64_t ErrorCode = 0;
	vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	printk(KERN_DEBUG "[HPV][ERR] VMRESUME failed with error code 0x%llx\n", ErrorCode);
}

int8_t MainVmExitHandler(PGUEST_REGS GuestRegs) {
	uint64_t ExitReason = 0;
	vmx_vmread(VM_EXIT_REASON, &ExitReason);

	uint64_t ExitQualification = 0;
	vmx_vmread(EXIT_QUALIFICATION, &ExitQualification);

	uint8_t Status = 0;
	ExitReason &= 0xffffffff;

	uint64_t GuestRIP = 0;
	vmx_vmread(GUEST_RIP, &GuestRIP);

	switch (ExitReason) {
		case EXIT_REASON_CPUID:{
			Status = HandleCPUID(GuestRegs);
			if (Status) {
				printk(KERN_DEBUG "[HPV] CPUID called with exit params\n");
				goto exit_vmx;
			}
			break;
		}
		// case EXIT_REASON_MSR_READ:{
		// 	uint32_t ECX = GuestRegs->RCX & 0xffffffff;
		// 	printk(KERN_DEBUG "[HPV] MSR Read for ecx: 0x%x\n", ECX);
		// 	HandleMSRRead(GuestRegs);
		// 	break;
		// }
		// case EXIT_REASON_MSR_LOADING:{
		// 	printk(KERN_DEBUG "[HPV] Exit reason msr loading\n");
		// 	break;
		// }
		// case EXIT_REASON_MSR_WRITE:{
		// 	uint32_t ECX = GuestRegs->RCX & 0xffffffff;
		// 	printk(KERN_DEBUG "[HPV] MSR Write for ecx: 0x%x\n", ECX);
		// 	HandleMSRWrite(GuestRegs);
		// 	break;
		// }
		// case EXIT_REASON_EXCEPTION_NMI: {
		// 	printk(KERN_DEBUG "[HPV] Exit reason Exception or NMI\n");
		// 	break;
		// }
		// case EXIT_REASON_IO_INSTRUCTION: {
		// 	printk(KERN_DEBUG "[HPV] Exit reason IO Instruction\n");
		// 	break;
		// }
		// case EXIT_REASON_VMCLEAR:
		// case EXIT_REASON_VMPTRLD:
		// case EXIT_REASON_VMPTRST:
		// case EXIT_REASON_VMREAD:
		// case EXIT_REASON_VMRESUME:
		// case EXIT_REASON_VMWRITE:
		// case EXIT_REASON_VMXOFF:
		// case EXIT_REASON_VMXON:
		// case EXIT_REASON_VMLAUNCH:
		// case EXIT_REASON_INVD:
		// case EXIT_REASON_VMCALL:
		// case EXIT_REASON_EPT_VIOLATION:
		// case EXIT_REASON_HLT:
		case EXIT_REASON_CR_ACCESS:{
			Status = HandleControlRegisterAccess(GuestRegs);
			if (Status) {
				goto exit_vmx;
			}
			break;
		}
		default: {
		exit_vmx:
			printk(KERN_DEBUG "[HPV] Execution stopped due to exit reason: 0x%x, exit qualification: 0x%llx \n", (uint32_t)ExitReason, ExitQualification);
			printk(KERN_DEBUG "[HPV] Exiting\n");
			g_VmxoffGuestRSP = 0;
			g_VmxoffGuestRIP = 0;
			g_VmxoffGuestRflags = 0;
			g_VmxoffGuestCs = 0;
			g_VmxoffGuestSs = 0;

			vmx_vmread(GUEST_RIP, (uint64_t*)&g_VmxoffGuestRIP);
			vmx_vmread(GUEST_RSP, (uint64_t*)&g_VmxoffGuestRSP);
			vmx_vmread(GUEST_RFLAGS, &g_VmxoffGuestRflags);
			vmx_vmread(GUEST_CS_SELECTOR, &g_VmxoffGuestCs);
			vmx_vmread(GUEST_SS_SELECTOR, &g_VmxoffGuestSs);

			g_VmxoffGuestPL = (uint8_t)(g_VmxoffGuestCs & RPL_MASK);

			SetHostStateFromGuest();
			Status = 1;
			break;
		}
	}
	if (Status == 0) {
		ResumeToNextInstruction();
	}
	return Status;
}