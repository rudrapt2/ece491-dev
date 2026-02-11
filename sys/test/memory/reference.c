/*! @file main.c‌‌‍‍‌‍⁠‌‌​‌‌‌⁠‍‌‌​⁠‍‌‌‌‍​⁠‍‌‌‍⁠​‌‌‍‌​⁠​‍‌‌‌‌‌⁠‍‍‌​⁠⁠‌‌‌​‌​‌‍‌‍‌‍‌‌‍‍​⁠​⁠‌​‍‍‌⁠‌‍‌‍‌​‌‌‍​‌​​‍‌‍‌‍‌​⁠‍‌‍‍‍‌‍​⁠​⁠⁠‌
  @brief main function of the kernel (called from start.s)
  @copyright Copyright (c) 2024-2025 University of Illinois

*/

#include "cache.h"
#include "conf.h"
#include "console.h"
#include "dev/rtc.h"
#include "dev/uart.h"
#include "dev/virtio.h"
#include "device.h"
#include "devimpl.h"
#include "elf.h"
#include "error.h"
#include "filesys.h"
#include "heap.h"
#include "intr.h"
#include "string.h"
#include "thread.h"
#include "timer.h"
#include "uio.h"
#include "uioimpl.h"
#include "misc.h"
#include "memory.h"
#include <stdint.h>

#define INITEXE "trek"  // FIXME

#define CMNTNAME "c"
#define DEVMNTNAME "dev"
#define CDEVNAME "vioblk"
#define CDEVINST 0

#ifndef NUART  // number of UARTs
#define NUART 2
#endif

#ifndef NVIODEV  // number of VirtIO devices
#define NVIODEV 8
#endif

#ifndef HEAP_INIT_MIN
#define HEAP_INIT_MIN 256
#endif

struct page_chunk{
	struct page_chunk* next;
	unsigned long pagecnt;
};

static void attach_devices(void);
static void mount_cdrive(void);  // mount primary storage device ("C drive")
static void memtest_64(void);
static int wait_for_user_input(struct uio* console, char* characters, char* prompt);
static uintptr_t xorshift64();

static uintptr_t rngseed = 0xECE391;

