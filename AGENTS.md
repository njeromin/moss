# OS

## Rules

- Use British English, do not use American English.
- All if, for, and while blocks should use curly braces, no matter how many lines are inside.
- Archicture headers in kernel/include/arch should be architecture agnostic in the sense that they do not contain any riscv,x86 etc specific code.

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

RISC-V paging integration tests use a separate `paging_tests.elf` with its own
entry point under `tests/memory/`. Build and run them in QEMU with:

```sh
cmake --preset riscv64-debug
cmake --build --preset riscv64-debug --target run_paging_tests
```

They check large-leaf selection, mixed page sizes, translation and permissions,
overlap rejection, and partial unmapping. The test runner uses two harts and
requires the concurrent-hart test to pass. It shuts QEMU down on success and
times out on a hang. The production `kernel.elf` contains no test entry points.

Memory operations use interrupt-safe spinlocks: one global paging lock covers
all roots and shared kernel tables, and a separate PMM lock protects bitmap
allocation/free. The lock order is paging then PMM. Queries hold the paging lock
while walking tables; VMM range operations hold it across the whole range.
Boot discovery, early allocation, and PMM initialization still run before other
harts use memory management. Creating the first additional address space reserves
shared lower-level tables for all kernel root slots (up to 1 MiB), including empty
slots, so later kernel mappings are visible in every space.

RISC-V mapping changes issue local fences and synchronous all-hart/all-ASID
[OpenSBI RFENCE](https://github.com/riscv-software-src/opensbi/blob/master/lib/sbi/sbi_tlb.c)
shootdowns before returning unmapped table frames to the PMM. RFENCE support is
required; a failed shootdown panics instead of allowing frame reuse. The global
lock and full flushes favor correctness over parallel throughput.

Callers must retain ownership of roots and data frames: a translation query does
not pin a frame, and destroying a space requires every hart to have switched away
and all users of that root to have stopped. Mapping/unmapping does not allocate
or free the mapped data frames. A range operation can still return a partial
failure; locking prevents competing operations during it, not rollback.

`tests/memory/concurrent_paging_test.c` contains the command for a host stress test
using the real PMM and paging code with CPU/TLB stubs. It runs eight writer threads
and a reader. The separate QEMU test image also starts one stopped hart
when available, exercise concurrent mapping, and check that replacing a mapping
invalidates a translation cached on that hart. Run QEMU with `-smp 2` or more to
exercise this test manually; `run_paging_tests` supplies two harts.
The secondary-hart entry is test-only, not a production SMP boot implementation.
