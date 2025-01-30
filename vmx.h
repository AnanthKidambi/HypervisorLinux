#pragma once

#include <linux/types.h>

#include "ept.h"

#define VMXON_SIZE 4096
#define VMCS_SIZE 4096
#define ALIGNMENT_PAGE_SIZE	4096
#define VMM_STACK_SIZE 0x8000

#define RPL_MASK       3
#define DPL_SYSTEM 0

#define CPUID_RAX_FOR_VMEXIT 0x41414141
#define CPUID_RCX_FOR_VMEXIT 0x42424242

#define HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS   0x40000000
#define HYPERV_CPUID_INTERFACE                  0x40000001
#define HYPERV_CPUID_VERSION                    0x40000002
#define HYPERV_CPUID_FEATURES                   0x40000003
#define HYPERV_CPUID_ENLIGHTMENT_INFO           0x40000004
#define HYPERV_CPUID_IMPLEMENT_LIMITS           0x40000005

#define HYPERV_HYPERVISOR_PRESENT_BIT           0x80000000
#define HYPERV_CPUID_MIN                        0x40000005
#define HYPERV_CPUID_MAX                        0x4000ffff

bool IsVmxSupported(void);
bool InitiateVmx(void);
void TerminateVmx(void);
// bool VirtualizeCurrentSystem(int ProcessorID, PEPTP EPTP, void* GuestStack);
// bool LaunchVm(PEPTP EPTP);
// int LaunchVmProcessor(void* pvoid_data);
int VirtualizeCurrentSystemProcessor(void *pvoid_data, void *GuestStack);
void VirtualizeCurrentSystem(void);

typedef struct _VmmExtraData {
    uint64_t ExpectedGuestCr3;
} VmmExtraData, *PVmmExtraData;

typedef struct _LaunchVMProcessorData {
	uint32_t processor_id;
	PEPTP EPTP;
} LaunchVMProcessorData, *PLaunchVMProcessorData;

typedef struct _VIRTUAL_MACHINE_STATE {
	uint64_t VmxonRegion;
	uint64_t VmcsRegion;
	uint64_t Eptp;
	void* VmmStack;
	void* MsrBitmap;
	uint64_t MsrBitmapPhysical;
} VIRTUAL_MACHINE_STATE, * PVIRTUAL_MACHINE_STATE;

typedef union SEGMENT_ATTRIBUTES {
    uint16_t UCHARs;
    struct {
        uint16_t TYPE : 4; /* 0;  Bit 40-43 */
        uint16_t S : 1;    /* 4;  Bit 44 */
        uint16_t DPL : 2;  /* 5;  Bit 45-46 */
        uint16_t P : 1;    /* 7;  Bit 47 */

        uint16_t AVL : 1; /* 8;  Bit 52 */
        uint16_t L : 1;   /* 9;  Bit 53 */
        uint16_t DB : 1;  /* 10; Bit 54 */
        uint16_t G : 1;   /* 11; Bit 55 */

        uint16_t GAP : 4;
    } Fields;
} SEGMENT_ATTRIBUTES;

typedef struct SEGMENT_SELECTOR {
	uint16_t SEL; // bits 15-3 are the selector index, bits 1-0 are the RPL, bit 2 is the Table Indicator
    SEGMENT_ATTRIBUTES ATTRIBUTES;
    uint32_t LIMIT;
    uint64_t BASE;
} SEGMENT_SELECTOR, *PSEGMENT_SELECTOR;

typedef struct _SEGMENT_DESCRIPTOR
{
    uint16_t LIMIT0;
    uint16_t BASE0;
    uint8_t  BASE1;
	uint8_t  ATTR0;   // contains type, s, dpl, p
    uint8_t  LIMIT1ATTR1;
    uint8_t  BASE2;
} SEGMENT_DESCRIPTOR, * PSEGMENT_DESCRIPTOR;

enum SEGREGS {
    ES = 0,
    CS,
	SS,
	DS,
	FS,
	GS,
	LDTR,
	TR
};

