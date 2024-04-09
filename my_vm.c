#include "my_vm.h"

//TODO: Define static variables and structs, include headers, etc.

#define PAGE_SIZE 8192
struct page *physical_mem;
char *physical;
char *virtual_m;
const unsigned long long physical_page_number = (MEMSIZE)/(PAGE_SIZE);
const unsigned long long virtual_page_number = (MAX_MEMSIZE)/(PAGE_SIZE);
int number_of_p = sizeof(physical)/sizeof(physical[0]);
int number_of_v = sizeof(virtual_m)/sizeof(virtual_m[0]);
typedef unsigned long page_table_entry;
typedef unsigned long page_directory_entry;

void set_physical_mem() {
    //TODO: Finish
	physical_mem = (struct page *)malloc(MEMSIZE);
    physical = (char *)malloc(physical_page_number/8);
    virtual_m = (char *)malloc(virtual_page_number/8);

	for(int i = 0; i < number_of_p; ++i) {
        physical[i] = NULL;
	}

    for(int i = 0; i < number_of_v; ++i) {
        virtual_m[i] = NULL;
	}

}

void * translate(unsigned int vp){
    //TODO: Finish
	unsigned long off_mask = (1 << offset_size);
	off_mask -= 1;
	unsigned long offset = vp & off_mask;
	++tlb_lookups;

	int tlb_page_number = check_TLB(vp);
	// if the page number = -1, then it doesn't exist in the TLB
	if (tlb_page_number != -1) {
		char* data = (char *)&physical_mem[tlb_page_number];
		data += offset;
		return data;
	}

	// we're here because the page isn't there in our TLB
	++tlb_miss;
	// now, we get the outer index
	unsigned int number_of_bits = 32 - outer_bits;
	unsigned long outer_idx = vp >> number_of_bits;

	// get vpn so that we can get the inner index
	unsigned int vpn = vp >> offset_size;
	unsigned long inner_bit_msk = 1 << inner_bits;
	--inner_bit_msk;

	unsigned long inner_idx = vpn & inner_bit_msk;
	int idx = (outer_idx * (1 << inner_bits)) + inner_idx;

	// check if this is a valid address using bitmap
	int bit = get_bit(virtual_m, idx);
	if (bit != 1)
		return NULL;
	page_directory = physical_mem + physical_page_address - 1;
	unsigned long *pd_entry = page_directory + outer_idx;

	unsigned long ipt_address = (unsigned long)&physical_mem[*pd_entry];
	unsigned long *pt_entry = ipt_address + inner_idx;
	unsigned long pp_address = (unsigned long)&physical_mem[*pt_entry];

	unsigned long final_address = (unsigned long)((char *)pp_address + offset_size);
	add_TLB(vpn, *pt_entry);

	return;
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