#include <assert.h>
#include <string.h> 
#include "sim.h"
#include "pagetable.h"

// The top-level page table (also known as the 'page directory')
pgdir_entry_t pgdir[PTRS_PER_PGDIR]; 

// Counters for various events.
// Your code must increment these when the related events occur.
int hit_count = 0;
int miss_count = 0;
int ref_count = 0;
int evict_clean_count = 0;
int evict_dirty_count = 0;

/*
 * Allocates a frame to be used for the virtual page represented by p.
 * If all frames are in use, calls the replacement algorithm's evict_fcn to
 * select a victim frame.  Writes victim to swap if needed, and updates 
 * pagetable entry for victim to indicate that virtual page is no longer in
 * (simulated) physical memory.
 *
 * Counters for evictions should be updated appropriately in this function.
 */
int allocate_frame(pgtbl_entry_t *p) {

	int i, swap_offset;
	int frame = -1;
	pgtbl_entry_t *temp;
	
	for(i = 0; i < memsize; i++) {
		if(!coremap[i].in_use) {
			frame = i;
			break;
		}
	}
	if(frame == -1) { // Didn't find a free page.
		// Call replacement algorithm's evict function to select victim
		frame = evict_fcn();

		// All frames were in use, so victim frame must hold some page
		// Write victim page to swap, if needed, and update pagetable
		// IMPLEMENTATION NEEDED

	

		// So at this point we need to 'evict' a frame. This would mean
		// writing it to the swap file, and using it's original spot in the
		// coremap for the next/new frame. 
		// This also means we have to update the vaddr or page table entry that
		// corresponds to this frame, so that it points to the swap file instead of coremap
		
		// Once eviction takes place, make sure to update the counter evict_clean_count/evict_dirty_count

		temp = coremap[frame].pte;
		off_t victims_swap_off = temp->swap_off;
		
		if ((swap_offset = swap_pageout(frame, victims_swap_off)) == INVALID_SWAP){
			fprintf(stderr, "Unable to store frame in swap file\n");
			//exit(1);
		}
		
		// Set bit to indicate frame resides on swap
		temp->frame = temp->frame | PG_ONSWAP;
		
		// Set page to invalid since it now resides in swap
		temp->frame = temp->frame ^ PG_VALID;
		
		// Set offset of frame that now resides in swap
		temp->swap_off = swap_offset;
		
		if (temp->frame & PG_DIRTY){
			evict_dirty_count++;
		} else {
			evict_clean_count++;
		}

		
		if((p->swap_off != INVALID_SWAP) && swap_pagein(frame, p->swap_off) != 0)
			fprintf(stderr, "Error: unable to retrieve frame from swap file\n");

	}

	// Record information for virtual page that will now be stored in frame
	coremap[frame].in_use = 1;
	coremap[frame].pte = p;

	return frame;
}

/*
 * Initializes the top-level pagetable.
 * This function is called once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is 
 * being simulated, so there is just one top-level page table (page directory).
 * To keep things simple, we use a global array of 'page directory entries'.
 *
 * In a real OS, each process would have its own page directory, which would
 * need to be allocated and initialized as part of process creation.
 */
void init_pagetable() {
	int i;
	// Set all entries in top-level pagetable to 0, which ensures valid
	// bits are all 0 initially.
	for (i=0; i < PTRS_PER_PGDIR; i++) {
		pgdir[i].pde = 0;
	}
}

// For simulation, we get second-level pagetables from ordinary memory
pgdir_entry_t init_second_level() {
	int i;
	pgdir_entry_t new_entry;
	pgtbl_entry_t *pgtbl;

	// Allocating aligned memory ensures the low bits in the pointer must
	// be zero, so we can use them to store our status bits, like PG_VALID
	if (posix_memalign((void **)&pgtbl, PAGE_SIZE, 
			   PTRS_PER_PGTBL*sizeof(pgtbl_entry_t)) != 0) {
		perror("Failed to allocate aligned memory for page table");
		exit(1);
	}

	// Initialize all entries in second-level pagetable
	for (i=0; i < PTRS_PER_PGTBL; i++) {
		pgtbl[i].frame = 0; // sets all bits, including valid, to zero
		pgtbl[i].swap_off = INVALID_SWAP;
	}

	// Mark the new page directory entry as valid
	new_entry.pde = (uintptr_t)pgtbl | PG_VALID;

	return new_entry;
}

/* 
 * Initializes the content of a (simulated) physical memory frame when it 
 * is first allocated for some virtual address.  Just like in a real OS,
 * we fill the frame with zero's to prevent leaking information across
 * pages. 
 * 
 * In our simulation, we also store the the virtual address itself in the 
 * page frame to help with error checking.
 *
 */
