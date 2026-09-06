# Code in this directory is written to be usable by both the kernel and
# userspace. Keep it freestanding: no kernel services and no libc dependency.
add_library(common STATIC
    endian.c fdt.c strings.c
)
target_include_directories(common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(common PRIVATE c_std_11)
target_compile_options(common PRIVATE
    -ffreestanding -fno-pic -fno-pie -nostdlibinc
    -Wall -Wextra)

# Add further freestanding sources here as they are introduced. The resulting
# library can be linked by the kernel and userspace programs independently of
# their runtime environments.
