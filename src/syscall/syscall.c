/*
 * Licensed under MIT License - URIX project.
 * syscall.c - System call implementation.
 */

#include <syscall/syscall.h>
#include <interrupts/idt.h>
#include <process/process.h>
#include <process/fd_table.h>
#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <lib/print.h>
#include <lib/string.h>
#include <fs/vfs.h>
#include <fs/elf.h>
#include <memory/vmm.h>
#include <cpu/gdt.h>
#include <memory/physical/pmm.h>
#include <lib/logo.h>
#include <memory/kmalloc.h>

extern void syscall_entry(void);

static long sys_exit(int status)
{
    debug_kprintf("syscall: exit(%d) from PID %u\n",
                  status, process_get_current()->pid);
    process_exit(status);
    return 0; // should never get here...
}

static long sys_read(int fd, void *buf, size_t count)
{
    process_t *current = process_get_current();
    if (!current || !current->fd_table)
        return -EBADF;

    fd_entry_t *entry = fd_table_get(current->fd_table, fd);
    if (!entry)
        return -EBADF;

    // check if the file/input is readable
    if (!(entry->flags & (O_RDONLY | O_RDWR)))
        return -EBADF;

    switch (entry->type)
    {
    case FD_TYPE_CONSOLE:
        // read from the keyboard
        if (entry->console_type == 0)
        {
            size_t i;
            char *cbuf = (char *)buf;
            for (i = 0; i < count; i++)
            {
                char c = keyboard_getchar_blocking();
                cbuf[i] = c;
                if (c == '\n')
                {
                    i++;
                    break;
                }
            }
            return i;
        }
        return -EBADF; // cannot read from the place specified

    case FD_TYPE_FILE:
        // read from a file
        if (!entry->vfs_file)
            return -EBADF;
        return vfs_read(entry->vfs_file, buf, count);

    default:
        return -EBADF;
    }
}

static long sys_write(int fd, const void *buf, size_t count)
{
    process_t *current = process_get_current();
    if (!current || !current->fd_table)
        return -EBADF;

    fd_entry_t *entry = fd_table_get(current->fd_table, fd);
    if (!entry)
        return -EBADF;

    // check if the file/place is writeable...
    if (!(entry->flags & (O_WRONLY | O_RDWR)))
        return -EBADF;

    switch (entry->type)
    {
    case FD_TYPE_CONSOLE:
        // write to the console
        if (entry->console_type == 1 || entry->console_type == 2)
        {
            const char *str = (const char *)buf;
            for (size_t i = 0; i < count; i++)
            {
                console_putchar(str[i]);
            }
            return count;
        }
        return -EBADF; // cannot write to stdin

    case FD_TYPE_FILE:
        // write to an actual file
        if (!entry->vfs_file)
            return -EBADF;
        return vfs_write(entry->vfs_file, buf, count);

    default:
        return -EBADF;
    }
}

static long sys_open(const char *path, int flags)
{
    process_t *current = process_get_current();
    if (!current || !current->fd_table)
        return -EBADF;

    // allocate a file descriptor
    int fd = fd_table_alloc(current->fd_table);
    if (fd < 0)
        return -EMFILE; // too many files, think over your life choices again.

    // convert the syscall flags into vfs flags
    uint32_t vfs_flags = 0;

    if (flags & O_RDONLY)
        vfs_flags |= VFS_READ;
    if (flags & O_WRONLY)
        vfs_flags |= VFS_WRITE;
    if (flags & O_RDWR)
        vfs_flags |= VFS_READ | VFS_WRITE;
    if (flags & O_CREAT)
        vfs_flags |= VFS_CREATE;
    if (flags & O_TRUNC)
        vfs_flags |= VFS_TRUNC;
    if (flags & O_APPEND)
        vfs_flags |= VFS_APPEND;

    // open the file through VFS
    file_t *file = NULL;
    int ret = vfs_open(path, vfs_flags, &file);
    if (ret != 0 || !file)
    {
        fd_table_free(current->fd_table, fd);
        return ret;
    }

    // set up the file descriptor entry
    fd_entry_t *entry = fd_table_get(current->fd_table, fd);
    entry->type = FD_TYPE_FILE;
    entry->flags = flags;
    entry->vfs_file = file;

    return fd;
}

