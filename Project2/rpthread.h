// File:	rpthread_t.h

/* 
 * Group Members Names and NetIDs:
 *   1. Matthew Notaro (myn7)
 *   2. Farrah Rahman (fr258)
 *
 * ILab Machine Tested on:
 *   kill.cs.rutgers.edu
 */


#ifndef RTHREAD_T_H
#define RTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_RTHREAD macro */
#define USE_RTHREAD 1

#ifndef TIMESLICE
/* defined timeslice to 5 ms, feel free to change this while testing your code
 * it can be done directly in the Makefile*/
#define TIMESLICE 5
#endif

#define STACKSIZE 16834

//remove later!
//#define SCHED RR

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <ucontext.h>
#include <string.h>

typedef uint rpthread_t;
typedef enum {RUNNING, READY, BLOCKED, TERMINATED} status;

// TCB Struct
typedef struct threadControlBlock {
	rpthread_t threadId;
	ucontext_t context;
	status state;
	void*(*function)(void*);
	void* input;
	void* retVal;
	int priority;
} tcb;

/* mutex struct definition */
typedef struct rpthread_mutex_t {
	int* lock;
} rpthread_mutex_t;

// TCB and extra fields to make linked lists
typedef struct Node {
	tcb* tcblock;
	struct Node* prev;
	struct Node* next;
    int priority;
} node;

// Queue node able to make a LL of queues for MLFQ
typedef struct Queue {
	node* head;
	node* tail;
    struct Queue* next;
    int priority;
} queue;

typedef struct LinkedList {
	node* head;
} llist;








/* Function Declarations: */

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void*(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int rpthread_yield();

/* terminate a thread */
void rpthread_exit(void *value_ptr);

/* wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr);

/* initial the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex);

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex);

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex);

static void schedule();

#ifdef USE_RTHREAD
#define pthread_t rpthread_t
#define pthread_mutex_t rpthread_mutex_t
#define pthread_create rpthread_create
#define pthread_exit rpthread_exit
#define pthread_join rpthread_join
#define pthread_yield rpthread_ield
#define pthread_mutex_init rpthread_mutex_init
#define pthread_mutex_lock rpthread_mutex_lock
#define pthread_mutex_unlock rpthread_mutex_unlock
#define pthread_mutex_destroy rpthread_mutex_destroy
#endif

#endif
