# Kernel module name
obj-m := databus_ai.o

# Object files that make up the module
databus_ai-objs := databus_core.o databus_ioctl.o databus_mmap.o databus_io_uring.o

# Kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Compiler flags
ccflags-y := -DDEBUG

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Install target
install: all
	sudo insmod databus_ai.ko

# Uninstall target
uninstall:
	sudo rmmod databus_ai

# Load module with debug info
load: all
	sudo dmesg -C
	sudo insmod databus_ai.ko
	dmesg | tail -20

# Unload module and show logs
unload:
	sudo rmmod databus_ai
	dmesg | tail -10

# Show module info
info:
	modinfo databus_ai.ko

# Check if module is loaded
status:
	lsmod | grep databus_ai

# Show device node
device:
	ls -l /dev/databus_ai

# Show kernel logs
logs:
	dmesg | grep databus_ai | tail -20

.PHONY: all clean install uninstall load unload info status device logs