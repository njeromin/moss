# Run with cmake -DKERNEL_ELF=build/riscv64-debug/kernel.elf
#                -P tests/smp/run.cmake
find_program(QEMU qemu-system-riscv64 REQUIRED)
foreach(cpu_count IN ITEMS 1 4 65)
    execute_process(
        COMMAND "${QEMU}" -machine virt -bios default -kernel "${KERNEL_ELF}"
            -m 256M -smp "${cpu_count}" -nographic -no-reboot
        TIMEOUT 15
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        RESULT_VARIABLE result)
    # Production deliberately parks rather than shutting down, so timeout is
    # expected. Require every acknowledgement and successful bootstrap return.
    if(NOT output MATCHES "All ${cpu_count} CPUs acknowledged startup[.]" OR
       NOT output MATCHES "Reached end of kmain[.]" OR
       output MATCHES "KERNEL PANIC|KERNEL TRAP")
        message(FATAL_ERROR "SMP startup failed (${cpu_count} CPUs, ${result}):\n${output}\n${errors}")
    endif()
    string(REGEX MATCHALL "CPU [0-9]+ started and parked[.]" acknowledgements "${output}")
    list(LENGTH acknowledgements acknowledgement_count)
    math(EXPR expected_count "${cpu_count} - 1")
    list(REMOVE_DUPLICATES acknowledgements)
    list(LENGTH acknowledgements unique_count)
    if(NOT acknowledgement_count EQUAL expected_count OR
       NOT unique_count EQUAL expected_count)
        message(FATAL_ERROR "Missing or duplicate CPU acknowledgements:\n${output}")
    endif()
    message(STATUS "SMP startup passed with ${cpu_count} CPUs")
endforeach()
