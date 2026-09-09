# OS

## RISC-V higher-half bootstrap

The OpenSBI boot entry loads at physical `0x80200000`. It creates Sv39 page
tables, then enters the kernel linked at virtual `0xffffffff80000000`, backed
by physical memory starting at `0x80400000`. The 2 MiB separation keeps the
bootstrap outside the kernel's initial 2 MiB superpage. The linker rejects a
kernel that exceeds that mapping, including its 16 KiB boot-hart stack.

For initial bring-up, both the high kernel mapping and the identity mapping
of `0x80000000..0xbfffffff` are supervisor-only read/write/execute mappings.
The identity mapping keeps the bootstrap accessible. Before enabling paging,
the bootstrap reads the DTB's size and identity-maps every 1 GiB region it
touches, so its placement may vary with QEMU's RAM size. These mappings do
not imply that the entire ranges contain usable RAM. DTBs must fit within
Sv39's lower canonical half (below 256 GiB). `entry.c` receives the DTB's
physical address and mapped buffer bound. Memory management must replace these temporary
mappings with section-specific permissions and a deliberate physical-memory
mapping before removing the identity mapping. Device MMIO is not mapped yet.
Only the OpenSBI boot hart may use this entry; secondary harts need another path.

At VMM initialization, the replacement root rebuilds the HHDM from discovered
RAM within the existing 128 GiB physical-address window. It maps only complete
RAM pages, excludes every page touched by a `NO_MAP` range, and keeps ordinary
reserved RAM mapped so kernel and allocator metadata remain accessible. Direct
mappings are supervisor read/write and non-executable. Core memory code selects
the ranges and maps them through the VMM. RISC-V selects 1 GiB, 2 MiB, or
4 KiB leaves according to alignment, range boundaries, and existing tables.
Unmapping a 4 KiB page splits larger leaves while preserving neighboring mappings.
When creating another address space, kernel 1 GiB leaves are demoted to shared
2 MiB tables so later partial unmaps remain consistent across copied kernel roots.
Accessed/dirty history from a split leaf is conservatively inherited by its children. The bootstrap HHDM stays active
until the new root is ready; kernel section-specific permissions remain TODO.

The direct-map builder has a standalone host test; its compile/run command is
at the top of `tests/memory/direct_map_test.c`.

Early allocation copies the discovered memory map into static bookkeeping and
reserves aligned allocations from available RAM mapped via the direct physical map (HHDM).
There is no fixed linker arena. The DTB, firmware reservations, kernel, and
bootstrap tables are excluded. The PMM allocates all descriptors and bitmaps
before marking reservations, then disables early allocation. Its metadata must
remain mapped when the temporary page tables are replaced. Each available and
reserved list currently has capacity for 64 entries; allocation fails when the
reserved list is full.

```sh
cmake --preset riscv64-debug
cmake --build --preset riscv64-debug --target run
```

The `run` target builds the kernel before launching QEMU with OpenSBI, 128 MiB
RAM, and one hart. Press **Ctrl-a, then x** to exit QEMU.
Use `--target debug` instead to pause at startup with GDB on localhost:1234.
In another terminal, run `gdb` from this repository and break on
`arch_startup_core_entry`, then `continue`; its PC and stack should both be high.
Both targets also work with the `riscv64-release` build preset.

## Assembly editor support

`.asm-lsp.toml` configures asm-lsp for RISC-V documentation/completions and
Clang diagnostics with C preprocessing and the kernel include paths. Run the
language server from the repository root and enable it for `.S` files in your
editor. Restart it after changing the configuration.

The config supplies explicit compiler flags because asm-lsp searches for a
compilation database at the root or in `build/`, while our CMake presets put
databases in architecture-specific subdirectories. Keep these flags in sync
with CMake. Add architecture-specific project overrides when introducing
assembly for other architectures; the current default is RISC-V.

## Building

Requires CMake 3.25+, Ninja, Clang with RISC-V/AArch64/x86 backends, and LLD.
Clang compiles both C and assembly; no NASM or separate GNU assembler is needed.

The selected architecture is exported to kernel C and preprocessed assembly as
one feature macro, such as `KERNEL_ARCH_RISCV64`, plus the string macro
`KERNEL_ARCH_NAME`. Use the feature macro for conditional code; compiler-provided
macros such as `__riscv` remain available for instruction-set details.

```sh
cmake --preset riscv64-debug
cmake --build --preset riscv64-debug
```

Also available: `aarch64-debug`, `x86_64-debug`, and corresponding `-release`
presets. Each configuration has its own `build/<preset>/` directory. Plain
`cmake -S . -B build/default -G Ninja` defaults to RISC-V and Clang.
Use a fresh build directory when changing architecture or toolchain.

Add explicit shared source paths
to `KERNEL_SOURCES` in `kernel/CMakeLists.txt`; architecture source lists can
live in `kernel/arch/<arch>/CMakeLists.txt` (paths are relative to `kernel/`).
The build automatically uses `kernel/arch/<arch>/linker.ld` to produce
`build/<preset>/kernel.elf`; architectures with sources must supply that script.
Changes to the script trigger relinking. RISC-V has QEMU `run` and `debug`
targets; other platforms and image packaging are not implemented yet.

## Assembly and headers

Use uppercase `.S` for assembly. The build explicitly invokes Clang's integrated
assembler with C preprocessing enabled. x86-64 uses Intel syntax via
`-masm=intel`; RISC-V and AArch64 use their native syntax. Assembler directives
and macros use GAS syntax, not NASM syntax.

Shared kernel headers belong in `kernel/include/kernel/`; architecture headers
belong in `kernel/arch/<arch>/include/arch/`. Private headers stay beside their
sources. The build also exposes `build/<preset>/generated/include/` for future
generated headers; generators must declare outputs and target dependencies.

Headers included by both C and assembly should expose plain numeric macros and
guard C declarations with `#ifndef __ASSEMBLER__`. C expressions such as
`offsetof` cannot be evaluated by the assembler: use checked numeric offsets
or generate them with the target compiler. See `tests/build/offsets.h`.

## Checking the toolchain

```sh
cmake --preset x86_64-debug -DOS_BUILD_CHECKS=ON
cmake --build --preset x86_64-debug --target build_check
```

Repeat with the other architectures to check cross compilation. This compiles
C and preprocessed assembly sharing a checked structure offset, then links a
freestanding ELF with LLD. It is a build check, not a bootable kernel or runtime
test.

Each populated build exports `compile_commands.json`. `.clangd` defaults to
`build/riscv64-debug`; change that path to select another configuration. When a
kernel ELF exists, use `gdb -ex 'file build/<preset>/kernel.elf'` to override the
default executable loaded by `.gdbinit`.

Optional RISC-V paging integration tests run inside QEMU after VMM initialization:

```sh
cmake --preset riscv64-debug -DOS_PAGING_TESTS=ON
cmake --build --preset riscv64-debug --target run
```

They check large-leaf selection, mixed page sizes, translation and permissions,
overlap rejection, and partial unmapping. Set `OS_PAGING_TESTS=OFF` to disable.
