#include "my_vm.h"

//TODO: Define static variables and structs, include headers, etc.

#define PAGE_SIZE 8192
#define array_size (PAGE_SIZE/sizeof(unsigned long))
struct page *physical_mem;
char *physical;
char *virtual_m;
const unsigned long long physical_page_number = (MEMSIZE)/(PAGE_SIZE);
const unsigned long long virtual_page_number = (MAX_MEMSIZE)/(PAGE_SIZE);
int number_of_p = sizeof(physical)/sizeof(physical[0]);
int number_of_v = sizeof(virtual_m)/sizeof(virtual_m[0]);
typedef unsigned long page_table_entry;
typedef unsigned long page_directory_entry;
bool initial = false;
//pthread_mutex_t mutex;
struct tlb t1;

int tlb_lookups = 0;
int offset_size = 0;
int tlb_miss = 0;
int vpn = 0;
int inner_bits = NULL; // fill this in later
int outer_bits = 0;
unsigned long* page_directory = NULL;
struct page{
	unsigned long arr[array_size];
};

struct tlb {
	unsigned long arr[TLB_ENTRIES][2];
};


void set_physical_mem() {
    //TODO: Finish
	 offset_size = log2(PAGE_SIZE);
	 vpn = 32 - offset_size;
	 inner_bits = NULL; // fill this in later
	 outer_bits = vpn - inner_bits;
	 
	physical_mem = (struct page *)malloc(MEMSIZE);
    physical = (char *)malloc(physical_page_number/8);
    virtual_m = (char *)malloc(virtual_page_number/8);

	for(int i = 0; i < number_of_p; ++i) {
        physical[i] = NULL;
	}

    for(int i = 0; i < number_of_v; ++i) {
        virtual_m[i] = NULL;
	}

	int idx = physical_page_number - 1;
	set_bit(physical,idx);
	struct page *p = &physical_mem[idx];
	unsigned long *l = p ->arr;
	for(int i=0;i<(1 << outer_bits);i++)
	{
		l[i]=-1;
	}

}

