# See after_dark.h for an explanation of each symbol
.section .data

.extern toaster_start
.extern toast_start
.extern toasters
.extern toaster_cnt
.extern toast_list

# Declare globally each function, then write all functions here
.section .text

.global draw_sprite
.type draw_sprite, @function

.global draw_toaster
.type draw_toaster, @function

.global draw_toast
.type draw_toast, @function

.global move_toaster
.type move_toaster, @function

.global move_toast
.type move_toast, @function

.global add_toaster
.type add_toaster, @function

.global add_toast
.type add_toast, @function

.global remove_toaster
.type remove_toaster, @function

.global remove_toast
.type remove_toast, @function

# Define constants
.set SPRITE_SIZE, 64
.set SCREEN_WIDTH, 640
.set SCREEN_HEIGHT, 480
.set TOASTERS_MAX, 50

###### Here is an example of how you can create a "function" (label) and how you can access the variables that we have declared using .extern ######
###### THIS FUNCTION IS NOT COMPLETE,  AND YOU SHOULD NOT USE THIS CODE IN YOUR SUBMISSION ######
###### WE ARE SIMPLY PROVIDING A FEW EXAMPLES OF RISC-V ASSEMBLY CODE TO HELP YOU OUT ######
###### AFTER RUNNING "make test.elf" AND "make run-test" AND/OR "make debug-test" YOU SHOULD DELETE ALL OF THIS CODE ######

# void draw_sprite(const char* sprite_rgb24, uint32_t* fbuf_rgbx32, int16_t x, int16_t y)
# a0 = sprite_rgb24
# a1 = fbuf_rgbx32
# a2 = x (int16)
# a3 = y (int16)
draw_sprite:
    # Save ra
    addi    sp, sp, -8
    sd      ra, 0(sp)

    # null checks
    beqz    a0, ds_done
    beqz    a1, ds_done

    # Outer loop: y_iter
    li      t0, 0                   # y_iter = 0
outer_y:
    li      a4, SPRITE_SIZE
    bge     t0, a4, ds_done
    add     t1, a3, t0              # sprite_y = y + y_iter
    blt     t1, zero, skip_y
    li      a4, SCREEN_HEIGHT
    bge     t1, a4, skip_y
    # row_offset = sprite_y * SCREEN_WIDTH
    li      t2, SCREEN_WIDTH
    mul     t2, t1, t2              # t2 = row_offset

    # Inner loop: x_iter
    li      t3, 0                   # x_iter = 0
inner_x:
    li      a4, SPRITE_SIZE
    bge     t3, a4, skip_y
    add     t4, a2, t3              # sprite_x = x + x_iter
    blt     t4, zero, next_x
    li      a4, SCREEN_WIDTH
    bge     t4, a4, next_x

    # sprite_index = y_iter * SPRITE_SIZE + x_iter
    li      t5, SPRITE_SIZE
    mul     t5, t0, t5
    add     t5, t5, t3

    # byte_offset = sprite_index * 3
    slli    t6, t5, 1               # sprite_index * 2
    add     t6, t6, t5              # sprite_index * 3

    add     t6, a0, t6              # sprite_rgb24 + byte_offset
    lbu     t1, 0(t6)               # r 
    lbu     a4, 1(t6)               # g 
    lbu     a5, 2(t6)               # b

    # magenta check 
    li      t5, 0xFF
    beq     t1, t5, check_g
    j       draw_pixel
check_g:
    li      t5, 0x00
    beq     a4, t5, check_b
    j       draw_pixel
check_b:
    li      t5, 0xFF
    beq     a5, t5, next_x
draw_pixel:
    # fbuf_index = row_offset + sprite_x
    add     t5, t2, t4
    # byte_addr = fbuf + fbuf_index * 4
    slli    t5, t5, 2
    add     t5, a1, t5
    # rgb25 -> rgbx32 
    slli    t1, t1, 24
    slli    a4, a4, 16
    or      t1, t1, a4
    slli    a4, a5, 8
    or      t1, t1, a4
    sw      t1, 0(t5)
