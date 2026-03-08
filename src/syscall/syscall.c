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

#define EXEC_MAX_ARGS 16
#define EXEC_MAX_ARG_LEN 256

extern void syscall_entry(void);

static int resolve_path(const char *path, char *out_path)
{
    process_t *current = process_get_current();
    if (!path || !out_path)
        return -EINVAL;

    char temp[512];
    if (path[0] == '/')
    {
        strncpy(temp, path, 511);
    }
    else
    {
        strncpy(temp, current->cwd, 511);
        size_t len = strlen(temp);
        if (len > 0 && temp[len - 1] != '/')
        {
            if (len < 511)
            {
                temp[len] = '/';
                temp[len + 1] = '\0';
            }
        }
        size_t remain = 511 - strlen(temp);
        strncpy(temp + strlen(temp), path, remain);
    }
    temp[511] = '\0';

    /* Normalize path (handle '.' and '..') */
    char *tokens[64];
    int count = 0;

    char *p = temp;
    while (*p)
    {
        while (*p == '/')
            p++; /* Skip redundant slashes */
        if (*p == '\0')
            break;

        char *start = p;
        while (*p && *p != '/')
            p++;

        if (*p)
        {
            *p = '\0';
            p++;
        }

        if (strcmp(start, ".") == 0)
        {
            continue; /* Ignore '.' */
        }
        else if (strcmp(start, "..") == 0)
        {
            if (count > 0)
                count--; /* Go back one directory */
        }
        else
        {
            if (count < 64)
            {
                tokens[count++] = start;
            }
        }
    }

    /* Reconstruct the absolute normalized path */
    out_path[0] = '\0';
    if (count == 0)
    {
        strcpy(out_path, "/");
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            strcat(out_path, "/");
            strcat(out_path, tokens[i]);
        }
    }
    return 0;
}

static long sys_exit(int status)
{
    debug_kprintf("syscall: exit(%d) from PID %u\n", status, process_get_current()->pid);
    process_t *current = process_get_current();

    if (current->fd_table)
    {
        for (int i = 0; i < MAX_FDS; i++)
        {
            fd_entry_t *entry = &current->fd_table->fds[i];
            if (entry->type != FD_TYPE_NONE)
            {
                // If it's a file, decrement the vnode refcount/close it
                if (entry->type == FD_TYPE_FILE && entry->vfs_file)
                {
                    vfs_close(entry->vfs_file);
                }
                entry->type = FD_TYPE_NONE;
            }
        }
    }
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
    // Mask out all flags except the access mode (O_ACCMODE is 3)
    int acc_mode = entry->flags & 3;
    // Deny read operations if the file was opened as write-only
    if (acc_mode == O_WRONLY)
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

    char abs_path[512];
    if (resolve_path(path, abs_path) != 0)
        return -EINVAL;

    int fd = fd_table_alloc(current->fd_table);
    if (fd < 0)
        return -EMFILE;

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

    file_t *file = NULL;
    int ret = vfs_open(abs_path, vfs_flags, &file);
    if (ret != 0 || !file)
    {
        return ret;
    }

    fd_entry_t *entry = &current->fd_table->fds[fd];
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

    /* copy path */
    char kpath[256];
    if (resolve_path(path, kpath) != 0)
        return -EINVAL;
    debug_kprintf("exec: '%s'\n", kpath);

    const char *basename = kpath;
    for (int i = 0; kpath[i] != '\0'; i++)
    {
        if (kpath[i] == '/')
        {
            basename = &kpath[i + 1];
        }
    }

    if (*basename == '\0')
    {
        basename = "unknown";
    }

    strncpy(current->name, basename, sizeof(current->name) - 1);
    current->name[sizeof(current->name) - 1] = '\0';

    /* copy argv strings into kernel memory */
    int argc = 0;
    char kargs[EXEC_MAX_ARGS][EXEC_MAX_ARG_LEN];
    if (argv)
        while (argc < EXEC_MAX_ARGS && argv[argc])
        {
            strncpy(kargs[argc], argv[argc], EXEC_MAX_ARG_LEN - 1);
            kargs[argc][EXEC_MAX_ARG_LEN - 1] = '\0';
            argc++;
        }

    /* load ELF */
    address_space_t *new_as = vmm_create_address_space();
    if (!new_as)
        return -ENOMEM;

    elf_info_t elf_info;
    if (elf_load_from_file(kpath, new_as, &elf_info) != 0)
    {
        vmm_destroy_address_space(new_as);
        return -ENOEXEC;
    }

    /* map user stack */
    for (uint64_t i = 0; i < USER_STACK_SIZE / PAGE_SIZE; i++)
    {
        uint64_t vaddr = USER_STACK_BOTTOM + i * PAGE_SIZE;
        uint64_t phys = pmm_alloc_frame();
        if (!phys)
        {
            vmm_destroy_address_space(new_as);
            return -ENOMEM;
        }
        memset((void *)phys_to_virt(phys), 0, PAGE_SIZE);
        if (vmm_map_page(new_as, vaddr, phys, VMM_USER | VMM_WRITE | VMM_PRESENT))
        {
            pmm_free_frame(phys);
            vmm_destroy_address_space(new_as);
            return -ENOMEM;
        }
    }

    /* switch address space */
    address_space_t *old_as = current->addr_space;
    current->addr_space = new_as;
    current->user_stack = USER_STACK_TOP;

    vmm_switch_address_space(new_as);
    gdt_set_kernel_stack(current->kernel_stack_virt + PAGE_SIZE);
    if (old_as)
        vmm_destroy_address_space(old_as);

    uint64_t sp = USER_STACK_TOP;
    uint64_t str_ptrs[EXEC_MAX_ARGS];

    /* push string data top-down */
    for (int i = argc - 1; i >= 0; i--)
    {
        size_t slen = strlen(kargs[i]) + 1;
        sp -= slen;
        sp &= ~7ULL; /* 8-byte align */

        /* directly copy into the user stack */
        memcpy((void *)sp, kargs[i], slen);
        str_ptrs[i] = sp;
    }

    /* align to 16 bytes */
    sp &= ~15ULL;

    /* push NULL (argv[argc] = NULL) */
    sp -= 8;
    *(uint64_t *)sp = 0;

    /* push argv pointers */
    for (int i = argc - 1; i >= 0; i--)
    {
        sp -= 8;
        *(uint64_t *)sp = str_ptrs[i];
    }

    /* push argc */
    sp -= 8;
    *(uint64_t *)sp = (uint64_t)argc;

    /* rewrite iretq frame */
    uint64_t *frame = (uint64_t *)trap_frame_ptr;
    frame[0] = elf_info.entry_point; /* RIP */
    frame[1] = 0x1B;                 /* CS  */
    frame[2] = 0x202;                /* RFLAGS */
    frame[3] = sp;                   /* RSP, points at argc */
    frame[4] = 0x23;                 /* SS   */

    /* zero all the general purpose registers */
    uint64_t *regs = (uint64_t *)(trap_frame_ptr - 120);
    for (int i = 0; i < 15; i++)
        regs[i] = 0;

    debug_kprintf("exec: ready — entry=%lx sp=%lx argc=%d\n",
                  elf_info.entry_point, sp, argc);
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
    char abs_path[512];
    if (resolve_path(path, abs_path) != 0)
        return -EINVAL;
    return vfs_mkdir(abs_path);
}