static long sys_close(int fd)
{
    process_t *current = process_get_current();
    if (!current || !current->fd_table)
        return -EBADF;

    // don't allow closing stdin/stdout/stderr because it will just cause problems...
    if (fd < 3)
        return -EBADF;

    fd_entry_t *entry = fd_table_get(current->fd_table, fd);
    if (!entry)
        return -EBADF;

    fd_table_free(current->fd_table, fd);
    return 0;
}

static long sys_getpid(void)
{
    process_t *current = process_get_current();
    return current ? current->pid : (uint32_t)-1;
}

static long sys_yield(void)
{
    process_yield();
    return 0;
}

static long sys_getchar(void)
{
    return keyboard_getchar_blocking();
}

static long sys_putchar(int c)
{
    console_putchar((char)c);
    return c;
}

static long sys_gets(char *buf, size_t size)
{
    return keyboard_gets(buf, size);
}

static long sys_puts(const char *str)
{
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++)
    {
        console_putchar(str[i]);
    }
    console_putchar('\n');
    return len;
}

static long sys_fork(void)
{
    return process_fork();
}

static long sys_exec(const char *path, char *const argv[])
{
    extern uint64_t syscall_frame_rsp;
    uint64_t trap_frame_ptr = syscall_frame_rsp;

    if (!path)
        return -EINVAL;

    process_t *current = process_get_current();
    if (!current)
        return -ESRCH;

    /* copy path into kernel memory */
    char kpath[256];
    strncpy(kpath, path, sizeof(kpath) - 1);
    kpath[sizeof(kpath) - 1] = '\0';

    debug_kprintf("exec: Loading '%s' for PID %u\n", kpath, current->pid);

    address_space_t *new_as = vmm_create_address_space();
    if (!new_as)
        return -ENOMEM;

    elf_info_t elf_info;
    if (elf_load_from_file(kpath, new_as, &elf_info) != 0)
    {
        vmm_destroy_address_space(new_as);
        return -ENOEXEC;
    }

    uint64_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    for (uint64_t i = 0; i < stack_pages; i++)
    {
        uint64_t vaddr = USER_STACK_BOTTOM + (i * PAGE_SIZE);
        uint64_t phys = pmm_alloc_frame();
        if (phys == 0)
        {
            vmm_destroy_address_space(new_as);
            return -ENOMEM;
        }

        memset((void *)phys_to_virt(phys), 0, PAGE_SIZE);

        if (vmm_map_page(new_as, vaddr, phys, VMM_USER | VMM_WRITE | VMM_PRESENT) != 0)
        {
            pmm_free_frame(phys);
            vmm_destroy_address_space(new_as);
            return -ENOMEM;
        }
    }

    int argc = 0;
    if (argv)
        while (argv[argc] != NULL)
            argc++;

    address_space_t *old_as = current->addr_space;
    current->addr_space = new_as;
    current->user_stack = USER_STACK_TOP;

    vmm_switch_address_space(new_as); // Load new CR3
    gdt_set_kernel_stack(current->kernel_stack_virt + PAGE_SIZE);

    if (old_as)
    {
        vmm_destroy_address_space(old_as);
    }

    uint64_t *frame = (uint64_t *)trap_frame_ptr;
    frame[0] = elf_info.entry_point; /* RIP */
    frame[1] = 0x1B;                 /* CS */
    frame[2] = 0x202;                /* RFLAGS */
    frame[3] = USER_STACK_TOP;       /* RSP */
    frame[4] = 0x23;                 /* SS */

    uint64_t *regs = (uint64_t *)(trap_frame_ptr - 120);
    regs[0] = 0;    /* rax */
    regs[1] = 0;    /* rbx */
    regs[2] = 0;    /* rcx */
    regs[3] = 0;    /* rdx */
    regs[4] = 0;    /* rsi */
    regs[5] = argc; /* rdi, argc */
    regs[6] = 0;    /* rbp */
    regs[7] = 0;    /* r8 */
    regs[8] = 0;    /* r9 */
    regs[9] = 0;    /* r10 */
    regs[10] = 0;   /* r11 */
    regs[11] = 0;   /* r12 */
    regs[12] = 0;   /* r13 */
    regs[13] = 0;   /* r14 */
    regs[14] = 0;   /* r15 */

    debug_kprintf("exec: '%s' ready, entry=%lx stack=%lx\n", kpath, elf_info.entry_point, USER_STACK_TOP);

    return 0;
}

