/*
 * Licensed under MIT License - URIX project.
 * process.c - Process Management implementation.
 */

#include <process/process.h>
#include <process/fd_table.h>
#include <process/pipe.h>
#include <memory/vmm.h>
#include <memory/kmalloc.h>
#include <memory/physical/pmm.h>
#include <lib/print.h>
#include <lib/panic.h>
#include <string.h>
#include <fs/elf.h>
#include <cpu/gdt.h>

/* GDT segment selectors */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x1B
#define USER_DS 0x23

#ifndef USER_STACK_TOP
#define USER_STACK_TOP 0x0000800000000000ULL
#endif

typedef struct ready_queue
{
    process_t *head;
    process_t *tail;
    uint32_t count;
} ready_queue_t;

static ready_queue_t ready_queues[5];
static process_t *all_processes = NULL;
static process_t *current_process = NULL;
static uint32_t next_pid = 0;
static uint32_t total_processes = 0;
static uint64_t context_switches = 0;

static void enqueue(process_t *proc)
{
    if (!proc)
        return;

    ready_queue_t *q = &ready_queues[proc->effective_priority];
    proc->next_in_queue = NULL;

    if (!q->head)
    {
        q->head = q->tail = proc;
    }
    else
    {
        q->tail->next_in_queue = proc;
        q->tail = proc;
    }
    q->count++;
}

static process_t *dequeue_highest_priority(void)
{
    for (int p = PRIORITY_REALTIME; p >= PRIORITY_IDLE; p--)
    {
        ready_queue_t *q = &ready_queues[p];
        if (q->head)
        {
            process_t *proc = q->head;
            q->head = proc->next_in_queue;
            if (!q->head)
                q->tail = NULL;
            q->count--;
            proc->next_in_queue = NULL;
            return proc;
        }
    }
    return NULL;
}

static void remove_from_ready_queue(process_t *p)
{
    ready_queue_t *q = &ready_queues[p->effective_priority];
    process_t *prev = NULL;
    process_t *curr = q->head;

    while (curr)
    {
        if (curr == p)
        {
            if (prev)
                prev->next_in_queue = curr->next_in_queue;
            else
                q->head = curr->next_in_queue;

            if (q->tail == curr)
                q->tail = prev;

            q->count--;
            curr->next_in_queue = NULL;
            break;
        }
        prev = curr;
        curr = curr->next_in_queue;
    }
}

static void remove_from_all_processes(process_t *p)
{
    if (all_processes == p)
    {
        all_processes = p->next_all;
    }
    else
    {
        process_t *prev = all_processes;
        while (prev && prev->next_all != p)
        {
            prev = prev->next_all;
        }
        if (prev)
        {
            prev->next_all = p->next_all;
        }
    }
    total_processes--;
}

static void register_process(process_t *proc)
{
    enqueue(proc);
    proc->next_all = all_processes;
    all_processes = proc;
    total_processes++;
}

static void cleanup_process_resources(process_t *p)
{
    if (p->fd_table)
        fd_table_destroy(p->fd_table);
    if (p->addr_space)
        vmm_destroy_address_space(p->addr_space);
    if (p->kernel_stack_phys)
        pmm_free_frame(p->kernel_stack_phys);
    kfree(p);
}

static process_t *alloc_process(const char *name, process_priority_t priority, process_privilege_t privilege)
{
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    if (!proc)
        return NULL;

    memset(proc, 0, sizeof(process_t));

    process_t *current = process_get_current();
    if (current)
        strncpy(proc->cwd, current->cwd, sizeof(proc->cwd));
    else
        strcpy(proc->cwd, "/");

    /* Setup identity */
    proc->pid = next_pid++;
    proc->parent_pid = current ? current->pid : 0;
    proc->exit_status = 0;
    proc->is_zombie = 0;
    strncpy(proc->name, name, 31);
    proc->priority = priority;
    proc->effective_priority = priority;
    proc->privilege = privilege;
    proc->state = PROCESS_STATE_READY;
    proc->time_slice_remaining = TIME_SLICE;

    /* Create address space */
    proc->addr_space = vmm_create_address_space();
    if (!proc->addr_space)
    {
        kfree(proc);
        return NULL;
    }

    /* Allocate kernel stack */
    proc->kernel_stack_phys = pmm_alloc_frame();
    if (!proc->kernel_stack_phys)
    {
        vmm_destroy_address_space(proc->addr_space);
        kfree(proc);
        return NULL;
    }

    /* Map kernel stack */
    proc->kernel_stack_virt = KERNEL_VIRT_BASE + proc->kernel_stack_phys;
    vmm_map_page(proc->addr_space, proc->kernel_stack_virt,
                 proc->kernel_stack_phys, VMM_KERNEL_FLAGS);

    proc->fd_table = fd_table_create();
    if (!proc->fd_table)
    {
        vmm_destroy_address_space(proc->addr_space);
        pmm_free_frame(proc->kernel_stack_phys);
        kfree(proc);
        return NULL;
    }

    return proc;
}