typedef struct _GUEST_REGS { 
	uint64_t RAX;
	uint64_t RCX;
	uint64_t RDX;
	uint64_t RBX;
	uint64_t RSP;
	uint64_t RBP;
	uint64_t RSI;
	uint64_t RDI;
	uint64_t R8;
	uint64_t R9;
	uint64_t R10;
	uint64_t R11;
	uint64_t R12;
	uint64_t R13;
	uint64_t R14;
	uint64_t R15;
} GUEST_REGS, * PGUEST_REGS;

typedef union _RFLAGS {
    struct {
        unsigned Reserved1 : 10;
        unsigned ID : 1;  // Identification flag
        unsigned VIP : 1; // Virtual interrupt pending
        unsigned VIF : 1; // Virtual interrupt flag
        unsigned AC : 1;  // Alignment check
        unsigned VM : 1;  // Virtual 8086 mode
        unsigned RF : 1;  // Resume flag
        unsigned Reserved2 : 1;
        unsigned NT : 1;   // Nested task flag
        unsigned IOPL : 2; // I/O privilege level
        unsigned OF : 1;
        unsigned DF : 1;
        unsigned IF : 1; // Interrupt flag
        unsigned TF : 1; // Task flag
        unsigned SF : 1; // Sign flag
        unsigned ZF : 1; // Zero flag
        unsigned Reserved3 : 1;
        unsigned AF : 1; // Borrow flag
        unsigned Reserved4 : 1;
        unsigned PF : 1; // Parity flag
        unsigned Reserved5 : 1;
        unsigned CF : 1; // Carry flag [Bit 0]
        unsigned Reserved6 : 32;
    } Fields;

    uint64_t Content;
} RFLAGS;

typedef union _MOV_CR_QUALIFICATION
{
    uint32_t* All;
    struct
    {
        uint32_t ControlRegister : 4;
        uint32_t AccessType : 2;
        uint32_t LMSWOperandType : 1;
        uint32_t Reserved1 : 1;
        uint32_t Register : 4;
        uint32_t Reserved2 : 4;
        uint32_t LMSWSourceData : 16;
        uint32_t Reserved3;
    } Fields;
} MOV_CR_QUALIFICATION, *PMOV_CR_QUALIFICATION;

bool ClearVmcsState(PVIRTUAL_MACHINE_STATE GuestState);
bool LoadVmcs(PVIRTUAL_MACHINE_STATE GuestState);
bool GetSegmentDescriptor(PSEGMENT_SELECTOR SegmentSelector, uint16_t Selector, uint8_t* GdtBase);
void FillGuestSelectorData(void* GdtBase, uint32_t Segreg, uint16_t Selector);
uint32_t AdjustControls(uint32_t Ctl, uint32_t Msr);
bool SetupVmcsAndVirtualizeMachine(PVIRTUAL_MACHINE_STATE GuestState, PEPTP EPTP, void* GuestStack);
int8_t MainVmExitHandler(PGUEST_REGS GuestRegs);

#define CPU_BASED_VIRTUAL_INTR_PENDING        1 << 2
#define CPU_BASED_USE_TSC_OFFSETING           1 << 3
#define CPU_BASED_HLT_EXITING                 1 << 7
#define CPU_BASED_INVLPG_EXITING              1 << 9
#define CPU_BASED_MWAIT_EXITING               1 << 10
#define CPU_BASED_RDPMC_EXITING               1 << 11
#define CPU_BASED_RDTSC_EXITING               1 << 12
#define CPU_BASED_CR3_LOAD_EXITING            1 << 15
#define CPU_BASED_CR3_STORE_EXITING           1 << 16
#define CPU_BASED_ACTIVATE_TERTIARY_CONTROLS  1 << 17	
#define CPU_BASED_CR8_LOAD_EXITING            1 << 19
#define CPU_BASED_CR8_STORE_EXITING           1 << 20
#define CPU_BASED_TPR_SHADOW                  1 << 21
#define CPU_BASED_VIRTUAL_NMI_PENDING         1 << 22
#define CPU_BASED_MOV_DR_EXITING              1 << 23
#define CPU_BASED_UNCOND_IO_EXITING           1 << 24
#define CPU_BASED_ACTIVATE_IO_BITMAP          1 << 25
#define CPU_BASED_MONITOR_TRAP_FLAG           1 << 27
#define CPU_BASED_ACTIVATE_MSR_BITMAP         1 << 28
#define CPU_BASED_MONITOR_EXITING             1 << 29
#define CPU_BASED_PAUSE_EXITING               1 << 30
#define CPU_BASED_ACTIVATE_SECONDARY_CONTROLS 1 << 31