next_x:
    addi    t3, t3, 1
    j       inner_x
skip_y:
    addi    t0, t0, 1
    j       outer_y
ds_done:
    # Restore ra
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

# a0 = t
# a1 = fbuf_rgbx32
draw_toaster:
    # Save ra 
    addi    sp, sp, -8
    sd      ra, 0(sp)

    # Null checks 
    beqz    a0, dtstr_done
    beqz    a1, dtstr_done

    lbu     t0, 0(a0)           # t->phase
    
    # sprite_offset = phase * 64 * 64 * 3
    li      t1, 12288          # 64 * 64 * 3
    mul     t0, t0, t1

    # sprite_addr = toaster_start + sprite_offset
    la      t1, toaster_start
    add     t0, t0, t1          # t0 = &sprite_rgb24

    lh      a2, 2(a0)           # t->x
    lh      a3, 4(a0)           # t->y 
    
    # Set up arguments for draw_sprite call
    mv      a0, t0              # a0 = sprite_rgb24
    # a1 is alr fbuf ptr

    # Call draw_sprite
    call    draw_sprite

dtstr_done:
    # Restore ra 
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

draw_toast:
    # Save ra
    addi    sp, sp, -8
    sd      ra, 0(sp)

    # Null checks
    beqz    a0, dtst_done
    beqz    a1, dtst_done 

    lh      a2, 0(a0)           # t->x 
    lh      a3, 2(a0)           # t->y 

    la      a0, toast_start     # a0 = ptr to sprite_rgb24

    call draw_sprite
dtst_done:
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

move_toaster:
    addi    sp, sp, -8
    sd      ra, 0(sp)

    beqz    a0, mtstr_done

    # Decrement t->x 
    lh      t0, 2(a0)       # t->x 
    addi    t0, t0, -1
    sh      t0, 2(a0)

    # Increment t->y 
    lh      t0, 4(a0)       # t->y 
    addi    t0, t0, 1
    sh      t0, 4(a0)

    # Increment t->phase 
    lbu     t0, 0(a0)       # t->phase 
    addi    t0, t0, 1
    li      t1, 4
    remu    t0, t0, t1      # t0 = t0 % 4
    sb      t0, 0(a0)

mtstr_done:
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

move_toast:
    addi    sp, sp, -8
    sd      ra, 0(sp)

    beqz    a0, mtst_done

    # Decrement t->x 
    lh      t0, 0(a0)       # t->x 
    addi    t0, t0, -1
    sh      t0, 0(a0)

    # Increment t->y 
    lh      t0, 2(a0)       # t->y
    addi    t0, t0, 1
    sh      t0, 2(a0)

mtst_done:
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

add_toaster:
    addi    sp, sp, -8
    sd      ra, 0(sp)

    la      t0, toaster_cnt
    lhu     t1, 0(t0)       # toaster_cnt

    li      t2, TOASTERS_MAX
    bge     t1, t2, atstr_done

    # new toaster addr = toasters + toaster_cnt * sizeof(struct toaster)
    la      t3, toasters
    li      t2, 16          # sizeof(struct toaster) = 16 bytes 
    mul     t1, t1, t2
    add     t3, t3, t1      # t3 = &toasters[toaster_cnt]

    # Store phase 
    sb      a2, 0(t3)

    # Store x 
    sh      a0, 2(t3)

    # Store y 
    sh      a1, 4(t3)

    # Increment toaster_cnt
    lhu     t1, 0(t0)
    addi    t1, t1, 1 
    sh      t1, 0(t0)

atstr_done:
    ld      ra, 0(sp)
    addi    sp, sp, 8
    ret