void memtest_64(void) {
	extern char _kimg_end[];  // provided by kernel.ld
	struct uio* console = NULL;
	int fail = 0;
	int total = 0;

	int result = open_file(DEVMNTNAME, "uart1", &console);
	if (result != 0) {
		kprintf("/dev/uart1: %s\n", error_name(result));
		halt_failure();
	}

	wait_for_user_input(console, "\r\n", "Press ENTER in serial window to start the Interactive Memory Test...\n\n");

	kprintf("TEST 1: CONTROL TEST - YES\n");
	total++;
	fail+=(wait_for_user_input(console, "yYnN", "Press Y\n")/2);

	kprintf("TEST 2: CONTROL TEST - NO\n");
	total++;
	fail+=(1-wait_for_user_input(console, "yYnN", "Press N\n")/2);

	if(fail!=0){
		kprintf("\nWARN: CONTROL TESTS FAILED\n\n");
	}else{
		kprintf("CONTROL PASS\n\n");
	}

	//Hacky way to retrieve the free chunk list, we won't use this past testing whether
	//coalesce works since the pointer will quit working as soon as the head is reallocated
	//elsewhere.
	uintptr_t heap_end = ROUND_UP((uintptr_t)_kimg_end, PAGE_SIZE);
	struct page_chunk* chunklist = (void*)(heap_end + ROUND_UP(HEAP_INIT_MIN - (heap_end - (uintptr_t)_kimg_end), PAGE_SIZE));

	kprintf("TEST 3: Initialization Test\n");
	total++;
	kprintf("Started with: %u Pages to allocate\n", chunklist->pagecnt);
	uintptr_t max = chunklist->pagecnt;
	if(chunklist->pagecnt != free_phys_page_count() || chunklist->next != NULL){
		fail+=1;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("\n----BEGIN ALLOCATOR TESTS----\n- alloc_phys_pages\n- alloc_phys_page\n- free_phys_pages\n- free_phys_page\n- free_phys_page_count\n----         ~*~         ----\n");
	kprintf("TEST 4: Alloc 1 Simple\n");
	total++;
	void* blk1 = alloc_phys_page();
	if(free_phys_page_count() != max-1 || blk1 == NULL || ((uintptr_t)blk1 != ((uintptr_t)chunklist + chunklist->pagecnt * PAGE_SIZE))){
		fail+=1;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 5: Alloc 4 Simple\n");
	total++;
	void* blk5 = alloc_phys_pages(4);
	if(free_phys_page_count() != max-5 || blk5 == NULL || ((uintptr_t)blk5 != ((uintptr_t)chunklist + chunklist->pagecnt * PAGE_SIZE))){
		fail+=1;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}
	
	int res = 0;
	kprintf("Test 6: Free 2 Simple\n");
	total++;
	void* blk3 = (void*)((uintptr_t)blk5 + 2*PAGE_SIZE);
	free_phys_pages(blk3, 2);
	if(free_phys_page_count() != max-3){
		res=1;
		kprintf("Count incorrect\n");
	}
	if(chunklist->pagecnt != max-5){
		res=1;
		kprintf("First chunk changed size\n");
	}
	if(chunklist->next == NULL){
		res = 1;
		kprintf("Linked list did not update\n");
	}else{
		if(chunklist->next->pagecnt != 2){
			res=1;
			kprintf("Second entry wrong size\n");
		}
	}
	if(res){
		fail+=1;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 7: Best fit page alloc\n");
	total++;
	void* blk2 = alloc_phys_page();
	if(free_phys_page_count() != max-4 || chunklist->next->pagecnt != 1 || blk2 != (void*)((uintptr_t)chunklist + PAGE_SIZE * (max-2))){
		kprintf("FAIL\n");
		kprintf("%u, %u, %u, %p == %p\n", chunklist->next->pagecnt, free_phys_page_count(), max, (uintptr_t)(chunklist) + PAGE_SIZE * (max-2), blk2);
		fail+=1;
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 8: Clear block from chunk list\n");
	total++;
	blk3 = alloc_phys_page();
	if(free_phys_page_count() != max-5 || chunklist->next != NULL || blk3 != (void*)((uintptr_t)chunklist + PAGE_SIZE * (max-3))){
		kprintf("FAIL\n");
		kprintf("%u, %u, %u, %p == %p\n", chunklist->next->pagecnt, free_phys_page_count(), max, (uintptr_t)(chunklist) + PAGE_SIZE * (max-3), blk3);
		fail+=1;
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 9: Coalesce Forward\n");
	total++;
	free_phys_page(blk2);
	free_phys_page(blk3);
	if(free_phys_page_count() != max-3 || chunklist->next == NULL || chunklist->next->next != NULL || chunklist->next->pagecnt != 2){
		kprintf("FAIL\n");
		fail++;
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 10: Coalesce Back\n");
	total++;
	free_phys_page(blk1);
	if(free_phys_page_count() != max-2 || chunklist->next == NULL || chunklist->next->next != NULL || chunklist->next->pagecnt != 3){
		kprintf("FAIL\n");
		fail++;
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 11: Coalesce Back And Forward\n");
	total++;
	free_phys_pages(blk5, 2);
	if(free_phys_page_count() != max || chunklist->next != NULL || chunklist->pagecnt != max){
		kprintf("FAIL\n");
		fail++;
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 12: Allocate Full Range\n");
	total++;
	void* start = alloc_phys_pages(max);
	if(free_phys_page_count() != 0 || start == NULL){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 13: Fail Allocation\n");
	total++;
	void* l = alloc_phys_page();
	if(l != NULL){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}


	kprintf("TEST 14: Free Some Block In Range\n");
	total++;
	free_phys_page(blk5);
	if(free_phys_page_count() != 1){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 15: Fail Allocation Due To Size\n");
	total++;
	l = alloc_phys_pages(2);
	if(l != NULL){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 16: Allocate Last Block\n");
	total++;
	void* tmp = alloc_phys_page();
	if(free_phys_page_count() != 0 && tmp != blk5){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 17: Free Complete Range\n");
	total++;
	free_phys_pages(start, max);
	if(free_phys_page_count() != max || chunklist->next != NULL || chunklist->pagecnt != max){
		kprintf("FAIL\n");
		fail++;
	}else{
		kprintf("PASS\n");
	}

	kprintf("---- END ALLOCATOR TESTS ----\n");

	kprintf("TEST 18: Map (Almost) Complete Virtual Range\n");
	total++;
	uintptr_t max_bytes = (max-20)*PAGE_SIZE;
	if(max_bytes > (0x100000000 - 0xC0001000)){
		max_bytes = 0x100000000 - 0xc0001000;
	}
	int* t18 = alloc_and_map_range(0xc0001000, max_bytes, PTE_R | PTE_W | PTE_X | PTE_U);
	kprintf("Range: %p-%p\n", t18, (uintptr_t)t18 + max_bytes);
	kprintf("Remaining Pages: %u\n", free_phys_page_count());
	if(wait_for_user_input(console, "yYnN", "Check that full user range was mapped Y/N\n")/2){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 19: Clear X bit\n");
	total++;
	set_range_flags((void*)0xc0001000, max_bytes, PTE_R | PTE_W | PTE_U);
	if(wait_for_user_input(console, "yYnN", "Check that X bit has been cleared for U range Y/N\n")/2){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	kprintf("TEST 20: R/W (Almost) Complete Virtual Range\n");
	total++;
	int t19_bad = 0;
	for(int i = 0; i < max_bytes/sizeof(int); i++){
		t18[i] = i;
		if(t18[i] != i){
			t19_bad = 1;
			kprintf("Mismatch at array index %d at address %p\n", i, t18+i);
		}
	}
	if(t19_bad){
		fail+=1;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}
	
	int fail_t21 = 0;
	kprintf("TEST 21: Validation Test\n");
	total++;
	//Wellformed
	if(validate_vptr((void*)0xc0001000, PAGE_SIZE * 20, PTE_R | PTE_W) != 0)fail_t21 = 1;
	if(validate_vptr((void*)0x8200000c0001000, 1, PTE_R | PTE_W) != -EBADFMT){
		fail_t21 = 1;
		kprintf("%p = %d\n", 0x8200000c0001000, validate_vptr((void*)0x8200000c0001000, 1, PTE_R | PTE_W));
	}
	//X not allowed
	if(validate_vptr((void*)0xc0001000, 1, PTE_R | PTE_W | PTE_X) != -EACCESS){
		fail_t21 = 1;
		kprintf("%p = %d\n", 0xc0001000, validate_vptr((void*)0xc0001000, 1, PTE_R | PTE_W | PTE_X));
	}
	//Size too large
	if(validate_vptr((void*)0xc0001004, ~((size_t)0), PTE_R | PTE_W) != -EACCESS){
		fail_t21 = 1;
		kprintf("%p = %d\n", 0x8200000c0001000, validate_vptr((void*)0xc0001004, ~((size_t)0), PTE_R | PTE_W));
	}
	//Overrun into unmapped page
	if(validate_vptr((void*)0xc0001000, PAGE_SIZE * 3000, PTE_R | PTE_W) != -ENOENT){
		fail_t21 = 1;
		kprintf("%p = %d\n", 0xc0001000, (validate_vptr((void*)0xc0001000, PAGE_SIZE * 3000, PTE_R | PTE_W)));
	}
	//Unmapped page overrun into mapped region
	if(validate_vptr((void*)0xc0000000, PAGE_SIZE * 3000, PTE_R | PTE_W) != -ENOENT){
		fail_t21 = 1;
		kprintf("%p = %d\n", 0xc0000000, (validate_vptr((void*)0xc0000000, PAGE_SIZE * 3000, PTE_R | PTE_W)));
	}
	if(validate_vptr((void*)0xc0000000, PAGE_SIZE * 2, PTE_R | PTE_X) != -EACCESS){
		fail_t21 = 1;
		kprintf("%p x2 = %d\n", 0xc0000000, (validate_vptr((void*)0xc0000000, PAGE_SIZE * 2, PTE_R | PTE_X)));
	}
	char* vstr = (void*)0xc0001000;
	memset(vstr, 1, 2*4096);
	vstr[4096*2] = 0;
	if(validate_vstr(vstr, PTE_R | PTE_U) < 0){
		fail_t21 = 1;
		kprintf("FAIL: Valid VSTR failed.\n");
	}
	vstr = (void*)((uintptr_t)0xc0001000 + max_bytes - 2*4096);
	memset(vstr, 1, 4096*2);
	if(validate_vstr(vstr, PTE_R | PTE_U) >= 0){
		fail_t21 = 1;
		kprintf("FAIL: Invalid VSTR validated.\n");
	}
	if(fail_t21){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}
	
	kprintf("TEST 22: Reset active mspace\n");
	total++;
	reset_active_mspace();
	if(wait_for_user_input(console, "yYnN", "Check that only global mappings were retained Y/N\n")/2){
		fail++;
		kprintf("FAIL\n");
	}else{
		if(free_phys_page_count() == max && chunklist->pagecnt == max && chunklist->next == NULL){
			kprintf("PASS\n");
		}else{
			fail++;
			kprintf("FAIL\n");
			kprintf("%u %u %u %p\n", max, free_phys_page_count(), chunklist->pagecnt, chunklist->next);
		}
	}

	kprintf("TEST 23: Smode Page Fault\n");
	total++;
	*(int*)(void*)0xc0001000 = 1;
	//-3 because we have to freshly allocate a new lv1 and lv0 page table as well as the page for our fault. However if this works any other page faults should work the same way.
	if(*(int*)(void*)0xc0001000 == 1 && free_phys_page_count() == max-3){
		kprintf("PASS\n");
	}else{
		fail+=1;
		kprintf("FAIL\n");
		kprintf("%d == %d\n",free_phys_page_count(), max-3);
	}

	int free_pages = free_phys_page_count();
	
	int passtest24 = 1;
	kprintf("TEST 24: Clone Active Memory Space\n");
	total++;
	memset((void*)0xc0001000, 1, 4096);
	mtag_t new = clone_active_mspace();
	wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
	switch_mspace(new);
	wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
	for(int i = 0; i < 4096; i+=1){
		if(*(uint8_t*)0xc0001000 != 1){
			kprintf("Mismatch at address %d\n", i);
			passtest24 = 0;
		}
	}
	if(passtest24){
		kprintf("PASS\n");
	}else{
		fail++;
		kprintf("FAIL\n");
	}
	
	int passtest25 = 1;
	kprintf("TEST 25: Discard Active Memory Space\n");
	total++;
	discard_active_mspace();
	wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
	for(int i = 0; i < 4096; i+=1){
		if(*(uint8_t*)0xc0001000 != 1){
			kprintf("Mismatch at address %d\n", i);
			passtest25 = 0;
		}
	}
	if(passtest25 && free_pages == free_phys_page_count()){
		kprintf("PASS\n");
	}else{
		fail++;
		kprintf("FAIL: Lost %d pages\n", free_pages - free_phys_page_count());
	}

	kprintf("TEST 26: Smode Page Fault - Die :)\n");
	total++;
	if(handle_umode_page_fault(NULL, (uintptr_t)0xc0001000) == 0){
		kprintf("PASS\n");
	}else{
		fail++;
		kprintf("FAIL\n");
	}

	//Reset active memory space for memtest64
	reset_active_mspace();
	goto skip_long; //COMMENT OUT TO INCLUDE FULL RANGE TESTS
	int leak_check = free_phys_page_count();
	kprintf("%d : %d\n", free_phys_page_count(), max);
	int fail_memtest64 = 0;
	kprintf("TEST 27: True MemTest64 - This may take a while...\n");
	total++;
	for(uintptr_t i = 0xc0000000; i < 0x100000000; i+=PAGE_SIZE){
		//1. Allocate and map the range. Write a pattern to the range. Read back. Unmap and free. Check for memory leaks.
		//I choose to use a misaligned pointer here as a stronger test.
		char* mem = alloc_and_map_range(i, 1, PTE_R | PTE_W);
		if(mem == NULL){
			kprintf("Failed at %p, Allocation is NULL\n", i);
			fail_memtest64 = 1;
			break;
		}
		memset((void*)(i), (i & 0xFF), PAGE_SIZE);
		for(int j = 0; j < PAGE_SIZE; j++){
			if(((char*)(void*)(i))[j] != (i & 0xFF)){
				kprintf("Mismatch at address %p\n", i);
				fail_memtest64=1;
			}
		}
		reset_active_mspace();
		if(leak_check != free_phys_page_count()){
			kprintf("Memory leak when freeing %p: Expected %d pages but got %d pages.\n", mem, leak_check, free_phys_page_count());
			fail_memtest64=1;
		}
	}
	if(fail_memtest64){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	int fail_memtest64_28 = 0;
	kprintf("TEST 28: MemTest64: Misaligned forward - This may take a while...\n");
	total++;
	for(uintptr_t i = 0xc0000001; i < 0x100000000; i+=PAGE_SIZE){
		//1. Allocate and map the range. Write a pattern to the range. Read back. Unmap and free. Check for memory leaks.
		//I choose to use a misaligned pointer here as a stronger test.
		char* mem = alloc_and_map_range(i, 1, PTE_R | PTE_W);
		if(mem == NULL){
			kprintf("Failed at %p, Allocation is NULL\n", i);
			fail_memtest64_28 = 1;
			break;
		}
		memset((void*)(i-1), (i & 0xFF), PAGE_SIZE);
		for(int j = 0; j < PAGE_SIZE; j++){
			if(((char*)(void*)(i-1))[j] != (i & 0xFF)){
				kprintf("Mismatch at address %p\n", i);
				fail_memtest64_28=1;
			}
		}
		reset_active_mspace();
		if(leak_check != free_phys_page_count()){
			kprintf("Memory leak when freeing %p: Expected %d pages but got %d pages.\n", mem-1, leak_check, free_phys_page_count());
			fail_memtest64_28=1;
		}
	}
	if(fail_memtest64_28){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	int fail_memtest64_29 = 0;
	kprintf("TEST 29: MemTest64: Misaligned backward - This may take a while...\n");
	total++;
	for(uintptr_t i = 0xc0000000-1+PAGE_SIZE; i < 0x100000000-PAGE_SIZE; i+=PAGE_SIZE){
		//2 bytes should be enough to allocate 2 pages
		char* mem = alloc_and_map_range(i, 2, PTE_R | PTE_W);
		if(mem == NULL){
			kprintf("Failed at %p, Allocation is NULL\n", i);
			fail_memtest64_29 = 1;
			break;
		}
		memset((void*)(i-PAGE_SIZE + 1), (i & 0xFF), PAGE_SIZE*2);
		for(int j = 0; j < PAGE_SIZE*2; j++){
			if(((char*)(void*)(i+1 - PAGE_SIZE))[j] != (i & 0xFF)){
				kprintf("Mismatch at address %p\n", i);
				fail_memtest64_29=1;
			}
		}
		reset_active_mspace();
		if(leak_check != free_phys_page_count()){
			kprintf("Memory leak when freeing %p: Expected %d pages but got %d pages.\n", mem-1, leak_check, free_phys_page_count());
			fail_memtest64_29=1;
		}
	}
	if(fail_memtest64_29){
		fail++;
		kprintf("FAIL\n");
	}else{
		kprintf("PASS\n");
	}

	//At this point we can basically guarantee reset active mspace works.
	//Because unmap and free is just deleting pages, I'm assuming that one
	//is okay too.
skip_long:
	int fail_memtest64_30 = 0;
	kprintf("TEST 30: MemTest64: Validate Vstr\n");
	total++;
	//Write random number of characters greater than 0 to random addresses since we're allowed to fault, then check whether they are equal.
	for(int i = 0; i < 64; i++){
		uintptr_t start = xorshift64() % (0x100000000 - 0xc0000000) + 0xc0000000;
		uintptr_t len = xorshift64() % max_bytes;
		for(int j = 0; j < len - 1; j++){
			((char*)start)[j] = (char)(i & 0xFF);
		}
		((char*)start)[len] = 0;
		if(validate_vstr((char*)(void*)start, PTE_R | PTE_U) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d", start, len);
		}
		reset_active_mspace();
	}
	//Take a bunch of valid strings and compare flags
	for(int i = 0; i < 64; i++){
		uintptr_t start = xorshift64() % (0x100000000 - 0xc0000000) + 0xc0000000;
		uintptr_t len = xorshift64() % max_bytes;
		//Map the range to rwu
		alloc_and_map_range(start, len, PTE_R | PTE_W | PTE_U);
		for(int j = 0; j < len - 1; j++){
			((char*)start)[j] = (char)((i+1) & 0xFF);
		}
		((char*)start)[len] = 0; //Basically means the string will always be valid because
					 //there's a termination
		//We only need to support checking for R and U flags in strings, so we only need to
		//make masks for those, both, and neither (phew!)
		set_range_flags((void*)start, len, PTE_R | PTE_U);
		if(validate_vstr((char*)(void*)start, PTE_R | PTE_U) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d\n", start, len);
		}
		if(validate_vstr((char*)(void*)start, PTE_R) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d\n", start, len);
		}
		if(validate_vstr((char*)(void*)start, PTE_U) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d\n", start, len);
		}
		if(validate_vstr((char*)(void*)start, 0) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d\n", start, len);
		}
		if(len > PAGE_SIZE){
			set_range_flags((void*)(start+PAGE_SIZE), len-PAGE_SIZE, PTE_R);
		}else{
			set_range_flags((void*)(start), len, PTE_R);
		}
		if(validate_vstr((char*)(void*)start, PTE_R | PTE_U) >= 0){
			fail_memtest64_30 = 1;
			kprintf("R: RU Validated BAD vstr at %p with length %d\n", start, len);
		}
		if(validate_vstr((char*)(void*)start, PTE_R) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d\n", start, len);
		}
		if(validate_vstr((char*)(void*)start, PTE_U) >= 0){
			fail_memtest64_30 = 1;
			kprintf("R: U Validated BAD vstr at %p with length %d\n", start, len);
		}
		if(validate_vstr((char*)(void*)start, 0) != 0){
			fail_memtest64_30 = 1;
			kprintf("Could not validate vstr at %p with length %d\n", start, len);
		}
		reset_active_mspace();
	}

	if(fail_memtest64_30){
		kprintf("FAIL\n");
		fail++;
	}else{
		kprintf("PASS\n");
	}
	int fail_memtest64_31 = 0;
	int fail_31 = 0;
	kprintf("TEST 31: MemTest64: Validate Vptr\n");
	total++;
	for(int i = 0; i < 64; i++){
		void* start = (void*)(xorshift64() % (0x100000000 - 0xc0000000) + 0xc0000000);
		uintptr_t len = xorshift64() % max_bytes;
		if(validate_vptr(start, len, PTE_R | PTE_W | PTE_U) != -ENOENT){
			kprintf("Incorrect invalid test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
			wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
		}
		void* ptr = alloc_and_map_range((uintptr_t)start, len, PTE_R | PTE_W | PTE_U);
		if(validate_vptr(start, len, PTE_R | PTE_W | PTE_U) != 0){
			kprintf("Incorrect Exact Match test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
			wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
		}
		if(validate_vptr(start, len, PTE_R | PTE_U) != 0){
			kprintf("Incorrect Partial Match test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
			wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
		}
		if(validate_vptr(start, len, PTE_X | PTE_U) != -EACCESS){
			kprintf("Incorrect Partial Mismatch test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
			wait_for_user_input(console, NULL, "Debug Hold, Press Any Key To Continue...\n");
		}
		set_range_flags((void*)start, 1, PTE_R | PTE_U);
		if(validate_vptr(start, len, PTE_R | PTE_W | PTE_U) != -EACCESS){
			kprintf("Incorrect Misaligned (F) Exact Match test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		if(validate_vptr(start, len, PTE_R | PTE_U) != 0){
			kprintf("Incorrect Misaligned (FS) Partial Mismatch test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		if(validate_vptr(start, len, PTE_X | PTE_U) != -EACCESS){
			kprintf("Incorrect Misaligned (FX) Partial Mismatch test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		set_range_flags((void*)start, 1, PTE_R | PTE_W | PTE_U);
		set_range_flags((void*)(start+len), 1, PTE_R | PTE_U);
		if(validate_vptr(start, len, PTE_R | PTE_W | PTE_U) != -EACCESS){
			kprintf("Incorrect Misaligned (R) Partial Mismatch test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		if(validate_vptr(start, len, PTE_R | PTE_U) != 0){
			kprintf("Incorrect Misaligned Partial Match test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		if(validate_vptr(start, len, PTE_X | PTE_U) != -EACCESS){
			kprintf("Incorrect Misaligned Partial Mismatch (RX) test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		reset_active_mspace();
		ptr = alloc_and_map_range((uintptr_t)(start+PAGE_SIZE), len-PAGE_SIZE, PTE_R | PTE_W | PTE_U);
		if(validate_vptr(start, len, PTE_R | PTE_W | PTE_U) != -ENOENT){
			kprintf("Incorrect invalid test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		reset_active_mspace();
		alloc_and_map_range((uintptr_t)start, len-PAGE_SIZE, PTE_R | PTE_W | PTE_U);
		if(validate_vptr(start, len, PTE_R | PTE_W | PTE_U) != -ENOENT){
			kprintf("Incorrect invalid test at %p with length %d\n", start, len);
			fail_memtest64_31 = 1;
		}
		reset_active_mspace();
		if(fail_memtest64_31){
			fail_31++;
		}

	}
	if(fail_memtest64_31){
		kprintf("FAIL %d\n", fail_31);
		fail++;
	}else{
		kprintf("PASS\n");
	}

	kprintf("Test Complete!\n");
	kprintf("%d PASSES, %d FAILS out of %d TOTAL\n", total - fail, fail, total);
	uio_close(console);

	kprintf("Test Complete!\n");
	kprintf("%d PASSES, %d FAILS out of %d TOTAL\n", total - fail, fail, total);
	uio_close(console);
}

uintptr_t xorshift64(){
	rngseed ^= rngseed << 13;
	rngseed ^= rngseed >> 7;
	rngseed ^= rngseed << 17;
	return rngseed;
}

int wait_for_user_input(struct uio* console, char* characters, char* prompt){
	kprintf(prompt);
	for (;;) {
		char ch;
		long readcnt = uio_read(console, &ch, 1);
		if (readcnt < 0) {
			kprintf("read(/dev/uart1): %s\n", error_name((int)readcnt));
			halt_failure();
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

void main(void) {
	console_init();
	intrmgr_init();
	devmgr_init();
	thrmgr_init();
	memory_init();

	attach_devices();

	enable_interrupts();

	mount_cdrive();
	memtest_64();
}

void attach_devices(void) {
	int i;
	int result;

	rtc_attach((void*)RTC_MMIO_BASE);

	for (i = 0; i < NUART; i++) attach_uart((void*)UART_MMIO_BASE(i), UART0_INTR_SRCNO + i);

	for (i = 0; i < NVIODEV; i++) attach_virtio((void*)VIRTIO_MMIO_BASE(i), VIRTIO0_INTR_SRCNO + i);

	result = mount_devfs(DEVMNTNAME);

	if (result != 0) {
		kprintf("mount_devfs(%s) failed: %s\n", CDEVNAME, error_name(result));
		halt_failure();
	}
}

void mount_cdrive(void) {
	struct storage* hd;
	struct cache* cache;
	int result;

	hd = find_storage(CDEVNAME, CDEVINST);

	if (hd == NULL) {
		kprintf("Storage device %s%d not found\n", CDEVNAME, CDEVINST);
		halt_failure();
	}

	result = storage_open(hd);

	if (result != 0) {
		kprintf("storage_open failed on %s%d: %s\n", CDEVNAME, CDEVINST, error_name(result));
		halt_failure();
	}

	result = create_cache(hd, &cache);

	if (result != 0) {
		kprintf("create_cache(%s%d) failed: %s\n", CDEVNAME, CDEVINST, error_name(result));
		halt_failure();
	}

	result = mount_ktfs(CMNTNAME, cache);

	if (result != 0) {
		kprintf("mount_ktfs(%s, cache(%s%d)) failed: %s\n", CMNTNAME, CDEVNAME, CDEVINST,
				error_name(result));
		halt_failure();
	}
}


