/**
 * Licensed under MIT License - URIX project
 * auth.h - Auth helpers for userspace
 */
#ifndef AUTH_H
#define AUTH_H

#include "urix.h"
#include "string.h"

#define PASSWD_FILE "/etc/passwd.ps"
#define SESSION_FILE "/etc/session_user"

static inline int is_locked_command_allowed(void) {
    char user[64];
    int fd = open(SESSION_FILE, O_RDONLY);
    if (fd >= 0) {
        ssize_t len = read(fd, user, sizeof(user) - 1);
        close(fd);
        if (len > 0) {
            user[len] = '\0';
            // Trim newline if exists
            for (int i = 0; i < len; i++) {
                if (user[i] == '\n' || user[i] == '\r') {
                    user[i] = '\0';
                    break;
                }
            }
            if (strcmp(user, "root") == 0 || strcmp(user, "admin") == 0) {
                return 1;
            }
        }
    }
    return 0; // Not allowed
}

static inline void require_root(void) {
    if (!is_locked_command_allowed()) {
        println("Access denied: root privilege required.");
        exit(1);
    }
}

static inline int is_ps_file(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    if (len < 3) return 0;
    if (path[len-3] == '.' && path[len-2] == 'p' && path[len-1] == 's') {
        return 1;
    }
    return 0;
}

static inline void require_root_for_ps(const char *path) {
    if (is_ps_file(path) && !is_locked_command_allowed()) {
        println("Access denied: .ps files require root privileges.");
        exit(1);
    }
}

static inline int is_session_file(const char *path) {
    if (!path) return 0;
    const char *filename = path;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            filename = &path[i + 1];
        }
    }
    if (strcmp(filename, "session_user") == 0) {
        return 1;
    }
    return 0;
}

#endif
