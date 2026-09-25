#include <syscall/syscall.h>

#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <asm/paging_info.h>
#include <asm/trap_layout.h>
#include <common/memory.h>
#include <common/syscall.h>
#include <core/console.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <proc/capabilities.h>
#include <proc/process.h>
#include <proc/task.h>
#include <sched/sched.h>
#include <stddef.h>
#include <stdint.h>

#define _CONSOLE_LOG_SUBJECT "syscall"

void syscall_handler(size_t syscall_num,
                     const trap_register_t args[TRAP_ARG_REGISTER_COUNT],
                     trap_register_t* ret0, trap_register_t* ret1) {
  if (ret0 != NULL) {
    *ret0 = (trap_register_t)SYS_OK;
  }
  if (ret1 != NULL) {
    *ret1 = 0;
  }

  struct task* current = sched_current_task();
  struct process* caller_proc = (current != NULL) ? current->process : NULL;
  struct capability_table* cnode =
      (caller_proc != NULL) ? &caller_proc->capabilities : NULL;

  switch (syscall_num) {
    case SYSCALL_YIELD:
      {
        sched_preempt();
        break;
      }

    case SYSCALL_EXIT:
      {
        console_log(_CONSOLE_LOG_SUBJECT, "Process exited with status %d.\n",
                    (int)args[0]);
        sched_exit((int)args[0]);
        break;
      }

    case SYSCALL_FRAME_ALLOC:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        size_t count = (size_t)args[0];
        cap_ptr_t out_slot = (cap_ptr_t)args[1];

        if (count == 0) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        uintptr_t phys = 0;
        if (pmm_alloc(count, &phys) != PMM_OK) {
          *ret0 = (trap_register_t)SYS_ERR_NO_MEM;
          break;
        }

        // Zero physical memory frames
        void* kvirt = (void*)arch_phys_to_virt(phys);
        memory_set(kvirt, 0, count * PMM_FRAME_SIZE);

        struct capability cap = {
            .type = CAP_TYPE_FRAME,
            .rights = CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
            .badge = count,
            .object = (void*)phys,
        };

        cap_ptr_t assigned_slot = 0;
        int ins_res = cap_table_insert(cnode, out_slot, &cap, &assigned_slot);
        if (ins_res != SYS_OK) {
          pmm_free(phys, count);
          *ret0 = (trap_register_t)ins_res;
          break;
        }

        *ret0 = (trap_register_t)SYS_OK;
        if (ret1 != NULL) {
          *ret1 = (trap_register_t)assigned_slot;
        }
        break;
      }

    case SYSCALL_FRAME_FREE:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t frame_slot = (cap_ptr_t)args[0];
        struct capability cap_frame;
        int lookup_res = cap_table_lookup(cnode, frame_slot, CAP_TYPE_FRAME,
                                          CAP_RIGHT_WRITE, &cap_frame);
        if (lookup_res != SYS_OK) {
          *ret0 = (trap_register_t)lookup_res;
          break;
        }

        phys_addr_t phys = (phys_addr_t)(uintptr_t)cap_frame.object;
        size_t count = (size_t)cap_frame.badge;

        cap_table_delete(cnode, frame_slot);
        pmm_free(phys, count);

        *ret0 = (trap_register_t)SYS_OK;
        break;
      }

    case SYSCALL_VSPACE_CREATE:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t out_slot = (cap_ptr_t)args[0];
        struct process* child_proc = process_create();
        if (child_proc == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_NO_MEM;
          break;
        }

        struct capability cap_vspace = {
            .type = CAP_TYPE_VSPACE,
            .rights = CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
            .badge = child_proc->pid,
            .object = child_proc,
        };

        cap_ptr_t assigned_slot = 0;
        int ins_res =
            cap_table_insert(cnode, out_slot, &cap_vspace, &assigned_slot);
        if (ins_res != SYS_OK) {
          process_destroy(child_proc);
          *ret0 = (trap_register_t)ins_res;
          break;
        }

        *ret0 = (trap_register_t)SYS_OK;
        if (ret1 != NULL) {
          *ret1 = (trap_register_t)assigned_slot;
        }
        break;
      }

    case SYSCALL_VSPACE_DESTROY:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t vspace_slot = (cap_ptr_t)args[0];
        struct capability cap_vspace;
        int lookup_res = cap_table_lookup(cnode, vspace_slot, CAP_TYPE_VSPACE,
                                          CAP_RIGHT_WRITE, &cap_vspace);
        if (lookup_res != SYS_OK) {
          *ret0 = (trap_register_t)lookup_res;
          break;
        }

        struct process* target_proc = (struct process*)cap_vspace.object;
        if (target_proc == caller_proc) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_table_delete(cnode, vspace_slot);
        process_destroy(target_proc);

        *ret0 = (trap_register_t)SYS_OK;
        break;
      }

    case SYSCALL_VSPACE_MAP:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t vspace_slot = (cap_ptr_t)args[0];
        cap_ptr_t frame_slot = (cap_ptr_t)args[1];
        virt_addr_t vaddr = (virt_addr_t)args[2];
        uint32_t perms = (uint32_t)args[3];

        struct capability cap_vspace;
        int vspace_res = cap_table_lookup(cnode, vspace_slot, CAP_TYPE_VSPACE,
                                          CAP_RIGHT_WRITE, &cap_vspace);
        if (vspace_res != SYS_OK) {
          *ret0 = (trap_register_t)vspace_res;
          break;
        }
        struct process* target_proc = (struct process*)cap_vspace.object;

        struct capability cap_frame;
        int frame_res = cap_table_lookup(cnode, frame_slot, CAP_TYPE_FRAME,
                                         CAP_RIGHT_READ, &cap_frame);
        if (frame_res != SYS_OK) {
          *ret0 = (trap_register_t)frame_res;
          break;
        }

        phys_addr_t phys = (phys_addr_t)(uintptr_t)cap_frame.object;
        size_t frame_count = (size_t)cap_frame.badge;
        size_t map_size = frame_count * PMM_FRAME_SIZE;

        if ((vaddr & PAGE_MASK) != 0 || vaddr < PAGE_SIZE ||
            vaddr >= USER_STACK_TOP || map_size > USER_STACK_TOP - vaddr) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        uint8_t flags = PAGE_USER;
        if (perms & PAGE_READ) {
          flags |= PAGE_READ;
        }
        if (perms & PAGE_WRITE) {
          if (!(cap_frame.rights & CAP_RIGHT_WRITE)) {
            *ret0 = (trap_register_t)SYS_ERR_RIGHTS;
            break;
          }
          flags |= PAGE_WRITE | PAGE_READ;
        }
        if (perms & PAGE_EXEC) {
          flags |= PAGE_EXEC;
        }

        if (!(flags & (PAGE_READ | PAGE_EXEC))) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        int map_res =
            vmm_map(&target_proc->memory_space, vaddr, phys, map_size, flags);
        if (map_res != VMM_OK) {
          *ret0 = (trap_register_t)SYS_ERR_ALREADY_MAPPED;
          break;
        }

        *ret0 = (trap_register_t)SYS_OK;
        break;
      }

    case SYSCALL_VSPACE_UNMAP:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t vspace_slot = (cap_ptr_t)args[0];
        virt_addr_t vaddr = (virt_addr_t)args[1];
        size_t size = (size_t)args[2];

        if ((vaddr & PAGE_MASK) != 0 || (size & PAGE_MASK) != 0 || size == 0 ||
            vaddr < PAGE_SIZE || vaddr >= USER_STACK_TOP ||
            size > USER_STACK_TOP - vaddr) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        struct capability cap_vspace;
        int vspace_res = cap_table_lookup(cnode, vspace_slot, CAP_TYPE_VSPACE,
                                          CAP_RIGHT_WRITE, &cap_vspace);
        if (vspace_res != SYS_OK) {
          *ret0 = (trap_register_t)vspace_res;
          break;
        }
        struct process* target_proc = (struct process*)cap_vspace.object;

        int unmap_res = vmm_unmap(&target_proc->memory_space, vaddr, size);
        *ret0 = (unmap_res == VMM_OK) ? (trap_register_t)SYS_OK
                                      : (trap_register_t)SYS_ERR_INVALID_ARG;
        break;
      }

    case SYSCALL_TASK_CREATE:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t vspace_slot = (cap_ptr_t)args[0];
        uintptr_t entry_point = (uintptr_t)args[1];
        uintptr_t arg0 = (uintptr_t)args[2];
        cap_ptr_t out_slot = (cap_ptr_t)args[3];

        if (entry_point == 0 || entry_point >= USER_STACK_TOP) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        struct capability cap_vspace;
        int vspace_res = cap_table_lookup(cnode, vspace_slot, CAP_TYPE_VSPACE,
                                          CAP_RIGHT_WRITE, &cap_vspace);
        if (vspace_res != SYS_OK) {
          *ret0 = (trap_register_t)vspace_res;
          break;
        }
        struct process* target_proc = (struct process*)cap_vspace.object;

        struct task* task = task_create(target_proc, entry_point, arg0);
        if (task == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_NO_MEM;
          break;
        }

        struct capability cap_task = {
            .type = CAP_TYPE_TASK,
            .rights = CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_CALL |
                      CAP_RIGHT_GRANT,
            .badge = task->tid,
            .object = task,
        };

        cap_ptr_t assigned_slot = 0;
        int ins_res =
            cap_table_insert(cnode, out_slot, &cap_task, &assigned_slot);
        if (ins_res != SYS_OK) {
          task_destroy(task);
          *ret0 = (trap_register_t)ins_res;
          break;
        }

        *ret0 = (trap_register_t)SYS_OK;
        if (ret1 != NULL) {
          *ret1 = (trap_register_t)assigned_slot;
        }
        break;
      }

    case SYSCALL_TASK_CONFIGURE:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t task_slot = (cap_ptr_t)args[0];
        uintptr_t pc = (uintptr_t)args[1];
        uintptr_t sp = (uintptr_t)args[2];
        uintptr_t a0 = (uintptr_t)args[3];

        struct capability cap_task;
        int lookup_res = cap_table_lookup(cnode, task_slot, CAP_TYPE_TASK,
                                          CAP_RIGHT_WRITE, &cap_task);
        if (lookup_res != SYS_OK) {
          *ret0 = (trap_register_t)lookup_res;
          break;
        }
        struct task* target_task = (struct task*)cap_task.object;

        if (target_task->trap_frame == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        if (pc != 0) {
          if (pc >= USER_STACK_TOP) {
            *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
            break;
          }
          target_task->trap_frame->sepc = pc;
        }

        if (sp != 0) {
          if (sp >= USER_STACK_TOP) {
            *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
            break;
          }
          target_task->trap_frame->sp = sp;
        }

        target_task->trap_frame->a0 = a0;
        *ret0 = (trap_register_t)SYS_OK;
        break;
      }

    case SYSCALL_TASK_START:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t task_slot = (cap_ptr_t)args[0];
        uint8_t priority = (uint8_t)args[1];

        struct capability cap_task;
        int lookup_res = cap_table_lookup(cnode, task_slot, CAP_TYPE_TASK,
                                          CAP_RIGHT_WRITE, &cap_task);
        if (lookup_res != SYS_OK) {
          *ret0 = (trap_register_t)lookup_res;
          break;
        }
        struct task* target_task = (struct task*)cap_task.object;

        if (priority > 0) {
          target_task->priority = priority;
        }

        sched_schedule(target_task);
        *ret0 = (trap_register_t)SYS_OK;
        break;
      }

    case SYSCALL_TASK_SUSPEND:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t task_slot = (cap_ptr_t)args[0];
        struct capability cap_task;
        int lookup_res = cap_table_lookup(cnode, task_slot, CAP_TYPE_TASK,
                                          CAP_RIGHT_WRITE, &cap_task);
        if (lookup_res != SYS_OK) {
          *ret0 = (trap_register_t)lookup_res;
          break;
        }
        struct task* target_task = (struct task*)cap_task.object;

        if (target_task == current) {
          sched_preempt();
        } else {
          target_task->state = TASK_STATE_DEAD;
        }

        *ret0 = (trap_register_t)SYS_OK;
        break;
      }

    case SYSCALL_CNODE_CREATE:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t vspace_slot = (cap_ptr_t)args[0];
        cap_ptr_t out_slot = (cap_ptr_t)args[1];

        struct capability cap_vspace;
        int lookup_res = cap_table_lookup(cnode, vspace_slot, CAP_TYPE_VSPACE,
                                          CAP_RIGHT_READ, &cap_vspace);
        if (lookup_res != SYS_OK) {
          *ret0 = (trap_register_t)lookup_res;
          break;
        }
        struct process* target_proc = (struct process*)cap_vspace.object;

        struct capability cap_cnode = {
            .type = CAP_TYPE_CNODE,
            .rights = CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
            .badge = target_proc->pid,
            .object = &target_proc->capabilities,
        };

        cap_ptr_t assigned_slot = 0;
        int ins_res =
            cap_table_insert(cnode, out_slot, &cap_cnode, &assigned_slot);
        *ret0 = (trap_register_t)ins_res;
        if (ret1 != NULL) {
          *ret1 = (trap_register_t)assigned_slot;
        }
        break;
      }

    case SYSCALL_CAP_COPY:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t src_slot = (cap_ptr_t)args[0];
        cap_ptr_t dest_cnode_slot = (cap_ptr_t)args[1];
        cap_ptr_t dest_slot = (cap_ptr_t)args[2];
        uint8_t rights_mask = (uint8_t)args[3];
        uint64_t badge = (uint64_t)args[4];

        struct capability_table* dest_table = cnode;
        if (dest_cnode_slot != 0 && dest_cnode_slot != CAP_SLOT_CNODE) {
          struct capability cap_cnode;
          int cnode_res = cap_table_lookup(cnode, dest_cnode_slot,
                                           CAP_TYPE_CNODE, CAP_RIGHT_WRITE,
                                           &cap_cnode);
          if (cnode_res != SYS_OK) {
            *ret0 = (trap_register_t)cnode_res;
            break;
          }
          dest_table = (struct capability_table*)cap_cnode.object;
        }

        cap_ptr_t assigned_slot = 0;
        int copy_res =
            cap_table_copy(cnode, src_slot, dest_table, dest_slot, rights_mask,
                           badge, &assigned_slot);
        *ret0 = (trap_register_t)copy_res;
        if (ret1 != NULL) {
          *ret1 = (trap_register_t)assigned_slot;
        }
        break;
      }

    case SYSCALL_CAP_DELETE:
      {
        if (cnode == NULL) {
          *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
          break;
        }

        cap_ptr_t slot = (cap_ptr_t)args[0];
        int del_res = cap_table_delete(cnode, slot);
        *ret0 = (trap_register_t)del_res;
        break;
      }

    default:
      {
        console_log(_CONSOLE_LOG_SUBJECT, "Unexpected syscall number %zu.\n",
                    syscall_num);
        *ret0 = (trap_register_t)SYS_ERR_INVALID_ARG;
        break;
      }
  }
}
