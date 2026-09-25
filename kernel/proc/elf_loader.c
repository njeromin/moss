#include <proc/elf_loader.h>

#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <asm/paging_info.h>
#include <common/elf.h>
#include <common/memory.h>
#include <core/console.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <proc/process.h>
#include <proc/task.h>

#define _CONSOLE_LOG_SUBJECT "elf_loader"

#define MAX_BOOTSTRAP_SEGMENTS 16

struct loaded_segment {
  virt_addr_t virt_start;
  phys_addr_t phys_start;
  size_t frame_count;
};

int elf_load_bootstrap(struct process* proc, const void* elf_data,
                       size_t elf_size, struct elf_load_result* out_result) {
  if (proc == NULL || elf_data == NULL || out_result == NULL ||
      elf_size < sizeof(struct elf64_ehdr)) {
    return ELF_LOAD_ERR_INVALID_HEADER;
  }

  const struct elf64_ehdr* ehdr = (const struct elf64_ehdr*)elf_data;

  // Validate ELF magic bytes.
  if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
      ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
    console_log(_CONSOLE_LOG_SUBJECT, "Invalid ELF magic bytes.\n");
    return ELF_LOAD_ERR_INVALID_HEADER;
  }

  // Validate 64-bit and little-endian encoding.
  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
      ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
    console_log(_CONSOLE_LOG_SUBJECT, "Unsupported ELF class or encoding.\n");
    return ELF_LOAD_ERR_UNSUPPORTED;
  }

  // Validate executable type.
  if (ehdr->e_type != ET_EXEC) {
    console_log(_CONSOLE_LOG_SUBJECT, "ELF is not an executable.\n");
    return ELF_LOAD_ERR_UNSUPPORTED;
  }

  // Validate target machine architecture.
#if defined(KERNEL_ARCH_RISCV64)
  if (ehdr->e_machine != EM_RISCV) {
    console_log(_CONSOLE_LOG_SUBJECT,
                "Mismatch in machine architecture (expected RISC-V).\n");
    return ELF_LOAD_ERR_UNSUPPORTED;
  }
#elif defined(KERNEL_ARCH_X86_64)
  if (ehdr->e_machine != EM_X86_64) {
    console_log(_CONSOLE_LOG_SUBJECT,
                "Mismatch in machine architecture (expected x86-64).\n");
    return ELF_LOAD_ERR_UNSUPPORTED;
  }
#elif defined(KERNEL_ARCH_AARCH64)
  if (ehdr->e_machine != EM_AARCH64) {
    console_log(_CONSOLE_LOG_SUBJECT,
                "Mismatch in machine architecture (expected AArch64).\n");
    return ELF_LOAD_ERR_UNSUPPORTED;
  }
