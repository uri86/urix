/**
 * Licensed under MIT License - URIX project
 * kill.c - kill a process
 */
#include "urix.h"
#include "auth.h"

int main(int argc, char **argv)
{
    require_root();
    if (argc < 2)
    {
        println("Usage: kill <pid>");
        exit(1);
    }
    
    int pid = atoi(argv[1]);
    if (kill(pid, 9) < 0) {
        println("Failed to kill process.");
        exit(1);
    }
    
    return 0;
}
