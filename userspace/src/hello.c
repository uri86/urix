#include "urix.h"

int main(void)
{
    println("Hello from URIX userspace!");
    println("The userspace is working!");
    
    pid_t pid = getpid();
    print("My PID is: ");
    
    /* Simple number printing */
    if (pid >= 100) putchar('0' + (pid / 100));
    if (pid >= 10) putchar('0' + ((pid / 10) % 10));
    putchar('0' + (pid % 10));
    putchar('\n');
    
    return 0;
}
