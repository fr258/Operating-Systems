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
void exitMain();
void contextExiter();

//GLOBALS
//***************************************************
ucontext_t *schedCon = NULL;
queue MQueue = {NULL, NULL, NULL, 1};
queue mutexList = {NULL, NULL}; 
tcb* TCBcurrent;
struct itimerval timer, zeroTimer, tempTimer;
struct sigaction sa;
int scheduler; //0 if RR, 1 if MLFQ
tcb *threadId[500] = {NULL};
//***************************************************


int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	static int needInit = 1;
	static int maxThreadId = 0; //thread 0 is main
	if(needInit) {
		needInit = 0;
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
	TCBtemp->priority = 1;
	
	setitimer(ITIMER_REAL, &zeroTimer, &tempTimer); //disarm timer
	
	makecontext(&TCBtemp->context, functionCaller, 0);

	TCBtemp->threadId = ++maxThreadId;
	threadId[TCBtemp->threadId] = TCBtemp; //store thread tcb in array
	*thread = TCBtemp->threadId; //set input val to set thread
	enqueue(&MQueue, TCBtemp); //enqueue in level 1
	
	setitimer(ITIMER_REAL, &tempTimer, NULL); //resume timer

	return 0;
}

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {
	/// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// switch from thread context to scheduler context

	// Get queue corresponding to current thread's priority
	queue* queuePtr = &MQueue;
	while(queuePtr->priority < TCBcurrent->priority){
		queuePtr = queuePtr->next;
	}
	// Enqueue at same level
	enqueue(queuePtr, TCBcurrent);

	// else {
	// 	//a dequeued thread would get lost here sadly
	// 	//printf("thread terminated: %d\n", TCBcurrent->threadId);
	// 	tempIds[TCBcurrent->threadId] = 0;  
	// 	//^ REMOVE LATER!******************************************************
	// }
	
	// setitimer(ITIMER_REAL, &zeroTimer, NULL);
	
	// TCBcurrent->state = READY;
	
	// if(TCBcurrent->priority==1)
	// 	enqueue(&MQueue, temp);
	// else if(TCBcurrent->priority==2)
	// 	enqueue(MQueue.next, temp);
	// else if(TCBcurrent->priority==3)
	// 	enqueue(MQueue.next->next, temp);
	// else
	// 	enqueue(MQueue.next->next->next, temp);

	// swapcontext(&TCBcurrent->context, schedCon);

	return 0;
}


/* terminate a thread */
void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread
	// This rpthread_exit function is an explicit call to the rpthread_exit library to end the pthread that called it.
	// If the value_ptr isn’t NULL, any return value from the thread will be saved. Think about what things you
	// should clean up or change in the thread state and scheduler state when a thread is exiting.

	// Stop timer - thread already exiting, no point in switching out now
	// IDK HOW TO DO THIS
	setitimer(ITIMER_REAL, &zeroTimer, NULL);

	// Set thread's retVal if value_ptr != NULL - used later to be accessed by join
	value_ptr = TCBcurrent->retVal;
	int tempId = TCBcurrent->threadId;
	
	// Always frees context stack
	free(TCBcurrent->context.uc_stack.ss_sp);
	// TCBcurrent->context = NULL;

	TCBcurrent->state = TERMINATED;
	
	if(TCBcurrent->joins == 0) {
		free(TCBcurrent);
	}
	
	threadId[tempId] = NULL;
	

	setcontext(schedCon);
}


