# Reuse production sources, replacing only the kernel entry point. This target
# is opt-in and never injects test code or defines into the production kernel.
get_target_property(kernel_sources kernel SOURCES)
get_target_property(kernel_source_dir kernel SOURCE_DIR)
set(paging_sources)
foreach(source IN LISTS kernel_sources)
    if(NOT source STREQUAL "core/kmain.c")
        get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${kernel_source_dir}")
        list(APPEND paging_sources "${source}")
    endif()
endforeach()

add_executable(paging_tests EXCLUDE_FROM_ALL ${paging_sources}
    kmain.c paging_test.c paging_smp_test.c paging_smp_entry.S)
set_target_properties(paging_tests PROPERTIES SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_link_libraries(paging_tests PRIVATE kernel_options)
target_include_directories(paging_tests PRIVATE "${PROJECT_SOURCE_DIR}/kernel/arch/riscv64")
set(linker_script "${PROJECT_SOURCE_DIR}/kernel/arch/riscv64/linker.ld")
target_link_options(paging_tests PRIVATE "LINKER:-T,${linker_script}")
set_property(TARGET paging_tests APPEND PROPERTY LINK_DEPENDS "${linker_script}")

add_custom_target(run_paging_tests
    COMMAND "${CMAKE_COMMAND}" "-DTEST_ELF=$<TARGET_FILE:paging_tests>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run.cmake"
    DEPENDS paging_tests
    USES_TERMINAL
    VERBATIM)
