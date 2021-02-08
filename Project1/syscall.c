/* syscall.c
 *
 * Group Members Names and NetIDs:
 *   1. Matthew Notaro (myn7)
 *   2. Farrah Rahman (fr258)
 *
 * ILab Machine Tested on:
 *   kill.cs.rutgers.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>                                                                                
#include <sys/syscall.h>
#include <unistd.h>

double avg_time = 0;

int main(int argc, char *argv[]) {
    // Initialize start and stop structs
    struct timeval start, stop;

    // Sets initial time
    gettimeofday(&start, NULL);

    // System call loop
    int i;
    pid_t pid;
    for(i = 1; i <= 10000; i++){
        pid = syscall(SYS_getpid);
    }

    // Sets final time
    gettimeofday(&stop, NULL);

    // Calculates microsecond time difference and average
    double total_time = (double)(stop.tv_usec - start.tv_usec) + (double)(stop.tv_sec - start.tv_sec) * 1000000;
    avg_time = total_time/i;

    // Prints average time per system call
    printf("Average time per system call is %f microseconds\n", avg_time);

    return 0;
}