static long sys_rmdir(const char *path)
{
    char abs_path[512];
    if (resolve_path(path, abs_path) != 0)
        return -EINVAL;
    return vfs_rmdir(abs_path);
}

static long sys_unlink(const char *path)
{
    char abs_path[512];
    if (resolve_path(path, abs_path) != 0)
        return -EINVAL;
    return vfs_unlink(abs_path);
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

static long sys_change_terminal_color(vga_color_t fg, vga_color_t bg)
{
    set_color(fg, bg);
    return 0;
}

static long sys_clear_screen()
{
    clear_screen();
    return 0;
}

static long sys_readdir(int fd, dirent_t *user_entry)
{
    process_t *current = process_get_current();
    if (!current || !current->fd_table)
        return -EBADF;

    fd_entry_t *entry = fd_table_get(current->fd_table, fd);
    if (!entry || entry->type != FD_TYPE_FILE || !entry->vfs_file)
        return -EBADF;

    if (!user_entry)
        return -EINVAL;

    /* Use vfs_readdir — it advances the file's internal offset */
    dirent_t kentry;
    int ret = vfs_readdir(entry->vfs_file, &kentry);
    if (ret != 0)
        return -1; /* no more entries */

    /* Copy result to userspace */
    memcpy(user_entry, &kentry, sizeof(dirent_t));
    return 0;
}

static long sys_getcwd(char *buf, size_t size)
{
    if (!buf || size == 0)
        return -EINVAL;

    process_t *current = process_get_current();
    if (!current)
        return -ESRCH;

    size_t len = strlen(current->cwd);
    if (len + 1 > size)
        return -EINVAL; /* buffer too small */
    strncpy(buf, current->cwd, size);
    buf[0] = '/';
    return (long)(len + 1);
}

static long sys_chdir(const char *path)
{
    if (!path)
        return -EINVAL;
    process_t *current = process_get_current();
    if (!current)
        return -ESRCH;

    char abs_path[512];
    if (resolve_path(path, abs_path) != 0)
        return -EINVAL;

    file_t *f = NULL;
    int ret = vfs_open(abs_path, VFS_READ, &f);
    if (ret != 0 || !f)
        return -ENOENT;

    if (f->vnode->type != VFS_DIR)
    {
        vfs_close(f);
        return -ENOTDIR;
    }
    vfs_close(f);

    strncpy(current->cwd, abs_path, sizeof(current->cwd) - 1);
    current->cwd[sizeof(current->cwd) - 1] = '\0';
    return 0;
}

static long sys_dup2(int oldfd, int newfd)
{
    process_t *current = process_get_current();
    if (!current || !current->fd_table)
    {
        return -EBADF;
    }

    return fd_table_dup2(current->fd_table, oldfd, newfd);
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
    [SYS_TERMINAL_COLOR] = (syscall_fn_t)sys_change_terminal_color,
    [SYS_CLEAR_SCREEN] = (syscall_fn_t)sys_clear_screen,
    [SYS_CHDIR] = (syscall_fn_t)sys_chdir,
    [SYS_GETCWD] = (syscall_fn_t)sys_getcwd,
    [SYS_READDIR] = (syscall_fn_t)sys_readdir,
    [SYS_DUP2] = (syscall_fn_t)sys_dup2,
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