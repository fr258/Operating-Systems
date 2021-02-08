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

double avg_time = 0;

int main(int argc, char *argv[]) {

    /* Implement Code Here */
    float avg_time = 0;
    int i;
    for(i = 0; i < 1000; i++){

        avg_time += 0;
    }
    // Remember to place your final calculated average time
    // per system call into the avg_time variable

    printf("Average time per system call is %f microseconds\n", avg_time);

    return 0;
}