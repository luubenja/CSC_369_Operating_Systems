#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int clock_hand;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
	pgtbl_entry_t * p, *evicted;
	int evicted_not_found = 1;

	//clock hand rotates untill evicted is found
	while (evicted_not_found){
		if (clock_hand>=memsize){
			clock_hand = 0;
		}

		p = coremap[clock_hand].pte;

		//uses reference bit to see if recently used
		if (p->frame & PG_REF){
			p->frame = p->frame ^ PG_REF;
		}
		else{
			evicted = p;
			evicted_not_found = 0;
		}

		clock_hand++;
	}
	
	return evicted->frame >> PAGE_SHIFT;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
	clock_hand = 0;
}
