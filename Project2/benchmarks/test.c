#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../rpthread.h"
#include <sys/types.h>
#include <sys/syscall.h>

/* A scratch program template on which to call and
 * test rpthread library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

void* test (void* arg){
	int value = 100;
	int* ret = malloc(sizeof(int));
	*ret = value;

	printf("Thread res: %d\n", *ret);
	printf("Thread addy: %p\n", ret);
	//return (void*) ret;
	rpthread_exit((void*)ret);
}

int main(int argc, char **argv) {
	//pid_t threadid = syscall(SYS_gettid);
	//struct pthread *pd = (struct pthread *) threadid;
	int x = 4;
	int* res = &x;
	rpthread_t* thread;
	
	printf("Main res: %d\n", *res);
	printf("Main addy: %p\n", res);

	rpthread_create(thread, NULL, test, NULL);
	int run1=0, run2=0;
	while(run1<10000000){
		while(run2<100000000){
			run2++;
		}
		run1++;
	}
	rpthread_join(thread, (void**) &res);

	printf("Main res: %d\n", *res);
	printf("Main addy: %p\n", res);
	//free(res);
	return 0;
}