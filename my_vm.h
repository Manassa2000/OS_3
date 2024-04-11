#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "math.h"
#include "stdbool.h"

#define MAX_MEMSIZE (1UL<<32)
#define MEMSIZE (1UL<<30)
#define TLB_ENTRIES 256
#define PAGE_SIZE 8192

void set_physical_mem();

static void set_bit (void *bitmap, int idx);

static int get_bit(char *bitmap, int idx);

static void reset_bit(char *bitmap, int idx);

unsigned long get_next_avail(int pages);

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
