CONFIG_MODULE_SIG=n

undefine CONFIG_OBJTOOL # temporoary, remove later once the jmp issue is fixed

BUILD_DIR := $(PWD)/build 

obj-m += hellomod.o
hellomod-objs := driver.o main_function.o asm.o memory.o save_state.o vmcs.o vmx.o

all:
	# make -C /lib/modules/$(shell uname -r)/build M=$(BUILD_DIR) src=$(PWD) modules
	make -C /home/ananthkk/linux-6.8.0 M=$(BUILD_DIR) src=$(PWD) modules "KCFLAGS += -Wno-error" 

clean:
	# make -C /lib/modules/$(shell uname -r)/build M=$(BUILD_DIR) src=$(PWD) clean
	make -C /home/ananthkk/linux-6.8.0 M=$(BUILD_DIR) src=$(PWD) clean
