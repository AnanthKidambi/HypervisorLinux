#! /bin/bash

MEMORY="2G"
NPROC=2

HOME="/home/$(whoami)"
KERNEL_DIR="$HOME/linux-6.8.0"
KERNEL_IMAGE_PATH="$KERNEL_DIR/arch/x86_64/boot/bzImage"
KERNEL_MODULE_DIR="$HOME/Hypervisor/kernel_mode"
KERNEL_MODULE_PATH="$KERNEL_MODULE_DIR/build/hellomod.ko"
INITRAMFS_DIR="$HOME/initramfs"
INITRAMFS_PATH="$KERNEL_MODULE_DIR/initramfs.cpio.gz"


# compile the kernel module
cd $KERNEL_MODULE_PATH
make clean
make

if [[ $? -ne 0 ]]; then
    echo "Error: Kernel module compilation failed"
    exit 1
fi

sleep 1

# copy the kernel module to the initramfs
cp $KERNEL_MODULE_PATH $INITRAMFS_DIR

# compress initramfs
cd $INITRAMFS_DIR
find . -print0 | cpio --null -ov --format=newc | gzip -9 > $INITRAMFS_PATH

enable_debugging=""
if [[ $1 == "-g" ]]; then
    enable_debugging="-s -S"
fi

# run qemu
qemu-system-x86_64 -kernel $KERNEL_IMAGE_PATH -m $MEMORY -smp $NPROC -initrd $INITRAMFS_PATH -append "nokaslr console=ttyS0" -nographic $enable_debugging -cpu host -accel kvm 
