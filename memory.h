#pragma once

#include <linux/types.h>

#include "vmx.h"

uint64_t VirtualToPhysicalAddress(void* va);
uint64_t PhysicalToVirtualAddress(uint64_t pa);
bool AllocateVMXonRegion(PVIRTUAL_MACHINE_STATE GuestState);
bool AllocateVmcsRegion(PVIRTUAL_MACHINE_STATE GuestState);
bool AllocateVmmStack(PVIRTUAL_MACHINE_STATE GuestState);
bool AllocateMsrBitmap(PVIRTUAL_MACHINE_STATE GuestState);
