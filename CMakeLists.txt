cmake_minimum_required(VERSION 3.25)

# Select a cross toolchain before CMake detects any compilers.
set(OS_ARCH riscv64 CACHE STRING "Target architecture")
set_property(CACHE OS_ARCH PROPERTY STRINGS riscv64 aarch64 x86_64)
if(NOT OS_ARCH MATCHES "^(riscv64|aarch64|x86_64)$")
    message(FATAL_ERROR "Unsupported OS_ARCH: ${OS_ARCH}")
endif()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/${OS_ARCH}-clang.cmake"
        CACHE FILEPATH "Cross toolchain")
endif()

project(os LANGUAGES C ASM)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL OS_ARCH)
    message(FATAL_ERROR "OS_ARCH disagrees with the toolchain; use a fresh build directory")
endif()
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
   NOT CMAKE_ASM_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "This configuration requires Clang for both C and assembly")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(common)

add_library(kernel_options INTERFACE)
string(TOUPPER "${OS_ARCH}" OS_ARCH_DEFINE_SUFFIX)
target_compile_definitions(kernel_options INTERFACE
    "KERNEL_ARCH_${OS_ARCH_DEFINE_SUFFIX}=1"
    "KERNEL_ARCH_NAME=\"${OS_ARCH}\"")
target_compile_features(kernel_options INTERFACE c_std_11)
target_link_libraries(kernel_options INTERFACE common)
target_include_directories(kernel_options INTERFACE
    "${PROJECT_SOURCE_DIR}/kernel/include"
    "${PROJECT_SOURCE_DIR}/kernel/arch/${OS_ARCH}/include"
    "${PROJECT_BINARY_DIR}/generated/include")
target_compile_options(kernel_options INTERFACE
    -ffreestanding -fno-pic -fno-pie -nostdlibinc
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-fno-stack-protector>"
    "$<$<COMPILE_LANGUAGE:ASM>:-x;assembler-with-cpp;-integrated-as>")
target_link_options(kernel_options INTERFACE -nostdlib -static -fuse-ld=lld
    "LINKER:--build-id=none")

if(OS_ARCH STREQUAL "x86_64")
    target_compile_options(kernel_options INTERFACE
        -mno-red-zone -mgeneral-regs-only -masm=intel)
elseif(OS_ARCH STREQUAL "riscv64")
    target_compile_options(kernel_options INTERFACE
        -march=rv64imac -mabi=lp64 -mcmodel=medany -mno-relax)
    target_link_options(kernel_options INTERFACE -march=rv64imac -mabi=lp64
        "LINKER:--no-relax")
elseif(OS_ARCH STREQUAL "aarch64")
    target_compile_options(kernel_options INTERFACE -mgeneral-regs-only)
endif()

add_subdirectory(kernel)
add_subdirectory(userspace)

if(OS_ARCH STREQUAL "riscv64")
    add_subdirectory(tests/memory)
endif()

option(OS_BUILD_CHECKS "Build a freestanding C/assembly toolchain check" OFF)
if(OS_BUILD_CHECKS)
    add_subdirectory(tests/build)
endif()
