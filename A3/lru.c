#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

extern int memsize;

extern int debug;

extern struct frame *coremap;

struct frame *top_node, *bottom_node; 

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	
	struct frame *evict = bottom_node;
	struct frame *evics_parent =  evict->frame_above;

	// Ensure bottom_node now points to evicted node's parent
	bottom_node = evics_parent;
	bottom_node->frame_below = NULL;
	
	// Reset pointers for evicted frame
	evict->frame_above = NULL;
	evict->frame_below = NULL;
	
	return evict->pte->frame >> PAGE_SHIFT;
	
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	struct frame *latest_fref = &coremap[p->frame >> PAGE_SHIFT];

	if (!bottom_node){
	
		bottom_node = latest_fref;
		bottom_node->frame_above = NULL;
		bottom_node->frame_below = NULL;
		
	} else if (!top_node && latest_fref != bottom_node){
	
		top_node = latest_fref;
		top_node->frame_below = bottom_node;
		bottom_node->frame_above = latest_fref;
		
	} 
	// Enter main block below only when both bottom_node & top_node are initialized with
	// 2 unique pointers.
	else if (top_node) {
		
		// Case: Frame referenced does not exist yet on physmem (possibly came in from swap), 
		// since it's double pointers are NULL.
		if(latest_fref->frame_above == NULL && latest_fref->frame_below == NULL){
		
			top_node->frame_above = latest_fref;
			latest_fref->frame_below = top_node;
			top_node = latest_fref;
			
		} 
		
		// Case: Frame referenced is currently in physmem and already is the "most fresh"/recent
		// frame. Nothing to do in this case.  
		else if (latest_fref->frame_below && !latest_fref->frame_above){
		
			assert(latest_fref == top_node);
			
		} 
		
		// Case: Frame referenced was the stalest, but now becomes "fresh" again, most
		// recently used.
		else if (!latest_fref->frame_below && latest_fref->frame_above) {
		
			assert(latest_fref == bottom_node);
			
			bottom_node = bottom_node->frame_above;
			bottom_node->frame_below = NULL;
			
			top_node->frame_above = latest_fref;
			latest_fref->frame_below = top_node;
			top_node = latest_fref;
			top_node->frame_above = NULL;
			
		} 
		
		// Case: Reference frame was neither the first nor the last node in the list. 
		else {
			
			assert(latest_fref->frame_above && latest_fref->frame_below);
			
			struct frame *top_temp = latest_fref->frame_above;
			struct frame *bottom_temp = latest_fref->frame_below;
			
			bottom_temp->frame_above = top_temp;
			top_temp->frame_below = bottom_temp; 
			
			top_node->frame_above = latest_fref;
			latest_fref->frame_below = top_node;
			latest_fref->frame_above = NULL;
			
			top_node = latest_fref;
			
		}
	}
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
}
