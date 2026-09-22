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
        -march=rv64imac_zicfilp1p0 -menable-experimental-extensions -fcf-protection=branch
        -mabi=lp64 -mcmodel=medany -mno-relax)
    target_link_options(userspace_options INTERFACE
        -march=rv64imac_zicfilp1p0 -menable-experimental-extensions
        -mabi=lp64
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

find_program(PYTHON NAMES python3 python REQUIRED)

# Helper function to register a user-space programme target for inclusion in the
# initramfs CPIO archive.
# ARCHIVE_PATH specifies the relative path inside the archive (defaults to target name).
function(register_userspace_program target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "ARCHIVE_PATH" "")
    if(NOT ARG_ARCHIVE_PATH)
        set(ARG_ARCHIVE_PATH "${target}")
    endif()
    get_property(registered_targets GLOBAL PROPERTY USERSPACE_ARCHIVE_TARGETS)
    if("${target}" IN_LIST registered_targets)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY USERSPACE_ARCHIVE_TARGETS "${target}")
    set_property(GLOBAL APPEND PROPERTY USERSPACE_ARCHIVE_PATHS "${ARG_ARCHIVE_PATH}")
endfunction()

# Helper function to define a standard user-space executable and register it
# automatically for inclusion in the initramfs CPIO archive.
function(add_userspace_executable target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE userspace_options userspace_crt)
    set_target_properties(${target} PROPERTIES SUFFIX .elf
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/userspace")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${USERSPACE_LINKER_SCRIPT}")
    register_userspace_program(${target})
endfunction()

# User-space programme subdirectories
add_subdirectory(services/initd)

# List of user-space programmes to package into the CPIO newc archive.
# Future programmes or services can be added to this list or registered using
# register_userspace_program(<target> [ARCHIVE_PATH <name>]).
set(USERSPACE_PROGRAMS
    initd
)

foreach(prog IN LISTS USERSPACE_PROGRAMS)
    register_userspace_program("${prog}")
endforeach()

get_property(archive_targets GLOBAL PROPERTY USERSPACE_ARCHIVE_TARGETS)
get_property(archive_paths GLOBAL PROPERTY USERSPACE_ARCHIVE_PATHS)

set(INITRAMFS_ARCHIVE "${PROJECT_BINARY_DIR}/initramfs.cpio" CACHE FILEPATH
    "Path to generated initramfs CPIO archive")

set(cpio_entries)
list(LENGTH archive_targets target_count)
if(target_count GREATER 0)
    math(EXPR last_idx "${target_count} - 1")
    foreach(idx RANGE ${last_idx})
        list(GET archive_targets ${idx} tgt)
        list(GET archive_paths ${idx} path_in_archive)
        list(APPEND cpio_entries "--entry" "$<TARGET_FILE:${tgt}>:${path_in_archive}")
    endforeach()
endif()

add_custom_command(
    OUTPUT "${INITRAMFS_ARCHIVE}"
    COMMAND "${PYTHON}" "${PROJECT_SOURCE_DIR}/cmake/create_cpio.py"
        --output "${INITRAMFS_ARCHIVE}"
        ${cpio_entries}
    DEPENDS ${archive_targets} "${PROJECT_SOURCE_DIR}/cmake/create_cpio.py"
    COMMENT "Generating CPIO newc initramfs archive (${INITRAMFS_ARCHIVE})"
    VERBATIM
)

add_custom_target(initramfs ALL DEPENDS "${INITRAMFS_ARCHIVE}")
add_custom_target(userspace DEPENDS ${archive_targets} initramfs)

