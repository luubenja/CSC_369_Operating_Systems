#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"

//should be larger than or equal to expected swapsize
//thus the hash table will have a value for each unique page
#define VADDRMAX 100000

extern int debug;

extern struct frame *coremap;

extern char*tracefile;

extern unsigned memsize;

//nodes for the queue segments
struct node {
	long index;
	struct node* next;
};

struct queue{
	struct node* head, *tail;
};

struct queue vaddr_table[VADDRMAX];

long vaddr_counter;

//pops head of queue in FIFO order
long pop(struct queue* q){
	struct node* head = q->head;
	q->head = head->next;
	return head->index;
}

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	long furthest_index,hashcode;
	struct frame evicted,candidate;
	struct queue* q;

	//looks through coremap to find evictee
	for (int i = 0; i<memsize; i++){
		candidate = coremap[i];
		hashcode = candidate.vaddr % VADDRMAX;
		q = &vaddr_table[hashcode];

		//if page corresponding to vaddress is not referenced again or last reference has already passed
		if (!q->head || (q->tail->index < vaddr_counter)){
			evicted = candidate;
			break;
		}

		//looks through queue to find next reference that has yet to happen
		while(q->head && (q->head->index < vaddr_counter)){
			pop(q);
		}

		//if the next reference further away, becomes the new potential evictee
		if(!furthest_index || q->head->index > furthest_index){
			furthest_index = q->head->index;
			evicted = candidate;
		}
	}

	
	return evicted.pte->frame >> PAGE_SHIFT;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
	vaddr_counter++;
	return;
}

/*Helper function inserts each instance of the virtual address into its
*corresponding queue stored at the hash value. Each entry in the queue is stored
with its index relative to all the traces in the trace file*/
void insert(addr_t vaddr, long counter){
	//generates hashcode based on maximum number of entries in hashtable
	long hashcode = vaddr % VADDRMAX;

	//nodes of the queue
	struct node*new_node = NULL;

	//enumerates each of the virtual addresses each time it is seen
	new_node->index = counter;

	//adds to queue stored as hashvalue
	struct queue *q = &vaddr_table[hashcode];

	//if queue is empty
	if (!q->head){
		q->head = new_node;
		q->tail = new_node;
	}
	else{
		q->tail->next = new_node;
		q->tail = new_node;
	}
}



/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
	//read trace file to obtain virtual addresses
	FILE *tfp = stdin;
	if(tracefile != NULL) {
		if((tfp = fopen(tracefile, "r")) == NULL) {
			perror("Error opening tracefile:");
			exit(1);
		}
	}

	//vaddr_counter will count the number of traces
	char buf[MAXLINE];
	addr_t vaddr = 0;
	vaddr_counter = 0;

	while(fgets(buf, MAXLINE, tfp) != NULL) {
		if(buf[0] != '=') {
			sscanf(buf, "%lx", &vaddr);
			if(debug)  {
				printf("%lx\n", vaddr);
			}

			//insert each vaddr into the hashtable
			insert(vaddr,vaddr_counter);
			vaddr_counter++;
		} else {
			continue;
		}

	}

	//will be used to track progress during the simulation
	vaddr_counter = 0;

}

