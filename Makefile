# databus_ai Engine - Main Makefile
# High-performance AI preprocessing driver engine

.PHONY: all clean kernel user install uninstall load unload test help

# Default target
all: kernel user

# Build kernel module
kernel:
	@echo "=== Building Kernel Module ==="
	$(MAKE) -C kernel_module
	@echo "Kernel module build completed.\n"

# Build user application
user:
	@echo "=== Building User Application ==="
	$(MAKE) -C user_application
	@echo "User application build completed.\n"

# Clean all
clean:
	@echo "=== Cleaning All ==="
	$(MAKE) -C kernel_module clean
	$(MAKE) -C user_application clean
	@echo "Clean completed.\n"

# Install kernel module and user application
install: all
	@echo "=== Installing databus_ai Engine ==="
	@echo "Loading kernel module..."
	sudo insmod kernel_module/databus_ai.ko
	@echo "Installing user application..."
	$(MAKE) -C user_application install
	@echo "Installation completed.\n"

# Uninstall
uninstall:
	@echo "=== Uninstalling databus_ai Engine ==="
	@echo "Unloading kernel module..."
	-sudo rmmod databus_ai
	@echo "Uninstalling user application..."
	$(MAKE) -C user_application uninstall
	@echo "Uninstall completed.\n"

# Load kernel module with debug info
load: kernel
	@echo "=== Loading Kernel Module ==="
	sudo dmesg -C
	sudo insmod kernel_module/databus_ai.ko
	@echo "Module loaded. Recent kernel messages:"
	dmesg | tail -10
	@echo ""

# Unload kernel module
unload:
	@echo "=== Unloading Kernel Module ==="
	sudo rmmod databus_ai
	@echo "Module unloaded. Recent kernel messages:"
	dmesg | tail -5
	@echo ""

# Quick test
test: all load
	@echo "=== Running Quick Test ==="
	@echo "Checking device node..."
	ls -l /dev/databus_ai
	@echo "\nRunning client for 10 seconds..."
	timeout 10s sudo user_application/databus_client || true
	@echo "\nTest completed.\n"

# Show system status
status:
	@echo "=== System Status ==="
	@echo "Kernel module status:"
	lsmod | grep databus_ai || echo "Module not loaded"
	@echo "\nDevice node:"
	ls -l /dev/databus_ai 2>/dev/null || echo "Device node not found"
	@echo "\nRecent kernel messages:"
	dmesg | grep databus_ai | tail -5 || echo "No recent messages"
	@echo ""

# Development targets
dev-cycle: clean all load
	@echo "=== Development Cycle Completed ==="
	@echo "Ready for testing!\n"

# Check build environment
check-env:
	@echo "=== Checking Build Environment ==="
	@echo "Kernel version: $$(uname -r)"
	@echo "Kernel headers: $$(ls /lib/modules/$$(uname -r)/build 2>/dev/null && echo 'OK' || echo 'MISSING')"
	@echo "GCC version: $$(gcc --version | head -1)"
	@echo "liburing: $$(pkg-config --exists liburing && echo 'OK' || echo 'MISSING')"
	@echo "Root access: $$(sudo -n true 2>/dev/null && echo 'OK' || echo 'REQUIRED')"
	@echo ""

# Show project structure
tree:
	@echo "=== Project Structure ==="
	tree . 2>/dev/null || find . -type f -name '*.c' -o -name '*.h' -o -name 'Makefile' | sort
	@echo ""

# Show help
help:
	@echo "databus_ai Engine - Build System"
	@echo "================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build kernel module and user application (default)"
	@echo "  kernel     - Build kernel module only"
	@echo "  user       - Build user application only"
	@echo "  clean      - Clean all build artifacts"
	@echo "  install    - Install kernel module and user application"
	@echo "  uninstall  - Uninstall kernel module and user application"
	@echo "  load       - Load kernel module with debug info"
	@echo "  unload     - Unload kernel module"
	@echo "  test       - Run quick functionality test"
	@echo "  status     - Show system status"
	@echo "  dev-cycle  - Clean, build, and load (development workflow)"
	@echo "  check-env  - Check build environment"
	@echo "  tree       - Show project structure"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make              # Build everything"
	@echo "  make dev-cycle    # Development workflow"
	@echo "  make test         # Quick test"
	@echo "  make load && make user_application/databus_client  # Load and run"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - Linux kernel headers (linux-headers-\$(uname -r))"
	@echo "  - GCC compiler"
	@echo "  - liburing development package"
	@echo "  - Root privileges for module loading"
	@echo ""