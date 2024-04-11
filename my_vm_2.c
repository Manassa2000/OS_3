#include "my_vm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <string.h>

#define N_LEVELS 2
#define PAGE_SIZE 8192 // Some number that is a power of 2. WARNING IT WILL FAIL IF NOT A POWER OF 2.
#define BITS 32 // 32 for 32-bit systems
#define DEBUG 1
#define RESERVED_MEM 1024 // In units of pages.
						  // This memory is used for storing the page tables.
						  // Increase this to a suitable level if any of the above are modified

#define FATAL(m) fprintf(stderr, "[FATAL] %d: %s\n", __LINE__, (m)); exit(-1);
#if DEBUG==1
#define DEBUG_PRINT(m) fprintf(stderr, "[DEBUG] %d: %s\n", __LINE__, (m)); 
#else
#define DEBUG_PRINT(m) 0;
#endif
//TODO: Define static variables and structs, include headers, etc.

typedef struct PageTableEntry{
	union{
		struct PageTableEntry* table;
		char* mem;
	};
} PageTableEntry;

typedef struct State{
	int init;
	char* pmem; // Physical memory
	PageTableEntry* top_level_table; // Virtual address decoding happens here
	int bit_split[N_LEVELS + 1]; // Number of bits used per level. Last level is the number of bits allocated
								 // to physical memory
	char p_bitmap[MEMSIZE/PAGE_SIZE/8]; // Creating a bit map to keep track of the physical pages
	char v_bitmap[MAX_MEMSIZE/PAGE_SIZE/8]; // Creating a bit map to keep track of the virtual pages

} State;

typedef struct AllocatedListNode{
	struct AllocatedListNode* next;
	char* mem; // "Physical" memory
	int pages; // Pages allocated
} AllocatedListNode;

typedef struct TLBEntry{
	void* vpage;
	void* ppage;
	int p;
} TLBEntry;
typedef size_t TLB_Index;
TLBEntry TLB[TLB_ENTRIES];
TLB_Index tlb_put_index = 0;
unsigned long TLB_accesses = 0;
unsigned long TLB_misses = 0;
State state = {0, NULL, NULL, {0, 0, 0}};
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex to make the code threadsafe
AllocatedListNode* head = NULL;

void set_bit_at_index(char *bitmap, long index)
{
	//Implement your code here
	if(!bitmap){
		fprintf(stderr, "Null Pointer Passed!\n");
		exit(-2);
	}
	long bitmap_i = index/8;
	if((bitmap_i < 0)) {
		fprintf(stderr, "Invalid index!\n");
		exit(-3);
	}
	int bit_i = index % 8;
	bitmap[bitmap_i] |= (1 << bit_i);
	return;
}

void clear_bit_at_index(char *bitmap, long index)
{
	//Implement your code here
	if(!bitmap){
		fprintf(stderr, "Null Pointer Passed!\n");
		exit(-2);
	}
	long bitmap_i = index/8;
	if((bitmap_i < 0)) {
		fprintf(stderr, "Invalid index!\n");
		exit(-3);
	}
	int bit_i = index % 8;
	bitmap[bitmap_i] &= ~(1 << bit_i);
	return;
}

/*
 * Function 3: GETTING A BIT AT AN INDEX
 * Function to get a bit at "index"
 */
int get_bit_at_index(char *bitmap, long index)
{
	//Get to the location in the character bitmap array
	//Implement your code here
	if(!bitmap) {
		fprintf(stderr, "Null Pointer Passed!");
		exit(-4);
	}
	long bitmap_i = index/8;
	if((bitmap_i < 0)) {
		fprintf(stderr, "Invalid index!\n");
		exit(-3);
	}
	int bit_i = index % 8;
	int val = bitmap[bitmap_i] & (1 << bit_i);
	if(val)
		return 1;
	return 0;
}

void init_page_tables(int level, PageTableEntry** table_ptr){
	//Assuming the mutex has already been locked
	if(level == N_LEVELS) {
		// Initialization over
		return;
	}

	*table_ptr = malloc(sizeof(PageTableEntry) * ( 1UL << state.bit_split[level] ));
	memset(*table_ptr, 0, sizeof(PageTableEntry) * (1UL << state.bit_split[level] ));
	for(unsigned long long i = 0; i < (1UL << state.bit_split[level]); i++) {
		init_page_tables(level + 1, &((*table_ptr)[i].table));
	}
#if DEBUG==1
	printf("Level %d Table Initialized\n", level);
#endif
}