void * translate(unsigned int page_directory, unsigned int vp){
    //TODO: Finish
	unsigned long off_mask = (1 << offset_size);
	off_mask -= 1;
	unsigned long offset = vp & off_mask;
	++tlb_lookups;

	int tlb_page_number = check_TLB(vp);
	// if the page number = -1, then it doesn't exist in the TLB
	if (tlb_page_number != -1) {
		char *data = (char *)&physical_mem[tlb_page_number];
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
	
	unsigned long *pd_entry = page_directory + outer_idx;

	unsigned long ipt_address = (unsigned long)&physical_mem[*pd_entry];
	unsigned long *pt_entry = ipt_address + inner_idx;
	unsigned long pp_address = (unsigned long)&physical_mem[*pt_entry];

	unsigned long final_address = (unsigned long)((char *)pp_address + offset_size);
	add_TLB(vpn, *pt_entry);

	return;
}

unsigned int page_map(unsigned int page_directory, unsigned int vp, unsigned int pa){
    //TODO: Finish
	unsigned long off_mask = (1 << offset_size);
	off_mask -= 1;
	unsigned long offset = vp & off_mask;

	unsigned long phy_page_num = pa >> offset_size;
	unsigned long virt_page_num = vp >> offset_size;


	int check = check_TLB(vp);
	++tlb_lookups;
	if (check == phy_page_num)
	{
		return 0;
	}
	++tlb_miss;

	// get the outer and then the inner index
	unsigned int num_bits = (32 - outer_bits);
	unsigned long outer_idx = vp >> num_bits;

	unsigned long in_bit_msk = 1 << inner_bits;
	in_bit_msk -= 1;
	unsigned long inner_idx = vpn & in_bit_msk;

	unsigned int pd_entry = page_directory + outer_idx;
	if (pd_entry == -1)
	{
		int last_page = phy_page_num - 1;
		while (last_page >= 0)
		{
			int bit = get_bit(physical, last_page);
			if (bit == 0)
			{
				set_bit(physical, last_page);
				pd_entry = last_page;
				break;
			}

			--last_page;
		}
	}

	unsigned int inner_pt_add = (unsigned int)&physical_mem[pd_entry];
	unsigned int *pt_entry = phy_page_num;

	add_TLB(vpn, phy_page_num);
	return 1;
}

void * t_malloc(size_t n){
    //TODO: Finish
	//pthread_mutex_lock(&mutex);
	if(initial == false)
	{
		set_physical_mem();
		initial = true;
	}

	int pages = n/(PAGE_SIZE);
	int rest = n%(PAGE_SIZE);
	if(rest>0)
	{
		pages++;
	}

	//We need to store the physical pages that can be allocated
	int physical_memory_pages[pages];
	int count = 0;
	int i=0;

	//finding free pages
	while(count<pages && i<physical_page_number)
	{
		int bit = get_bit(physical,i);
		if(bit == 0)
		{
			physical_memory_pages[count] = i;
			count++;
		}

		++i;
	}

	//reach here if we didn't find required physical pages required
	if(count<pages)
	{
		//pthread_mutex_unlock(&mutex);
		return NULL;
	}

	//Need to find corresponding page in virtual memory
	int start = get_next_avail(pages);

	//reach here if failed
	if(start == -1)
	{
		//pthread_mutex_unlock(&mutex);
		return NULL;
	}
	int end = start + pages -1;
	count = 0;
	for (int i= start;i<=end;i++)
	{
		unsigned long v_temp = i << offset_size;
		unsigned long p_temp = physical_memory_pages[count] << offset_size;
		set_bit(virtual_m,i);
		set_bit(physical,physical_memory_pages[count]);
		count ++;
		page_map((unsigned int)(physical_mem + physical_page_number - 1), (unsigned int)v_temp, (unsigned int)p_temp);

		//virtual address
		unsigned long virtual_address = start << offset_size;
		//pthread_mutex_unlock(&mutex);
		return;
	}
}

int t_free(unsigned int vp, size_t n){
    //TODO: Finish
	//pthread_mutex_lock(&mutex);
	int pages = n/(PAGE_SIZE);
	int rest = n % (PAGE_SIZE);

	if(rest>0)
	{
		pages++;
	}

	bool is_valid = true;

	unsigned long start = vp >> offset_size;
	for(int i=start;i<(start+pages);i++)
	{
		int b = get_bit(virtual_m,i);
		if(b==0)
		{
			is_valid = false;
			break;
		}
	}

	if(is_valid == false)
	{
		return;
	}
	for(int i=start;i<(start+pages);i++)
	{
		void * v_add = (void*)(start << offset_size);
		unsigned long p_add = (unsigned long)(traslate(v_add));
		unsigned long physical_page = (p_add >> offset_size);
		reset_bit(virtual_m,i);
		reset_bit(physical,physical_page);
	}
	for(int i=start;i<(start+pages);i++)
	{
		int idx = i % TLB_ENTRIES;
		if(t1.arr[idx][0] == i)
		{
			t1.arr[idx][0] = -1;
			t1.arr[idx][1] = -1;
		} 
	}

}

int put_value(unsigned int vp, void *val, size_t n){
    //TODO: Finish
	char *value = (char*) val;
	unsigned long phys_ad = (unsigned long)(translate((unsigned int)(physical_mem + physical_page_number - 1), vp));
	char *pa = (char *)phys_ad;
	char *va = (char *)vp;
	char *lva = va + n;
	unsigned int f_vpn = (unsigned int)va >> offset_size;
	unsigned int l_vpn = (unsigned int)lva >> offset_size;

	for(int i = f_vpn;i<=l_vpn;i++)
	{
		int b = get_bit(virtual_m,i);
		if(b ==0)
		{
			return;
		}
	}

	int t = 0;
	while(t<n)
	{
		*pa = *value;
		pa++;
		va++;
		value++;
		t++;

		int ob_mask = (1 << offset_size);
		ob_mask-=1;
		int o = (unsigned int)va & ob_mask;

		if(o ==0)
		{
			phys_ad = (unsigned long)(translate((unsigned int)(physical_mem + physical_page_number - 1), vp));
			pa = (char *)phys_ad;
		}
	}
}

int get_value(unsigned int vp, void *dst, size_t n){
    //TODO: Finish
	char *value = (char*) dst;
	unsigned long phys_ad = (unsigned long)(translate((unsigned int)(physical_mem + physical_page_number - 1), vp));
	char *pa = (char *)phys_ad;
	char *va = (char *)vp;
	char *lva = va + n;
	unsigned int f_vpn = (unsigned int)va >> offset_size;
	unsigned int l_vpn = (unsigned int)lva >> offset_size;

	for(int i = f_vpn;i<=l_vpn;i++)
	{
		int b = get_bit(virtual_m,i);
		if(b ==0)
		{
			return;
		}
	}

	
	for(int i=0;i<n;i++)
	{
		*value = phys_ad;
		pa++;
		va++;
		value++;
		

		int ob_mask = (1 << offset_size);
		ob_mask-=1;
		int o = (unsigned int)va & ob_mask;

		if(o ==0)
		{
			phys_ad = (unsigned long)(translate((unsigned int)(physical_mem + physical_page_number - 1), va));
		}
	}

}

void mat_mult(unsigned int a, unsigned int b, unsigned int c, size_t l, size_t m, size_t n){
    //TODO: Finish
}

void add_TLB(unsigned int vpage, unsigned int ppage){
    //TODO: Finish
	int idx = vpage % TLB_ENTRIES;
	t1.arr[idx][0] = vpage;
	t1.arr[idx][1] = ppage;
}

int check_TLB(unsigned int vpage){
    //TODO: Finish
	int idx = vpage % TLB_ENTRIES;
	if(t1.arr[idx][0] == vpage)
	{
		unsigned long ppage = t1.arr[idx][1];
		return ppage;
	}
	else
	{
		return -1;
	}
}

void print_TLB_missrate(){
    //TODO: Finish
	double rate = 0;
	rate = (tlb_miss) / (tlb_lookups);
	printf("TLB Miss Rate : %f \n", rate);
}

static void set_bit (void *bitmap, int idx)
{
    // Calculate the starting location where index is present
    char *location = ((char *) bitmap) + (idx / 8);
    char bit = 1 << (idx % 8);
    *location |= bit;
   
    return;
}

static int get_bit(char *bitmap, int idx)
{
    char *location = ((char *) bitmap) + (idx / 8);
    int bit = (int)(*location >> (idx % 8)) & 0x1;
    
    return bit;
}

static void reset_bit(char *bitmap, int idx)
{
    char *location = ((char *) bitmap) + (idx / 8);
    char bit = 1 << (idx % 8);
    *location &= ~bit;
   
    return;
}

unsigned long get_next_avail(int pages) {
 
    //Use virtual address bitmap to find the next free page

    int start = 0;
    int i = 0;

    while(i < virtual_page_number)
    {
        int bit = get_bit(virtual_m, i);
        if(bit == 0)
        {
            int c = 1;
            int j = i + 1;

            while(j < virtual_page_number && c < pages)
            {
                bit = get_bit(virtual_m, j);
                if(bit == 1)
                {
                    break;
                }
                else
                {
                    c++;
                    j++;
                }
            }
            if(c == pages)
            {
                start = i;
                return start;
            }
            i = j;
            continue;
        }
        i++;
    }
    return -1;
}
