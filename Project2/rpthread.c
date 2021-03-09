/* 
 * Group Members Names and NetIDs:
 *   1. Matthew Notaro (myn7)
 *   2. Farrah Rahman (fr258)
 *
 * ILab Machine Tested on:
 *   kill.cs.rutgers.edu
 */
#include "rpthread.h"


// INITIALIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
/* create a new thread */
int create(rpthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);
void* dequeue(queue *inQueue);
void enqueue(queue *inQueue, void* inElement);
void functionCaller();
void* printTest(void*);//remove with function later
void sigHandler(int);
void schedule();
int init();
void* printTest2(void* input);
void* printTest3(void* input);
static void sched_rr();
static void sched_mlfq();


//GLOBALS
//***************************************************
ucontext_t *schedCon = NULL, *mainCon = NULL;
queue RRqueue = {NULL, NULL};
queue MLqueue = {NULL, NULL};
queue blockList = {NULL, NULL}; //not in use
llist threadMap[97] = {NULL}; //not in use, but should be because who knows where terminated threads are going at present
tcb* TCBcurrent;
struct itimerval timer, zeroTimer;
struct sigaction sa;
int tempIds[20] = {0}; //remove later!
//***************************************************

int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	//printf("create\n");
	static int needInit = 0;
	static int maxThreadId = 0; //thread 0 is main
	if(!needInit) {
		needInit = 1;
		init();
	}

	//set up tcb to enqueue
	tcb* TCBtemp = malloc(sizeof(tcb));
	//set input function and args in tcb 
	getcontext(&(TCBtemp->context));
	TCBtemp->context.uc_stack.ss_sp = malloc(STACKSIZE);
	TCBtemp->context.uc_stack.ss_size = STACKSIZE;
	TCBtemp->context.uc_link = schedCon;
	TCBtemp->function=function;
	TCBtemp->input = arg;
	TCBtemp->state = READY;
	makecontext(&TCBtemp->context, functionCaller, 0);
	
	//printf("attempt to disarm timer\n");
	setitimer(ITIMER_REAL, &zeroTimer, NULL); //this line succeeds in disarming
	
	TCBtemp->threadId = ++maxThreadId;
	*thread = TCBtemp->threadId;
	tempIds[TCBtemp->threadId] = 1;
	enqueue(&RRqueue, TCBtemp);
	
	//printf("attempt to arm timer\n");
	setitimer(ITIMER_REAL, &timer, NULL);
	
	//printf("end create\n");

	
	return 0;
}

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {
	/// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	
	
	return 0;
};


/* terminate a thread */
void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	// This rpthread_exit function is an explicit call to the rpthread_exit library to end the pthread that called it.
	// If the value_ptr isn’t NULL, any return value from the thread will be saved. Think about what things you
	// should clean up or change in the thread state and scheduler state when a thread is exiting.
	tcb* TCBcurrent = malloc(sizeof(tcb));

	// Handles context switching back to the current thread's uc_link
	ucontext_t currContext = TCBcurrent->context;
	ucontext_t* link = currContext.uc_link;
	
	if (swapcontext(&uctx_main, &uctx_func2) == -1)
		handle_error("swapcontext");

	// Frees current thread's TCB struct
	free(TCBcurrent);

	// Return 1 on success
	if(value_ptr != NULL)
		*value_ptr = 1;
}; 


/* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {
	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread
	while(tempIds[thread]!=0); //testing only! tempIds won't exist later
	// de-allocate any dynamic memory created by the joining thread - not sure how to do this

	// Invalid thread id
	if(thread < 1){
		return -1;
	}

	// Find thread's tcb from the tid
	// Linearly search thru linked list of all created threads
	tcbList* threadListPtr = list;
	tcb* temp;
	while(threadListPtr != NULL){
		if(threadListPtr->currTCB->tid == thread){
			temp = currTCB;
		}
	}

	while(1){
		if(temp->state == TERMINATED){
			break;
		}
		rpthread_yield();
	}
	// think this works as a simplified version of the above but not as clear
	// while(thread->status != TERMINATED)

	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
                         const pthread_mutexattr_t *mutexattr) {
	/*  //initialize data structures for this mutex
	*(mutex->lock) = 0;
	mutex->list = malloc(sizeof(LinkedList));
	mutex->list.head = NULL;
	// YOUR CODE HERE*/
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquisigHandler mutex fails, push current thread into block list and //  
        // context switch to the scheduler thread
		
      /*  // YOUR CODE HERE
		if(__sync_lock_test_and_set(mutex->lock, 1)==1) { //lock not acquired
			TCBcurrent.state = BLOCKED;
			//add(mutex->list, TCBcurrent);
			swapcontext(TCBcurrent.context, &schedCon);
		}
		*/
		
        return 0;
};


/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	__sync_lock_test_and_set(mutex->lock, 0); //release lock
	
	return 0;
};


/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init

	return 0;
};


/* scheduler */
static void schedule() {
	// Every time when timer interrupt happens, your thread library 
	// should be contexted switched from thread context to this 
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (RR or MLFQ)

	// if (sched == RR)
	//		sched_rr();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE
	//printf("in schedule\n");

// schedule policy
#ifndef MLFQ
	//printf("round robin in schedule\n");
	sched_rr();
	//TCBcurrent = dequeue(RRqueue
#else 
	// Choose MLFQ
     // CODE 2
#endif
	//printf("in schedule\n");

}

