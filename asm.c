#include <linux/types.h>
#include "asm.h"

uint64_t GetGdtBase(){
    uint64_t gdt_base;
    asm __volatile (
        "lea -0xa(%%rsp), %%rax\n"
        "sgdt (%%rax)\n"
        "mov -0x8(%%rsp), %0\n"
        : "=r"(gdt_base)
        : 
        : "%rax"
    );
    return gdt_base;
}

uint16_t GetGdtLimit(){
    uint16_t gdt_limit;
    asm __volatile (
        "lea -0xa(%%rsp), %%rax\n"
        "sgdt (%%rax)\n"
        "mov -0xa(%%rsp), %0\n"
        : "=r"(gdt_limit)
        : 
        : "%rax"
    );
    return gdt_limit;
}

uint64_t GetCs(){
    uint64_t cs;
    asm __volatile (
        "mov %%cs, %0\n"
        : "=r"(cs)
        :
        :
    );
    return cs;
}

uint64_t GetDs(){
    uint64_t ds;
    asm __volatile (
        "mov %%ds, %0\n"
        : "=r"(ds)
        :
        :
    );
    return ds;
}

uint64_t GetEs(){
    uint64_t es;
    asm __volatile (
        "mov %%es, %0\n"
        : "=r"(es)
        :
        :
    );
    return es;
}

uint64_t GetFs(){
    uint64_t fs;
    asm __volatile (
        "mov %%fs, %0\n"
        : "=r"(fs)
        :
        :
    );
    return fs;
}

uint64_t GetGs(){
    uint64_t gs;
    asm __volatile (
        "mov %%gs, %0\n"
        : "=r"(gs)
        :
        :
    );
    return gs;
}

uint64_t GetSs(){
    uint64_t ss;
    asm __volatile (
        "mov %%ss, %0\n"
        : "=r"(ss)
        :
        :
    );
    return ss;
}

uint64_t GetLdtr(){
    uint64_t ldtr;
    asm __volatile (
        "sldt %0\n"
        : "=r"(ldtr)
        :
        :
    );
    return ldtr;
}

uint64_t GetTr(){
    uint64_t tr;
    asm __volatile (
        "str %0\n"
        : "=r"(tr)
        :
        :
    );
    return tr;
}

uint64_t GetIdtBase(){
    uint64_t idt_base;
    asm __volatile (
        "lea -0xa(%%rsp), %%rax\n"
        "sidt (%%rax)\n"
        "mov -0x8(%%rsp), %0\n"
        : "=r"(idt_base)
        : 
        : "%rax"
    );
    return idt_base;
}

uint16_t GetIdtLimit(){
    uint16_t idt_limit;
    asm __volatile (
        "lea -0xa(%%rsp), %%rax\n"
        "sidt (%%rax)\n"
        "mov -0xa(%%rsp), %0\n"
        : "=r"(idt_limit)
        : 
        : "%rax"
    );
    return idt_limit;
}

uint64_t GetRflags(){
    uint64_t rflags;
    asm __volatile (
        "pushfq\n"
        "pop %0\n"
        : "=r"(rflags)
        :
        :
    );
    return rflags;
}

uint64_t readmsr(uint64_t msr_id){
    uint64_t msr_low, msr_high;
    asm __volatile (
        "mov %2, %%rcx\n"
        "rdmsr\n"
        "mov %%rax, %0\n"
        "mov %%rdx, %1\n"
        : "=r"(msr_low), "=r"(msr_high)
        : "r"(msr_id)
        : "%rax", "%rdx", "%rcx"
    );
    return (msr_high << 32) | msr_low;
}

uint64_t readcr0(){
    uint64_t cr0;
    asm __volatile (
        "mov %%cr0, %0\n"
        : "=r"(cr0)
        :
        :
    );
    return cr0;
}

uint64_t readcr3(){
    uint64_t cr3;
    asm __volatile (
        "mov %%cr3, %0\n"
        : "=r"(cr3)
        :
        :
    );
    return cr3;
}

uint64_t readcr4(){
    uint64_t cr4;
    asm __volatile (
        "mov %%cr4, %0\n"
        : "=r"(cr4)
        :
        :
    );
    return cr4;
}

void vmx_on(uint64_t* physical_address){
    asm __volatile (
        "vmxon (%0)\n"
        :
        : "r"(physical_address)
        :
    );
}

void vmx_off(){
    asm __volatile (
        "vmxoff\n"
        :
        :
        :
    );
}


uint64_t vmx_vmptrst() {
	uint64_t vmcs_pa = 0;
    asm __volatile(
        "vmptrst (%0)\n"
        : 
        : "r"(&vmcs_pa)
        :
    );
	return vmcs_pa;
}

void vmx_vmptrld(uint64_t* vmcs_pa){
    asm __volatile(
        "vmptrld (%0)\n"
        : 
        : "r"(vmcs_pa)
        :
    );
}

int vmx_vmclear(uint64_t vmcs_pa){
    asm __volatile(
        "vmclear (%0)\n"
        :
        : "r"(&vmcs_pa) // why was =r here
        :
    );
    // If the operand is the current-VMCS pointer, then that pointer is made invalid (set to FFFFFFFF_FFFFFFFFH).
    return (~vmcs_pa) == 0;
}

void vmx_vmread(uint64_t field, uint64_t* value){
    uint64_t temp = 0;
    asm __volatile(
        "mov %1, %%rax\n"
        "vmread %%rax, %0\n"    // field, value
        : "=r"(temp)
        : "r"(field)
        : "%rax"
    );
    *value = temp;
}

void vmx_vmwrite(uint64_t field, uint64_t value){
    asm __volatile(
        "mov %0, %%rax\n"
        "vmwrite %1, %%rax\n"   // value, field
        : 
        : "r"(field), "r"(value)
        : "%rax"
    );
}

void vmx_vmresume(void){
    asm __volatile(
        "vmresume\n"
        :
        :
        :
    );
}

void AsmEnableVmxOperation(){
    uint64_t cr4 = 0;
    asm __volatile (
        "mov %%cr4, %0\n"
        :"=r"(cr4) 
        :
        : 
    );
    cr4 |= (1 << 13);
    asm __volatile (
        "mov %0, %%cr4\n"
        :
        : "r"(cr4)
        :
    );
}