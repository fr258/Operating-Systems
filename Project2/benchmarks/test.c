#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
//#include "../rpthread.h"
#include <sys/types.h>
#include <sys/syscall.h>

/* A scratch program template on which to call and
 * test rpthread library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */
int main(int argc, char **argv) {
	pid_t threadid = syscall(SYS_gettid);
	struct pthread *pd = (struct pthread *) threadid;

	return 0;
}
