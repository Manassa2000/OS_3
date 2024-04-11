#include <stdio.h>
#include "my_vm.h"

#define NUM_LEVELS 4
typedef struct PageTable {
	int level;
	int there;
	char* mem;
} PageTable;

typedef struct GlobalState {
	char* physical_mem;
	unsigned int tlb_lookups;
	unsigned int tlb_misses;
	PageTable* table;
	int divisions[NUM_LEVELS + 1]; // one for the offset as well
} GlobalState;

typedef struct Node {
	struct Node *next;
	int mem_loc;
	int pages;
} Node;

GlobalState s;
Node *head = NULL;
int list_size = 0;
pthread_mutex_t safety_lock = PTHREAD_MUTEX_INITIALIZER;


void set_physical_mem(){
	printf("LOCKING THE set_physical_mem() FUNCTION\n");
	pthread_mutex_lock(&safety_lock);

	// offset for the size of the page
	s.divisions[NUM_LEVELS] = round(log2(PAGE_SIZE));
	int bits_left = 32 - s.divisions[NUM_LEVELS];
	int carry = 0;
	int equal_div = 0;

	if (bits_left % NUM_LEVELS != 0) {
		carry = bits_left % NUM_LEVELS;
		--bits_left;
	}

	equal_div = (bits_left) / NUM_LEVELS;
	for (int i = 0; i < NUM_LEVELS; ++i)
		s.divisions[i] = equal_div;
	if (carry > 0)
		for (int i = NUM_LEVELS - 1; i > 0; --i)
			s.divisions[i] += 1;

	for (int i = 0; i <= NUM_LEVELS; ++i)
		printf("Level %d: %d bits\n ", i, s.divisions[i]);


	// allocate physical memory if not already allocated
	if (!s.physical_mem) {
		s.physical_mem = (char *)malloc(MEMSIZE);
		s.table = malloc(sizeof(PageTable) * 1UL << s.divisions[0]);
		for (int i = 0; i < (1UL << s.divisions[0]); ++i) {
			s.table[i].level = 0;
			s.table[i].there = 0;
			s.table[i].mem = NULL;
		}

		s.tlb_lookups = 0;
		s.tlb_misses = 0;
	}

	if (!s.physical_mem)
		printf("yeeaahh something went wrong in trying to allocate physical memory\n");

	printf("UNLOCKING THE set_physical_mem() FUNCTION\n");
	pthread_mutex_unlock(&safety_lock);
}

void * translate(unsigned int vp){
    pthread_mutex_lock(&safety_lock);

	// just check the TLB first

	int addy = check_TLB(vp);
	if (addy != -1)
		return addy;
	
	unsigned int virtual_addy = vp; // just in case i mess anything up
	int bits_at_division[NUM_LEVELS + 1];
	int off = 0;

	for (int i = NUM_LEVELS; i >= 0; --i) {
		bits_at_division[i] = (vp >> off) & ((1UL << s.divisions[i]) - 1);
		off += s.divisions[i];
	}

	


	pthread_mutex_unlock(&safety_lock);
}

unsigned int page_map(unsigned int vp){
    //TODO: Finish
}

void * t_malloc(size_t n){
    //TODO: Finish
}

int t_free(unsigned int vp, size_t n){
    //TODO: Finish
}

int put_value(unsigned int vp, void *val, size_t n){
    //TODO: Finish
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
}

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n){
    //TODO: Finish
}

void add_TLB(unsigned int vpage, unsigned int ppage){
    //TODO: Finish
}

int check_TLB(unsigned int vpage){
    //TODO: Finish
}

void print_TLB_missrate(){
    //TODO: Finish
}

