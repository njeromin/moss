find_program(QEMU NAMES qemu-system-riscv64 REQUIRED)
execute_process(
    COMMAND "${QEMU}" -machine virt -cpu rv64,ssdbltrp=true,smdbltrp=true
        -m 128M -smp 2 -bios default -kernel "${TEST_ELF}" -nographic
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors
    TIMEOUT 20)
if(NOT result STREQUAL "0" OR
   NOT output MATCHES "All paging tests passed[.]" OR
   NOT output MATCHES "Concurrent hart mapping and remote TLB shootdown passed[.]")
    message(FATAL_ERROR "Paging tests failed (${result}):\n${output}\n${errors}")
endif()
message(STATUS "Paging tests passed, including concurrent harts and remote TLB shootdown")
