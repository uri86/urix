/**
 * Licensed under MIT License - URIX project
 * sudo.c - elevate privileges temporarily
 */
#include "urix.h"
#include "string.h"

#define PASSWD_FILE "/etc/passwd.ps"
#define SESSION_FILE "/etc/session_user"

static void trim_newline(char *s) {
    int len = strlen(s);
    for (int i = 0; i < len; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            break;
        }
    }
}

static int check_root_password(const char *password) {
    char buf[512];
    int fd = open(PASSWD_FILE, O_RDONLY);
    if (fd < 0) return 0;
    
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (bytes <= 0) return 0;
    buf[bytes] = '\0';

    char *line = buf;
    while (*line) {
        char *colon = NULL;
        char *newline = NULL;
        
        char *ptr = line;
        while (*ptr && *ptr != '\n') {
            if (*ptr == ':') {
                colon = ptr;
            }
            ptr++;
        }
        if (*ptr == '\n') newline = ptr;
        
        if (colon) {
            *colon = '\0';
            if (newline) *newline = '\0';
            
            char *file_user = line;
            char *file_pass = colon + 1;
            
            if (strcmp(file_user, "root") == 0 && strcmp(file_pass, password) == 0) {
                return 1;
            }
        }
        
        if (newline) {
            line = newline + 1;
        } else {
            break;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        println("Usage: sudo <command> [args...]");
        exit(1);
    }
    
    char prev_user[64];
    prev_user[0] = '\0';
    
    int fd = open(SESSION_FILE, O_RDONLY);
    if (fd >= 0) {
        ssize_t len = read(fd, prev_user, sizeof(prev_user) - 1);
        close(fd);
        if (len >= 0) prev_user[len] = '\0';
    }
    
    if (strcmp(prev_user, "root") != 0 && strcmp(prev_user, "admin") != 0) {
        char password[64];
        print("[sudo] password for root: ");
        gets(password, sizeof(password));
        trim_newline(password);

        if (!check_root_password(password)) {
            println("Sorry, try again.");
            exit(1);
        }
    }
    
    // Elevate
    fd = open(SESSION_FILE, O_WRONLY | O_TRUNC | O_CREAT);
    if (fd >= 0) {
        write(fd, "root", 4);
        close(fd);
    }
    
    if (fork() == 0) {
        char path[256];
        if (argv[1][0] == '/' || (argv[1][0] == '.' && argv[1][1] == '/')) {
            strcpy(path, argv[1]);
        } else {
            strcpy(path, "/bin/");
            strcpy(path + 5, argv[1]);
        }
        
        exec(path, &argv[1]);
        print("sudo: command not found: ");
        println(argv[1]);
        exit(1);
    }
    
    wait(NULL);
    
    // Restore
    fd = open(SESSION_FILE, O_WRONLY | O_TRUNC | O_CREAT);
    if (fd >= 0) {
        write(fd, prev_user, strlen(prev_user));
        close(fd);
    }
    
    return 0;
}
