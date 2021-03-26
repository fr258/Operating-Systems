#include "mymalloc.h"

static char myblock[4096] = {4094>>7, (4094<<1)&255};



void metadata(int inuse,  int size, int index)
{
    short values = (size<<1) + inuse;
    myblock[index] = values >> 8;
    myblock[index+1] = values & 255;
}

int sizedata(int index)
{
    int meta = (myblock[index]<<8) + (myblock[index+1]&255);
    int size = meta >> 1;
    if(meta & 1 == 1)
        size *= -1;
    return size;
}

void printTest()
{
    int i = 0;
    while(i<4095)
    {   
        int s = sizedata(i);
        int size = abs(s);
        int use = 0;
        if(s<0) use = 1;
        printf("Node of size %d, use status: %d\n", size, use);
        i+=(size+2);
    }
}

void* mymalloc(int size, char const *file, long line)
{
    
    int i = 0;
    int s = sizedata(i);
    while(i<4095 && (s < 0 || s<size))
    {
        i+=(abs(s)+2);
        s = sizedata(i);
    }
    if(i==4096 || size<=0)
    {
        printf("Insufficient space for request at: %s:%ld\n", file, line);
        return NULL;
    }
    else
    {
        s = abs(s);
        if(s-(size+2)<3)
        {
            metadata(1, s, i);
        }
        else
        {
            metadata(1, size, i);
            metadata(0, s-(size+2), i+size+2);
        }
        return myblock+i+2;
    }
}

void myfree(void* p, char const *file, long line)
{
    if(p==NULL) return;
    int i = 0;
    int prev = i;
    int prevFree = -1;
    int pointerIndex = (char*)p-(myblock+2);
    int s = sizedata(i);
    
    while(i<pointerIndex)
    {
        prev = i;
        i+=(abs(s)+2) ;
        if(s<0) prevFree = -1;
        else prevFree = 1;
        s = sizedata(i);
    }
    if(i!=pointerIndex || (i==pointerIndex && sizedata(i)>0))
    {
        printf("Invalid free request at: %s:%ld\n", file, line);
    }
    else
    {
        s = abs(s);
        if(i+s+2 < 4093 && sizedata(i+s+2)>0)
        {
            metadata(0, (s+abs(sizedata(i+s+2))+2), i);
        }
        else
        {
            metadata(0, s, i);
        }
        if(prevFree == 1)
        {
            metadata(0, (abs(sizedata(prev)) + abs(sizedata(i)) + 2), prev);
        }
    }
}
/*
int main(int argc, char *argv[])
{
    int* test1 = (int*)malloc(4000);
    int* test2 = (int*)malloc(200);
    int* test3 = (int*)malloc(300);
    int* test4 = (int*)malloc(400);


    printTest();
    return EXIT_SUCCESS;
}*/