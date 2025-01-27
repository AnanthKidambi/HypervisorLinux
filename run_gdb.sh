#! /bin/bash

HOME="/home/$(whoami)"
KERNEL_DIR="$HOME/linux-6.8.0"
VMLINUX_PATH="$KERNEL_DIR/vmlinux"
KERNEL_MODULE_DIR="$HOME/Hypervisor/kernel_mode"
KERNEL_MODULE_PATH="$KERNEL_MODULE_DIR/build/hellomod.ko"

gdb $VMLINUX_PATH   -ex "target remote localhost:1234" \
                    -ex "c" \
                    -ex "lx-symbols" \
                    -ex "lx-symbols $KERNEL_MODULE_DIR" \
