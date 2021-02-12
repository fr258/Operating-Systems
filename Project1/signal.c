/* syscall.c
 *
 * Group Members Names and NetIDs:
 *   1. Matthew Notaro (myn7)
 *   2. Farrah Rahman (fr258)
 *
 * ILab Machine Tested on:
 *   kill.cs.rutgers.edu
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>


/* Part 1 - Step 2 to 4: Do your tricks here
 * Your goal must be to change the stack frame of caller (main function)
 * such that you get to the line after "r2 = *( (int *) 0 )"
 */
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

    /* Part 1 - Step 1: Registering signal handler */
    signal(SIGSEGV, segment_fault_handler);

    // This will generate segmentation fault
    r2 = *( (int *) 0 );

    // Target statement
    printf("I live again!\n");

    return 0;
}