# Microkernel

A capability-based, preemptive microkernel written in C targeting 64-bit architectures, primarily RISC-V.

## Building

The project uses standard CMake presets.

```sh
cmake --preset riscv64-debug
cmake --build --preset riscv64-debug
```

## Running

### QEMU

Run the kernel and bundled initramfs directly using the preconfigured CMake target:

```sh
cmake --build --preset riscv64-debug --target run
```