#endif

  if (ehdr->e_phentsize < sizeof(struct elf64_phdr) || ehdr->e_phnum == 0) {
    console_log(_CONSOLE_LOG_SUBJECT,
                "Invalid program header table parameters.\n");
    return ELF_LOAD_ERR_CORRUPT;
  }

  size_t ph_table_size = (size_t)ehdr->e_phnum * ehdr->e_phentsize;
  if (ehdr->e_phoff > elf_size || ph_table_size > elf_size - ehdr->e_phoff) {
    console_log(_CONSOLE_LOG_SUBJECT,
                "Program header table exceeds file bounds.\n");
    return ELF_LOAD_ERR_CORRUPT;
  }

  if (ehdr->e_entry == 0 || ehdr->e_entry >= USER_STACK_TOP) {
    console_log(_CONSOLE_LOG_SUBJECT, "Invalid entry point: 0x%zx.\n",
                (size_t)ehdr->e_entry);
    return ELF_LOAD_ERR_CORRUPT;
  }

  struct loaded_segment segments[MAX_BOOTSTRAP_SEGMENTS];
  size_t loaded_count = 0;
  int status = ELF_LOAD_OK;

  for (elf64_half_t i = 0; i < ehdr->e_phnum; i++) {
    const struct elf64_phdr* phdr =
        (const struct elf64_phdr*)((const uint8_t*)elf_data + ehdr->e_phoff +
                                   (size_t)i * ehdr->e_phentsize);

    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    if (phdr->p_memsz == 0) {
      continue;
    }

    if (phdr->p_memsz < phdr->p_filesz) {
      console_log(_CONSOLE_LOG_SUBJECT, "Segment memsz < filesz.\n");
      status = ELF_LOAD_ERR_CORRUPT;
      goto fail;
    }

    if (phdr->p_offset > elf_size ||
        phdr->p_filesz > elf_size - phdr->p_offset) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Segment file range exceeds file bounds.\n");
      status = ELF_LOAD_ERR_CORRUPT;
      goto fail;
    }

    if (phdr->p_vaddr < PAGE_SIZE || phdr->p_vaddr >= USER_STACK_TOP ||
        phdr->p_memsz > USER_STACK_TOP - phdr->p_vaddr) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Segment virtual address out of bounds: 0x%zx.\n",
                  (size_t)phdr->p_vaddr);
      status = ELF_LOAD_ERR_CORRUPT;
      goto fail;
    }

    if (loaded_count >= MAX_BOOTSTRAP_SEGMENTS) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Exceeded maximum bootstrap segments (%d).\n",
                  MAX_BOOTSTRAP_SEGMENTS);
      status = ELF_LOAD_ERR_UNSUPPORTED;
      goto fail;
    }

    virt_addr_t seg_start = align_down(phdr->p_vaddr, PAGE_SIZE);
    virt_addr_t seg_end = align_up(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
    size_t seg_size = seg_end - seg_start;
    size_t frame_count = seg_size / PAGE_SIZE;

    uintptr_t phys_addr = 0;
    if (pmm_alloc(frame_count, &phys_addr) != PMM_OK) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Failed to allocate physical memory for segment.\n");
      status = ELF_LOAD_ERR_NOMEM;
      goto fail;
    }

    void* kernel_virt = (void*)arch_phys_to_virt(phys_addr);
    memory_set(kernel_virt, 0, seg_size);

    if (phdr->p_filesz > 0) {
      size_t offset_in_page = phdr->p_vaddr - seg_start;
      memory_copy((uint8_t*)kernel_virt + offset_in_page,
                  (const uint8_t*)elf_data + phdr->p_offset, phdr->p_filesz);
    }

    uint8_t page_flags = PAGE_USER;
    if (phdr->p_flags & PF_R) {
      page_flags |= PAGE_READ;
    }
    if (phdr->p_flags & PF_W) {
      page_flags |= PAGE_WRITE | PAGE_READ;
    }
    if (phdr->p_flags & PF_X) {
      page_flags |= PAGE_EXEC;
    }

    if (!(page_flags & (PAGE_READ | PAGE_EXEC))) {
      pmm_free(phys_addr, frame_count);
      console_log(_CONSOLE_LOG_SUBJECT, "Unsupported segment flags: 0x%x.\n",
                  (unsigned)phdr->p_flags);
      status = ELF_LOAD_ERR_UNSUPPORTED;
      goto fail;
    }

    if (vmm_map(&proc->memory_space, seg_start, phys_addr, seg_size,
                page_flags) != VMM_OK) {
      pmm_free(phys_addr, frame_count);
      console_log(_CONSOLE_LOG_SUBJECT, "Failed to map segment at 0x%zx.\n",
                  (size_t)seg_start);
      status = ELF_LOAD_ERR_MAP_FAILED;
      goto fail;
    }

    segments[loaded_count].virt_start = seg_start;
    segments[loaded_count].phys_start = phys_addr;
    segments[loaded_count].frame_count = frame_count;
    loaded_count++;
  }

  if (loaded_count == 0) {
    console_log(_CONSOLE_LOG_SUBJECT, "No loadable segments found.\n");
    return ELF_LOAD_ERR_CORRUPT;
  }

  out_result->entry_point = ehdr->e_entry;
  return ELF_LOAD_OK;

fail:
  for (size_t j = 0; j < loaded_count; j++) {
    vmm_unmap(&proc->memory_space, segments[j].virt_start,
              segments[j].frame_count * PAGE_SIZE);
    pmm_free(segments[j].phys_start, segments[j].frame_count);
  }
  return status;
}
