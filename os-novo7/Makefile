# Kompajlerski alati
CXX = riscv64-unknown-elf-g++
AS  = riscv64-unknown-elf-gcc
LD  = riscv64-unknown-elf-ld

# Flegovi za bare-metal C++20 (promeni na -std=c++23 ako tvoj toolchain
# podržava, pa proveri da li se build/ponašanje menja kod tebe)
CXXFLAGS = -mcmodel=medany -std=c++20 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti -nostdlib -O2 -g
ASFLAGS  = -mcmodel=medany -g
LDFLAGS  = -T kernel.ld

# QEMU flegovi (-bios none sprečava preklapanje sa OpenSBI; -smp 2 za SMP
# korak -- mora se poklapati sa RISCV::MaxHarts u riscv.hpp i MAX_HARTS u
# boot.S)
QEMUFLAGS = -machine virt -cpu rv64 -smp 2 -m 128M -nographic -bios none -kernel kernel.elf

# Fajlovi
AS_SRCS  = boot.S contextSwitch.S
CXX_SRCS = main.cpp kalloc.cpp uart.cpp thread.cpp scheduler.cpp semaphore.cpp smp.cpp
OBJS     = $(AS_SRCS:.S=.o) $(CXX_SRCS:.cpp=.o)

all: kernel.elf

%.o: %.S
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

%.o: %.cpp
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

kernel.elf: $(OBJS)
	@echo "[LD] Linkujem kernel.elf..."
	@$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "--- Uspesno kreiran bare-metal C++20 kernel! ---"

run: kernel.elf
	@echo "[QEMU] Pokrecem kernel u QEMU..."
	@qemu-system-riscv64 $(QEMUFLAGS)

clean:
	@rm -f *.o kernel.elf
	@echo "Ociscen."

.PHONY: all run clean
