#include <stdio.h>
#include "my_vm.h"

#define NUM_LEVELS 2
#define NUM_BITS 32 // for 32b systems
#define RESERVED 1024
typedef struct PageTable {
	union {
		struct PageTable* table;
		char* mem;
	};
} PageTable;

typedef struct GlobalState {
	int init;
	char* physical_mem;
	PageTable* table;
	unsigned int tlb_lookups;
	unsigned int tlb_misses;
	int divisions[NUM_LEVELS + 1]; // one for the offset as well
	char physical_map[MEMSIZE / PAGE_SIZE / 8];
	char virtual_map[MAX_MEMSIZE / PAGE_SIZE / 8];
} GlobalState;

typedef struct TLB {
	unsigned long buffer[TLB_ENTRIES][2];
	int there;
} tlb;

tlb look_buffer;

GlobalState s;
int list_size = 0;
pthread_mutex_t safety_lock = PTHREAD_MUTEX_INITIALIZER;

void initialize_table(int level, PageTable** table) {
	if (level == NUM_LEVELS)
		return;

	*table = malloc(sizeof(PageTable) * (1UL << s.divisions[level]));
	memset(*table, 0, sizeof(PageTable) * (1UL << s.divisions[level]));
	for (unsigned long long i = 0; i < (1UL << s.divisions[level]); ++i)
		initialize_table(level + 1, &((*table)[i].table));
}

void set_physical_mem(){
	printf("LOCKING THE set_physical_mem() FUNCTION\n");
	pthread_mutex_lock(&safety_lock);

	if (!s.init) {
		memset(s.physical_map, 0, MEMSIZE / 8 / PAGE_SIZE);
		memset(s.virtual_map, 0, MAX_MEMSIZE / 8 / PAGE_SIZE);
		memset(s.physical_map, 0xff, RESERVED / 8);
		memset(s.virtual_map, 0xff, RESERVED / 8);

		s.physical_mem = malloc(MEMSIZE);

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

		initialize_table(0, &s.table);
		s.init = 1;
		s.tlb_lookups = 0;
		s.tlb_misses = 0;
	}


	// allocate physical memory if not already allocated
	if (!s.physical_mem) {
		s.physical_mem = (char *)malloc(MEMSIZE);
		
	}

	if (!s.physical_mem)
		printf("yeeaahh something went wrong in trying to allocate physical memory\n");

	printf("UNLOCKING THE set_physical_mem() FUNCTION\n");
	pthread_mutex_unlock(&safety_lock);
}

void * translate(unsigned int vp){
    pthread_mutex_lock(&safety_lock);

	// just check the TLB first
	++s.tlb_lookups;
	int addy = check_TLB(vp);
	if (addy != -1)
		return addy;
	
	// if we're here, then the page requested isn't there in the tlb
	++s.tlb_misses;
	int isThere = get_bit(s.virtual_map, vp / PAGE_SIZE);
	if (!isThere) {
		printf("NO ENTRY PRESENT!");
		exit(-1);
	}
	unsigned int virtual_addy = vp; // just in case i mess anything up
	int bits_at_division[NUM_LEVELS + 1];
	int off = 0;

	for (int i = NUM_LEVELS; i >= 0; --i) {
		bits_at_division[i] = (vp >> off) & ((1UL << s.divisions[i]) - 1);
		off += s.divisions[i];
	}

	PageTable* curr_table = s.table[bits_at_division[0]].table;
	// now that we have all the bit values in bits_at_division, just keep going to the last level
	for (int i = 1; i <= NUM_LEVELS; ++i) {
		if (!curr_table)
			return NULL;

		curr_table = curr_table[bits_at_division[i]].table;
	}

	char *memory = curr_table[bits_at_division[NUM_LEVELS - 1]].mem;

	// unsigned int return_addy = (unsigned int)(curr_table) + (vp & ((1UL << s.divisions[NUM_LEVELS]) - 1));
	char* return_addy = memory + bits_at_division[NUM_LEVELS]; // add the page offset
	add_TLB(vp, return_addy);

	pthread_mutex_unlock(&safety_lock);
	return return_addy;
}

long find_free_virtual() {
	for (unsigned long i = 0; i < MAX_MEMSIZE / PAGE_SIZE; ++i) {
		if (!get_bit(s.virtual_map, i))
			return i;
	}
}

long find_free_phy() {
	for (unsigned long i = 0; i < MEMSIZE / PAGE_SIZE; ++i) {
		if (!get_bit(s.physical_map, i))
			return i;
	}

	return -1; // failure to fidn physical page
}

unsigned int page_map(unsigned int vp){
	pthread_mutex_lock(&safety_lock);
	// check the tlb to see if it there already
	++s.tlb_lookups;
	int addy = check_TLB(vp);
	if (addy != -1)
		return 0; // success
	
	// if we're here, then the page requested isn't there in the tlb
	++s.tlb_misses;
	unsigned int virtual_addy = vp; // just in case i mess anything up
	int bits_at_division[NUM_LEVELS + 1];
	int off = 0;

	for (int i = NUM_LEVELS; i >= 0; --i) {
		bits_at_division[i] = (vp >> off) & ((1UL << s.divisions[i]) - 1);
		off += s.divisions[i];
	}

	if (!get_bit(s.virtual_map, vp / PAGE_SIZE)) {
		long i = find_free_phy();
		if (i == -1) {
			printf("no free memory, or something messed up with pagetable init\n");
			exit(1);
		}

		PageTable* curr_table = s.table[bits_at_division[0]].table;
		for (int j = 1; j < NUM_LEVELS - 1; ++j) {
			if (!curr_table) {
				printf("Something ain't right here...\n");
				exit(1);
			} curr_table = curr_table[bits_at_division[j]].table;
		}

		curr_table[bits_at_division[NUM_LEVELS - 1]].mem = s.physical_mem + i * PAGE_SIZE;
		set_bit(s.virtual_map, vp / PAGE_SIZE);
		set_bit(s.physical_map, i / PAGE_SIZE);

		pthread_mutex_unlock(&safety_lock);
		return 0;
	}


	pthread_mutex_unlock(&safety_lock);
	return 1; // not success
}