/* Wait for thread termination */
// Returns int value of 0 on success, -1 if thread not found
int rpthread_join(rpthread_t thread, void **value_ptr) {
	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread
	// while(tempIds[thread]!=0); //testing only! tempIds won't exist later
	// de-allocate any dynamic memory created by the joining thread - not sure how to do this
	// Invalid thread id
	//setitimer(ITIMER_REAL, &zeroTimer, NULL);
	if(thread < 1 || thread >= 500){
		return -1;
	}
	
	tcb *TCBtemp = threadId[thread];
	
	//thread not found
	if(TCBtemp == NULL) {
		return -1;
	}
	else { //indicate intention to join
		TCBtemp->joins++;
	}
	
	while(TCBtemp->state != TERMINATED); //busy wait for thread to finish
	
	value_ptr = &TCBtemp->retVal;

	

	if(TCBtemp->joins == 1)
		free(TCBtemp);
		
	TCBtemp->joins--;

	
	
/*
	// Linearly search thru linked list of all created threads
	// Can also just search thru all threads in scheduler - don't have to wait for a thread that's not running
	// Assume 2d linked list scheduler since join has to work for both RR and MLFQ.

	// Which queue to use - probably unnecessary if we decide to only use 1 queue for both RR And ML
	queue* level = &MQueue;
	node* temp = NULL;
	tcb* foundTCB = NULL;
	int found = 1;

	// For each level in the queue, go thru each thread on that level
	while(level != NULL && found){
		// For each thread on current level
		temp = level->head;
		while(temp != NULL){
			// Found thread with desired tid
			if((temp->element != NULL) && (((tcb*)(temp->element))->threadId == thread)){
				foundTCB = temp->element;

				// Undefined behavior if more than 1 thread joins on a single thread
				if(foundTCB->joins > 0){
					return -1;
				}

				// Incr number of threads joining on desired thread to 1
				foundTCB->joins++;

				// Use found to break out of outer while loop
				found = 0;
				break;
			}
			temp = temp->next;
		}
		level = level->next;
	}

	// // If thread not found in scheduler, also go thru blocked thread list
	// rpthread_mutex_t* mutexPtr;
	// // Outer loop for each mutex
	// while(mutexPtr != NULL && found){
	// 	// Inner loop for each thread in the mutex's blocklist - reused code from above
	// 	while(temp != NULL){
	// 		// Found thread with desired tid
	// 		if((temp->element != NULL) && ((tcb*)(temp->element)->threadId == thread)){
	// 			foundTCB = temp->element;

	// 			// Undefined behavior if more than 1 thread joins on a single thread
	// 			if(foundTCB->joins > 0){
	// 				return -1;
	// 			}

	// 			// Incr number of threads joining on desired thread to 1
	// 			foundTCB->joins++;

	// 			// Use found to break out of outer while loop
	// 			found = 0;
	// 			break;
	// 		}
	// 		temp = temp->next;
	// 	}
	// 	mutexPtr = mutexPtr->next;
	// }

	// Fail to find a thread with the desired tid in scheduler AND blocked list
	if(foundTCB == NULL){
		return -1;
	}

	// If tid found, loop until desired thread is terminated, at which point we are free to return
	// think this works as a simplified version but not as readable
	// while(foundTCB != NULL && foundTCB->state != TERMINATED)
	while(1){
		if(foundTCB == NULL || foundTCB->state == TERMINATED){
			break;
		}
		// yielding might be needed here since not very efficient to waste time slice
		// pthread_yield();
	}

	// Decr number of threads joining desired thread after joined thread finishes/is freed (shouldn't be freed)
	foundTCB->joins--;

	// Free thread stuff if no more threads are joining it
	if(foundTCB->joins == 0){
		free(foundTCB);
		foundTCB = NULL;
	}
	
	// Pass on exit value
	if(value_ptr != NULL){
		*value_ptr = foundTCB->retVal;
	}
	*/
	return 0;
}

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
                         const pthread_mutexattr_t *mutexattr) {
	 //initialize data structures for this mutex

	mutex->lock = malloc(sizeof(int*));
	*(mutex->lock) = 0;
	mutex->blockList = malloc(sizeof(queue));
	mutex->blockList->head = NULL;
	mutex->locker = -1;
	enqueue(&mutexList, mutex);
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquisigHandler mutex fails, push current thread into block list and //  
        // context switch to the scheduler thread
		getcontext(&TCBcurrent->context);
		if(__sync_lock_test_and_set(mutex->lock, 1)==1) { //lock not acquired
			//printf("lock not acquired\n");
			TCBcurrent->state = BLOCKED;
			setitimer(ITIMER_REAL, &zeroTimer, &tempTimer); //disarm timer for enqueueing
			enqueue(mutex->blockList, TCBcurrent);
			setcontext(schedCon);
			return 0;
		}
		else {
			mutex->locker = TCBcurrent->threadId;	
		}
        return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	if(mutex->locker != TCBcurrent->threadId) //nonlocking thread is attempting to free
		return -1;
		
	__sync_lock_test_and_set(mutex->lock, 0); //release lock
	setitimer(ITIMER_REAL, &zeroTimer, &tempTimer); //pause timer
	tcb* TCBtemp;
	queue* queuePtr = &MQueue;
	//printf("unlock\n");
	while((TCBtemp = (tcb*)dequeue(mutex->blockList)) != NULL) {
		// Get queue corresponding to current thread's priority
		TCBtemp->state = READY;
		queuePtr = &MQueue;
		//printf("queue:%d current:%d\n",queuePtr->priority,TCBcurrent->priority );
		while(queuePtr->priority < TCBcurrent->priority){
			queuePtr = queuePtr->next;
		}
		// Enqueue at same level
		enqueue(queuePtr, TCBtemp);
	}
	//printf("unlocked\n");
	setitimer(ITIMER_REAL, &tempTimer, NULL); //resume timer
	
	return 0;
};