#define CPU_BASED_CTL2_VIRT_APIC_ACCESS             1 << 0
#define CPU_BASED_CTL2_ENABLE_EPT                   1 << 1
#define CPU_BASED_CTL2_DESC_TABLE_EXITING           1 << 2
#define CPU_BASED_CTL2_RDTSCP                       1 << 3
#define CPU_BASED_CTL2_VIRTUALIZE_X2APIC_MODE       1 << 4
#define CPU_BASED_CTL2_ENABLE_VPID                  1 << 5
#define CPU_BASED_CTL2_WBINVD_EXITING               1 << 6
#define CPU_BASED_CTL2_UNRESTRICTED_GUEST           1 << 7
#define CPU_BASED_CTL2_APIC_REGISTER_VIRT           1 << 8
#define CPU_BASED_CTL2_VIRTUAL_INTERRUPT_DELIVERY   1 << 9
#define CPU_BASED_CTL2_PAUSE_LOOP_EXITING           1 << 10
#define CPU_BASED_CTL2_RDRAND_EXITING               1 << 11
#define CPU_BASED_CTL2_ENABLE_INVPCID               1 << 12
#define CPU_BASED_CTL2_ENABLE_VMFUNC                1 << 13
#define CPU_BASED_CTL2_VMCS_SHADOWING               1 << 14
#define CPU_BASED_CTL2_ENABLE_ENCLS_EXITING         1 << 15
#define CPU_BASED_CTL2_RDSEED_EXITING               1 << 16
#define CPU_BASED_CTL2_ENABLE_PML                   1 << 17
#define CPU_BASED_CTL2_EPT_VIOLATION                1 << 18
#define CPU_BASED_CTL2_CONCEAL_VMX_FROM_PT          1 << 19
#define CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS         1 << 20
#define CPU_BASED_CTL2_PASID_TRANSLATION            1 << 21
#define CPU_BASED_CTL2_MODE_BASED_EXECUTION         1 << 22
#define CPU_BASED_CTL2_SUBPAGE_WRITE_PERM_EPT       1 << 23
#define CPU_BASED_CTL2_USE_TSC_SCALING              1 << 25
#define CPU_BASED_CTL2_ENABLE_USER_WAIT_PAUSE       1 << 26
#define CPU_BASED_CTL2_ENABLE_PCONFIG               1 << 27
#define CPU_BASED_CTL2_ENCLV_EXITING                1 << 28
#define CPU_BASED_CTL2_VMM_BUS_LOCK_DETECTION       1 << 30
#define CPU_BASED_CTL2_INSTR_TIMEOUT                1 << 31

#define VM_ENTRY_IA32E_MODE             1 << 9
#define VM_ENTRY_SMM                    1 << 10
#define VM_ENTRY_DEACT_DUAL_MONITOR     1 << 11
#define VM_ENTRY_LOAD_GUEST_PAT         1 << 14

#define VM_EXIT_IA32E_MODE              1 << 9
#define VM_EXIT_ACK_INTR_ON_EXIT        1 << 15
#define VM_EXIT_SAVE_GUEST_PAT          1 << 18
#define VM_EXIT_LOAD_HOST_PAT           1 << 19

#define PIN_BASED_VM_EXECUTION_CONTROLS_EXTERNAL_INTERRUPT        1 << 0
#define PIN_BASED_VM_EXECUTION_CONTROLS_NMI_EXITING               1 << 3
#define PIN_BASED_VM_EXECUTION_CONTROLS_VIRTUAL_NMI               1 << 5
#define PIN_BASED_VM_EXECUTION_CONTROLS_ACTIVE_VMX_TIMER          1 << 6
#define PIN_BASED_VM_EXECUTION_CONTROLS_PROCESS_POSTED_INTERRUPTS 1 << 7

