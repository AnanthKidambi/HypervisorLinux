.globl AsmSaveRegsAndLaunchVM
.globl AsmVmxOffAndRestoreState
.globl AsmVmexitHandler

.type AsmSaveRegsAndLaunchVM, @function
.type AsmVmxOffAndRestoreState, @function
.type AsmVmexitHandler, @function

#include <linux/linkage.h>

SYM_CODE_START(AsmSaveRegsAndLaunchVM)
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
	push %rdx
	push %rcx
	push %rbx
	push %rax

	mov %rsp, g_StackPointerForReturning
	mov %rbp, g_BasePointerForReturning
	vmlaunch

	// the following instructions are executed only if vmlaunch fails

	// return 1 to state that vmlaunch failed 
	add $120, %rsp
	mov $1, %rax
	RET

SYM_CODE_END(AsmSaveRegsAndLaunchVM)

SYM_CODE_START(AsmVmxOffAndRestoreState)
	mov g_StackPointerForReturning, %rsp
	mov g_BasePointerForReturning, %rbp

	// restore the old registers
	pop %rax
	pop %rbx
	pop %rcx
	pop %rdx
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

	mov $0, %rax
	RET

SYM_CODE_END(AsmVmxOffAndRestoreState)

SYM_CODE_START(AsmVmexitHandler)
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
	push %rdx
	push %rcx
	push %rbx
	push %rax

	mov %rsp, %rdi 	// first function argument is in rdi

	sub $32, %rsp 	// allocate shadow space
	call MainVmExitHandler
	add $32, %rsp	// deallocate shadow space

	pop %rax
	pop %rbx
	pop %rcx
	pop %rdx
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
	
	sub $32, %rsp 	// allocate shadow space
	// jmp VmResumeInstruction
	// non returning function
SYM_CODE_END(AsmVmexitHandler)