void set_physical_mem(){
    //TODO: Finish
	// Lock pmem to make sure that only one thread accesses it
	DEBUG_PRINT("Locking");
	pthread_mutex_lock(&global_lock);
	if(!state.init){
		memset(state.p_bitmap, 0, MEMSIZE/8/PAGE_SIZE);
		memset(state.v_bitmap, 0, MAX_MEMSIZE/8/PAGE_SIZE);
		memset(state.p_bitmap, 0xff, RESERVED_MEM/8);
		memset(state.v_bitmap, 0xff, RESERVED_MEM/8);
		// If the memory part is uninitialized, initialize it
		state.pmem = malloc(MEMSIZE);
		if(!state.pmem){
			FATAL("malloc failure");
		}
		// Set the bit-split
		// Lowest level
		state.bit_split[N_LEVELS] = round(log2(PAGE_SIZE));
		// Try and see if an even number can be achieved
		int rem = (BITS - state.bit_split[N_LEVELS]) % N_LEVELS;
		int quo = (BITS - state.bit_split[N_LEVELS]) / N_LEVELS;
		if(rem) {
			DEBUG_PRINT("Uneven divide");
			if(!quo){
				DEBUG_PRINT("Quotient is 0");
				FATAL("Unsupported number of levels!!!");
			}
			for(int i = 0; i < N_LEVELS - 1; i++) {
				state.bit_split[i] = quo;
			}
			state.bit_split[N_LEVELS - 1] = quo + rem;
		} else {
			DEBUG_PRINT("Even divide");
			if(!quo){
				FATAL("Invalid state!");
			}
			for(int i = 0; i < N_LEVELS; i++) {
				state.bit_split[i] = quo;
			}
		}
#if DEBUG==1
		DEBUG_PRINT("state.bit_split: ");
		for(int i = 0; i < N_LEVELS + 1; i++) {
			printf("%d, ", state.bit_split[i]);
		}
		printf("\n");
#endif
		init_page_tables(0, &state.top_level_table);
		state.init = 1;
		DEBUG_PRINT("Unlocked");
		DEBUG_PRINT("Initialization done");
	}
	else {
		DEBUG_PRINT("ALREADY INITIALIZED!");
	}
	pthread_mutex_unlock(&global_lock);
}

void divide_bits(unsigned int vp, int* split_bits_out) {
	//int split_bits[N_LEVELS + 1];
	int bit_acc = 0;
	for(int i = N_LEVELS; i >= 0; i--) {
		split_bits_out[i] = (vp >> bit_acc) & ((1UL << state.bit_split[i]) - 1);
		bit_acc += state.bit_split[i];
//#if DEBUG==1
//		printf("%s: %d\n", __FUNCTION__, split_bits_out[i]);
//#endif
	}
}

void * translate(unsigned int vp){
    //TODO: Finish
	int present = get_bit_at_index(state.v_bitmap, vp/PAGE_SIZE);
	if(!present) {
		FATAL("No entry present!");
	}
	// Split the bits of the virtual pointer
	int split_bits[N_LEVELS + 1];
	divide_bits(vp, split_bits);
	// Use the split bits to travel down the page table tree
	PageTableEntry* curr_table = state.top_level_table[split_bits[0]].table;
	for(int i = 1; i < N_LEVELS - 1; i++) {
		if(!curr_table) {
			FATAL("Should not happen!");
		}
		curr_table = curr_table[split_bits[i]].table;
	}
	char* mem = curr_table[split_bits[N_LEVELS - 1]].mem;
	if(!mem){
		FATAL("Should not happen!");
	}
	return mem + split_bits[N_LEVELS];
}

long find_first_free_vpage(){
	for(long i = 0; i < MAX_MEMSIZE/PAGE_SIZE; i++) {
		if(!get_bit_at_index(state.v_bitmap, i)){
			return i;
		}
	}
	return -1;
}
long find_first_free_ppage(){
	for(long i = 0; i < MEMSIZE/PAGE_SIZE; i++) {
		if(!get_bit_at_index(state.p_bitmap, i)){
			return i;
		}
	}
	return -1;
}

unsigned int page_map(unsigned int vp){
	// Assuming the global lock was already locked
    //TODO: Finish
	// Split the bits of the virutal pointer
	int split_bits[N_LEVELS + 1];
	divide_bits(vp, split_bits);
	if(!get_bit_at_index(state.v_bitmap, vp/PAGE_SIZE)){
		long first_free = find_first_free_ppage();
		if(first_free == -1) {
			FATAL("Out of free memory");
		}
		PageTableEntry* curr_table = state.top_level_table[split_bits[0]].table;
		for(int i = 1; i < N_LEVELS - 1; i++) {
			if(!curr_table) {
				FATAL("Something is wrong with PT initialization!!");
			}
			curr_table = curr_table[split_bits[i]].table;
		}
		curr_table[split_bits[N_LEVELS - 1]].mem = state.pmem + first_free*PAGE_SIZE;
		set_bit_at_index(state.v_bitmap, vp/PAGE_SIZE);
		set_bit_at_index(state.p_bitmap, first_free);
		return 1;
	}
	return 0;
}

