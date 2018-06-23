#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

struct frame *youngestf, *oldestf = NULL; // pointers to oldest enqueued & newly enqueued frames

/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
	
	struct frame *evic_victim = oldestf;
	
	if (oldestf->frame_above != NULL){
		oldestf = oldestf->frame_above;
	} else {
		oldestf = NULL;
	}
	
	// Reset victim's above pointer (it's child frame) so that it may re-enter FIFO queue when required.
	evic_victim->frame_above = NULL;

	return evic_victim->pte->frame >> PAGE_SHIFT;
}

/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {
	
	unsigned int f_num = p->frame >> PAGE_SHIFT;
	struct frame *latest_fref = &coremap[f_num];

	// Check if frame is already a member of FIFO queue
	// Having no frame_above (no parent frame) implies it's not in queue/physmem, 
	// so can be added, otherwise, it already exits in queue so ignore it.
	if (latest_fref->frame_above == NULL){
		
		if (oldestf == NULL){
		
			oldestf = latest_fref;
			oldestf->frame_above = NULL;
			
		} else if (youngestf == NULL){
		
			youngestf = latest_fref;
			oldestf->frame_above = latest_fref;
			
		} else {
		
			assert(youngestf != NULL);
			
			// Get the index difference between the 2 pointers
			int f_offset = latest_fref - youngestf;
			
			// Set the current youngest frame to point to the new youngest frame whose
			// index is calculated based on f_offset value.
			youngestf->frame_above = youngestf + f_offset;
			
			// Updated new youngest
			youngestf = latest_fref;
			
		}
		
	}

	return;
}

/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {
}
