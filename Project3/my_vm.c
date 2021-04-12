#include "my_vm.h"


char* vMap = NULL;
char* pMap = NULL;
char* pageDir = NULL;
char* totalMem = NULL;

int numPages;
int offsetBits;
int pageBits;
int dirBits;

int TLBsize;

int init = 0;

int numPTE = 0;
int numPDE = 0;
int entriesPerPT = 0;

/*here's my set_physical_mem()
sure hope this doesnt mess things up lol
void set_physical_mem() {
    if(init){
        return;
    }
    init = 1;

    offsetBits = log(PGSIZE);
	pageBits = (int)ceil((32-offsetBits)/2.0);
	dirBits = 32 - offsetBits - pageBits;
	numPTE = MEMSIZE/(PGSIZE + 4 + pow(2, 2-PTBITS));
	entriesPerPT = pow(2, pageBits);
	numPDE = (int)ceil(numPTE/entriesPerPT);
	
	totalMem = malloc(MEMSIZE);
	vMap = malloc(numPTE);
	pMap = malloc(numPTE);
	pageDir = totalMem + MEMSIZE - (4*numPDE + 4*numPDE*numPTE);
	
	memset(vMap, 0, numPTE);
	memset(pMap, 0, numPTE);
}
*/


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
	int currPDE = (*(char*)va) >> (32 - dirBits);
	int currPTE = ((*(char*)va) >> offsetBits) & (pow(2, pageBits)-1);
	//beginning of current PT
	char* PDEaddress = pageDir + (numPDE * 4) + (currPDE * entriesPerPT) * 4; //4 bytes per entry
	
	int mappingExists = 0;
	for(int i = 0; i < numPDE; i++) {
		if(*(pageDir + i*4) != 0) {//current PDE entry has mapping
			if(*(pageDir + i*4) == PTEaddress) {
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
	int firstEntry = -1;
	int currEntry = vMap;
	int pageCount = 0;
	for(int currPDE = 0; currPDE < numPDE; currPDE++) {
		for(int currPTE = 0; currPTE < entriesPerPT; currPTE++) {
			if((currEntry & 0x80) == 1) {
				pageCount = 0; 
				firstEntry = -1;
			}				
			else {
				pageCount++;
				if(firstEntry == -1) {
					firstEntry = currPTE;
				}
				if(pageCount == num_pages) {
					retVal->PDE = currPDE;
					retVal->PTE = currPTE;
					retVal->PHYS = totalMem + currPDE * entriesPerPT + currPTE;
					return retVal;
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
	
	int numPages = (int)ceil((long double)num_bytes / PGSIZE);
	nextContent next = *(nextContent*)get_next_avail(numPages);
	
	//there are enough free pages
	if(next.PTE != NULL) {
		unsigned long VA = 0;
		
		VA |= next.PDE << (32 - dirBits); //set PD bits
		VA |= next.PTE << (32 - dirBits - pageBits); //set PT bits
		
		if(page_map(pageDir, &VA, next.PHYS)!=-1) {
			//(num page tables before current * entries per table + number of entries from start of current table) / 8 bits/byte
			int totalBits = next.PDE * entriesPerPT + next.PTE;
			
			//settingbitmap bit to true
			*(vMap + VbitmapBytes) |= 1 << VbitmapBit;
			for(unsigned long i = vMap + totalBits, j = 0, k = pMap + totalBits; j < numPages; i >>=1, j++, k >>= 1) {
				i |= 0x80;
			}
			
			return VA;
		}
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

    long startAdd = (long)va;
    long endAdd = ((long)va + size);

    long startOffset = startAdd >> offsetBits;
    long endOffset = endAdd >> offsetBits;


    int i;
    char *byte;

    // Make sure this is a valid region of virtual memory
    // WIP
    for(i = startAdd; i < endOffset; i++){
        byte = (char*)vMap + i;

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

    return -1;
}
/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */

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




    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}
