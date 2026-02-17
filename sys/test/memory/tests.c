#include "../../io.h"
#include "../../console.h"
#include "../../string.h"

#include <limits.h>

struct page_chunk {
	struct page_chunk * next;   // next page chunk in list
	unsigned long pagecnt;      // number of pages in chunk
};

//You can change this, this is mainly just 
//for some random set of hits on memory.
uintptr_t rngseed = 0xece391;

//Random number generator
uintptr_t xorshift64(){
	rngseed ^= rngseed << 13;
	rngseed ^= rngseed >> 7;
	rngseed ^= rngseed << 17;
	return rngseed;
}

//Wait for user input - in case I need you to read memory tables
//This function requires a spinwait implementation of the UART provided in
//the adjacent UART file.
int wait_for_user_input(struct io* uart, char* characters, char* prompt){
        kprintf(prompt);
        for (;;) {                                                                 
		char ch;
                long readcnt = ioread(uart, &ch, 1);
                if (readcnt < 0) {
                        kprintf("read(/dev/uart1): %s\n", error_name((int)readcnt));
                }
                if (readcnt == 0) continue;
                if (characters==NULL)return 0;
                int found = -1;
                for(int i = 0; characters[i] != 0; i++){
                        if(ch != characters[i])continue;
                        found = i;
                        break;
                }
                if(found >= 0)return found;
        }
        return 0;
}

// ---- BEGIN TESTS ----

//Test 1: Control Test - YES
//This test checks that I'm able to grab user input for YES correctly

void test_control_y(struct io* console, int* fail, int* total){
	kprintf("Test 1: Control Test - YES\n");
	(*total)++;
	int bad = (wait_for_user_input(console, "yYnN", "Press Y\n")/2);
	fail+=bad;
	if(bad)kprintf("FAIL\n");
	else kprintf("PASS\n");
}

//Test 2: Control Test - NO
//This test checks that I'm able to grab user input for NO correctly

void test_control_n(struct io* console, int* fail, int* total){
	kprintf("Test 2: Control Test - NO\n");
	(*total)++;
	int bad = (1-wait_for_user_input(console, "yYnN", "Press N\n")/2);
	fail+=bad;
	if(bad)kprintf("FAIL\n");
	else kprintf("PASS\n");
}

//Test 3: Initialization Test
//Checks that the chunk list is initialized with one large chunk that has a number of
//pages equal to the free_phys_page_count, and that there are more than 0 pages.
//
//Returns max page count
//
//TODO: Do we care about the chunklist being one large chunk? I think there's some
//allocation strategies that split it up at startup.
unsigned long test_init(struct page_chunk* chunklist, int* fail, int* total){
	kprintf("Test 3: Initialization Test\n");
	(*total)++;
	kprintf("Found %u Free Pages\n", free_phys_page_count());
	unsigned long max = free_phys_page_count();
	if(chunklist->pagecnt != free_phys_page_count() || chunklist->next != NULL || max <= 0){
		fail+=1;
		kprintf("FAIL\n");
		return 0;
	}
	kprintf("PASS\n");
	return max;
}

//Internal function to check for overlap of any memory range with a linked list of
//page chunks
static int check_overlap(void* pageptr, uintptr_t range, struct page_chunk* chunklist){
	struct page_chunk* curr = chunklist;
	while(curr != NULL){
		if((uintptr_t)(void*)curr < ((uintptr_t)pageptr) + range){
			if(((uintptr_t)(void*)curr + curr->pagecnt * PAGE_SIZE) > (uintptr_t)pageptr)
				return 1;
		}
		curr = curr->next;
	}
	return 0;
}

//Test 4: Alloc 1 simple
//Allocates one page and checks that we have a page pointer, that the free_phys_page_count
//went down by exactly 1, and that there is no overlap between the returned page and the chunk list.
void* test_alloc_1_simple(struct page_chunk* chunklist, unsigned long max, int* fail, int* total){
	kprintf("Test 4: Alloc 1 Simple\n");
	(*total)++;
	void* ret = alloc_phys_page();
	if(free_phys_page_count() != max-1 || ret == NULL || check_overlap(ret, PAGE_SIZE, chunklist)){
		(*fail)++;
		kprintf("FAIL\n");
	}else kprintf("PASS\n");
	return ret;
}

//Test 5: Simple Reclaim
//Frees a single allocated page and checks that the free phys page count is equivalent
//to the max as passed in. This can be called with some pages allocated as long as you know
//how many pages you expect to have after.
void test_free_simple(void* reclaim, unsigned long max, int* fail, int* total){
	kprintf("Test 5: Simple Reclaim\n");
	(*total)++;
	free_phys_page(reclaim);
	if(free_phys_page_count() != max){
		(*fail)++;
		kprintf("FAIL\n");
	}else kprintf("PASS\n");
}