static long sys_wait(int *status)
{
    return process_wait(status);
}

static long sys_kill(uint32_t pid, int sig)
{
    (void)sig;
    return process_kill(pid);
}

static long sys_mkdir(const char *path, uint32_t mode)
{
    (void)mode;
    return vfs_mkdir(path);
}

static long sys_rmdir(const char *path)
{
    return vfs_rmdir(path);
}

static long sys_unlink(const char *path)
{
    return vfs_unlink(path);
}

static long sys_kernel_print(int flag)
{
    switch (flag)
    {
    case KPPMM:
    {
        pmm_print_stats();
        break;
    }
    case KPMAL:
    {
        kmalloc_print_stats();
        break;
    }
    case KPLG:
    {
        print_logo();
        break;
    }
    case KPPPT:
    {
        process_print_table();
        break;
    }
    default:
        break;
    }
    return 0;
}

typedef long (*syscall_fn_t)(long, long, long, long, long, long);

static syscall_fn_t syscall_table[SYS_MAX] = {
    [SYS_EXIT] = (syscall_fn_t)sys_exit,
    [SYS_READ] = (syscall_fn_t)sys_read,
    [SYS_WRITE] = (syscall_fn_t)sys_write,
    [SYS_OPEN] = (syscall_fn_t)sys_open,
    [SYS_CLOSE] = (syscall_fn_t)sys_close,
    [SYS_FORK] = (syscall_fn_t)sys_fork,
    [SYS_EXEC] = (syscall_fn_t)sys_exec,
    [SYS_WAIT] = (syscall_fn_t)sys_wait,
    [SYS_GETPID] = (syscall_fn_t)sys_getpid,
    [SYS_YIELD] = (syscall_fn_t)sys_yield,
    [SYS_KILL] = (syscall_fn_t)sys_kill,
    [SYS_MKDIR] = (syscall_fn_t)sys_mkdir,
    [SYS_RMDIR] = (syscall_fn_t)sys_rmdir,
    [SYS_UNLINK] = (syscall_fn_t)sys_unlink,
    [SYS_GETCHAR] = (syscall_fn_t)sys_getchar,
    [SYS_PUTCHAR] = (syscall_fn_t)sys_putchar,
    [SYS_GETS] = (syscall_fn_t)sys_gets,
    [SYS_PUTS] = (syscall_fn_t)sys_puts,
    [SYS_KPS] = (syscall_fn_t)sys_kernel_print,
};

void syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    __asm__ volatile("sti");
    long ret;

    // make sure the system call exists
    if (syscall_num >= SYS_MAX || !syscall_table[syscall_num])
    {
        debug_kprintf("Invalid syscall %llu from PID %u\n",
                      syscall_num, process_get_current()->pid);
        ret = -ENOSYS;
    }
    else
    {
        // call the correct system call handler
        ret = syscall_table[syscall_num](arg1, arg2, arg3, arg4, arg5, arg6);
    }

    (void)ret;
}

void syscall_init(void)
{
    debug_kprintf("Initializing system call interface...\n");
    idt_set_gate(0x80, (uint64_t)syscall_entry, 0x08, 0xEE);
    debug_kprintf("System calls initialized (INT 0x80)\n");
}