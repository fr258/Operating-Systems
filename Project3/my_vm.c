#include "my_vm.h"


void* vMap = NULL;
void* pMap = NULL;
void* pageDir = NULL;
void* totalMem = NULL;

int numPages;
int offsetBits;
int pageBits;
int dirBits;

int TLBsize;

int init = 0;

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {
    if(init){
        return;
    }
    init = 1;
    
    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    totalMem = (void*)malloc(MEMSIZE);

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    
    // CEIL OR FLOOR FOR DIVISION???
    numPages = MEMSIZE / PGSIZE;

    // Init virtual and physical bitmaps
    vMap = (void*)malloc(numPages);
    pMap = (void*)malloc(numPages);

    memset(vMap, 0, numPages * sizeof(pte_t));
    memset(pMap, 0, numPages * sizeof(pte_t));

    // Init page directory
    offsetBits = (int)log(PGSIZE)/log(2);
    pageBits = (32 - offsetBits) / 2;
    dirBits = 32 - pageBits - pageBits;
    pageDir = (void*)malloc(pow(2,dirBits) * sizeof(pde_t));
    memset(pageDir, 0, pow(2,dirBits) * sizeof(pde_t));
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    unsigned long virAdd = (long)va;
    unsigned long virAddOffset = (virAdd<<(dirBits+pageBits)) >> (dirBits+pageBits);

    // Page directory index
    unsigned long pageDirIndex = virAdd >> (offsetBits+pageBits);
    // Page table index
    unsigned long pageTabIndex = (virAdd<<dirBits) >> (pageBits + offsetBits);

    // Check page directory
    unsigned char *pgByte = (char*)pageDir + pageDirIndex;
    // Page directory index DNE
    if(!(*pgByte & (1 << pageDirIndex))){
        return NULL;
    }

    // Check page table
    pde_t *pdEntry = pgdir + pageDirIndex;

    pte_t *ptEntry = (pte_t*)((*pdEntry >> offsetBits) * PGSIZE) + pageTabIndex;

    // Successful
    return (pte_t*)(*ptEntry + virAddOffset);
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
	
	int currPDIndex = va >> (32 - dirBits); //extract directory bits to get PD index
	
	int currPTIndex = (va >> (32 - pageBits)) & (2^pageBits - 1); //extract page bits to get PT index
	
	int vMapEntry = currPDIndex * (2^pageBits - 1) + currPTIndex;
	int vMapOffset = vMapEntry % 8;
	
	if(0 == (*(vMap + (vMapEntry / 8)) >> vMapOffset)) {
	
		pte_t *currPTEntry = *(pgdir + currPDIndex) + currPTIndex;
		
		if(*currPTEntry == 0) {
			currPTEntry = pa;
			
			*(vMap + (vMapEntry / 8)) |= 1 << vMapOffset;
			
			return 0;
		}
		
	}
	
    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page

}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *a_malloc(unsigned int num_bytes) {
    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */
    if(!init){
        set_physical_mem();
    }

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    return NULL;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     */

    long startAdd = (long)va;
    long endAdd = ((long)va + size);

    long startOffset = startAdd >> offsetBits;
    long endOffset = endAdd >> offsetBits;

    unsigned long index = startOffset % TLB_ENTRIES;
    char *byte;

    // Make sure this is a valid region of virtual memory
    unsigned long i, j;

    for(i = 0; i < size; i++){
        unsigned long virIndex = (startAdd>>offsetBits) + i;
        byte = (char*)vMap + virIndex;
        for(j = 0; j < 8; j++){
            char bit = *byte << j;
            bit = bit >> (7-j);
            if((i < size) && (bit & 1 == 0)){
                printf("Memory not allocated");
                return;
            }
        }
        i++;
    }

    /* 
     * Part 2: Also, remove the translation from the TLB
     */
    
    if(tlb_store.entries[index] != NULL && tlb_store.entries[index]->va == startAdd){
        tlb_store.entries[index]->pa = NULL;
        tlb_store.entries[index]->va = NULL;
    }



}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */

    long startAdd = (long)va;

    // Make sure this is a valid region of virtual memory
    
    unsigned long i, j;
    char *byte;
    for(i = 0; i < size; i++){
        unsigned long virIndex = (startAdd>>offsetBits) + i;
        byte = (char*)vMap + virIndex;
        for(j = 0; j < 8; j++){
            char bit = *byte << j;
            bit = bit >> (7-j);
            if((i < size) && (bit & 1 == 0)){
                printf("Memory not allocated");
                return;
            }
        }
        i++;
    }
    
    // Makes the copy from val to va
    void* physAdd, *va2;
    for(i = 0; i < size; i++){
        va2 = startAdd + i;
        physAdd = translate(pageDir, va2);
        *(char*)physAdd = *(char*)val;
        val = (char*)val + 1;
    }
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

    long startAdd = (long)va;

    // Make sure this is a valid region of virtual memory
    
    unsigned long i, j;
    char *byte;
    for(i = 0; i < size; i++){
        unsigned long virIndex = (startAdd>>offsetBits) + i;
        byte = (char*)vMap + virIndex;
        for(j = 0; j < 8; j++){
            char bit = *byte << j;
            bit = bit >> (7-j);
            if((i < size) && (bit & 1 == 0)){
                printf("Memory not allocated");
                return;
            }
        }
        i++;
    }

    // Makes the copy from va to val
    void* physAdd, *va2;
    for(i = 0; i < size; i++){
        va2 = startAdd + i;
        physAdd = translate(pageDir, va2);
        *(char*)val = *(char*)physAdd;
        val = (char*)val + 1;
    }
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int i, j, k, sum;
    int *mat1Val, *mat2Val, *ansVal;

    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            *ansVal = 0;
            for(k = 0; k < size; k++){
                get_value((int*)mat1 + (i*size) + k, (void*)mat1Val, sizeof(int));
                get_value((int*)mat2 + (k*size) + j, (void*)mat2Val, sizeof(int));
                *ansVal = *mat1Val * *mat2Val;
            }
            put_value((int*)answer + (i*size) + j, (void*)ansVal, sizeof(int));
        }
    }
}




////////////////////
////// Part 2 //////
////////////////////
/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */

int counter = 0;
int
add_TLB(void *va, void *pa)
{
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

    tlb_store.entries[counter]->va = va;
    tlb_store.entries[counter]->pa = pa;
    
    counter = (counter+1) % TLB_ENTRIES;

    return 0;
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */

    int i;
    for(i = 0; i < TLB_ENTRIES; i++){
        if(tlb_store.entries[i] != NULL && tlb_store.entries[i]->va == va){
            return tlb_store.entries[i]->pa;
        }
    }

    return -1;
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */

unsigned long missCount;
unsigned long hitCount;
unsigned long referenceCount;

void
print_TLB_missrate()
{
    double miss_rate = missCount/referenceCount;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}
