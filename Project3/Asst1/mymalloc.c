#include "mymalloc.h"

//the first two bytes of myblock are the metadata storing 4094 free bytes not in use
static char myblock[4096] = {4094>>7, (4094<<1)&255};


void metadata(int inuse,  int size, int index)
{
    short values = (size<<1) + inuse; //values is two byte sequence of metadata. Inuse is 0th bit and following bits store size.
    myblock[index] = values >> 8; //upper metadata byte is stored
    myblock[index+1] = values & 255; //lower metadata byte is stored
}

int sizedata(int index)
{
    int meta = (myblock[index]<<8) + (myblock[index+1]&255); //two bytes are rejoined into metadata sequence
    int size = meta >> 1;
    if(meta & 1) //size is positive if node is free, and negative if in use
        size *= -1; 
    return size;
}


void* mymalloc(int sizeMalloc, char const *file, long line)
{
    
    int index = 0;
    int sizeMemory = sizedata(index);
    while(index<4095 && (sizeMemory < 0 || sizeMemory<sizeMalloc)) //checks metadata in order to find first able to fit desired size
    {
        index+=(abs(sizeMemory)+2);
        sizeMemory = sizedata(index);
    }
	if(sizeMalloc<=0) //desired memory was too small
	{
		return NULL;
	}
    if(index==4096) //desired size was too large to fit in memory
    {
        printf("Insufficient space for request at: %s:%ld\n", file, line);
        return NULL;
    }
    else
    {
        sizeMemory = abs(sizeMemory);
        if(sizeMemory-(sizeMalloc+2) < 3) //current node is size or very near size of desired memory
        {
            metadata(1, sizeMemory, index); //desire memory plus up to 2 bytes to avoid hole is given to user
        }
        else //current node is larger than desired memory
        {
            metadata(1, sizeMalloc, index); //user is given desired memory
            metadata(0, sizeMemory-(sizeMalloc+2), index+sizeMalloc+2); //remaining memory is new free node
        }
        return myblock+index+2; //return pointer to beginning of allocated memory
    }
}

void myfree(void* p, char const *file, long line)
{
    if(p==NULL) return;
    int arrayIndex = 0;
    int prev = arrayIndex;
    int prevFree = -1;
    int pointerIndex = (char*)p-(myblock+2);
    int sizeMemory = sizedata(arrayIndex);
    
    while(arrayIndex<pointerIndex) //checks each memory allocation to see if they match given pointer
    {
        prev = arrayIndex;
        arrayIndex+=(abs(sizeMemory)+2) ;
        if(sizeMemory<0) prevFree = -1;
        else prevFree = 1;
        sizeMemory = sizedata(arrayIndex);
    }
    if(arrayIndex!=pointerIndex || (arrayIndex==pointerIndex && sizedata(arrayIndex)>0)) //pointer is not in memory or doesn't point to beginning of a memory allocation
    {
        printf("Invalid free request at: %s:%ld\n", file, line);
    }
    else
    {
        sizeMemory = abs(sizeMemory);
        if(arrayIndex+sizeMemory+2 < 4093 && sizedata(arrayIndex+sizeMemory+2)>0) //if node after pointer is free, merge nodes
        {
            metadata(0, (sizeMemory + abs(sizedata(arrayIndex+sizeMemory+2))+2), arrayIndex);
        }
        else //free memory
        {
            metadata(0, sizeMemory, arrayIndex);
        }
        if(prevFree == 1) //if node before pointer is free, merge nodes
        {
            metadata(0, (abs(sizedata(prev)) + abs(sizedata(arrayIndex)) + 2), prev);
        }
    }
}