void * t_malloc(size_t n){
    //TODO: Finish
	long pages = n/PAGE_SIZE;
	long rem = n % PAGE_SIZE;
	void* ret = NULL;
	if(rem){
		pages += 1;
	}
#if DEBUG==1
	printf("pages: %ld\n", pages);
#endif
	pthread_mutex_lock(&global_lock);
	for(long i = 0; i < pages; i++) {
		long first_vp = find_first_free_vpage()*PAGE_SIZE;
		if(i == 0)
			ret = first_vp;
		if(first_vp == -1){
			FATAL("VP is -1");
		}
		page_map(first_vp);
	}
	pthread_mutex_unlock(&global_lock);
	return ret;
}


PageTableEntry* travel_page_tree(unsigned long vp) {
	int split_bits[N_LEVELS + 1];
	divide_bits(vp, split_bits);
	PageTableEntry* curr_table = state.top_level_table[split_bits[0]].table;
	for(int i = 1; i < N_LEVELS - 1; i++) {
		if(!curr_table) {
			FATAL("Something is wrong with PT initialization!!");
		}
		curr_table = curr_table[split_bits[i]].table;
	}
	return &curr_table[split_bits[N_LEVELS - 1]];
}

void clear_table(unsigned long vp) {
	if(!get_bit_at_index(state.v_bitmap, vp/PAGE_SIZE)) {
		FATAL("Invalid access! This page was never allocated to be deallocated!");
	}
	PageTableEntry* last_table = travel_page_tree(vp);
	last_table->mem = NULL;
	clear_bit_at_index(state.v_bitmap, vp/PAGE_SIZE);
}

int t_free_one_page(unsigned int vp) {
	void* phy_addr = translate(vp);
	if(!get_bit_at_index(state.p_bitmap, (long)( (char*)phy_addr - state.pmem)/PAGE_SIZE)) {
		FATAL("Something is verry wrong");
	}
	clear_bit_at_index(state.p_bitmap, (long)( (char*)phy_addr - state.pmem)/PAGE_SIZE);
	clear_table(vp);
	return 1;
}

int t_free(unsigned int vp, size_t n){
    //TODO: Finish
	long pages = n / PAGE_SIZE;
	if(n % PAGE_SIZE)
		pages += 1;
	int ret = 1;
	pthread_mutex_lock(&global_lock);
	for(long i = 0; i < pages; i++) {
		ret = ret & t_free_one_page(vp + i * PAGE_SIZE);
	}
	pthread_mutex_unlock(&global_lock);
	return ret;
}

int put_value(unsigned int vp, void *val, size_t n){
    //TODO: Finish
	pthread_mutex_lock(&global_lock);
	for(long i = 0; i < n; i++) {
		char* dest = translate(vp + i);
		char* src = val + i;
		*dest = *src;
	}
	pthread_mutex_unlock(&global_lock);
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
	pthread_mutex_lock(&global_lock);
	for(long i = 0; i < n; i++) {
		char* dest = dst + i;
		char* src = translate(vp + i);
		*dest = *src;
	}
	pthread_mutex_unlock(&global_lock);
}

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n){
    //TODO: Finish
	// Assuming that a and b are the input matrices, c is the output matrix, l is the size of each elemnt, m and n are
	// the dimensions of the matrices
	pthread_mutex_lock(&global_lock);
	//for(long j = 0; j < m; j++) {
	//	for(long i = 0; i < n; i++) {
	//		for(long k = 0; k < 
	//	}
	//}
	pthread_mutex_unlock(&global_lock);
}

void add_TLB(unsigned int vpage, unsigned int ppage){
    //TODO: Finish
	// If we are at the end of the TLB,
	// we rollover
	// Since this is an internal function,
	// it cannot be used in a threadsafe manner
	// outside of the API. 
	int i = 0;
	int found_tran = 0;
	for(i = 0; i < TLB_ENTRIES; i++) {
		if(TLB[i].vpage == vpage)
			found_tran = 1;
	}
	if(!found_tran){
		if(tlb_put_index == TLB_ENTRIES) {
			tlb_put_index = 0;
		}

		TLB[tlb_put_index].vpage = vpage;
		TLB[tlb_put_index].ppage = ppage;
		TLB[tlb_put_index].p = 1;
		tlb_put_index++;
	} else {
		TLB[i].ppage = ppage;
	}
}

int check_TLB(unsigned int vpage){
    //TODO: Finish
	for(int i = 0; i < TLB_ENTRIES; i++) {
		if(TLB[i].vpage == vpage) {
			return 1;
		}
	}
	return 0;
}

void print_TLB_missrate(){
    //TODO: Finish
}