//Returns difference between chunk size and user specified size
int cmp_chunk_size(struct page_chunk* chunk, unsigned long size){
	return chunk->pagecnt*PAGE_SIZE - size;
}

//Test 6: Alloc 4
//Allocates 4 pages and makes the same checks as test 4
void* test_alloc_4(struct page_chunk* chunklist, unsigned long max, int* fail, int* total){
	kprintf("Test 6: Alloc 4\n");
	(*total)++;
	void* ret = alloc_phys_pages(4);
	if(free_phys_page_count() != max-4 || ret == NULL || check_overlap(ret, PAGE_SIZE*4, chunklist)){
		(*fail)++;
		kprintf("FAIL\n");
		return NULL;
	}
	kprintf("PASS\n");
	return ret;
}

//Test 7: Free 4
//Allocates 4 pages and makes the same checks as test 5
void test_free_4(void* reclaim, unsigned long max, int* fail, int* total){
	kprintf("Test 7: Free 4\n");
	(*total)++;
	free_phys_pages(reclaim, 4);
	if(free_phys_page_count() != max){
		(*fail)++;
		kprintf("FAIL\n");
	}else kprintf("PASS\n");
}

//Finds up to max best fit chunks
int find_best_chunks(struct page_chunk* chunklist, struct page_chunk** viable, int size, int max){
	if(chunklist == NULL)return -1;
	struct page_chunk* curr = chunklist;
	int best_size = INT_MAX;
	while(curr != NULL){
		if((curr->pagecnt < best_size) && (curr->pagecnt >= size))best_size = curr->pagecnt;
		curr = curr->next;
	}
	curr = chunklist;
	int idx = 0;
	while(curr != NULL && idx < max){
		if(curr->pagecnt == best_size)viable[idx++] = curr;
		curr = curr->next;
	}
	return idx;
}

//Test 8: Best Fit
//This test looks for all possible best fit chunks for size 1 and 4
//and checks whether you allocate correctly. This test will fail if you
//have more than 128 free best fit chunks for your allocation (we love fragmentation)
//You cannot do better with this test because we cannot allocate more heap space for
//a linked list :(
void test_best_fit(struct page_chunk* chunklist, int* fail, int* total){
	kprintf("Test 8: Best fit\n");
	(*total)++;
	struct page_chunk* viable[128];
	memset(viable, 0, 128*sizeof(struct page_chunk*));
	int found = find_best_chunks(chunklist, viable, 1, 128);
	if(found <= 0){
		kprintf("WARN: Out of memory, skipping.\n");
		(*fail)++; //Log OOM as fail
		return;
	}
	void* pick = alloc_phys_page();
	free_phys_page(pick);
	int good = 0;
	for(int i = 0; i < found; i++){
		if(pick == viable[i]){
			good = 1;
			break;
		}
	}
	if(!good){
		kprintf("FAIL: Size 1\n");
		(*fail)++;
		return;
	}
	memset(viable, 0, 128*sizeof(struct page_chunk*));
	found = find_best_chunks(chunklist, viable, 4, 128);
	if(found <= 0){
		kprintf("WARN: Out of memory, skipping.\n");
		(*fail)++; //Log OOM as fail
		return;
	}
	pick = alloc_phys_pages(4);
	free_phys_pages(pick, 4);
	good = 0;
	for(int i = 0; i < found; i++){
		if(pick == viable[i]){
			good = 1;
			break;
		}
	}
	if(!good){
		kprintf("FAIL: Size 4\n");
		(*fail)++;
		return;
	}
	kprintf("PASS\n");
}

//Test 9: Map User Page (Simple)
//Maps page at ptr, makes user check.
void map_usr_simple(struct io* console, void* ptr, int* fail, int* total){
	kprintf("Test 9: Map User Page (Simple)\n");
	(*total)++;
	int* pg = alloc_and_map_range((uintptr_t)ptr, PAGE_SIZE, PTE_R | PTE_W | PTE_X | PTE_U);
	if(wait_for_user_input(console, "yYnN", "Check that page in U range was mapped Y/N\n")/2){
		(*fail)++;
		kprintf("FAIL\n");
	}else kprintf("PASS\n");
}

//Test 10: Clear X bit on page (Simple)
//Clears X bit on mapped page, makes user check.
void clear_x_simple(struct io* console, void* ptr, int* fail, int* total){
	kprintf("Test 10: Clear X bit on page (Simple)\n");
	(*total)++;
	set_range_flags(ptr, PAGE_SIZE, PTE_R | PTE_W | PTE_U);
	if(wait_for_user_input(console, "yYnN", "Check that X bit was cleared for page in U range Y/N\n")/2){
		(*fail)++;
		kprintf("FAIL\n");
	}else kprintf("PASS\n");
}

