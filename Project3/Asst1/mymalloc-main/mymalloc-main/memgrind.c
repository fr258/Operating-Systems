#include "mymalloc.h"
#include <sys/time.h>

int timePassed(struct timeval* time_before, struct timeval* time_after);

int main(int argc, char *argv[])
{
	struct timeval time_before, time_after;

	float timeA = 0;
	float timeB = 0;
	float timeC = 0;
	float timeD = 0;
	float timeE = 0;
	
	for(int b = 0; b<50; b++)
	{	
		//PART A
		gettimeofday(&time_before, NULL);
		
		char* temp;
		for(int i=0; i<50; i++)
		{
			temp = (char*)malloc(1);
			free(temp);
		}
		gettimeofday(&time_after, NULL);
		
		timeA += timePassed(&time_before, &time_after);	
		
		
		//PART B
		gettimeofday(&time_before, NULL);
		char* arrayB[120];
		
		for(int i=0; i<120; i++)
		{
			arrayB[i] = (char*)malloc(1);
		}

		for(int i=0; i<120; i++)
		{
			free(arrayB[i]);
		}
		gettimeofday(&time_after, NULL);
		
		timeB += timePassed(&time_before, &time_after);	
		
		
		//PART C
		gettimeofday(&time_before, NULL);
		
		char* arrayC[120];
		int random;
		int arrIndex = 0;
		int mallocCount = 0;
		
		for(int i=0; i<240; i++)
		{
			random = rand()%2;
			if((random==0 && arrIndex>0) ||  (mallocCount == 120) || (random==1 && arrIndex>=120)) //will free
			{
				free(arrayC[arrIndex-1]);
				arrIndex--;
			}
			else //will malloc
			{
				arrayC[arrIndex] = (char*)malloc(1);
				arrIndex++;
				mallocCount++;
			}
		}

		gettimeofday(&time_after, NULL);
		
		timeC += timePassed(&time_before, &time_after);	
			
		
		//PART D
		//Malloc and free a data whose size increases by 1 each time and show that anything over 4094 fails (as opposed to 4096)
		gettimeofday(&time_before, NULL);
		char* pointer = NULL;	
		for(int i = 4094-120; i < 4094; i++) 
		{
			pointer = (char*)malloc(i);
			free(pointer);
		}
		gettimeofday(&time_after, NULL);
		
		timeD += timePassed(&time_before, &time_after);	
		
		
		//PART E
		gettimeofday(&time_before, NULL);
		char* arrayE[128];
		for(int i = 0; i<128; i++)
		{
			arrayE[i] = (char*)malloc(30);
		}
		
		for(int i = 0; i<128; i+=2)
		{
			free(arrayE[i]);
		}

		for(int i = 1, n = 3; i<128-1; i+=2, n+=2)
		{
			free(arrayE[i]);
			arrayE[0] = malloc(32*n-2);
			free(arrayE[0]);
		}
		
		free(arrayE[127]);
		arrayE[0] = malloc(4094);
		free(arrayE[0]);
		
		gettimeofday(&time_after, NULL);
		timeE += timePassed(&time_before, &time_after);	
		
	}

	timeA /= 50;
	timeB /= 50;
	timeC /= 50;
	timeD /= 50;
	timeE /= 50;
	
	printf("Mean time for Workload A: %6.2f microseconds\n", timeA);
	printf("Mean time for Workload B: %6.2f microseconds\n", timeB);
	printf("Mean time for Workload C: %6.2f microseconds\n", timeC);
	printf("Mean time for Workload D: %6.2f microseconds\n", timeD);
	printf("Mean time for Workload E: %6.2f microseconds\n", timeE);

	return EXIT_SUCCESS;
}

int timePassed(struct timeval* time_before, struct timeval* time_after)
{
	return (time_after->tv_sec- time_before->tv_sec)*1000000 + time_after->tv_usec - time_before->tv_usec;	
}	
