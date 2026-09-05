# Keep source lists explicit. Add shared .c files here and architecture .c/.S
# files in arch/<architecture>/CMakeLists.txt using list(APPEND ...).
set(KERNEL_SOURCES)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/arch/${OS_ARCH}/CMakeLists.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/arch/${OS_ARCH}/CMakeLists.txt")
endif()

set(linker_script "${CMAKE_CURRENT_SOURCE_DIR}/arch/${OS_ARCH}/linker.ld")
if(KERNEL_SOURCES)
    if(EXISTS "${linker_script}")
        add_executable(kernel ${KERNEL_SOURCES})
        set_target_properties(kernel PROPERTIES SUFFIX .elf
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
        target_link_options(kernel PRIVATE "LINKER:-T,${linker_script}")
        set_property(TARGET kernel APPEND PROPERTY LINK_DEPENDS "${linker_script}")
    else()
        message(FATAL_ERROR "Missing architecture linker script: ${linker_script}")
    endif()
    target_link_libraries(kernel PRIVATE kernel_options)
    if(OS_ARCH STREQUAL "riscv64")
        foreach(mode IN ITEMS run debug)
            add_custom_target(${mode}
                COMMAND "${CMAKE_COMMAND}"
                    "-DKERNEL_ELF=$<TARGET_FILE:kernel>"
                    "-DMODE=${mode}"
                    -P "${PROJECT_SOURCE_DIR}/cmake/run-riscv64.cmake"
                DEPENDS kernel
                USES_TERMINAL
                VERBATIM)
        endforeach()
    endif()
else()
    add_custom_target(kernel
        COMMAND "${CMAKE_COMMAND}" -E echo
            "No kernel sources yet. Add sources to kernel/CMakeLists.txt.")
    message(STATUS "No kernel sources yet; no kernel ELF will be produced")
endif()
