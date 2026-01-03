/*
 * Licensed under MIT License - URIX project.
 * process.c - Simplified process management
 */

#include <process/process.h>
#include <memory/vmm.h>
#include <memory/physical/pmm.h>
#include <lib/print.h>
#include <lib/string.h>
#include <lib/panic.h>

/* GDT segment selectors */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x1B
#define USER_DS 0x23

#define USER_STACK_TOP 0x0000800000000000ULL

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

int process_create(uint64_t entry_point, const char *name,
                   process_priority_t priority, process_privilege_t privilege)
{
    if (!entry_point || priority > PRIORITY_REALTIME)
    {
        return -1;
    }

    /* Allocate PCB */
    uint64_t pcb_frame = pmm_alloc_frame();
    if (!pcb_frame)
        return -1;

    process_t *proc = (process_t *)phys_to_virt(pcb_frame);
    memset(proc, 0, sizeof(process_t));

    /* Setup identity */
    proc->pid = next_pid++;
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
        pmm_free_frame(pcb_frame);
        return -1;
    }

    /* Allocate kernel stack */
    proc->kernel_stack_phys = pmm_alloc_frame();
    if (!proc->kernel_stack_phys)
    {
        vmm_destroy_address_space(proc->addr_space);
        pmm_free_frame(pcb_frame);
        return -1;
    }

    /* Map kernel stack */
    proc->kernel_stack_virt = KERNEL_VIRT_BASE + proc->kernel_stack_phys;
    vmm_map_page(proc->addr_space, proc->kernel_stack_virt,
                 proc->kernel_stack_phys, VMM_KERNEL_FLAGS);

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

    /* Add to ready queue */
    enqueue(proc);

    /* Add to all processes list */
    proc->next_all = all_processes;
    all_processes = proc;

    total_processes++;

    debug_kprintf("Created process '%s' (PID %u, Pri %d, Ring %d)\n",
                  proc->name, proc->pid, proc->priority, proc->privilege);

    return proc->pid;
}

