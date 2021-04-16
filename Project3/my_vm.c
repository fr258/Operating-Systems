#include "my_vm.h"

char* vMap = NULL;
char* pMap = NULL;
char* pageDir = NULL;


int init = 0;
char* totalMem = NULL;
int offsetBits = 0;
int pageBits = 0;
int dirBits = 0;
int numPTE = 0;
int numPDE = 0;
int entriesPerPT = 0;

//TLB
unsigned int oldestEntry = 0;
unsigned int size = 0;
unsigned int missCount = 0;
unsigned int totalCalls = 0;

pte_t *check_TLB(void *va);
/*
Function responsible for allocating and setting your physical memory
*/
void set_physical_mem() {
    if(init){
        return;
    }
    init = 1;

    offsetBits = log(PGSIZE);
	pageBits = (int)ceil((32-offsetBits)/2.0);
	dirBits = 32 - offsetBits - pageBits;
	numPTE = MEMSIZE/(PGSIZE + 4 + pow(2, 2-pageBits));
	entriesPerPT = pow(2, pageBits);
	numPDE = (int)ceil(numPTE/entriesPerPT);

	totalMem = malloc(MEMSIZE);
	vMap = malloc(numPTE/8);
	pMap = malloc(numPTE/8);
	pageDir = totalMem + MEMSIZE - (4*numPDE + 4*numPDE*numPTE);

	memset(vMap, 0, numPTE/8);
	memset(pMap, 0, numPTE/8);

    // TLB init
    tlb_store.entries = (struct tlbEntry**)malloc(sizeof(struct tlbEntry*) * TLB_ENTRIES);
    int i;
    for(i = 0; i < TLB_ENTRIES; i++){
        tlb_store.entries[i] = (struct tlbEntry*)malloc(sizeof(struct tlbEntry));
    }
}


pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

	unsigned long PDE = ((unsigned long)va) >> (32 - dirBits);
	unsigned long PTE =  (((unsigned long)va) >> offsetBits) & (unsigned long)(pow(2, pageBits)-1);

    unsigned long virAdd = (unsigned long)va;
    unsigned long virPage = virAdd >> offsetBits;
    unsigned long virAddOffset = (virAdd<<(dirBits+pageBits)) >> (dirBits+pageBits);

    // Check TLB - sequential search TLB (no hashing)
    unsigned long tlbIndex;
	
	pte_t *physAddr = check_TLB((void*)va);

	totalCalls++;
		char* physAdd;
	if(physAddr != NULL)	return physAddr;
	

	else {
		physAdd = (char*)((pageDir) + 4*(numPDE + entriesPerPT*PDE + PTE));
		add_TLB(va, physAdd);
		missCount++;
	}
	
   return (pte_t*)physAdd;
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
	unsigned long currPDE = ((unsigned long)va) >> (32 - dirBits);
	unsigned long currPTE = (((unsigned long)va) >> offsetBits) & (unsigned long)(pow(2, pageBits)-1);
	//beginning of current PT
	char* PDEaddress = pageDir + (numPDE * 4) + (currPDE * entriesPerPT) * 4; //4 bytes per entry
	int mappingExists = 0;
	for(int i = 0; i < numPDE; i++) {
		if(*(pageDir + i*4) != 0) {//current PDE entry has mapping
			if(*(unsigned long*)(pageDir + i*4) == PDEaddress) {
				mappingExists = 1;
				break;
			}
		}
	}
	if(!mappingExists) {
		*(unsigned long*)(pageDir + currPDE*4) = PDEaddress;
	}
	*(char*)(PDEaddress + 4*currPTE) = pa;
	return 1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
	unsigned int firstEntry = -1;
	unsigned int firstPDE = -1;
	char current;
	int pageCount = 0;
	nextContent *retVal = malloc(sizeof(nextContent));
	
	//traverse virtual bitmap
	for(unsigned long currByte = 0; currByte < numPTE/8; currByte++) {
		for(int i = 0; i < 8; i++) {
			current = *(char*)(vMap+currByte);
			current <<= i;
			current >>= 7;
			
			if(current&1 == 1) {
				firstEntry = -1;
				pageCount = 0;
			}
			else {
				//printf("found available in  
				pageCount++;
				if(firstEntry == -1) {
					firstEntry = currByte*8 + i;
				}
				if(pageCount == num_pages) {
					unsigned long *PHYS = malloc(sizeof(unsigned long) * num_pages);
					//traverse physical bitmap
					pageCount = 0;
					for(unsigned long PcurrByte = 0; PcurrByte < numPTE/8; PcurrByte++) {
						for(int j = 0; j < 8; j++) {
							current = *(char*)(pMap+PcurrByte);
							current <<= j;
							current >>= 7;
							
							if((char)(current&1) == (char)0) {
								PHYS[pageCount] = PcurrByte*8 + j;
								pageCount++;
							}
							if(pageCount == num_pages) {
								retVal->PTE = firstEntry;
								retVal->PHYS = PHYS;
								return retVal;
							}
						}
					}
					free(PHYS);
					retVal->PTE = -1;
					return retVal;
				}
			}
		}
	}
	retVal->PTE = -1;
	return retVal;
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
	unsigned long numPages = num_bytes / PGSIZE;
	if(num_bytes % PGSIZE != 0) numPages++; //round up
	
	nextContent next = *(nextContent*)get_next_avail(numPages);


	//there are enough free pages
	if(next.PTE != -1) {
		unsigned long VA = 0;

		unsigned long PDE = next.PTE/entriesPerPT;
		int PTE = next.PTE%entriesPerPT;

		char VbitTracker = next.PTE % 8;
		
		for(int i = 0; i < numPages; i++) {
			VA = 0;
			PTE++;
			
			if(PTE > entriesPerPT) {
				PTE = 0;
				PDE++;
			}
			VA |= PDE << (32 - dirBits); //set PD bits
			VA |= PTE << (offsetBits); //set PT bits
			

			//map physical to virtual
			page_map(pageDir, (void*)VA, totalMem + next.PHYS[i]*4);

			//set virtual bitmap
			*(char*)(vMap+(next.PTE+i)/8) |= (char)(1 << (7-VbitTracker));
			
			//set physical bitmap
			*(char*)(pMap+next.PHYS[i]/8) |= (char)(1 << (7-next.PHYS[i]%8));
			
			VbitTracker = (VbitTracker+1) % 8;
		}
		VA = 0;
		VA |= (next.PTE/entriesPerPT) << (32 - dirBits); //set PD bits
		VA |= (next.PTE%entriesPerPT) << (offsetBits); //set PT bits
		return VA;
	}
    return -1;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {
    pthread_mutex_lock(&mutex);
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
                pthread_mutex_unlock(&mutex);
                return;
            }
        }
        i++;
    }
	
	//printf("after checks\n");
	
	unsigned long numPages = size/ PGSIZE;
	if(size % PGSIZE != 0) numPages++; //round up
	
	unsigned long PDE = ((unsigned long)va) >> (32 - dirBits);
	unsigned long PTE =  (((unsigned long)va) >> offsetBits) & (unsigned long)(pow(2, pageBits)-1);
	
	unsigned long vMapIndex = PDE * entriesPerPT + PTE;

	char VbitTracker = vMapIndex % 8;
	
	for(int i = 0; i < numPages; i++) {
		//map physical to virtual
		//page_map(pageDir, (void*)VA, totalMem + next.PHYS[i]*4);

		//set virtual bitmap
		*(char*)(vMap+(vMapIndex+i)/8) -= (char)(1 << (7-VbitTracker));
		
		unsigned long currPhys = *(unsigned long*)(pageDir + 4*numPDE + 4*entriesPerPT*PDE + 4*PTE);
		
		unsigned long currPhysIndex = (currPhys - (unsigned long)(totalMem))/PGSIZE;
		//set physical bitmap
		*(char*)(pMap+currPhysIndex/8) -= (char)(1 << (7-currPhysIndex%8));
		VbitTracker = (VbitTracker+1) % 8;
	}
	

    /* 
     * Part 2: Also, remove the translation from the TLB
     */
