# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
# LDFLAGS = -luring  # 暂时注释掉liburing依赖
LDFLAGS =

# Target executable
TARGET = databus_client

# Source files
SRCS = databus_client.c

# Object files
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = databus_shared.h

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build completed: $(TARGET)"

# Compile source files
%.o: %.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	@echo "Cleaning up..."
	rm -f $(OBJS) $(TARGET)
	@echo "Clean completed."

# Install target (copy to /usr/local/bin)
install: $(TARGET)
	@echo "Installing $(TARGET)..."
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)
	@echo "Installation completed."

# Uninstall target
uninstall:
	@echo "Uninstalling $(TARGET)..."
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstall completed."

# Run target (requires root for device access)
run: $(TARGET)
	@echo "Running $(TARGET)..."
	sudo ./$(TARGET)

# Debug build
debug: CFLAGS += -DDEBUG -g
debug: $(TARGET)

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@which gcc > /dev/null || (echo "ERROR: gcc not found" && exit 1)
	# @pkg-config --exists liburing || (echo "ERROR: liburing not found" && exit 1)  # 暂时注释掉liburing检查
	@echo "All dependencies satisfied."

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the application (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  run       - Build and run the application"
	@echo "  debug     - Build with debug symbols"
	@echo "  check-deps- Check build dependencies"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make              # Build the application"
	@echo "  make run          # Build and run"
	@echo "  make clean all    # Clean and rebuild"
	@echo "  sudo make install # Install system-wide"

.PHONY: all clean install uninstall run debug check-deps help