add_toast:
    addi    sp, sp, -24
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    sd      s1, 16(sp)

    # Save a0 and a1 in s regs
    mv      s0, a0 
    mv      s1, a1

    # Call malloc(sizeof(struct toast))
    li      a0, 16
    call    malloc 
    beqz    a0, atst_done 
    mv      t0, a0          # t0 = new toast ptr 

    # Save old toast_list 
    la      t1, toast_list
    ld      t2, 0(t1)       # t2 = old head 

    # Initialize new toast 
    sh      s0, 0(t0)       # new_toast->x = x 
    sh      s1, 2(t0)       # new_toast->y = y 
    sd      t2, 8(t0)       # new_toast->next = old head 

    # Update toast_list to point to new toast 
    sd      t0, 0(t1)
atst_done:
    ld      ra, 0(sp)
    ld      s0, 8(sp)
    ld      s1, 16(sp)
    addi    sp, sp, 24
    ret

remove_toaster:
    addi    sp, sp, -24 
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    sd      s1, 16(sp)
    
    # Save a0 and a1
    mv      s0, a0 
    mv      s1, a1 

    # Load toaster_cnt 
    la      t1, toaster_cnt
    lhu     t2, 0(t1)
    li      t3, 0           # index = 0

loop_start:
    bge     t3, t2, rmtstr_done
    la      t4, toasters 
    slli    t5, t3, 4       # index * sizeof(struct toaster) = index * 16 
    add     t4, t4, t5
    lh      t6, 2(t4)       # current x 
    lh      t0, 4(t4)       # current y 

    bne     t6, s0, next_idx 
    bne     t0, s1, next_idx

    # Match found - replace with last toaster 
    addi    t2, t2, -1      # decrement toaster_cnt 
    slli    t5, t2, 4       # last_idx * 16 
    la      t0, toasters
    add     t0, t0, t5      # address of last toaster 

    # copy last toaster over current slot 
    ld      a2, 0(t0)
    sd      a2, 0(t4)
    ld      a2, 8(t0)
    sd      a2, 8(t4)

    # Update toaster_cnt 
    sh      t2, 0(t1)
    j       rmtstr_done

next_idx:
    addi    t3, t3, 1
    j       loop_start
rmtstr_done:
    ld      ra, 0(sp)
    ld      s0, 8(sp)
    ld      s1, 16(sp)
    addi    sp, sp, 24
    ret

remove_toast:
    addi    sp, sp, -32 
    sd      ra, 0(sp)
    sd      s0, 8(sp)
    sd      s1, 16(sp)
    sd      s2, 24(sp)

    # Save a0 and a1 in s regs 
    mv      s0, a0 
    mv      s1, a1 
    
    # Load toast_list 
    la      t0, toast_list
    ld      t1, 0(t0)           # t1 = head pointer 
    beqz    t1, rmtst_done 

    # Check if head matches 
    lh      t2, 0(t1)           # t1->x 
    lh      t3, 2(t1)           # t1->y 
    bne     t2, s0, check_next 
    bne     t3, s1, check_next 

    # Head matches, remove head 
    ld      t2, 8(t1)           # t1->next 
    sd      t2, 0(t0)           # toast_list = t1->next 
    mv      a0, t1 
    call    free
    j       rmtst_done

check_next:
    mv      s2, t1              # s2 = current toast ptr 
loop:
    ld      t1, 8(s2)           # t1 = s2->next 
    beqz    t1, rmtst_done
    lh      t2, 0(t1)           # next->x 
    lh      t3, 2(t1)           # next->y 
    bne     t2, s0, loop_continue
    bne     t3, s1, loop_continue

    # Match found
    ld      t4, 8(t1)           # next->next 
    sd      t4, 8(s2)           # s2->next = next->next 
    mv      a0, t1 
    call    free 
    j       rmtst_done
loop_continue:
    mv      s2, t1 
    j       loop 
rmtst_done:
    ld      ra, 0(sp)
    ld      s0, 8(sp)
    ld      s1, 16(sp)
    ld      s2, 24(sp)
    addi    sp, sp, 32
    ret
