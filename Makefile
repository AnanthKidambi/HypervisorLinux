CONFIG_MODULE_SIG=n

BUILD_DIR := $(PWD)/build 

obj-m += hellomod.o
hellomod-objs := asm.o driver.o memory.o save_state.o vmcs.o vmx.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(BUILD_DIR) src=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(BUILD_DIR) src=$(PWD) clean