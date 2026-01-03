/*
 * Licensed under MIT License - URIX project.
 * process.h - Process management and scheduling interface
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <memory/vmm.h>

/* Time slice for scheduling (in timer ticks) */
#define TIME_SLICE 10

/* Priority boost frequency to prevent starvation */
#define PRIORITY_BOOST_THRESHOLD 50 /* ticks waiting before boost */

/* Process priority levels (0 = lowest, 4 = highest) */
typedef enum
{
    PRIORITY_IDLE = 0,    /* Idle/background tasks */
    PRIORITY_LOW = 1,     /* Low priority batch jobs */
    PRIORITY_NORMAL = 2,  /* Normal user processes */
    PRIORITY_HIGH = 3,    /* Interactive processes */
    PRIORITY_REALTIME = 4 /* Real-time/kernel tasks */
} process_priority_t;

/* Process states */
typedef enum
{
    PROCESS_STATE_READY = 0, /* Ready to run */
    PROCESS_STATE_RUNNING,   /* Currently executing */
    PROCESS_STATE_BLOCKED,   /* Waiting for I/O or event */
    PROCESS_STATE_TERMINATED /* Finished execution */
} process_state_t;

/* Process privilege levels */
typedef enum
{
    PROCESS_KERNEL = 0, /* Ring 0 - kernel mode */
    PROCESS_USER = 3    /* Ring 3 - user mode */
} process_privilege_t;

/*
 * CPU context saved during context switch
 * Layout matches what 'iretq' expects on stack for mode switches
 */
typedef struct cpu_context
{
    /* General purpose registers (saved by software) */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    /* Interrupt frame (saved by hardware) */
    uint64_t rip;    /* Instruction pointer */
    uint64_t cs;     /* Code segment */
    uint64_t rflags; /* CPU flags */
    uint64_t rsp;    /* Stack pointer */
    uint64_t ss;     /* Stack segment */
} __attribute__((packed)) cpu_context_t;

/*
 * Process Control Block (PCB)
 * Contains all state needed to manage a process
 */
typedef struct process
{
    /* Identity */
    uint32_t pid;  /* Process ID */
    char name[32]; /* Process name for debugging */

    /* State */
    process_state_t state;                 /* Current state */
    process_priority_t priority;           /* Base priority level */
    process_priority_t effective_priority; /* Current priority (after aging) */
    process_privilege_t privilege;         /* Ring 0 or Ring 3 */

    /* CPU context */
    cpu_context_t context; /* Saved registers */

    /* Memory */
    address_space_t *addr_space; /* Virtual address space */
    uint64_t kernel_stack_phys;  /* Kernel stack (physical) */
    uint64_t kernel_stack_virt;  /* Kernel stack (virtual) */
    uint64_t user_stack;         /* User stack top (virtual) */

    /* Scheduling info */
    uint32_t time_slice_remaining; /* Ticks left in quantum */
    uint64_t total_runtime;        /* Total CPU time used */
    uint64_t wait_time;            /* Ticks spent waiting */

    /* Queue links */
    struct process *next_in_queue; /* Next process in ready queue */
    struct process *next_all;      /* Next in global process list */

} process_t;

/**
 * process_init - Initialize process management subsystem
 *
 * Sets up:
 *  - Ready queues for all priority levels
 *  - Idle process (PID 0)
 *  - Process statistics
 *
 * Must be called after VMM, GDT, IDT initialization.
 */
void process_init(void);

/**
 * process_create - Create a new process
 *
 * entry_point: Virtual address where execution begins
 * name: Process name (max 31 chars)
 * priority: Base scheduling priority
 * privilege: PROCESS_KERNEL (ring 0) or PROCESS_USER (ring 3)
 *
 * Creates a new process with its own address space and stacks.
 * Process starts in READY state and will run when scheduled.
 *
 * For user processes (ring 3):
 *  - Uses user code/data segments (0x18, 0x20)
 *  - Sets up user stack in high memory
 *  - RFLAGS includes interrupt enable
 *
 * For kernel processes (ring 0):
 *  - Uses kernel code/data segments (0x08, 0x10)
 *  - Uses kernel stack only
 *
 * Returns: PID on success, -1 on failure
 */
int process_create(uint64_t entry_point, const char *name,
                   process_priority_t priority, process_privilege_t privilege);

/**
 * process_exit - Terminate current process
 *
 * exit_code: Exit status
 *
 * Marks process as TERMINATED, schedules next process, and queues
 * cleanup.
 */
void process_exit(int exit_code) __attribute__((noreturn));

/**
 * process_yield - Voluntarily yield CPU to next process
 *
 * Current process moves to back of its priority queue.
 */
void process_yield(void);

/**
 * process_schedule - Select and switch to next process
 *
 * Anti-starvation: Processes waiting too long get priority boost.
 */
void process_schedule(void);

/**
 * process_get_current - Get current running process
 *
 * Returns: Pointer to current PCB, or NULL if no process running
 */
process_t *process_get_current(void);

int process_kill(uint32_t pid);

process_t *process_get(uint32_t pid);


/**
 * process_timer_tick - Handle timer interrupt
 *
 * Called every timer tick:
 *  - Decrements current process time slice
 *  - Increments wait time for ready processes (for aging)
 *  - Triggers reschedule when time slice expires
 */
void process_timer_tick(void);

/**
 * process_print_table - Display all processes (debugging)
 *
 * Shows process table with PID, state, priority, name, runtime.
 */
void process_print_table(void);

/**
 * process_context_switch - Low-level context switch
 *
 * old_context: Where to save current CPU state (can be NULL)
 * new_context: CPU state to restore
 *
 * Implemented in assembly (switch.S):
 *  - Saves all general purpose registers
 *  - Switches stack pointer
 *  - Restores new context registers
 *  - Returns to new process
 *
 * If old_context is NULL (first run or exiting process),
 * skips save and just restores new context.
 */
extern void process_context_switch(cpu_context_t *old_context,
                                   cpu_context_t *new_context);

#endif /* PROCESS_H */