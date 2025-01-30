.globl AsmVmexitHandler
.globl AsmVmxSaveState
.globl AsmVmxRestoreState
.globl AsmSetCs

.type AsmVmexitHandler, @function
.type AsmVmxSaveState, @function
.type AsmVmxRestoreState, @function
.type AsmSetCs, @function

#include <linux/linkage.h>

SYM_CODE_START(AsmVmexitHandler)
	// Guest state
	push %r15
	push %r14
	push %r13
	push %r12
	push %r11
	push %r10
	push %r9
	push %r8
	push %rdi
	push %rsi
	push %rbp
	push %rbp
	push %rbx
	push %rdx
	push %rcx
	push %rax
	// store and restore rflags as well

	mov %rsp, %rdi 	// first function argument is in rdi

	sub $32, %rsp 	// allocate shadow space
	call MainVmExitHandler
	add $32, %rsp	// deallocate shadow space

	// check whether we have to turn VMX off
	cmp $0, %al
	jne	VmxoffHandler

	pop %rax
	pop %rcx
	pop %rdx
	pop %rbx
	pop %rbp
	pop %rbp
	pop %rsi
	pop %rdi 
	pop %r8
	pop %r9
	pop %r10
	pop %r11
	pop %r12
	pop %r13
	pop %r14
	pop %r15
	
	// sub $32, %rsp 	// allocate shadow space
	vmresume
	// non returning function
SYM_CODE_END(AsmVmexitHandler)

SYM_CODE_START(AsmVmxSaveState)
	push %rax
	push %rcx
	push %rdx
	push %rbx
	push %rbp
	push %rbp
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15
	
	sub $32, %rsp
	mov %rsp, %rsi

	call VirtualizeCurrentSystemProcessor

	add $32, %rsp

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rbp
	pop %rbp
	pop %rbx
	pop %rdx
	pop %rcx
	pop %rax

	mov $1, %rax
	RET
SYM_CODE_END(AsmVmxSaveState)

SYM_CODE_START(AsmVmxRestoreState)
	add $32, %rsp

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rbp
	pop %rbp
	pop %rbx
	pop %rdx
	pop %rcx
	pop %rax

	mov $0, %rax

	RET
SYM_CODE_END(AsmVmxRestoreState)

SYM_CODE_START(VmxoffHandler) 

	vmxoff

	pop %rax
	pop %rcx
	pop %rdx
	pop %rbx
	pop %rbp
	pop %rbp
	pop %rsi
	pop %rdi
	pop %r8
	pop %r9
	pop %r10
	pop %r11
	pop %r12
	pop %r13
	pop %r14
	pop %r15

	push %rax
	push %rbx

	// rax = processor_id
	call GetProcessorId

	// rbx = g_VmxoffGuestPL[rax]
	mov g_VmxoffGuestPL, %rbx
	add %rax, %rbx // uint8_t array

	cmpb $0, (%rbx)
	jne ExitToUserMode

	cmpb $0, %al 
	je CaseCpu0Kernel
	
	cmpb $1, %al
	je CaseCpu1Kernel

CaseCpu0Kernel:
	// executing on cpu0
	mov %rsp, %rbx

	mov 0 + g_VmxoffGuestRflags, %rax
	push (%rax)
	popfq

	mov 0 + g_VmxoffGuestRSP, %rax
	mov (%rax), %rsp

	mov 0 + g_VmxoffGuestRIP, %rax
	mov (%rax), %rax
	mov %rax, Cpu0ScratchData

	mov 8(%rbx), %rax
	mov (%rbx), %rbx

	jmp *Cpu0ScratchData

CaseCpu1Kernel:
	// executing on cpu1
	mov %rsp, %rbx

	mov 0 + g_VmxoffGuestRflags, %rax
	add $8, %rax
	push (%rax)
	popfq

	mov 0 + g_VmxoffGuestRSP, %rax
	add $8, %rax
	mov (%rax), %rsp

	mov 0 + g_VmxoffGuestRIP, %rax
	add $8, %rax
	mov (%rax), %rax
	mov %rax, Cpu1ScratchData

	jmp *Cpu1ScratchData


ExitToUserMode:
	
	cmpb $0, %al 
	je CaseCpu0User
	
	cmpb $1, %al
	je CaseCpu1User

CaseCpu0User:

	mov %rsp, %rbx

	mov 0 + g_VmxoffGuestSs, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestRSP, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestRflags, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestCs, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestRIP, %rax
	push (%rax) 
	
	mov 8(%rbx), %rax
	mov (%rbx), %rbx
	
	iretq


CaseCpu1User:
	
	mov %rsp, %rbx

	mov 0 + g_VmxoffGuestSs, %rax
	add $8, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestRSP, %rax
	add $8, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestRflags, %rax
	add $8, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestCs, %rax
	add $8, %rax
	push (%rax)

	mov 0 + g_VmxoffGuestRIP, %rax
	add $8, %rax
	push (%rax) 
	
	mov 8(%rbx), %rax
	mov (%rbx), %rbx
	
	iretq


SYM_CODE_END(VmxoffHandler) 

SYM_FUNC_START(AsmSetCs)

	mov %rdi, %cs
	RET

SYM_FUNC_END(AsmSetCs)