//Test 11: R/W Mapped Page
//Writes to all bytes on mapped page, then reads them back.
void read_write_mapped_page(char* ptr, int* fail, int* total){
	kprintf("Test 11: R/W Mapped Page\n");
	(*total)++;
	for(int i = 0; i < PAGE_SIZE; i++){
		ptr[i] = 0x69;
	}
	for(int i = 0; i < PAGE_SIZE; i++){
		if(ptr[i] != 0x69){
			(*fail)++;
			kprintf("FAIL at byte %d\n", i);
			return;
		}
	}
	kprintf("PASS\n");
}

//Test 12: S-Mode Faulter
//Writes a whole unmapped page and reads it back
void s_mode_faulter(char* ptr, int* fail, int* total){
	kprintf("Test 12: S-Mode Faulter\n");
	(*total)++;
	for(int i = 0; i < PAGE_SIZE; i++){
		ptr[i] = 0x67;
	}
	for(int i = 0; i < PAGE_SIZE; i++){
		if(ptr[i] != 0x67){
			(*fail)++;
			kprintf("FAIL at byte %d\n", i);
			return;
		}
	}
	kprintf("PASS\n");
}

//Test 13: Validation Test
//Try all the failure modes for validation and make sure they actually fail
void test_validation(int* fail, int* total){
	int bad = 0;
	kprintf("TEST 13: Validation Test\n");
	(*total)++;
	//Wellformed
	if(validate_vptr((void*)0xc0000000, PAGE_SIZE * 2, PTE_R | PTE_W) != 0){bad = 1;kprintf("GOOD pointer rejected.\n");}
	if(validate_vptr((void*)0x8200000c0001000, 1, PTE_R | PTE_W) >= 0){
		bad = 1;
		kprintf("%p = %d\n", 0x8200000c0001000, validate_vptr((void*)0x8200000c0001000, 1, PTE_R | PTE_W));
	}
	//X not allowed
	if(validate_vptr((void*)0xc0001000, 1, PTE_R | PTE_W | PTE_X) >= 0){
		bad = 1;
		kprintf("%p = %d\n", 0xc0001000, validate_vptr((void*)0xc0001000, 1, PTE_R | PTE_W | PTE_X));
	}
	//Size too large
	if(validate_vptr((void*)0xc0001004, ~((size_t)0), PTE_R | PTE_W) >= 0){
		bad = 1;
		kprintf("%p = %d\n", 0x8200000c0001000, validate_vptr((void*)0xc0001004, ~((size_t)0), PTE_R | PTE_W));
	}
	/* What should the behavior be here?
	//Overrun into unmapped page
	if(validate_vptr((void*)0xc0001000, PAGE_SIZE * 3000, PTE_R | PTE_W) != -ENOENT){
		bad = 1;
		kprintf("%p = %d\n", 0xc0001000, (validate_vptr((void*)0xc0001000, PAGE_SIZE * 3000, PTE_R | PTE_W)));
	}
	//Unmapped page overrun into mapped region
	if(validate_vptr((void*)0xc0000000, PAGE_SIZE * 3000, PTE_R | PTE_W) != -ENOENT){
		bad = 1;
		kprintf("%p = %d\n", 0xc0000000, (validate_vptr((void*)0xc0000000, PAGE_SIZE * 3000, PTE_R | PTE_W)));
	}
	if(validate_vptr((void*)0xc0000000, PAGE_SIZE * 2, PTE_R | PTE_X) != -EACCESS){
		bad = 1;
		kprintf("%p x2 = %d\n", 0xc0000000, (validate_vptr((void*)0xc0000000, PAGE_SIZE * 2, PTE_R | PTE_X)));
	}
	*/
	char* vstr = (void*)0xc0001000;
	memset(vstr, 1, 2*4096); //Relies on s mode faulter
	vstr[4096*2] = 0;
	if(validate_vstr(vstr, PTE_R | PTE_U) < 0){
		bad = 1;
		kprintf("FAIL: Valid VSTR failed.\n");
	}
	vstr = (void*)((uintptr_t)0xc0001000 + 2*4096);
	memset(vstr, 1, 4096*2);
	if(validate_vstr(vstr, PTE_R | PTE_U) >= 0){
		bad = 1;
		kprintf("FAIL: Invalid VSTR validated.\n");
	}
	if(bad){
		(*fail)++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}
}

