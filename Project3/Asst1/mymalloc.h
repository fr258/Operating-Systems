//#ifndef MYMALLOC_H_
//#define MYMALLOC_H_

#include <stdio.h>	
#include <stdlib.h>	

#define malloc( x ) mymalloc( x, __FILE__, __LINE__ )
#define free( x ) myfree( x, __FILE__, __LINE__ )

void metadata(int inuse,  int size, int index); 
/*
	Helper function called by mymalloc
	Precondition: inuse is 0 or 1, size is <= 4094, index is <= 4093
	Inserts metadata into array given whether or not the data is in use, the size of the allocation, and the index of the metadata
*/

int sizedata(int index);
/*
	Helper function called by mymalloc and myfree
	Precondition: Index points to the beginning of a metadata token
	Returns the size of the memory the metadata token indicates
	If the data indicated by the metadata is free, positive size is returned
	If the data indicated by the metadata is in use, negative size is returned
*/


void* mymalloc(int size, char const *file, long line);
/*
	This will attempt to allocate and return pointer to memory of given size
	If there is insufficient space for desired size, malloc is failed and error message is printed
	If there is enough space for desired space, void pointer to beginning of space is returned
*/

void myfree(void* p, char const *file, long line);
/*
	This will attempt to free the space allocated by the pointer passed, and assumes that p is a pointer
	If the pointer is not within the array, or not a pointer previously returned by mymalloc, freeing will fail and error message is printed
	
*/


//#endif
