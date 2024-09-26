#pragma once

#include <linux/types.h>

uint64_t inline GetGdtBase(void);
uint16_t inline GetGdtLimit(void);
uint64_t inline GetCs(void);
uint64_t inline GetDs(void);
uint64_t inline GetEs(void);
uint64_t inline GetFs(void);
uint64_t inline GetGs(void);
uint64_t inline GetSs(void);
uint64_t inline GetLdtr(void);
uint64_t inline GetTr(void);
uint64_t inline GetIdtBase(void);
uint16_t inline GetIdtLimit(void);
uint64_t inline GetRflags(void);

uint64_t inline readmsr(uint64_t msr_id);
uint64_t inline readcr0(void);
uint64_t inline readcr3(void);
uint64_t inline readcr4(void);
uint64_t inline readdr(uint64_t reg);

void inline vmx_on(uint64_t* physical_address);
void inline vmx_off(void);
uint64_t inline vmx_vmptrst(void);
void inline vmx_vmptrld(uint64_t* vmcs_pa);
int inline vmx_vmclear(uint64_t vmcs_pa);
void inline vmx_vmread(uint64_t field, uint64_t* value);
void inline vmx_vmwrite(uint64_t field, uint64_t value);
void inline vmx_vmresume(void);

void inline AsmEnableVmxOperation(void);