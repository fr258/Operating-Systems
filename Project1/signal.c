#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// Changes the stack frame of caller (main function)
// such that you get to the line after "r2 = *( (int *) 0 )"
void segment_fault_handler(int signum) {

    printf("I am slain!\n");

    int *p;

    // Move pointer up to segfaulting address in main.
    p = &signum + 0x33;

    // Increment past segfaulting instruction up to print statement.
    *p += 0x5;
}

int main(int argc, char *argv[]) {

    int r2 = 0;

    // Register segfault signal handler
    signal(SIGSEGV, segment_fault_handler);

    // This will generate segmentation fault
    r2 = *( (int *) 0 );

    // Target statement
    printf("I live again!\n");

    return 0;
}