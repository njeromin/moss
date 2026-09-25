# Code in this directory is written to be usable by both the kernel and
# userspace. Keep it freestanding: no kernel services and no libc dependency.
add_library(common STATIC
    endian.c fdt.c strings.c memory.c maths.c
)
target_include_directories(common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(common PRIVATE c_std_11)
target_compile_options(common PRIVATE
    -ffreestanding -fno-pic -fno-pie -nostdlibinc
    -Wall -Wextra)
if(OS_ARCH STREQUAL "riscv64")
    target_compile_options(common PRIVATE
        -march=rv64imac_zicfilp1p0 -menable-experimental-extensions -fcf-protection=branch
        -mabi=lp64 -mcmodel=medany -mno-relax)
endif()

# Add further freestanding sources here as they are introduced. The resulting
# library can be linked by the kernel and userspace programs independently of
# their runtime environments.