void set_bit(char* map, unsigned long idx) {
	if (!map) {
		exit -1;
	}

	pthread_mutex_lock(&safety_lock);

	int map_idx = idx / 8;
	if (map_idx < 0) {
		printf("Negative map index!\n");
		exit -1;
	}

	map[map_idx] = map[map_idx] | (1 << (idx % 8));

	pthread_mutex_unlock(&safety_lock);
	return;
}

int clear_bit(char *map, unsigned long idx) {
	if (!map) {
		exit -1;
	}

	pthread_mutex_lock(&safety_lock);

	int map_idx = idx / 8;
	if (map_idx < 0) {
		printf("Negative map index!\n");
		exit -1;
	}

	map[map_idx] &= ~(1 << (idx % 8));

	pthread_mutex_unlock(&safety_lock);
	return;
}

int get_bit(char* map, unsigned long idx) {
	if (!map) {
		exit -1;
	}

	int map_idx = idx / 8;
	if (map_idx < 0) {
		printf("Negative map index!\n");
		exit -1;
	}

	int val = map[map_idx] & (1 << (idx % 8));
	return val;
}

void * t_malloc(size_t n){
	pthread_mutex_lock(&safety_lock);
	unsigned long num_pages = n / PAGE_SIZE;
	void *return_val = NULL;
	if (n % PAGE_SIZE)
		++num_pages;

	for (int i = 0; i < num_pages; ++i) {
		unsigned long first_v = find_free_virtual() * PAGE_SIZE;
		if (i == 0)
			return_val = first_v;
		if (first_v == -1) {
			printf("error in t_malloc while calling find_free_virtual()\n");
			exit(1);
		} page_map(first_v);
	}


	pthread_mutex_unlock(&safety_lock);
	return return_val;
}

PageTable* travel_through_tree(unsigned long vp) {
	int bits_at_division[NUM_LEVELS + 1];
	int off = 0;

	for (int i = NUM_LEVELS; i >= 0; --i) {
		bits_at_division[i] = (vp >> off) & ((1UL << s.divisions[i]) - 1);
		off += s.divisions[i];
	}

	PageTable *curr_table = s.table[bits_at_division[0]].table;
	for (int i = 1; i < NUM_LEVELS - 1; ++i) {
		if (!curr_table) {
			printf("error in travel_tree func, going through table...");
			exit(1);
		}

		curr_table = curr_table[bits_at_division[i]].table;
	}

	return &curr_table[bits_at_division[NUM_LEVELS - 1]];
}

void remove_from_table(unsigned long vp) {
	if (!get_bit(s.virtual_map, vp / PAGE_SIZE))
		exit(1);
	
	PageTable* last = travel_through_tree(vp);
	last->mem = NULL;
	clear_bit(s.virtual_map, vp / PAGE_SIZE);
}

int free_one(unsigned long vp) {
	void* physical_addy = translate(vp);
	if (!get_bit(s.physical_map, (unsigned long)((char *)physical_addy - s.physical_map) / PAGE_SIZE)) {
		printf("this isn't suppored to happen... trying to free one page\n");
		exit(1);
	}

	clear_bit(s.physical_map, (unsigned long) ((char *)physical_addy - s.physical_map) / PAGE_SIZE);
	remove_from_table(vp);
}

int t_free(unsigned int vp, size_t n){
    //TODO: Finish
}

int put_value(unsigned int vp, void *val, size_t n){
	for (unsigned long i = 0; i < n; ++i) {
		char *where_we_are = translate(vp + i);
		char *what_to_put = val + i;
		*where_we_are = *what_to_put;
	}
}

int get_value(unsigned int vp, void *dst, size_t n){
	for (unsigned long i = 0; i < n; ++i) {
		char *where_we_are = dst + i;
		char *what_we_want = translate(vp + i);
		*where_we_are = *what_we_want;
	}
}

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n){
    //TODO: Finish
}

void add_TLB(unsigned int vpage, unsigned int ppage){
	int idx = vpage % TLB_ENTRIES;
	look_buffer.buffer[idx][0] = vpage;
	look_buffer.buffer[idx][1] = ppage;

	return;
}

int check_TLB(unsigned int vpage){
	int idx = vpage % TLB_ENTRIES;
	if (look_buffer.buffer[idx][0] == vpage) {
		return 1;
	} return 0;
}

void print_TLB_missrate(){
	double miss_rate = 0;
	miss_rate = (s.tlb_misses * 1.0) / (s.tlb_lookups * 1.0);

	printf("The TLB miss-rate is: %d\n", miss_rate);

	return;
}

