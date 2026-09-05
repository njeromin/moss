set confirm off
set pagination off
set disassemble-next-line on
set print pretty on
set history save on

# QEMU gdbstub
file build/riscv64-debug/kernel.elf
# Try to connect; user can rerun "target remote" if QEMU not ready yet.
target remote :1234

# Common breakpoints
hbreak trap_handler

# Handy aliases
define rr
  target remote :1234
end

# Uncomment if you want to start immediately
# continue