<<<<<<< HEAD
    int tlbIndex;
    for(tlbIndex = 0; tlbIndex < TLB_ENTRIES; tlbIndex++){
        if(tlb_store.entries[tlbIndex] != NULL && tlb_store.entries[tlbIndex]->va == startAdd){
            tlb_store.entries[index]->pa = (unsigned long)NULL;
            tlb_store.entries[index]->va = (unsigned long)NULL;
        }
=======
    /*
    if(tlb_store.entries[index] != NULL && tlb_store.entries[index]->va == startAdd){
        tlb_store.entries[index]->pa = (unsigned long)NULL;
        tlb_store.entries[index]->va = (unsigned long)NULL;
>>>>>>> bb90d30020158da88f094d226939b805c373d33e
    }
   
	*/
	pthread_mutex_unlock(&mutex);
}



/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void *va, void *val, int size) {
    pthread_mutex_lock(&mutex);
    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
/*
	unsigned long PDE = ((unsigned long)va) >> (32 - dirBits);
	unsigned long PTE =  (((unsigned long)va) >> offsetBits) & (unsigned long)(pow(2, pageBits)-1);
	printf("PDE is %lu\n", PDE);
	printf("PTE is %lu\n", PTE);
	printf("va is %lu\n", (unsigned long)va);
*/
    unsigned long startAdd = (unsigned long)va;

    // Make sure this is a valid region of virtual memory
    unsigned long i, j;
    char *byte;
    for(i = 0; i < size; i++){
        unsigned long virIndex = (startAdd>>offsetBits) + i;
		//printf("this line\n");
        byte = (char*)vMap + virIndex;
		//printf("end line\n");
        for(j = 0; j < 8; j++){
		//	printf("this far\n");
            char bit = *byte << j;
			//printf("end this far\n");
            bit = bit >> (7-j);
            if((i < size) && (bit & 1 == 0)){
               // printf("Memory not allocated\n");
                pthread_mutex_unlock(&mutex);
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
    pthread_mutex_unlock(&mutex);
}

/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

    long startAdd = (long)va;
    // long endAdd = ((long)va + size);

    // long startOffset = startAdd >> offsetBits;
    // long endOffset = endAdd >> offsetBits;

    // Makes the copy from va to val
    int i;
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
	mat1Val = malloc(sizeof(int));
	mat2Val = malloc(sizeof(int));
	ansVal = malloc(sizeof(int));
    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            *ansVal = 0;
            for(k = 0; k < size; k++){
                get_value((int*)mat1 + (i*size) + k, (void*)mat1Val, sizeof(int));
                get_value((int*)mat2 + (k*size) + j, (void*)mat2Val, sizeof(int));
                *ansVal += *mat1Val * *mat2Val;
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
	if(size < TLB_ENTRIES) {
		int i = 0;
		while(i < TLB_ENTRIES) {
			if(tlb_store.entries[i]->va == 0) {
				
				tlb_store.entries[i]->va = (unsigned long)va;
				
				tlb_store.entries[i]->pa = (unsigned long)pa;
				size++;
			}
			i++;
		}
	}
	else {
		tlb_store.entries[oldestEntry]->va = (unsigned long)va;
		tlb_store.entries[oldestEntry]->pa = (unsigned long)pa;
		oldestEntry = (++oldestEntry)%TLB_ENTRIES;
	}
    return 1;
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {
	totalCalls++;
    for(int i = 0; i < TLB_ENTRIES; i++) {
		if(tlb_store.entries[i]->va == (unsigned long)va) {
			return (pte_t*)tlb_store.entries[i]->pa;
		}
	}
	missCount++;
	return NULL;
}

/*
 * Part 2: Print TLB miss rate.
 * Calculates and prints the TLB miss rate
 */
void
print_TLB_missrate()
{
    /*Part 2 Code here to */
    fprintf(stderr, "TLB miss rate %lf \n", (missCount * 1.0f / totalCalls));
}
