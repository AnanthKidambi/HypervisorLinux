#include <linux/init.h>
// #include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/cpu.h>

// #include "ept.h"
// #include "vmx.h"
#include "memory.h"
#include "asm.h"

int noinline start(void);
void noinline end(void);