enum VMCS_FIELDS {
    GUEST_ES_SELECTOR = 0x00000800,
    GUEST_CS_SELECTOR = 0x00000802,
    GUEST_SS_SELECTOR = 0x00000804,
    GUEST_DS_SELECTOR = 0x00000806,
    GUEST_FS_SELECTOR = 0x00000808,
    GUEST_GS_SELECTOR = 0x0000080a,
    GUEST_LDTR_SELECTOR = 0x0000080c,
    GUEST_TR_SELECTOR = 0x0000080e,
    HOST_ES_SELECTOR = 0x00000c00,
    HOST_CS_SELECTOR = 0x00000c02,
    HOST_SS_SELECTOR = 0x00000c04,
    HOST_DS_SELECTOR = 0x00000c06,
    HOST_FS_SELECTOR = 0x00000c08,
    HOST_GS_SELECTOR = 0x00000c0a,
    HOST_TR_SELECTOR = 0x00000c0c,
    IO_BITMAP_A = 0x00002000,
    IO_BITMAP_A_HIGH = 0x00002001,
    IO_BITMAP_B = 0x00002002,
    IO_BITMAP_B_HIGH = 0x00002003,
    MSR_BITMAP = 0x00002004,
    MSR_BITMAP_HIGH = 0x00002005,
    VM_EXIT_MSR_STORE_ADDR = 0x00002006,
    VM_EXIT_MSR_STORE_ADDR_HIGH = 0x00002007,
    VM_EXIT_MSR_LOAD_ADDR = 0x00002008,
    VM_EXIT_MSR_LOAD_ADDR_HIGH = 0x00002009,
    VM_ENTRY_MSR_LOAD_ADDR = 0x0000200a,
    VM_ENTRY_MSR_LOAD_ADDR_HIGH = 0x0000200b,
    TSC_OFFSET = 0x00002010,
    TSC_OFFSET_HIGH = 0x00002011,
    VIRTUAL_APIC_PAGE_ADDR = 0x00002012,
    VIRTUAL_APIC_PAGE_ADDR_HIGH = 0x00002013,
    VMFUNC_CONTROLS = 0x00002018,
    VMFUNC_CONTROLS_HIGH = 0x00002019,
    EPT_POINTER = 0x0000201A,
    EPT_POINTER_HIGH = 0x0000201B,
    EPTP_LIST = 0x00002024,
    EPTP_LIST_HIGH = 0x00002025,
    GUEST_PHYSICAL_ADDRESS = 0x2400,
    GUEST_PHYSICAL_ADDRESS_HIGH = 0x2401,
    VMCS_LINK_POINTER = 0x00002800,
    VMCS_LINK_POINTER_HIGH = 0x00002801,
    GUEST_IA32_DEBUGCTL = 0x00002802,
    GUEST_IA32_DEBUGCTL_HIGH = 0x00002803,
    PIN_BASED_VM_EXEC_CONTROL = 0x00004000,
    CPU_BASED_VM_EXEC_CONTROL = 0x00004002,
    EXCEPTION_BITMAP = 0x00004004,
    PAGE_FAULT_ERROR_CODE_MASK = 0x00004006,
    PAGE_FAULT_ERROR_CODE_MATCH = 0x00004008,
    CR3_TARGET_COUNT = 0x0000400a,
    VM_EXIT_CONTROLS = 0x0000400c,
    VM_EXIT_MSR_STORE_COUNT = 0x0000400e,
    VM_EXIT_MSR_LOAD_COUNT = 0x00004010,
    VM_ENTRY_CONTROLS = 0x00004012,
    VM_ENTRY_MSR_LOAD_COUNT = 0x00004014,
    VM_ENTRY_INTR_INFO_FIELD = 0x00004016,
    VM_ENTRY_EXCEPTION_ERROR_CODE = 0x00004018,
    VM_ENTRY_INSTRUCTION_LEN = 0x0000401a,
    TPR_THRESHOLD = 0x0000401c,
    SECONDARY_VM_EXEC_CONTROL = 0x0000401e,
    VM_INSTRUCTION_ERROR = 0x00004400,
    VM_EXIT_REASON = 0x00004402,
    VM_EXIT_INTR_INFO = 0x00004404,
    VM_EXIT_INTR_ERROR_CODE = 0x00004406,
    IDT_VECTORING_INFO_FIELD = 0x00004408,
    IDT_VECTORING_ERROR_CODE = 0x0000440a,
    VM_EXIT_INSTRUCTION_LEN = 0x0000440c,
    VMX_INSTRUCTION_INFO = 0x0000440e,
    GUEST_ES_LIMIT = 0x00004800,
    GUEST_CS_LIMIT = 0x00004802,
    GUEST_SS_LIMIT = 0x00004804,
    GUEST_DS_LIMIT = 0x00004806,
    GUEST_FS_LIMIT = 0x00004808,
    GUEST_GS_LIMIT = 0x0000480a,
    GUEST_LDTR_LIMIT = 0x0000480c,
    GUEST_TR_LIMIT = 0x0000480e,
    GUEST_GDTR_LIMIT = 0x00004810,
    GUEST_IDTR_LIMIT = 0x00004812,
    GUEST_ES_AR_BYTES = 0x00004814,
    GUEST_CS_AR_BYTES = 0x00004816,
    GUEST_SS_AR_BYTES = 0x00004818,
    GUEST_DS_AR_BYTES = 0x0000481a,
    GUEST_FS_AR_BYTES = 0x0000481c,
    GUEST_GS_AR_BYTES = 0x0000481e,
    GUEST_LDTR_AR_BYTES = 0x00004820,
    GUEST_TR_AR_BYTES = 0x00004822,
    GUEST_INTERRUPTIBILITY_INFO = 0x00004824,
    GUEST_ACTIVITY_STATE = 0x00004826,
    GUEST_SM_BASE = 0x00004828,
    GUEST_SYSENTER_CS = 0x0000482A,
    HOST_IA32_SYSENTER_CS = 0x00004c00,
    CR0_GUEST_HOST_MASK = 0x00006000,
    CR4_GUEST_HOST_MASK = 0x00006002,
    CR0_READ_SHADOW = 0x00006004,
    CR4_READ_SHADOW = 0x00006006,
    CR3_TARGET_VALUE0 = 0x00006008,
    CR3_TARGET_VALUE1 = 0x0000600a,
    CR3_TARGET_VALUE2 = 0x0000600c,
    CR3_TARGET_VALUE3 = 0x0000600e,
    EXIT_QUALIFICATION = 0x00006400,
    GUEST_LINEAR_ADDRESS = 0x0000640a,
    GUEST_CR0 = 0x00006800,
    GUEST_CR3 = 0x00006802,
    GUEST_CR4 = 0x00006804,
    GUEST_ES_BASE = 0x00006806,
    GUEST_CS_BASE = 0x00006808,
    GUEST_SS_BASE = 0x0000680a,
    GUEST_DS_BASE = 0x0000680c,
    GUEST_FS_BASE = 0x0000680e,
    GUEST_GS_BASE = 0x00006810,
    GUEST_LDTR_BASE = 0x00006812,
    GUEST_TR_BASE = 0x00006814,
    GUEST_GDTR_BASE = 0x00006816,
    GUEST_IDTR_BASE = 0x00006818,
    GUEST_DR7 = 0x0000681a,
    GUEST_RSP = 0x0000681c,
    GUEST_RIP = 0x0000681e,
    GUEST_RFLAGS = 0x00006820,
    GUEST_PENDING_DBG_EXCEPTIONS = 0x00006822,
    GUEST_SYSENTER_ESP = 0x00006824,
    GUEST_SYSENTER_EIP = 0x00006826,
    HOST_CR0 = 0x00006c00,
    HOST_CR3 = 0x00006c02,
    HOST_CR4 = 0x00006c04,
    HOST_FS_BASE = 0x00006c06,
    HOST_GS_BASE = 0x00006c08,
    HOST_TR_BASE = 0x00006c0a,
    HOST_GDTR_BASE = 0x00006c0c,
    HOST_IDTR_BASE = 0x00006c0e,
    HOST_IA32_SYSENTER_ESP = 0x00006c10,
    HOST_IA32_SYSENTER_EIP = 0x00006c12,
    HOST_RSP = 0x00006c14,
    HOST_RIP = 0x00006c16,
};

