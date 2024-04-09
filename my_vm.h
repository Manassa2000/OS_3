#include <stddef.h>
#include "math.h"

#define MAX_MEMSIZE (1UL<<32)
#define MEMSIZE (1UL<<30)
#define TLB_ENTRIES 256
#define PAGE_SIZE 8192

int tlb_lookups = 0;
int offset_size = log2(PAGE_SIZE);
int tlb_miss = 0;
int vpn = 32 - offset_size;
int inner_bits = NULL; // fill this in later
int outer_bits = vpn - inner_bits;
unsigned long* page_directory = NULL;

void set_physical_mem();

void * translate(unsigned int vp);

unsigned int page_map(unsigned int vp);

void * t_malloc(size_t n);

int t_free(unsigned int vp, size_t n);

int put_value(unsigned int vp, void *val, size_t n);

int get_value(unsigned int vp, void *dst, size_t n);

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n);

void add_TLB(unsigned int vpage, unsigned int ppage);

int check_TLB(unsigned int vpage);

void print_TLB_missrate();