.section .text.boot, "xa", @progbits
.balign 4


.global boot

boot:
	la		t0,	boot_table_pt_2 # address of boot table which is level 2

	li		t1,	0x80000000 # physical address to store in level 2 table

	# figure out which memory location / entry in the page table to store to
	mv		t2, t1 # vma = pma
	srli	t2, t2, 30 # vpn[2]
	andi	t2, t2, 0x1FF # 9 bits of index
	slli	t2, t2, 3 # multiply by 8 to get memory address
	add		t2, t0, t2 # t2 gets memory address of pte

	# create a page table entry whose physical page number is the above address and with D, A, V, R, W, and X flags set
	srli	t1, t1,	12
	slli	t1,	t1, 10
	ori		t1, t1, 0b11001111

	# store pte into table
	sd		t1, 0(t2)

	# repeat this process for the virtual addresses:
	# - 0xFFFFFFC000000000
	# - 0xFFFFFFC040000000
	# - 0xFFFFFFC080000000


	li		t1,	0x00000000 # physical address to store in level 2 table

	# figure out which memory location / entry in the page table to store to
	li		t2, 0xFFFFFFC000000000 # virtual address 
	srli	t2, t2, 30
	andi	t2, t2, 0x1FF
	slli	t2, t2, 3
	add		t2, t0, t2

	# create a page table entry whose physical page number is the above address and with D, A, V, R, W, and X flags set
	srli	t1,	t1, 12
	slli	t1, t1,	10
	ori		t1, t1, 0b11001111

	# store pte into table
	sd		t1, 0(t2)

	
	li		t1,	0x40000000 # physical address to store in level 2 table

	# figure out which memory location / entry in the page table to store to
	li		t2, 0xFFFFFFC040000000 # virtual address
	srli	t2, t2, 30
	andi	t2, t2, 0x1FF
	slli	t2, t2, 3
	add		t2, t0, t2

	# create a page table entry whose physical page number is the above address and with D, A, V, R, W, and X flags set
	srli	t1,	t1, 12
	slli	t1,	t1, 10
	ori		t1, t1, 0b11001111

	# store pte into table
	sd		t1, 0(t2)


	li		t1,	0x80000000 # physical address to store in level 2 table

	# figure out which memory location / entry in the page table to store to
	li		t2, 0xFFFFFFC080000000 # virtual address
	srli	t2, t2, 30
	andi	t2, t2, 0x1FF
	slli	t2, t2, 3
	add		t2, t0, t2

	# create a page table entry whose physical page number is the above address and with V, R, W, and X flags set
	srli	t1,	t1, 12
	slli	t1,	t1, 10
	ori		t1, t1, 0b1111

	# store pte into table
	sd		t1, 0(t2)


	# enable sv39
	srli	t0, t0, 12 # ppn of table
	li		t1, 8 # mode = 8 for sv39
	slli	t1, t1, 60 # mode is the upper 4 bits of satp
	or		t0, t0, t1 # t0 contains the value to write into satp
	csrw	satp, t0 # enable memory translation

	sfence.vma x0, x0 # flush the tlb


.extern _smode_trap_entry

	la		t0, _smode_trap_entry
	csrw	stvec, t0
	csrs	scounteren, 7
	csrw	sscratch, x0
	mv		fp, x0
	la		sp, _main_stack_anchor
	la		ra, sbi_shutdown

	la		t0, main_func
	ld		t0, 0(t0)
	jr		t0





.section .bss.pagetable, "wa"

.equ PAGE_SIZE, 4096

.balign 4096
boot_table_pt_2:
	.skip PAGE_SIZE


.section .data, "wx", @progbits
.extern main # main is defined externally

main_func:
	.dword main


.section	.data.stack, "wa", @progbts
.balign		16

    .equ		MAIN_STACK_SIZE, 4096

    .global		_main_stack_lowest
    .type		_main_stack_lowest, @object
    .size		_main_stack_lowest, MAIN_STACK_SIZE

    .global		_main_stack_anchor
    .type		_main_stack_anchor, @object
    .size		_main_stack_anchor, 16

_main_stack_lowest:
    .fill   MAIN_STACK_SIZE, 1, 0xA5

_main_stack_anchor:
    .dword  0 # ktp
    .dword  0 # kgp
    .end
