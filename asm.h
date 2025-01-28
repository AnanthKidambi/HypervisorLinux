#pragma once
#pragma GCC optimize ("O0") 

#include <linux/types.h>

uint64_t inline GetGdtBase(void);
uint16_t inline GetGdtLimit(void);

void inline SetGdtr(const char*);

uint64_t inline GetCs(void);
uint64_t inline GetDs(void);
uint64_t inline GetEs(void);
uint64_t inline GetFs(void);
uint64_t inline GetGs(void);
uint64_t inline GetSs(void);
uint64_t inline GetLdtr(void);
uint64_t inline GetTr(void);

void inline SetLdtr(uint64_t);
void inline SetTr(uint64_t);

uint64_t inline GetIdtBase(void);
uint16_t inline GetIdtLimit(void);

void inline SetIdtr(const char*);

uint64_t inline GetRflags(void);
void inline SetRflags(uint64_t);

uint64_t inline read_msr(uint64_t msr_id);
void inline write_msr(uint64_t msr_id, uint64_t msr_value);

uint64_t inline readcr0(void);
uint64_t inline readcr3(void);
uint64_t inline readcr4(void);

void inline writecr0(uint64_t);
void inline writecr3(uint64_t);
void inline writecr4(uint64_t);
void inline writedr7(uint64_t dr7);

void cpuidex(int32_t* CpuInfo, int32_t rax, int32_t rcx);

void inline vmx_on(uint64_t* physical_address);
void inline vmx_off(void);
uint64_t inline vmx_vmptrst(void);
void inline vmx_vmptrld(uint64_t* vmcs_pa);
int inline vmx_vmclear(uint64_t vmcs_pa);
void inline vmx_vmread(uint64_t field, uint64_t* value);
void inline vmx_vmwrite(uint64_t field, uint64_t value);
void inline vmx_vmresume(void);
void inline vmx_vmlaunch(void);

void inline AsmEnableVmxOperation(void);