#define EXIT_REASON_EXCEPTION_NMI                       0
#define EXIT_REASON_EXTERNAL_INTERRUPT                  1
#define EXIT_REASON_TRIPLE_FAULT                        2
#define EXIT_REASON_INIT                                3
#define EXIT_REASON_SIPI                                4
#define EXIT_REASON_IO_SMI                              5
#define EXIT_REASON_OTHER_SMI                           6
#define EXIT_REASON_PENDING_VIRT_INTR                   7
#define EXIT_REASON_PENDING_VIRT_NMI                    8
#define EXIT_REASON_TASK_SWITCH                         9
#define EXIT_REASON_CPUID                               10
#define EXIT_REASON_GETSEC                              11
#define EXIT_REASON_HLT                                 12
#define EXIT_REASON_INVD                                13
#define EXIT_REASON_INVLPG                              14
#define EXIT_REASON_RDPMC                               15
#define EXIT_REASON_RDTSC                               16
#define EXIT_REASON_RSM                                 17
#define EXIT_REASON_VMCALL                              18
#define EXIT_REASON_VMCLEAR                             19
#define EXIT_REASON_VMLAUNCH                            20
#define EXIT_REASON_VMPTRLD                             21
#define EXIT_REASON_VMPTRST                             22
#define EXIT_REASON_VMREAD                              23
#define EXIT_REASON_VMRESUME                            24
#define EXIT_REASON_VMWRITE                             25
#define EXIT_REASON_VMXOFF                              26
#define EXIT_REASON_VMXON                               27
#define EXIT_REASON_CR_ACCESS                           28
#define EXIT_REASON_DR_ACCESS                           29
#define EXIT_REASON_IO_INSTRUCTION                      30
#define EXIT_REASON_MSR_READ                            31
#define EXIT_REASON_MSR_WRITE                           32
#define EXIT_REASON_INVALID_GUEST_STATE                 33
#define EXIT_REASON_MSR_LOADING                         34
#define EXIT_REASON_MWAIT_INSTRUCTION                   36
#define EXIT_REASON_MONITOR_TRAP_FLAG                   37
#define EXIT_REASON_MONITOR_INSTRUCTION                 39
#define EXIT_REASON_PAUSE_INSTRUCTION                   40
#define EXIT_REASON_MCE_DURING_VMENTRY                  41
#define EXIT_REASON_TPR_BELOW_THRESHOLD                 43
#define EXIT_REASON_APIC_ACCESS                         44
#define EXIT_REASON_ACCESS_GDTR_OR_IDTR                 46
#define EXIT_REASON_ACCESS_LDTR_OR_TR                   47
#define EXIT_REASON_EPT_VIOLATION                       48
#define EXIT_REASON_EPT_MISCONFIG                       49
#define EXIT_REASON_INVEPT                              50
#define EXIT_REASON_RDTSCP                              51
#define EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED        52
#define EXIT_REASON_INVVPID                             53
#define EXIT_REASON_WBINVD                              54
#define EXIT_REASON_XSETBV                              55
#define EXIT_REASON_APIC_WRITE                          56
#define EXIT_REASON_RDRAND                              57
#define EXIT_REASON_INVPCID                             58
#define EXIT_REASON_RDSEED                              61
#define EXIT_REASON_PML_FULL                            62
#define EXIT_REASON_XSAVES                              63
#define EXIT_REASON_XRSTORS                             64
#define EXIT_REASON_PCOMMIT                             65

// Exit Qualifications for MOV for Control Register Access
#define TYPE_MOV_TO_CR   0
#define TYPE_MOV_FROM_CR 1
#define TYPE_CLTS        2
#define TYPE_LMSW        3