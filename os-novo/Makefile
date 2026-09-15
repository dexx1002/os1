
TOOLCHAIN_PREFIX ?= riscv64-unknown-elf-
CC      := $(TOOLCHAIN_PREFIX)gcc
CXX     := $(TOOLCHAIN_PREFIX)g++
AS      := $(TOOLCHAIN_PREFIX)gcc
LD      := $(TOOLCHAIN_PREFIX)g++
QEMU    := qemu-system-riscv64

TARGET  := kernel.elf

ARCH_FLAGS := -march=rv64gc -mabi=lp64d -mcmodel=medany

GCC_INC := $(shell $(CXX) -print-file-name=include)

COMMON_FLAGS := $(ARCH_FLAGS) -O2 -g -Wall -Wextra \
                -ffreestanding -nostdlib -fno-builtin \
                -fno-stack-protector -fno-omit-frame-pointer \
                -I$(GCC_INC) -I.

CXXFLAGS := $(COMMON_FLAGS) -std=c++20 -fno-exceptions -fno-rtti
CFLAGS   := $(COMMON_FLAGS) -std=c11
ASFLAGS  := $(ARCH_FLAGS) -g -D__ASSEMBLY__
LDFLAGS  := $(ARCH_FLAGS) -T kernel.ld -nostdlib -Wl,--gc-sections

ASM_SRCS := $(shell find . -type f -name "*.S")
C_SRCS   := $(shell find . -type f -name "*.c")
CPP_SRCS := $(shell find . -type f -name "*.cpp")

BUILD_DIR := build
OBJS := $(addprefix $(BUILD_DIR)/, $(ASM_SRCS:.S=.o)) \
        $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o)) \
        $(addprefix $(BUILD_DIR)/, $(CPP_SRCS:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJS) kernel.ld
	@echo "[LD] Linkujem $@..."
	@$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "--- Uspešno kreiran bare-metal C++20/23 kernel! ---"

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "[QEMU] Pokrećem kernel u QEMU..."
	@$(QEMU) -machine virt -cpu rv64 -smp 1 -m 128M -nographic -kernel $(TARGET)

clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "[CLEAN] Očišćeno."

.PHONY: all run clean