/* Round Robin (RR) scheduling algorithm */
static void sched_rr() {
	if(TCBcurrent->state != TERMINATED) {
	//	printf("about to enqueue\n");
		enqueue(&RRqueue, TCBcurrent);
	}
	else {
		//a dequeued thread would get lost here sadly
		//printf("thread terminated: %d\n", TCBcurrent->threadId);
		tempIds[TCBcurrent->threadId] = 0;  
		//^ REMOVE LATER!******************************************************
	}

	tcb* temp = (tcb*)dequeue(&RRqueue);
	if(temp == NULL) {
		return; //see if this breaks anything?
	}
	//printf("wasn't null\n");
	TCBcurrent = temp;
	timer.it_value.tv_usec = 1000;
	//printf("val is %ld\n", timer.it_value.tv_usec);
	setitimer(ITIMER_REAL, &timer, NULL);
//	printf("swapping to %d \n", TCBcurrent->threadId);
	setcontext(&TCBcurrent->context);
	
	//printf("made it here\n");
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

// Feel free to add any other functions you need

void enqueue(queue* inQueue, void* inElement) {
	//is queue empty?
	if(inQueue->head != NULL) {
		//printf("enqueue with old head\n");
		node* temp = malloc(sizeof(node));
		temp->element = inElement;
		temp->next = NULL;
		temp->prev = inQueue->tail;
		inQueue->tail->next = temp;
		inQueue->tail = temp;
	}
	//at least one element exists in queue
	else {
		//printf("enqueue with new head\n");
		inQueue->head = malloc(sizeof(node));
		inQueue->tail = inQueue->head;
		inQueue->head->element = inElement;
		inQueue->head->next = NULL;
		inQueue->head->prev = NULL;
		//remove below later
		/*if(inQueue->head == RRqueue.head)
			printf("successfully modified rr queue\n");
		else {
			printf("inqueue head is %p but rr head is %p\n", inQueue->head, RRqueue.head);
		}*/
	}
	
	
	/* node* temp = inQueue->head;
	printf("queue: ");
	while(temp != NULL) {
		printf("%d ",(int)(((tcb*)(temp->element))->threadId));
		temp = temp->next;
	}
	printf("\n");*/
	
	//printf("successfully enqueued\n");
}

void* dequeue(queue *inQueue) {
	//printf("in dequeue\n");
	if(inQueue->head != NULL) {
		void* temp = inQueue->head->element;
		inQueue->head = inQueue->head->next;
		if(inQueue->head != NULL)
			inQueue->head->prev = NULL;
		return temp;
	}
	//printf("dequeue head is null\n");
	return NULL;
}

void functionCaller() {
	//call thread input function
	//printf("caller: %d\n", TCBcurrent->threadId);
	TCBcurrent->retVal = TCBcurrent->function(TCBcurrent->input);
	TCBcurrent->state = TERMINATED;
}

void* printTest(void* input) {
	
	printf("printing thread %d\n", TCBcurrent->threadId);
	int *x = malloc(sizeof(int));
	while(*x<100000) (*x)++;
	printf("printing thread %d again\n", TCBcurrent->threadId);
	rpthread_t thread;
	rpthread_create(&thread, NULL, printTest2, x);
	rpthread_join(thread, NULL);
	return NULL;
}

void* printTest2(void* input) {
	printf("in printTest2: %d called by %d\n", *(int*)input, TCBcurrent->threadId);
	int test = 17;
	rpthread_t thread;
	rpthread_create(&thread, NULL, printTest3, &test);
	rpthread_join(thread, NULL);
	return NULL;
}

void* printTest3(void* input) {
	printf("in printTest3: %d called by %d\n",  *(int*)input, TCBcurrent->threadId);
	return NULL;
}

void sigHandler(int signum) {
	//printf("caught signal\n");
	swapcontext(&TCBcurrent->context, schedCon);
}




int init() {
	//printf("init\n");
	//register signal handler
	TCBcurrent = malloc(sizeof(tcb));
	TCBcurrent->threadId = 0;
	memset (&sa, 0, sizeof(sigaction));
	sa.sa_handler = &sigHandler;
	sigaction (SIGALRM, &sa, NULL);
	
	//initialize timers
	timer.it_value.tv_usec = 1000; 
	timer.it_value.tv_sec = 0;
	timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 0;
	
	zeroTimer.it_value.tv_usec = 0; 
	zeroTimer.it_value.tv_sec = 0;
	zeroTimer.it_interval.tv_usec = 0; 
	zeroTimer.it_interval.tv_sec = 0;
	
	//create main context
	mainCon = malloc(sizeof(ucontext_t));
	getcontext(mainCon);
	TCBcurrent->context = *mainCon;

	
	//create scheduling context
	schedCon = malloc(sizeof(ucontext_t));
	getcontext(schedCon);
	schedCon->uc_stack.ss_sp = malloc(STACKSIZE);
	schedCon->uc_stack.ss_size = STACKSIZE;
	//schedCon->uc_link = mainCon;
	makecontext(schedCon, schedule, 0);
	setitimer(ITIMER_REAL, &timer, NULL);
	

	
	return 0;
}

int main() {
	rpthread_t thread1 = 0,	thread2 =0, thread3 =0, thread4=0, thread5=0;
	int a = 3;
	int b = 5;
	int c = 10;
	int d = 15;
	int e = 20;
	int x = 0;
	rpthread_create(&thread1, NULL, printTest, &a);
	rpthread_create(&thread2, NULL, printTest2, &b);
	rpthread_create(&thread3, NULL, printTest2, &c);
	rpthread_create(&thread4, NULL, printTest2, &d);
	rpthread_create(&thread5, NULL, printTest2, &e);
	//printf("%d %d %d %d %d\n", thread1, thread2, thread3, thread4, thread5);
	while(x<1000000) x++; //kill some time
	rpthread_join(thread1, NULL);
	rpthread_join(thread2, NULL);
	rpthread_join(thread3, NULL);
	rpthread_join(thread4, NULL);
	rpthread_join(thread5, NULL);
	printf("MAIN RETURNED\n");

	return 0;
}

