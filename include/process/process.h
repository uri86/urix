/*
 * Licensed under MIT License - URIX project.
 * process.h - Process management and scheduling interface
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <memory/vmm.h>
#include <process/fd_table.h>

/* Time slice for scheduling */
#define TIME_SLICE 10

/* Priority boost frequency to prevent starvation */
#define PRIORITY_BOOST_THRESHOLD 50

typedef enum
{
    PRIORITY_IDLE = 0,
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_HIGH = 3,
    PRIORITY_REALTIME = 4
} process_priority_t;

/* Process states */
typedef enum
{
    PROCESS_STATE_READY = 0,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} process_state_t;
typedef enum
{
    PROCESS_KERNEL = 0,
    PROCESS_USER = 3
} process_privilege_t;

typedef struct cpu_context
{
    /* General purpose registers */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    uint64_t rip;    /* Instruction pointer */
    uint64_t cs;     /* Code segment */
    uint64_t rflags; /* CPU flags */
    uint64_t rsp;    /* Stack pointer */
    uint64_t ss;     /* Stack segment */
} __attribute__((packed)) cpu_context_t;

/*
 * Process Control Block (PCB)
 */
typedef struct process
{
    uint32_t pid;
    uint32_t parent_pid;
    char name[32];

    int exit_status;
    int is_zombie;
    /* State of the proceess for context switching and priority scheduling */
    process_state_t state;
    process_priority_t priority;
    process_priority_t effective_priority;
    process_privilege_t privilege;
    cpu_context_t context;
    char cwd[512];
    
    /* Memory information, includes everything needed for a process to run */
    address_space_t *addr_space;
    uint64_t kernel_stack_phys;
    uint64_t kernel_stack_virt;
    uint64_t user_stack;
    fd_table_t *fd_table; // file table

    /* Scheduling info for priority scheduling and to prevent starvation */
    uint32_t time_slice_remaining;
    uint64_t total_runtime;
    uint64_t wait_time;

    /* Queue links */
    struct process *next_in_queue;
    struct process *next_all;

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
 * Returns: PID on success, -1 on failure
 */
int process_create(uint64_t entry_point, const char *name, process_priority_t priority, process_privilege_t privilege);

/**
 * process_load_elf - Create a new userspace process from ELF data
 *
 * name: Process name (max 31 characters)
 * elf_data: Pointer to ELF file data in memory
 * elf_size: Size of ELF data in bytes
 * priority: Base scheduling priority
 *
 * Creates a new userspace process, loads the ELF, sets up user stack, and adds to scheduler.
 * Returns: PID on success, -1 on failure
 */
int process_load_elf(const char *name, const void *elf_data, size_t elf_size, process_priority_t priority);

/**
 * process_fork - Create a copy of the current process
 * 
 * Returns:
 *   Parent: Child's PID
 *   Child:  0
 *   Error:  -1
 */
int process_fork(void);

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
 * process_wait - Wait for a child process to exit
 * 
 * Blocks the calling process until one of its children exits.
 * Returns the child's PID and stores exit status.
 * 
 * status: Pointer to store child's exit status (can be NULL)
 * 
 * Returns: Child's PID on success, -1 if no children
 */
int process_wait(int *status);

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
 *  - Increments wait time for ready processes
 *  - Triggers reschedule when time slice expires
 */
void process_timer_tick(void);

/**
 * process_print_table - Display all processes (debugging)
 *
 * Shows process table with PID, state, priority, name, runtime.
 */
void process_print_table(void);

void process_print_pcb(int pid);

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
extern void process_context_switch(cpu_context_t *old_context, cpu_context_t *new_context);

#endif /* PROCESS_H */