#include "my_vm.h"

char* vMap = NULL;
char* pMap = NULL;
char* pageDir = NULL;
char* totalMem = NULL;

int init = 0;

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

    unsigned long virAdd = (unsigned long)va;
    unsigned long virAddOffset = (virAdd<<(dirBits+pageBits)) >> (dirBits+pageBits);

    // Page directory index
    unsigned long pageDirIndex = virAdd >> (offsetBits+pageBits);
    // Page table index
    unsigned long pageTabIndex = (virAdd<<dirBits) >> (pageBits + offsetBits);

    // Check page directory
    unsigned char *pgByte = pageDir + pageDirIndex;
    // Page directory index DNE
    if(!(*pgByte & (1 << pageDirIndex))){
        return NULL;
    }

    // Check page table
    pde_t *pdEntry = pgdir + pageDirIndex;

    pte_t *ptEntry = (pte_t*)((*pdEntry >> offsetBits) * PGSIZE) + pageTabIndex;
	//pte_t *ptEntry = (pte_t*)(*pdEntry) + pageTabIndex; ??

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
	int currPDE = (*(char*)va) >> (32 - dirBits);
	int currPTE = ((*(char*)va) >> offsetBits) & (pow(2, pageBits)-1);
	//beginning of current PT
	char* PDEaddress = pageDir + (numPDE * 4) + (currPDE * entriesPerPT) * 4; //4 bytes per entry

	int mappingExists = 0;
	for(int i = 0; i < numPDE; i++) {
		if(*(pageDir + i*4) != 0) {//current PDE entry has mapping
			if(*(pageDir + i*4) == PDEaddress) {
				mappingExists = 1;
				break;
			}
		}
	}
	if(!mappingExists) {
		*(pageDir + currPDE*4) = PDEaddress;

	}
	*(char*)(PDEaddress + 4*currPTE) = pa;
	return 1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
	unsigned int firstEntry = -1;
	unsigned int firstPDE = -1;
	char currEntry = *vMap;
	int pageCount = 0;
	nextContent *retVal = malloc(sizeof(nextContent));
	//traverse virtual bitmap
	for(unsigned long currPDE = 0; currPDE < numPDE; currPDE++) {
		for(unsigned long currPTE = 0; currPTE < entriesPerPT; currPTE++) {
			if((currEntry & 0x80) == 1) {
				pageCount = 0;
				firstEntry = -1;
			}
			else {
				pageCount++;
				if(firstEntry == -1) {
					firstEntry = currPTE;
					firstPDE = currPDE;
				}
				//traverse physical bitmap
				if(pageCount == num_pages) {
					pageCount = 0; //now counting physical pages
					char currEntryP = *pMap;
					unsigned long *pIndex = malloc(sizeof(unsigned long) * num_pages); //holds indices of physical pages found
					for(unsigned long currPHYE = 0; currPHYE < numPTE; currPHYE++) {
						if((currEntryP & 0x80) == 0) {
							pIndex[pageCount] = currPHYE;
							pageCount++;
						}
						if(pageCount == num_pages) {
							retVal->PDE = firstPDE;
							retVal->PTE = firstEntry;
							retVal->PHYS = pIndex;
							return retVal;
						}
						currEntryP >>= 1;
					}
				}
			}
			currEntry >>= 1;
		}
	}
	retVal->PDE = NULL;
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
	unsigned int numPages = num_bytes / PGSIZE;
	if(num_bytes % PGSIZE != 0) numPages++; //round up

	nextContent next = *(nextContent*)get_next_avail(numPages);

	//there are enough free pages
	if(next.PDE != NULL) {
		unsigned long VA = 0;



		int PDE = next.PDE;
		int PTE = next.PTE;

		for(int i = 0; i < numPages; i++) {
			VA = 0;
			PTE = PTE + i;
			if(PTE > entriesPerPT) {
				PTE = 0;
				PDE++;
			}
			VA |= PDE << (32 - dirBits); //set PD bits
			VA |= PTE << (offsetBits); //set PT bits

			//map physical to virtual
			page_map(pageDir, &VA, totalMem + next.PHYS[i]*4);
			//set virtual bitmap
			char *temp = vMap + (PDE*entriesPerPT) + PTE;
			*temp |= 1 << (7 - (PTE % 8));
			//set physical bitmap
			temp = pMap + next.PHYS[i];
			*temp |= 1 << (7-(next.PHYS[i] % 8));
		}
		return VA;
	}
    return NULL;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */





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

    long startAdd = (unsigned long)va;
    long endAdd = ((unsigned long)va + size);

    long startOffset = startAdd >> offsetBits;
    long endOffset = endAdd >> offsetBits;


    int i;
    char *byte;

    // Make sure this is a valid region of virtual memory
    // WIP
    for(i = startAdd; i < endOffset; i++){
        byte = vMap + i;

    }

    // Makes the copy from val to va
    int i;
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
int
add_TLB(void *va, void *pa)
{
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
	if(size < TLB_ENTRIES) {
		int i = 0;
		while(i < TLB_ENTRIES) {
			if(tlb_store.entries[i].virtual_address == 0) {
				tlb_store.entries[i].virtual_address = (unsigned long)va;
				tlb_store.entries[i].physical_address = (unsigned long)pa;
				size++;
			}
		}
	}
	else {
		tlb_store.entries[oldestEntry].virtual_address = (unsigned long)va;
		tlb_store.entries[oldestEntry].physical_address = (unsigned long)pa;
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
		if(tlb_store.entries[i].virtual_address == (unsigned long)va) {
			return (pte_t*)tlb_store.entries[i].physical_address;
		}
	}
	missCount++;
	return NULL;
}
/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;

    /*Part 2 Code here to calculate and print the TLB miss rate*/
	miss_rate = missCount * 1.0f / totalCalls;

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

