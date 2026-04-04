/**
 * Licensed under MIT License - URIX project
 * login.c - Login program
 */
#include "urix.h"
#include "string.h"

#define PASSWD_FILE "/etc/passwd.ps"
#define SESSION_FILE "/etc/session_user"

static void init_passwd_if_missing(void) {
    int fd = open(PASSWD_FILE, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return;
    }
    // Create it
    fd = open(PASSWD_FILE, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        const char *default_users = "root:root\nuser:user\n";
        write(fd, default_users, strlen(default_users));
        close(fd);
    }
}

static void trim_newline(char *s) {
    int len = strlen(s);
    for (int i = 0; i < len; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            break;
        }
    }
}

static int check_login(const char *username, const char *password) {
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
            *colon = '\0'; // split at colon
            if (newline) *newline = '\0'; // split at newline
            
            char *file_user = line;
            char *file_pass = colon + 1;
            
            if (strcmp(file_user, username) == 0 && strcmp(file_pass, password) == 0) {
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

int main(void) {
    init_passwd_if_missing();
    clear_screen();
    
    char username[64];
    char password[64];

    while (1) {
        print("URIX Login\n");
        print("Username: ");
        gets(username, sizeof(username));
        trim_newline(username);

        print("Password: ");
        gets(password, sizeof(password));
        trim_newline(password);

        if (check_login(username, password)) {
            // Success
            int fd = open(SESSION_FILE, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd >= 0) {
                write(fd, username, strlen(username));
                close(fd);
            }
            
            exec("/bin/shell", NULL);
            // Should not return
            println("Failed to start shell!");
            exit(1);
        } else {
            println("Login incorrect.");
        }
    }
    return 0;
}
