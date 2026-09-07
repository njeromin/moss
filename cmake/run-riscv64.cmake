# Resolve QEMU at launch time so compile-only builds do not require it.
find_program(QEMU NAMES qemu-system-riscv64 REQUIRED)
set(args -machine virt -m 8192M -smp 1 -bios default
    -kernel "${KERNEL_ELF}" -nographic)
if(MODE STREQUAL "debug")
    list(APPEND args -S -gdb tcp:127.0.0.1:1234)
    message(STATUS "Waiting for GDB on localhost:1234")
endif()
message(STATUS "QEMU: press Ctrl-a, then x to exit")
execute_process(COMMAND "${QEMU}" ${args} RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "QEMU exited with: ${result}")
endif()