/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init
	tcb* temp;
	while((temp = (tcb*)dequeue(mutex->blockList)) != NULL); //destroy blocklist, doesn't set threads to ready
	free(mutex->blockList);
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
	// #ifndef MLFQ
	// 	//printf("round robin in schedule\n");
	// 	sched_rr();
	// 	//TCBcurrent = dequeue(RRqueue
	// #else 
	// 	// Choose MLFQ
	// 	// CODE 2
	// #endif
	// 	//printf("in schedule\n");

	// from RR
	// Dequeue from highest non-empty priority queue 
	queue* queuePtr = &MQueue;
	tcb* temp = NULL;
	// Keep dequeueing from ptr until a non-null tcb is returned
	/*tcb* temp = (tcb*)dequeue(queuePtr);
	while(queuePtr!=NULL && temp == NULL){
		temp = (tcb*)dequeue(queuePtr);
		queuePtr = queuePtr->next;
	}*/
	while(queuePtr != NULL) {
		if((temp = dequeue(queuePtr)) != NULL)
			break;
		else
			queuePtr = queuePtr->next; //check lower priority queue	
	}

	
	// // Keep dequeueing next node until a non-blocked node is returned (obselete)
	// while(temp != NULL && temp->state == BLOCKED) {
	// 	enqueue(&RRqueue, temp); //return blocked context to queue
	// 	temp = (tcb*)dequeue(&RRqueue); //try next context in list
	// }

	// Would only happen when there are no threads in the queue
	if(temp == NULL) {
		return; //see if this breaks anything?
	}
	//printf("wasn't null\n");
	TCBcurrent = temp;
	
	TCBcurrent->state = RUNNING;
	//printf("end schedCon\n");
	setitimer(ITIMER_REAL, &timer, NULL);
	setcontext(&TCBcurrent->context);
}

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
}

void* dequeue(queue *inQueue) {
	//printf("in dequeue\n");
	if(inQueue->head != NULL) {
		void* temp = inQueue->head->element;
		node* tempNode = inQueue->head;
		inQueue->head = inQueue->head->next;
		free(tempNode);
		if(inQueue->head != NULL)
			inQueue->head->prev = NULL;
		return temp;
	}
	return NULL;
}

void functionCaller() {
	//call thread input function
	//printf("caller: %d\n", TCBcurrent->threadId);
	TCBcurrent->retVal = TCBcurrent->function(TCBcurrent->input);
	TCBcurrent->state = TERMINATED;
	setitimer(ITIMER_REAL, &zeroTimer, NULL); //pause timer, returning to scheduler
}

