cmake_minimum_required(VERSION 3.20)

# Podešavanje cross-compilera pre definisanja projekta
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)

project(kernel C CXX ASM)

# Omogućavanje C++23 standarda (uz fallback na C++20 ako zatreba)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Zastavice za prevođenje samostalnog (bare-metal) jezgra
add_compile_options(
    -march=rv64ima_zicsr_zifencei
    -mabi=lp64
    -mcmodel=medany
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -g
)

# Putanje do zaglavlja (.hpp)
include_directories(h)

# Prikupljanje svih izvornih fajlova
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c" "src/*.s" "src/*.S")

# Definisanje izvršnog fajla
add_executable(kernel.elf ${SOURCES})

# Povezivanje sa linker skriptom
target_link_options(kernel.elf PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -nostdlib
)