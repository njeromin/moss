add_library(userspace_options INTERFACE)
target_compile_features(userspace_options INTERFACE c_std_11)
target_compile_options(userspace_options INTERFACE
    -ffreestanding -fno-pic -fno-pie -nostdlibinc
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-fno-stack-protector>"
    "$<$<COMPILE_LANGUAGE:ASM>:-x;assembler-with-cpp;-integrated-as>")
target_link_options(userspace_options INTERFACE
    -nostdlib -static -fuse-ld=lld
    "LINKER:--build-id=none")

if(OS_ARCH STREQUAL "riscv64")
    target_compile_options(userspace_options INTERFACE
        -march=rv64imac -mabi=lp64 -mcmodel=medany -mno-relax)
    target_link_options(userspace_options INTERFACE
        -march=rv64imac -mabi=lp64
        "LINKER:--no-relax")
elseif(OS_ARCH STREQUAL "x86_64")
    target_compile_options(userspace_options INTERFACE
        -mno-red-zone -mgeneral-regs-only -masm=intel)
elseif(OS_ARCH STREQUAL "aarch64")
    target_compile_options(userspace_options INTERFACE
        -mgeneral-regs-only)
endif()

set(USERSPACE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
target_link_options(userspace_options INTERFACE "LINKER:-T,${USERSPACE_LINKER_SCRIPT}")

add_library(userspace_crt OBJECT libc/crt0.S)
target_link_libraries(userspace_crt PUBLIC userspace_options)

add_subdirectory(services/initd)

add_custom_target(userspace DEPENDS initd)