void sigHandler(int signum) {
	//printf("caught signal: %d\n", TCBcurrent->threadId);
	// Enqueue in next level - for RR and ML last queue, next level is current level
	queue* queuePtr = &MQueue;
	int counter = 1;
	while(queuePtr->next != NULL && counter < TCBcurrent->priority+1){
		counter++;
		queuePtr = queuePtr->next;
	}
	// Update thread's priority to prio+1(move down) or prio (same level)
	TCBcurrent->priority = counter;
	
	// Enqueue thread in respective priority queue and swap context back to scheduler for next thread
	enqueue(queuePtr, TCBcurrent);
	swapcontext(&TCBcurrent->context, schedCon);
}

int init() {
	
	//set up schedule queue(s)
	#ifdef MLFQ
		//priority level 2
		MQueue.next = malloc(sizeof(queue)); 
		MQueue.next->priority = 2;
		MQueue.next->head = NULL;
		MQueue.next->tail = NULL;
		
		//priority level 3
		MQueue.next->next = malloc(sizeof(queue)); 
		MQueue.next->next->priority = 3;
		MQueue.next->next->head = NULL;
		MQueue.next->next->tail = NULL;
		
		//priority level 4
		MQueue.next->next->next = malloc(sizeof(queue)); 
		MQueue.next->next->next->priority = 4;
		MQueue.next->next->next->head = NULL;
		MQueue.next->next->next->tail = NULL;
		MQueue.next->next->next->next = NULL;
		
	#endif
	
	//register signal handler
	TCBcurrent = malloc(sizeof(tcb));
	TCBcurrent->threadId = 0;
	TCBcurrent->state = RUNNING;
	memset (&sa, 0, sizeof(sigaction));
	sa.sa_handler = &sigHandler;
	sigaction (SIGALRM, &sa, NULL);
	
	//initialize timers
	timer.it_value.tv_usec = 1000*TIMESLICE; 
	timer.it_value.tv_sec = 0;
	timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 0;
	
	zeroTimer.it_value.tv_usec = 0; 
	zeroTimer.it_value.tv_sec = 0;
	zeroTimer.it_interval.tv_usec = 0; 
	zeroTimer.it_interval.tv_sec = 0;
	
	tempTimer.it_value.tv_usec = 0; 
	tempTimer.it_value.tv_sec = 0;
	tempTimer.it_interval.tv_usec = 0; 
	tempTimer.it_interval.tv_sec = 0;
	
	//create main context
	getcontext(&TCBcurrent->context);
	
	//create scheduling context
	schedCon = malloc(sizeof(ucontext_t));
	getcontext(schedCon);
	schedCon->uc_stack.ss_sp = malloc(STACKSIZE);
	schedCon->uc_stack.ss_size = STACKSIZE;
	//schedCon->uc_link = mainCon;
	makecontext(schedCon, schedule, 0);
	
	atexit(exitMain);
	setitimer(ITIMER_REAL, &timer, NULL);
	
	return 0;
}




void exitMain() {
	//stop timer
	setitimer(ITIMER_REAL, &zeroTimer, NULL); 
	//free main context
	if(TCBcurrent != NULL) {
		free(TCBcurrent->context.uc_stack.ss_sp); 
		free(TCBcurrent);
	}
	queue* queuePtr = &MQueue;
	tcb *TCBtemp;
	//free multilevel queues
	#ifdef MLFQ
		//free contexts that might still exist at time of main's return
		while(queuePtr != NULL){ 
			while((TCBtemp = dequeue(queuePtr)) != NULL) {
				free(TCBtemp->context.uc_stack.ss_sp);
				free(TCBtemp);
			}
			queuePtr = queuePtr->next;
		}
		free(MQueue.next->next->next); //free priority 4 queue
		free(MQueue.next->next); //free priority 3 queue
		free(MQueue.next); //free priority 2 queue
		
	#else 
	//free RR queue's contexts that might still exist at time of main's return
		while((TCBtemp = dequeue(queuePtr)) != NULL) {
			free(TCBtemp->context.uc_stack.ss_sp);
			free(TCBtemp);
		}
	#endif
	free(schedCon->uc_stack.ss_sp); 
	free(schedCon); 
}