void init_frame(int frame, addr_t vaddr) {
	// Calculate pointer to start of frame in (simulated) physical memory
	//printf("frame number is : %d, index: %d\n", frame, frame*SIMPAGESIZE);

	char *mem_ptr = &physmem[frame*SIMPAGESIZE];
	// Calculate pointer to location in page where we keep the vaddr
        addr_t *vaddr_ptr = (addr_t *)(mem_ptr + sizeof(int));
	
	memset(mem_ptr, 0, SIMPAGESIZE); // zero-fill the frame
	*vaddr_ptr = vaddr;             // record the vaddr for error checking

	return;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the entry is invalid and not on swap, then this is the first reference 
 * to the page and a (simulated) physical frame should be allocated and 
 * initialized (using init_frame).  
 *
 * If the entry is invalid and on swap, then a (simulated) physical frame
 * should be allocated and filled by reading the page data from swap.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
char *find_physpage(addr_t vaddr, char type) {
	//printf("Getting idx\n");
	pgtbl_entry_t *p=NULL; // pointer to the full page table entry for vaddr
	unsigned idx = PGDIR_INDEX(vaddr); // get index into page directory

	// IMPLEMENTATION NEEDED
	// Use top-level page directory to get pointer to 2nd-level page table
	//(void)idx; // To keep compiler happy - remove when you have a real use.
	
	pgdir_entry_t *dir_en = &pgdir[idx];
	
	if(!(dir_en->pde & PG_VALID)){
		assert((dir_en->pde & PG_VALID) == 0);
		*dir_en = init_second_level();
	}
	
	// Use vaddr to get index into 2nd-level page table and initialize 'p'
	
	// Need to use PGTBL_INDEX, which shifts 12 bits to the right and uses a mask
	// I believe to set the first bit of the index to 0 so that we end up with
	// 4095 indices (as is proper in Comp Sci).
	
	assert((dir_en->pde & PG_VALID) == 1);
	
	//printf("hello3\n");
	
	// We first remove the extra 'valid' bit we tacked onto pde in init_second_level,
	// then add index to pde and finally cast it to a pgtbl_entry_t pointer
	// printf("dir_en->pde - 1: %d\n", (int)dir_en->pde);
	//printf("PGTBL_INDEX(vaddr): %d\n", (int)PGTBL_INDEX(vaddr));
	p = (pgtbl_entry_t *)((dir_en->pde - 0x1));

	p = p + PGTBL_INDEX(vaddr);	
	//printf("value of p: %p\n", p);
	
	//printf("set point of p\n");
	
	// Check if p is valid or not, on swap or not, and handle appropriately
	
	if (!(p->frame & PG_VALID) && !(p->frame & PG_ONSWAP)){

		unsigned int f = (unsigned int)allocate_frame(p);
		//printf("In: Miss\n");		
		init_frame(f, vaddr);
		p->frame = f << PAGE_SHIFT;
		p->frame = p->frame | PG_DIRTY;
		miss_count++;
		
	} else if (!(p->frame & PG_VALID) && (p->frame & PG_ONSWAP)){
		//printf("In: Partial Miss\n");
		p->frame = (unsigned int)(allocate_frame(p) << PAGE_SHIFT);
		//p->frame = p->frame ^ PG_DIRTY;
		miss_count++;
		
	} else {
		//printf("In: HIT\n");
		assert((dir_en->pde & PG_VALID) == 1 && (p->frame & PG_ONSWAP) == 0);
		// Direct hit, frame is in memory
		hit_count++;
	}

	//printf("Passed conditional checks on p->frame bits\n");
	
	ref_count++;
	
	// Is it possible to have a condition where a frame is valid and on swap?
	// Would this be considered invalid? 
	
	/* ANSWER: Yes, this would be consider invalid, since any frame that's not 
			   within physical mem is considered invalid.
	   More details:
	   When a bit is set to 'invalid', it indicates that the associated page is both
	   legal and in memory. If the bit is set to 'invalid', it indicates that page
	   either is not in the address space of the process or is valid but is currently
	   on disk.  
	*/
	
	// Make sure that p is marked valid and referenced. Also mark it
	// dirty if the access type indicates that the page will be written to.

	p->frame = p->frame | PG_VALID;
	p->frame = p->frame | PG_REF;
	
	if(type == 'S' || type == 'M')
		p->frame = p->frame | PG_DIRTY;
	//printf("Passed type check \n");
	// Call replacement algorithm's ref_fcn for this page
	coremap[p->frame >> PAGE_SHIFT].vaddr = vaddr;
	ref_fcn(p);
	
	//print_pagetbl(p);

	//printf("p->frame: %d", (int)(p->frame >> PAGE_SHIFT));
	// Return pointer into (simulated) physical memory at start of frame
	return  &physmem[(p->frame >> PAGE_SHIFT)*SIMPAGESIZE];
}

void print_pagetbl(pgtbl_entry_t *pgtbl) {
        int i;
        int first_invalid, last_invalid;
        first_invalid = last_invalid = -1;

        for (i=0; i < PTRS_PER_PGTBL; i++) {
                if (!(pgtbl[i].frame & PG_VALID) &&
                    !(pgtbl[i].frame & PG_ONSWAP)) {
                        if (first_invalid == -1) {
                                first_invalid = i;
                        }
                        last_invalid = i;
                } else {
                        if (first_invalid != -1) {
                                printf("\t[%d] - [%d]: INVALID\n",
                                       first_invalid, last_invalid);
                                first_invalid = last_invalid = -1;
                        }
                        printf("\t[%d]: ",i);
                        if (pgtbl[i].frame & PG_VALID) {
                                printf("VALID, ");
                                if (pgtbl[i].frame & PG_DIRTY) {
                                        printf("DIRTY, ");
                                }
                                printf("in frame %d\n",pgtbl[i].frame >> PAGE_SHIFT);
                        } else {
                                assert(pgtbl[i].frame & PG_ONSWAP);
                                printf("ONSWAP, at offset %lu\n",pgtbl[i].swap_off);
                        }
                }
        }
        if (first_invalid != -1) {
                printf("\t[%d] - [%d]: INVALID\n", first_invalid, last_invalid);
                first_invalid = last_invalid = -1;
        }
}

void print_pagedirectory() {
	int i; // index into pgdir
	int first_invalid,last_invalid;
	first_invalid = last_invalid = -1;

	pgtbl_entry_t *pgtbl;

	for (i=0; i < PTRS_PER_PGDIR; i++) {
		if (!(pgdir[i].pde & PG_VALID)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("[%d]: INVALID\n  to\n[%d]: INVALID\n", 
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			pgtbl = (pgtbl_entry_t *)(pgdir[i].pde & PAGE_MASK);
			printf("[%d]: %p\n",i, pgtbl);
			print_pagetbl(pgtbl);
		}
	}
}