void process_exit(int exit_code)
{
    if (!current_process)
    {
        PANIC("process_exit: no current process");
    }

    debug_kprintf("Process '%s' (PID %u) exiting with code %d\n",
                  current_process->name, current_process->pid, exit_code);

    /* Save what we need before cleanup */
    address_space_t *old_space = current_process->addr_space;
    uint64_t old_stack = current_process->kernel_stack_phys;
    uint64_t pcb_phys = virt_to_phys(current_process);

    /* Remove from all processes list */
    if (all_processes == current_process)
    {
        all_processes = current_process->next_all;
    }
    else
    {
        process_t *p = all_processes;
        while (p && p->next_all != current_process)
            p = p->next_all;
        if (p)
            p->next_all = current_process->next_all;
    }

    total_processes--;

    /* Get next process BEFORE we destroy anything */
    process_t *next = dequeue_highest_priority();

    if (!next)
    {
        PANIC("No process to switch to after exit!");
    }

    debug_kprintf("Switching from exiting process to '%s' (PID %u)\n",
                  next->name, next->pid);

    /* Update states */
    next->state = PROCESS_STATE_RUNNING;
    next->time_slice_remaining = TIME_SLICE;
    next->wait_time = 0;
    next->effective_priority = next->priority;

    current_process = next;
    context_switches++;

    /* Switch address space BEFORE cleanup */
    vmm_switch_address_space(next->addr_space);

    /* Now safe to cleanup old process */
    if (old_space)
    {
        vmm_destroy_address_space(old_space);
    }
    if (old_stack)
    {
        pmm_free_frame(old_stack);
    }
    pmm_free_frame(pcb_phys);

    /* Jump to new process */
    process_context_switch(NULL, &next->context);

    PANIC("Returned from context switch in exit");
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
    process_t *new = dequeue_highest_priority();

    if (!new)
    {
        kprintf("WARNING: No ready process!\n");
        if (old && old->state == PROCESS_STATE_RUNNING)
        {
            /* Keep running current */
            enqueue(old);
            return;
        }
        PANIC("No runnable process!");
    }

    /* Continues if the same process was chosen again */
    if (new == old && old && old->state == PROCESS_STATE_RUNNING)
    {
        // enqueue(new);
        return;
    }

    /* Update states */
    if (old && old->state == PROCESS_STATE_RUNNING)
    {
        old->state = PROCESS_STATE_READY;
        enqueue(old);
    }

    new->state = PROCESS_STATE_RUNNING;
    new->time_slice_remaining = TIME_SLICE;
    new->wait_time = 0; /* Reset wait time when it runs */

    new->effective_priority = new->priority;

    current_process = new;
    context_switches++;

    /* Switch address space */
    vmm_switch_address_space(new->addr_space);

    /* Context switch */
    if (old && old->state != PROCESS_STATE_TERMINATED)
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
    /* Never kill idle */
    if (pid == 0)
        return -1;

    process_t *p = process_get(pid);
    if (!p)
        return -1;

    /* Cannot kill currently running process - it must exit itself */
    if (p == current_process)
    {
        return -1;
    }

    /* Disable interrupts while we manipulate process lists */
    __asm__ volatile("cli");

    /* Remove from ready queue if present */
    ready_queue_t *q = &ready_queues[p->effective_priority];
    process_t *rq_prev = NULL;
    process_t *rq = q->head;

    while (rq)
    {
        if (rq == p)
        {
            if (rq_prev)
                rq_prev->next_in_queue = rq->next_in_queue;
            else
                q->head = rq->next_in_queue;

            if (q->tail == rq)
                q->tail = rq_prev;

            q->count--;
            break;
        }

        rq_prev = rq;
        rq = rq->next_in_queue;
    }

    /* Remove from all_processes list */
    process_t *prev = NULL;
    process_t *ap = all_processes;

    while (ap && ap != p)
    {
        prev = ap;
        ap = ap->next_all;
    }

    if (ap)
    {
        if (prev)
            prev->next_all = p->next_all;
        else
            all_processes = p->next_all;
    }

    total_processes--;

    /* Re-enable interrupts */
    __asm__ volatile("sti");

    /* Cleanup resources */
    if (p->addr_space)
        vmm_destroy_address_space(p->addr_space);

    if (p->kernel_stack_phys)
        pmm_free_frame(p->kernel_stack_phys);

    uint64_t pcb_phys = virt_to_phys(p);
    pmm_free_frame(pcb_phys);

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
                ready_queue_t *old_q = &ready_queues[p->effective_priority];
                if (old_q->head == p)
                {
                    old_q->head = p->next_in_queue;
                    if (!old_q->head)
                        old_q->tail = NULL;
                    old_q->count--;
                }
                else
                {
                    /* Find and remove from queue */
                    process_t *prev = old_q->head;
                    while (prev && prev->next_in_queue != p)
                        prev = prev->next_in_queue;
                    if (prev)
                    {
                        prev->next_in_queue = p->next_in_queue;
                        if (old_q->tail == p)
                            old_q->tail = prev;
                        old_q->count--;
                    }
                }

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
    kprintf("Total: %u processes, %llu context switches\n\n",
            total_processes, context_switches);

    kprintf("PID  Pri   Eff   State   Ring  Name              Runtime  Wait\n");
    kprintf("---  ----  ----  ------  ----  ----------------  -------  ----\n");

    const char *state_names[] = {"READY", "RUN   ", "BLOCK", "TERM  "};
    const char *pri_names[] = {"IDLE", "LOW ", "NORM", "HIGH", "RT  "};

    process_t *p = all_processes;
    while (p)
    {
        kprintf("%u  %s %s %s   %u   %s  %llu  %llu\n",
                p->pid,
                pri_names[p->priority],
                pri_names[p->effective_priority],
                state_names[p->state],
                p->privilege,
                p->name,
                p->total_runtime,
                p->wait_time);
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