static void idle_process(void)
{
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

void process_init(void)
{
    kprintf("\n=== Process Manager Init ===\n");

    memset(ready_queues, 0, sizeof(ready_queues));
    all_processes = NULL;
    current_process = NULL;
    next_pid = 0;
    total_processes = 0;
    context_switches = 0;

    int idle = process_create((uint64_t)idle_process, "idle",
                              PRIORITY_IDLE, PROCESS_KERNEL);
    if (idle < 0)
    {
        PANIC("Failed to create idle process");
    }

    kprintf("Idle process created (PID %d)\n", idle);
    kprintf("================================\n\n");
}

int process_create(uint64_t entry_point, const char *name, process_priority_t priority, process_privilege_t privilege)
{
    if (!entry_point || priority > PRIORITY_REALTIME)
    {
        return -1;
    }

    process_t *proc = alloc_process(name, priority, privilege);
    if (!proc)
    {
        debug_kprintf("Failed to allocate PCB for '%s'\n", name);
        return -1;
    }

    /* Setup context */
    memset(&proc->context, 0, sizeof(cpu_context_t));
    proc->context.rip = entry_point;
    proc->context.rflags = 0x202;

    if (privilege == PROCESS_USER)
    {
        proc->context.cs = USER_CS;
        proc->context.ss = USER_DS;
        proc->user_stack = USER_STACK_TOP;
        proc->context.rsp = proc->user_stack - 16;

        uint64_t user_stack_phys = pmm_alloc_frame();
        if (user_stack_phys)
        {
            vmm_map_page(proc->addr_space, proc->user_stack - PAGE_SIZE,
                         user_stack_phys, VMM_USER_FLAGS);
        }
    }
    else
    {
        proc->context.cs = KERNEL_CS;
        proc->context.ss = KERNEL_DS;
        uint64_t rsp = proc->kernel_stack_virt + PAGE_SIZE;
        rsp &= ~0xFULL; /* 16-byte alignment */
        rsp -= 8;       /* fake return address */
        proc->context.rsp = rsp;
        proc->context.rbp = rsp;
    }

    proc->context.rbp = proc->context.rsp;

    /* Register process */
    register_process(proc);

    debug_kprintf("Created process '%s' (PID %u, Pri %d, Ring %d)\n", proc->name, proc->pid, proc->priority, proc->privilege);

    return proc->pid;
}

static int process_alloc_user_stack(address_space_t *addr_space)
{
    uint64_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;

    for (uint64_t i = 0; i < stack_pages; i++)
    {
        uint64_t vaddr = USER_STACK_BOTTOM + (i * PAGE_SIZE);
        uint64_t phys = pmm_alloc_frame();

        if (phys == 0)
        {
            debug_kprintf("[ERROR] process_alloc_user_stack: out of memory at page %lu\n", i);
            return -1;
        }

        if (vmm_map_page(addr_space, vaddr, phys, VMM_USER | VMM_WRITE | VMM_PRESENT) != 0)
        {
            debug_kprintf("[ERROR] process_alloc_user_stack: vmm_map_page failed at 0x%lx\n", vaddr);
            pmm_free_frame(phys);
            return -1;
        }
    }

    return 0;
}

int process_load_elf(const char *name, const void *elf_data,
                     size_t elf_size, process_priority_t priority)
{
    if (!elf_data || !elf_size || priority > PRIORITY_REALTIME)
    {
        debug_kprintf("[ERROR] process_load_elf: Invalid parameters\n");
        return -1;
    }

    debug_kprintf("[DEBUG] process_load_elf: Loading '%s' (%lu bytes)\n",
                  name, elf_size);

    process_t *proc = alloc_process(name, priority, PROCESS_USER);
    if (!proc)
    {
        kprintf("[ERROR] Failed to allocate PCB for '%s'\n", name);
        return -1;
    }

    /* Load ELF into the new address space */
    elf_info_t elf_info;
    if (elf_load(elf_data, elf_size, proc->addr_space, &elf_info) != 0)
    {
        kprintf("[ERROR] elf_load failed for '%s'\n", name);
        fd_table_destroy(proc->fd_table);
        vmm_destroy_address_space(proc->addr_space);
        pmm_free_frame(proc->kernel_stack_phys);
        kfree(proc);
        return -1;
    }

    /* Allocate user stack */
    if (process_alloc_user_stack(proc->addr_space) != 0)
    {
        kprintf("[ERROR] Failed to allocate user stack for '%s'\n", name);
        fd_table_destroy(proc->fd_table);
        vmm_destroy_address_space(proc->addr_space);
        pmm_free_frame(proc->kernel_stack_phys);
        kfree(proc);
        return -1;
    }

    /* Initial CPU context */
    memset(&proc->context, 0, sizeof(cpu_context_t));
    uint64_t null_slot_virt = USER_STACK_TOP - 8;
    uint64_t argc_slot_virt = USER_STACK_TOP - 16;

    uint64_t null_phys = vmm_get_physical(proc->addr_space, null_slot_virt);
    uint64_t argc_phys = vmm_get_physical(proc->addr_space, argc_slot_virt);

    if (null_phys && argc_phys)
    {
        uint64_t null_off = null_slot_virt & (PAGE_SIZE - 1);
        uint64_t argc_off = argc_slot_virt & (PAGE_SIZE - 1);
        *(uint64_t *)((uint8_t *)phys_to_virt(null_phys) + null_off) = 0ULL;
        *(uint64_t *)((uint8_t *)phys_to_virt(argc_phys) + argc_off) = 0ULL;
    }
    else
    {
        kprintf("[WARN] process_load_elf: could not map stack top for argv\n");
    }

    proc->context.rip = elf_info.entry_point;
    proc->context.rsp = USER_STACK_TOP - 16; /* points at argc slot */
    proc->context.rbp = USER_STACK_TOP - 16;
    proc->context.cs = USER_CS;
    proc->context.ss = USER_DS;
    proc->context.rflags = 0x202;
    proc->context.rdi = 0;
    proc->context.rsi = 0;
    proc->user_stack = USER_STACK_TOP;

    /* Register with scheduler and process list */
    register_process(proc);

    kprintf("[SUCCESS] Loaded '%s' (PID %u, entry=0x%lx)\n",
            proc->name, proc->pid, elf_info.entry_point);

    return proc->pid;
}

int process_fork(void)
{
    process_t *parent = process_get_current();
    if (!parent)
    {
        debug_kprintf("no parent process to fork!\n");
        return -1;
    }

    extern uint64_t syscall_frame_rsp;
    uint64_t trap_frame_ptr = syscall_frame_rsp;
    uint64_t *regs = (uint64_t *)(trap_frame_ptr - 120);

    debug_kprintf("cloning process. name: %s, pid: %d\n", parent->name, parent->pid);

    /* create a new process info table */
    process_t *child = (process_t *)kmalloc(sizeof(process_t));
    if (!child)
    {
        debug_kprintf("failed to allocate child process table\n");
        return -1;
    }
    memcpy(child, parent, sizeof(process_t));
    strncpy(child->cwd, parent->cwd, sizeof(child->cwd));
    child->pid = next_pid++;
    child->parent_pid = parent->pid;
    child->exit_status = 0;
    child->is_zombie = 0;

    char name_buf[32];
    strncpy(name_buf, parent->name, 32);
    strncpy(child->name, name_buf, 32);

    /* reset process state */
    child->state = PROCESS_STATE_READY;
    child->time_slice_remaining = TIME_SLICE;
    child->total_runtime = 0;
    child->wait_time = 0;
    child->next_in_queue = NULL;
    child->next_all = NULL;

    child->addr_space = vmm_create_address_space();
    if (!child->addr_space)
    {
        debug_kprintf("failed to create address space for child PID %d\n", child->pid);
        kfree(child);
        return -1;
    }
    if (vmm_clone_user_space(parent->addr_space, child->addr_space) != 0)
    {
        debug_kprintf("fork: failed to clone user address space from parent PID %d\n", parent->pid);
        vmm_destroy_address_space(child->addr_space);
        kfree(child);
        return -1;
    }

    /* allocate kernel stack */
    child->kernel_stack_phys = pmm_alloc_frame();
    if (!child->kernel_stack_phys)
    {
        debug_kprintf("fork: failed to allocate kernel stack for child PID %d\n", child->pid);
        vmm_destroy_address_space(child->addr_space);
        kfree(child);
        return -1;
    }

    child->kernel_stack_virt = (uint64_t)phys_to_virt(child->kernel_stack_phys);
    vmm_map_page(vmm_get_kernel_space(), child->kernel_stack_virt, child->kernel_stack_phys, VMM_KERNEL_FLAGS);

    extern uint64_t syscall_saved_user_rip;
    extern uint64_t syscall_saved_user_rsp;
    extern uint64_t syscall_saved_user_rflags;
    child->context.rip = syscall_saved_user_rip;
    child->context.rsp = syscall_saved_user_rsp;
    child->context.rbp = parent->context.rbp;
    child->context.rflags = syscall_saved_user_rflags | 0x200;
    child->context.cs = 0x1B;
    child->context.ss = 0x23;
    child->context.rax = 0; // child gets 0 from fork
    child->context.rbx = regs[1];
    child->context.rcx = regs[2];
    child->context.rdx = regs[3];
    child->context.rsi = regs[4];
    child->context.rdi = regs[5];
    child->context.rbp = regs[6];
    child->context.r8 = regs[7];
    child->context.r9 = regs[8];
    child->context.r10 = regs[9];
    child->context.r11 = regs[10];
    child->context.r12 = regs[11];
    child->context.r13 = regs[12];
    child->context.r14 = regs[13];
    child->context.r15 = regs[14];

    /* copy the file descriptor table */
    if (parent->fd_table)
    {
        child->fd_table = fd_table_create();
        if (!child->fd_table)
        {
            debug_kprintf("fork: failed to create FD table for child PID %d\n", child->pid);
            pmm_free_frame(child->kernel_stack_phys);
            vmm_destroy_address_space(child->addr_space);
            kfree(child);
            return -1;
        }

        for (int i = 0; i < MAX_FDS; i++)
        {
            /* copy the entry struct first */
            child->fd_table->fds[i] = parent->fd_table->fds[i];
            fd_entry_t *child_fd = &child->fd_table->fds[i];

            switch (child_fd->type)
            {
            case FD_TYPE_FILE:
                if (child_fd->vfs_file)
                    vfs_retain_file(child_fd->vfs_file);
                break;

            case FD_TYPE_PIPE:
                if (child_fd->pipe)
                    pipe_retain((pipe_t *)child_fd->pipe, (child_fd->flags & PIPE_WRITE) ? 1 : 0);
                break;

            case FD_TYPE_CONSOLE:
                break;

            default:
                break;
            }
        }
    }

    /* add to process list and queue */
    child->parent_pid = parent->pid;
    register_process(child);

    debug_kprintf("fork: successfully created child PID %u from parent PID %u\n",
                  child->pid, parent->pid);

    /* return child's PID to parent */
    return child->pid;
}

void process_exit(int exit_code)
{
    process_t *proc = process_get_current();
    if (!proc)
    {
        PANIC("process_exit: no current process");
    }

    debug_kprintf("Process '%s' (PID %u) exiting with code %d\n", proc->name, proc->pid, exit_code);

    /* Store exit status */
    proc->exit_status = exit_code;

    /* Check if we have a living parent */
    process_t *parent = NULL;
    if (proc->parent_pid != 0)
    {
        parent = process_get(proc->parent_pid);
    }

    if (parent)
    {
        debug_kprintf("Process %u becoming zombie (parent %u)\n", proc->pid, proc->parent_pid);

        proc->is_zombie = 1;
        proc->state = PROCESS_STATE_TERMINATED;

        /* Wake up parent if it's blocked */
        if (parent->state == PROCESS_STATE_BLOCKED)
        {
            debug_kprintf("Waking up parent %u from wait()\n", proc->parent_pid);
            parent->state = PROCESS_STATE_READY;
            enqueue(parent);
        }
    }
    else
    {
        debug_kprintf("Process %u has no parent, will be cleaned by scheduler\n", proc->pid);
        proc->is_zombie = 0;
        proc->state = PROCESS_STATE_TERMINATED;
    }

    process_schedule();

    PANIC("Returned from schedule in process_exit");
    while (1);
}

int process_wait(int *status)
{
    process_t *parent = process_get_current();
    if (!parent)
        return -1;

    debug_kprintf("wait: PID %u waiting for children\n", parent->pid);

    /* Loop until a zombie child is found/find that a process doesn't have children */
    while (1)
    {
        int has_children = 0;
        process_t *zombie_child = NULL;

        /* Scan all processes for the child */
        process_t *p = all_processes;
        while (p)
        {
            if (p->parent_pid == parent->pid)
            {
                has_children = 1;

                /* Found a zombie child */
                if (p->is_zombie && p->state == PROCESS_STATE_TERMINATED)
                {
                    zombie_child = p;
                    break;
                }
            }
            p = p->next_all;
        }

        /* Found a zombie child, clean up */
        if (zombie_child)
        {
            debug_kprintf("wait: reaping zombie child PID %u (status %d)\n", zombie_child->pid, zombie_child->exit_status);

            uint32_t child_pid = zombie_child->pid;
            int child_status = zombie_child->exit_status;

            remove_from_all_processes(zombie_child);
            cleanup_process_resources(zombie_child);

            /* Return status to parent */
            if (status)
                *status = child_status;

            return child_pid;
        }

        /* No children at all */
        if (!has_children)
        {
            debug_kprintf("wait: PID %u has no children\n", parent->pid);
            return -1;
        }

        debug_kprintf("wait: PID %u blocking (children still running)\n", parent->pid);

        parent->state = PROCESS_STATE_BLOCKED;
        process_yield();
    }
}

void process_yield(void)
{
    if (!current_process || current_process->state == PROCESS_STATE_TERMINATED)
        return;

    /* Put current back in queue */
    current_process->state = PROCESS_STATE_READY;
    current_process->time_slice_remaining = TIME_SLICE;
    enqueue(current_process);

    process_schedule();
}

void process_schedule(void)
{
    process_t *old = current_process;

    if (old && old->state == PROCESS_STATE_TERMINATED)
    {
        /* Check if this process is a zombie or not */
        if (old->is_zombie)
        {
            debug_kprintf("Process %u is zombie (parent %u will reap it)\n", old->pid, old->parent_pid);
        }
        else
        {
            debug_kprintf("Cleaning up terminated process '%s' (PID %u)\n", old->name, old->pid);

            remove_from_all_processes(old);

            process_t *new = dequeue_highest_priority();
            if (!new)
            {
                PANIC("No runnable process after termination!");
            }

            new->state = PROCESS_STATE_RUNNING;
            new->time_slice_remaining = TIME_SLICE;
            new->wait_time = 0;
            new->effective_priority = new->priority;

            current_process = new;
            context_switches++;

            vmm_switch_address_space(new->addr_space);
            gdt_set_kernel_stack(new->kernel_stack_virt + PAGE_SIZE);

            cleanup_process_resources(old);

            process_context_switch(NULL, &new->context);

            PANIC("Returned from context switch after cleanup");
        }
    }

    process_t *new = dequeue_highest_priority();

    if (!new)
    {
        if (old && old->state == PROCESS_STATE_RUNNING)
        {
            return;
        }

        if (old && old->state == PROCESS_STATE_BLOCKED)
        {
            debug_kprintf("WARNING: All processes blocked or terminated!\n");

            process_t *p = all_processes;
            while (p)
            {
                if (p->state == PROCESS_STATE_READY)
                {
                    new = p;
                    break;
                }
                if (p->state == PROCESS_STATE_BLOCKED)
                {
                    debug_kprintf("Process %u (%s) is blocked\n", p->pid, p->name);
                }
                p = p->next_all;
            }

            if (!new)
                PANIC("All processes blocked or terminated");
        }
        else
        {
            PANIC("No runnable process!");
        }
    }

    /* Continues if the same process was chosen again */
    if (new == old && old && old->state == PROCESS_STATE_RUNNING)
    {
        return;
    }

    /* Update states */
    if (old && old->state == PROCESS_STATE_RUNNING)
    {
        old->state = PROCESS_STATE_READY;
        enqueue(old);
    }

    /* Update new process state */
    new->state = PROCESS_STATE_RUNNING;
    new->time_slice_remaining = TIME_SLICE;
    new->wait_time = 0;
    new->effective_priority = new->priority;

    current_process = new;
    context_switches++;

    /* Switch address space */
    vmm_switch_address_space(new->addr_space);
    gdt_set_kernel_stack(new->kernel_stack_virt + PAGE_SIZE);
    /* Context switch */
    if (old && old->state != PROCESS_STATE_TERMINATED && !old->is_zombie)
    {
        process_context_switch(&old->context, &new->context);
    }
    else
    {
        process_context_switch(NULL, &new->context);
    }
}

process_t *process_get_current(void)
{
    return current_process;
}

process_t *process_get(uint32_t pid)
{
    process_t *p = all_processes;
    while (p)
    {
        if (p->pid == pid)
            return p;
        p = p->next_all;
    }
    return NULL;
}

int process_kill(uint32_t pid)
{
    // Never kill idle
    if (pid == 0)
        return -1;

    process_t *p = process_get(pid);
    if (!p)
        return -1;

    // Cannot kill currently running process - it must exit on its own
    if (p == current_process)
    {
        return -1;
    }

    __asm__ volatile("cli");

    // Remove from ready queue if present
    remove_from_ready_queue(p);

    /* Remove from all_processes list */
    remove_from_all_processes(p);
    __asm__ volatile("sti");

    /* Cleanup resources */
    cleanup_process_resources(p);

    return 0;
}

void process_timer_tick(void)
{
    if (!current_process)
        return;

    current_process->total_runtime++;

    process_t *p = all_processes;
    while (p)
    {
        if (p->state == PROCESS_STATE_READY && p != current_process)
        {
            p->wait_time++;

            /* Boost priority if waiting too long */
            if (p->wait_time >= PRIORITY_BOOST_THRESHOLD &&
                p->effective_priority < PRIORITY_REALTIME && p->pid != 0)
            {

                debug_kprintf("Priority boost: '%s' (PID %u) %d -> %d (waited %llu ticks)\n",
                              p->name, p->pid, p->effective_priority,
                              p->effective_priority + 1, p->wait_time);

                /* Remove from current queue */
                remove_from_ready_queue(p);

                /* Boost priority and re-enqueue */
                p->effective_priority++;
                p->wait_time = 0;
                enqueue(p);
            }
        }
        p = p->next_all;
    }

    if (current_process->time_slice_remaining > 0)
    {
        current_process->time_slice_remaining--;
    }

    if (current_process->time_slice_remaining == 0)
    {
        /* Reschedule */
        current_process->state = PROCESS_STATE_READY;
        enqueue(current_process);
        process_schedule();
    }
}

void process_print_table(void)
{
    kprintf("\n=== Process Table ===\n");
    kprintf("Total: %u processes, %llu context switches\n\n", total_processes, context_switches);

    // Explicitly aligned column headers
    kprintf("%-4s %-4s %-4s %-6s %-4s %-16s %-8s %-6s %-11s\n",
            "PID", "Pri", "Eff", "State", "Ring", "Name", "Runtime", "Wait", "parent PID");
    kprintf("---- ---- ---- ------ ---- ---------------- -------- ------ -----------\n");

    const char *state_names[] = {"READY", "RUN ", "BLOCK", "TERM "};
    const char *pri_names[] = {"IDLE", "LOW ", "NORM", "HIGH", "RT "};

    process_t *p = all_processes;
    while (p)
    {
        kprintf("%-4u %-4s %-4s %-6s %-4u %-16s %-8llu %-6llu %-11u\n",
                p->pid,
                pri_names[p->priority],
                pri_names[p->effective_priority],
                state_names[p->state],
                p->privilege,
                p->name,
                p->total_runtime,
                p->wait_time, p->parent_pid);
        p = p->next_all;
    }

    debug_kprintf("\nReady queues:\n");
    for (int i = PRIORITY_REALTIME; i >= PRIORITY_IDLE; i--)
    {
        if (ready_queues[i].count > 0)
        {
            debug_kprintf("  %s: %u\n", pri_names[i], ready_queues[i].count);
        }
    }

    kprintf("=====================\n\n");
}

void process_print_pcb(int pid)
{
    const char *state_names[] = {"READY", "RUN ", "BLOCK", "TERM "};
    const char *pri_names[] = {"IDLE", "LOW ", "NORM", "HIGH", "RT  "};

    process_t *proc = process_get((uint32_t)pid);
    if (!proc)
    {
        kprintf("Process with pid (%d) not found!\n", pid);
        return;
    }

    cpu_context_t *c = &proc->context;

    kprintf("============== PROCESS CONTROL BLOCK ==============\n");
    kprintf("PID:%-6d PPID:%-6d STATE:%-6s ZOMBIE:%-5s\n",
            proc->pid,
            proc->parent_pid,
            state_names[proc->state],
            proc->is_zombie ? "true" : "false");

    kprintf("NAME:%-17s RING:%d\n", proc->name, proc->privilege);
    kprintf("PRIORITY:%-6s EFFECTIVE:%-6s\n", pri_names[proc->priority], pri_names[proc->effective_priority]);

    if (proc->addr_space)
    {
        kprintf("PML4_PHYS:  %016lx\n", proc->addr_space->pml4_phys);
        kprintf("PML4_VIRT:  %016lx\n", proc->addr_space->pml4_virt);
    }
    else
    {
        kprintf("PML4_PHYS:  (null)\n");
        kprintf("PML4_VIRT:  (null)\n");
    }

    kprintf("KSTACK_PHYS:%016lx\n", proc->kernel_stack_phys);
    kprintf("KSTACK_VIRT:%016lx\n", proc->kernel_stack_virt);
    kprintf("USER_STACK: %016lx\n", proc->user_stack);
    kprintf("TIMESLICE:%-6u RUNTIME:%-12lu WAIT:%-12lu\n", proc->time_slice_remaining, proc->total_runtime, proc->wait_time);
    kprintf("--------------- CPU CONTEXT -----------------------\n");
    kprintf("RAX:%016lx RBX:%016lx RCX:%016lx\n", c->rax, c->rbx, c->rcx);
    kprintf("RDX:%016lx RSI:%016lx RDI:%016lx\n", c->rdx, c->rsi, c->rdi);
    kprintf("RBP:%016lx RSP:%016lx RIP:%016lx\n", c->rbp, c->rsp, c->rip);
    kprintf("R8 :%016lx R9 :%016lx R10:%016lx\n", c->r8, c->r9, c->r10);
    kprintf("R11:%016lx R12:%016lx R13:%016lx\n", c->r11, c->r12, c->r13);
    kprintf("R14:%016lx R15:%016lx RFL:%016lx\n", c->r14, c->r15, c->rflags);
    kprintf("CS :%016lx SS :%016lx\n", c->cs, c->ss);
    kprintf("NEXT_QUEUE:%016lx\n", (uint64_t)proc->next_in_queue);
    kprintf("NEXT_ALL  :%016lx\n", (uint64_t)proc->next_all);
    kprintf("FD_TABLE  :%016lx\n", (uint64_t)proc->fd_table);
    kprintf("ADDRSPACE :%016lx\n", (uint64_t)proc->addr_space);
    kprintf("===================================================\n");
}