//Test 14: Reset Active Mspace
//Resets the active memory space and checks that all pages were recovered (as per expected), prompts user to check that only global mappings were retained
void test_reset(struct io* console, int expected, int* fail, int* total){
	kprintf("Test 14: Reset Active Mspace\n");
	(*total)++;
	reset_active_mspace();
	if(wait_for_user_input(console, "yYnN", "Check that only global mappings were retained Y/N\n")/2 || free_phys_page_count() != expected){
		(*fail)++;
		kprintf("FAIL\n");
	}
	kprintf("PASS\n");
}

//Test 15: Clone Active Mspace
//Creates one global mapping and creates one non-global mapping. Writes to both. Checks whether changes are visible from other
//memory space. Checks whether reset keeps the global mapping (makes it easier for the discard test to be chained to this)
//Returns newly created mtag
mtag_t test_clone(int* fail, int* total){
	kprintf("Test 15: Clone Active Mspace\n");
	(*total)++;
	alloc_and_map_range(0xc0000000, PAGE_SIZE, PTE_R | PTE_X | PTE_W | PTE_G | PTE_U);
	alloc_and_map_range(0xf0000000, PAGE_SIZE, PTE_R | PTE_W | PTE_X | PTE_U);
	*(char*)0xf0000000 = 0x42;
	mtag_t new = clone_active_mspace();
	mtag_t old = switch_mspace(new);
	int bad = 0;
	if(*(char*)0xf0000000 != 0x42){
		bad=1;
		kprintf("FAIL: Non-Global page not Copied\n");
	}
	*(char*)0xc0001000 = 0x42; // This should not be visible
	*(char*)0xc0000000 = 0x42; // This should be visible
	switch_mspace(old);
	if(*(char*)0xc0001000 == 0x42){
		bad = 1;
		kprintf("FAIL: Non-Global page visible from first memory space\n");
	}
	if(*(char*)0xc0000000 != 0x42){
		bad = 1;
		kprintf("FAIL: Global page does not share updates\n");
	}
	reset_active_mspace();
	*(char*)0xc0000000 = 0x69;
	switch_mspace(new);
	if(*(char*)0xc0000000 != 0x69){
                bad = 1;
                kprintf("FAIL: Global page does not share updates after reset\n");
        }
	switch_mspace(old);
	if(bad)(*fail)++;
	else kprintf("PASS\n");
	return new;
}

//Test 16: Discard Active Mspace
//Discards the active memory space then checks whether the expected number of pages were returned to the allocator
void test_discard(mtag_t mspace, int expected, int* fail, int* total){
	kprintf("Test 16: Discard Active Mspace\n");
	(*total)++;
	switch_mspace(mspace);
	discard_active_mspace();
	if(free_phys_page_count() != expected){
		(*fail)++;
		kprintf("FAIL - %d pages lost\n", expected - free_phys_page_count());
		return;
	}
	kprintf("PASS\n");
}

//Test 17: Write/Readback all available pages
//System test:
//- Faults into every mappable user page
//- Writes a pattern to the whole page
//- Reads back the entire page to see that every byte was set
//- Resets the memory space and performs a leak check
//- Takes a long time
//end argument is in case you mapped some stuff at the beginning of the user space.
void complete_memtest(uintptr_t end, int* fail, int* total){
	kprintf("Test 17: Write/Readback all available pages\n");
	(*total)++;
	int bad = 0;
	if(end < UMEM_START_VMA)end = UMEM_START_VMA;
	//You have to keep a tolerance of 3 because that's the max number of pages that can be
	//allocated for a single page fault.
	while(end < UMEM_END_VMA-PAGE_SIZE){
		int pages = 0;
		int leak_check = free_phys_page_count();
		for(; (free_phys_page_count() >= 3) & (end <= UMEM_END_VMA-PAGE_SIZE); pages++){
			memset((void*)(end), (pages & 0xFF), PAGE_SIZE);
			end+=PAGE_SIZE;
		}
		for(int i = 0; i < pages; i++){
			for(int j = 0; j < PAGE_SIZE; j++){
				if(((char*)(void*)(end-pages*PAGE_SIZE+i*PAGE_SIZE))[j] != (i & 0xFF)){
					kprintf("Mismatch at address %p - E: 0x%x GOT: 0x%x\n", end-pages*PAGE_SIZE+i*PAGE_SIZE+j, i & 0xFF, ((char*)(void*)(end-pages*PAGE_SIZE+i*PAGE_SIZE+j))[j]);
					bad = 1;
					//Honestly I'd suggest killing QEMU if you see this and you suspect yourself and not your RAM
				}
			}
		}
		reset_active_mspace();
		if(free_phys_page_count() != leak_check){
			kprintf("Expected %d pages, got %d pages after allocating up to %p.\n", leak_check, free_phys_page_count(), end);
			bad = 1;
		}
	}
	if(bad){
		(*fail)++;
		kprintf("FAIL\n");
	}else kprintf("PASS\n");
}
