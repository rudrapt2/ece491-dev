	.file	"mp1.c"
	.option nopic
	.option norelax
	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.file 1 "mp1.c"
	.align	2
	.globl	add_star
	.type	add_star, @function
add_star:
.LFB0:
	.loc 1 10 53
	.cfi_startproc
	addi	sp,sp,-48
.LCFI0:
	.cfi_def_cfa_offset 48
	sd	ra,40(sp)
	sd	s0,32(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,48
.LCFI1:
	.cfi_def_cfa 8, 0
	mv	a5,a0
	mv	a3,a1
	mv	a4,a2
	sw	a5,-36(s0)
	mv	a5,a3
	sw	a5,-40(s0)
	mv	a5,a4
	sw	a5,-44(s0)
	.loc 1 15 12
	li	a0,24
	call	kmalloc
	sd	a0,-24(s0)
	.loc 1 17 8
	ld	a5,-24(s0)
	beq	a5,zero,.L4
	.loc 1 20 13
	ld	a5,-24(s0)
	lw	a4,-36(s0)
	sw	a4,8(a5)
	.loc 1 21 13
	ld	a5,-24(s0)
	lw	a4,-40(s0)
	sw	a4,12(a5)
	.loc 1 22 17
	ld	a5,-24(s0)
	lw	a4,-44(s0)
	sw	a4,16(a5)
	.loc 1 23 16
	lla	a5,skyline_star_list
	ld	a4,0(a5)
	ld	a5,-24(s0)
	sd	a4,0(a5)
	.loc 1 24 23
	lla	a5,skyline_star_list
	ld	a4,-24(s0)
	sd	a4,0(a5)
	j	.L1
.L4:
	.loc 1 18 9
	nop
.L1:
	.loc 1 25 1
	ld	ra,40(sp)
	.cfi_restore 1
	ld	s0,32(sp)
.LCFI2:
	.cfi_restore 8
	.cfi_def_cfa 2, 48
	addi	sp,sp,48
.LCFI3:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE0:
	.size	add_star, .-add_star
	.align	2
	.globl	remove_star
	.type	remove_star, @function
remove_star:
.LFB1:
	.loc 1 27 40
	.cfi_startproc
	addi	sp,sp,-48
.LCFI4:
	.cfi_def_cfa_offset 48
	sd	ra,40(sp)
	sd	s0,32(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,48
.LCFI5:
	.cfi_def_cfa 8, 0
	mv	a5,a0
	mv	a4,a1
	sw	a5,-36(s0)
	mv	a5,a4
	sw	a5,-40(s0)
	.loc 1 31 11
	lla	a5,skyline_star_list
	sd	a5,-24(s0)
	.loc 1 33 11
	j	.L6
.L9:
	.loc 1 34 14
	ld	a5,-24(s0)
	ld	a5,0(a5)
	sd	a5,-32(s0)
	.loc 1 35 17
	ld	a5,-32(s0)
	lw	a5,8(a5)
	.loc 1 35 12
	lw	a4,-36(s0)
	sext.w	a4,a4
	bne	a4,a5,.L7
	.loc 1 35 33 discriminator 1
	ld	a5,-32(s0)
	lw	a5,12(a5)
	.loc 1 35 26 discriminator 1
	lw	a4,-40(s0)
	sext.w	a4,a4
	bne	a4,a5,.L7
	.loc 1 36 26
	ld	a5,-32(s0)
	ld	a4,0(a5)
	.loc 1 36 20
	ld	a5,-24(s0)
	sd	a4,0(a5)
	.loc 1 37 13
	ld	a0,-32(s0)
	call	kfree
	j	.L6
.L7:
	.loc 1 39 19
	ld	a5,-32(s0)
	sd	a5,-24(s0)
.L6:
	.loc 1 33 12
	ld	a5,-24(s0)
	ld	a5,0(a5)
	.loc 1 33 19
	bne	a5,zero,.L9
	.loc 1 41 1
	nop
	nop
	ld	ra,40(sp)
	.cfi_restore 1
	ld	s0,32(sp)
.LCFI6:
	.cfi_restore 8
	.cfi_def_cfa 2, 48
	addi	sp,sp,48
.LCFI7:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE1:
	.size	remove_star, .-remove_star
	.align	2
	.globl	draw_star
	.type	draw_star, @function
draw_star:
.LFB2:
	.loc 1 43 67
	.cfi_startproc
	addi	sp,sp,-32
.LCFI8:
	.cfi_def_cfa_offset 32
	sd	ra,24(sp)
	sd	s0,16(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,32
.LCFI9:
	.cfi_def_cfa 8, 0
	sd	a0,-24(s0)
	sd	a1,-32(s0)
	.loc 1 44 8
	ld	a5,-32(s0)
	beq	a5,zero,.L14
	.loc 1 47 8
	ld	a5,-24(s0)
	beq	a5,zero,.L15
	.loc 1 50 18
	ld	a5,-32(s0)
	lw	a5,8(a5)
	.loc 1 50 8
	blt	a5,zero,.L10
	.loc 1 50 29 discriminator 1
	ld	a5,-32(s0)
	lw	a4,8(a5)
	.loc 1 50 22 discriminator 1
	li	a5,639
	bgt	a4,a5,.L10
	.loc 1 50 61 discriminator 2
	ld	a5,-32(s0)
	lw	a5,12(a5)
	.loc 1 50 49 discriminator 2
	blt	a5,zero,.L10
	.loc 1 50 72 discriminator 3
	ld	a5,-32(s0)
	lw	a4,12(a5)
	.loc 1 50 65 discriminator 3
	li	a5,479
	bgt	a4,a5,.L10
	.loc 1 51 18
	ld	a5,-32(s0)
	lw	a4,8(a5)
	.loc 1 51 28
	ld	a5,-32(s0)
	lw	a5,12(a5)
	.loc 1 51 32
	mv	a3,a5
	mv	a5,a3
	slliw	a5,a5,2
	addw	a5,a5,a3
	slliw	a5,a5,7
	sext.w	a5,a5
	.loc 1 51 22
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 51 13
	slli	a5,a5,2
	ld	a4,-24(s0)
	add	a5,a4,a5
	.loc 1 51 55
	ld	a4,-32(s0)
	lw	a4,16(a4)
	.loc 1 51 49
	sw	a4,0(a5)
	j	.L10
.L14:
	.loc 1 45 9
	nop
	j	.L10
.L15:
	.loc 1 48 9
	nop
.L10:
	.loc 1 53 1
	ld	ra,24(sp)
	.cfi_restore 1
	ld	s0,16(sp)
.LCFI10:
	.cfi_restore 8
	.cfi_def_cfa 2, 32
	addi	sp,sp,32
.LCFI11:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE2:
	.size	draw_star, .-draw_star
	.align	2
	.globl	add_window
	.type	add_window, @function
add_window:
.LFB3:
	.loc 1 59 1
	.cfi_startproc
	addi	sp,sp,-48
.LCFI12:
	.cfi_def_cfa_offset 48
	sd	ra,40(sp)
	sd	s0,32(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,48
.LCFI13:
	.cfi_def_cfa 8, 0
	mv	a5,a0
	sw	a5,-36(s0)
	mv	a5,a1
	sw	a5,-40(s0)
	mv	a5,a2
	sb	a5,-41(s0)
	mv	a5,a3
	sb	a5,-42(s0)
	mv	a5,a4
	sw	a5,-48(s0)
	.loc 1 62 25
	lla	a5,skyline_win_cnt
	lhu	a5,0(a5)
	.loc 1 62 8
	sext.w	a4,a5
	li	a5,4096
	addi	a5,a5,-97
	bgtu	a4,a5,.L19
	.loc 1 67 44
	lla	a5,skyline_win_cnt
	lhu	a5,0(a5)
	addiw	a4,a5,1
	slli	a3,a4,48
	srli	a3,a3,48
	lla	a4,skyline_win_cnt
	sh	a3,0(a4)
	.loc 1 67 27
	slli	a4,a5,4
	.loc 1 67 9
	lla	a5,skyline_windows
	add	a5,a4,a5
	sd	a5,-24(s0)
	.loc 1 69 12
	ld	a5,-24(s0)
	lw	a4,-36(s0)
	sw	a4,0(a5)
	.loc 1 70 12
	ld	a5,-24(s0)
	lw	a4,-40(s0)
	sw	a4,4(a5)
	.loc 1 71 12
	ld	a5,-24(s0)
	lbu	a4,-41(s0)
	sb	a4,8(a5)
	.loc 1 72 12
	ld	a5,-24(s0)
	lbu	a4,-42(s0)
	sb	a4,9(a5)
	.loc 1 73 16
	ld	a5,-24(s0)
	lw	a4,-48(s0)
	sw	a4,12(a5)
	j	.L16
.L19:
	.loc 1 63 9
	nop
.L16:
	.loc 1 74 1
	ld	ra,40(sp)
	.cfi_restore 1
	ld	s0,32(sp)
.LCFI14:
	.cfi_restore 8
	.cfi_def_cfa 2, 48
	addi	sp,sp,48
.LCFI15:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE3:
	.size	add_window, .-add_window
	.align	2
	.globl	remove_window
	.type	remove_window, @function
remove_window:
.LFB4:
	.loc 1 76 42
	.cfi_startproc
	addi	sp,sp,-64
.LCFI16:
	.cfi_def_cfa_offset 64
	sd	ra,56(sp)
	sd	s0,48(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,64
.LCFI17:
	.cfi_def_cfa 8, 0
	mv	a5,a0
	mv	a4,a1
	sw	a5,-52(s0)
	mv	a5,a4
	sw	a5,-56(s0)
	.loc 1 81 25
	lla	a5,skyline_win_cnt
	lhu	a5,0(a5)
	.loc 1 81 8
	beq	a5,zero,.L26
	.loc 1 84 12
	sw	zero,-20(s0)
	.loc 1 84 5
	j	.L23
.L25:
	.loc 1 85 31
	lw	a5,-20(s0)
	slli	a4,a5,4
	.loc 1 85 13
	lla	a5,skyline_windows
	add	a5,a4,a5
	sd	a5,-32(s0)
	.loc 1 86 16
	ld	a5,-32(s0)
	lw	a5,0(a5)
	.loc 1 86 12
	lw	a4,-52(s0)
	sext.w	a4,a4
	bne	a4,a5,.L24
	.loc 1 86 31 discriminator 1
	ld	a5,-32(s0)
	lw	a5,4(a5)
	.loc 1 86 25 discriminator 1
	lw	a4,-56(s0)
	sext.w	a4,a4
	bne	a4,a5,.L24
	.loc 1 87 36
	lla	a5,skyline_win_cnt
	lhu	a5,0(a5)
	slli	a5,a5,4
	.loc 1 87 54
	addi	a4,a5,-16
	.loc 1 87 18
	lla	a5,skyline_windows
	add	a5,a4,a5
	sd	a5,-40(s0)
	.loc 1 88 26
	ld	a5,-40(s0)
	lw	a4,0(a5)
	.loc 1 88 20
	ld	a5,-32(s0)
	sw	a4,0(a5)
	.loc 1 89 26
	ld	a5,-40(s0)
	lw	a4,4(a5)
	.loc 1 89 20
	ld	a5,-32(s0)
	sw	a4,4(a5)
	.loc 1 90 26
	ld	a5,-40(s0)
	lbu	a4,8(a5)
	.loc 1 90 20
	ld	a5,-32(s0)
	sb	a4,8(a5)
	.loc 1 91 26
	ld	a5,-40(s0)
	lbu	a4,9(a5)
	.loc 1 91 20
	ld	a5,-32(s0)
	sb	a4,9(a5)
	.loc 1 92 30
	ld	a5,-40(s0)
	lw	a4,12(a5)
	.loc 1 92 24
	ld	a5,-32(s0)
	sw	a4,12(a5)
	.loc 1 93 29
	lla	a5,skyline_win_cnt
	lhu	a5,0(a5)
	addiw	a5,a5,-1
	slli	a4,a5,48
	srli	a4,a4,48
	lla	a5,skyline_win_cnt
	sh	a4,0(a5)
.L24:
	.loc 1 84 39 discriminator 2
	lw	a5,-20(s0)
	addiw	a5,a5,1
	sw	a5,-20(s0)
.L23:
	.loc 1 84 19 discriminator 1
	lla	a5,skyline_win_cnt
	lhu	a5,0(a5)
	sext.w	a5,a5
	lw	a4,-20(s0)
	sext.w	a4,a4
	blt	a4,a5,.L25
	j	.L20
.L26:
	.loc 1 82 9
	nop
.L20:
	.loc 1 96 1
	ld	ra,56(sp)
	.cfi_restore 1
	ld	s0,48(sp)
.LCFI18:
	.cfi_restore 8
	.cfi_def_cfa 2, 64
	addi	sp,sp,64
.LCFI19:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE4:
	.size	remove_window, .-remove_window
	.align	2
	.globl	draw_window
	.type	draw_window, @function
draw_window:
.LFB5:
	.loc 1 98 69
	.cfi_startproc
	addi	sp,sp,-48
.LCFI20:
	.cfi_def_cfa_offset 48
	sd	ra,40(sp)
	sd	s0,32(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,48
.LCFI21:
	.cfi_def_cfa 8, 0
	sd	a0,-40(s0)
	sd	a1,-48(s0)
	.loc 1 99 8
	ld	a5,-48(s0)
	beq	a5,zero,.L41
	.loc 1 102 8
	ld	a5,-40(s0)
	beq	a5,zero,.L42
.LBB2:
	.loc 1 107 14
	sw	zero,-20(s0)
	.loc 1 107 5
	j	.L31
.L40:
	.loc 1 108 18
	ld	a5,-48(s0)
	lw	a5,4(a5)
	.loc 1 108 22
	lw	a4,-20(s0)
	addw	a5,a4,a5
	sext.w	a4,a5
	.loc 1 108 12
	li	a5,479
	bgt	a4,a5,.L43
	.loc 1 108 59 discriminator 2
	ld	a5,-48(s0)
	lw	a5,4(a5)
	.loc 1 108 63 discriminator 2
	lw	a4,-20(s0)
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 108 51 discriminator 2
	blt	a5,zero,.L43
.LBB3:
	.loc 1 111 18
	sw	zero,-24(s0)
	.loc 1 111 9
	j	.L35
.L39:
	.loc 1 112 22
	ld	a5,-48(s0)
	lw	a5,0(a5)
	.loc 1 112 26
	lw	a4,-24(s0)
	addw	a5,a4,a5
	sext.w	a4,a5
	.loc 1 112 16
	li	a5,639
	bgt	a4,a5,.L44
	.loc 1 112 62 discriminator 2
	ld	a5,-48(s0)
	lw	a5,0(a5)
	.loc 1 112 66 discriminator 2
	lw	a4,-24(s0)
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 112 54 discriminator 2
	blt	a5,zero,.L44
	.loc 1 115 22
	ld	a5,-48(s0)
	lw	a5,4(a5)
	.loc 1 115 26
	lw	a4,-20(s0)
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 115 36
	mv	a4,a5
	mv	a5,a4
	slliw	a5,a5,2
	addw	a5,a5,a4
	slliw	a5,a5,7
	sext.w	a4,a5
	.loc 1 115 58
	ld	a5,-48(s0)
	lw	a5,0(a5)
	.loc 1 115 62
	lw	a3,-24(s0)
	addw	a5,a3,a5
	sext.w	a5,a5
	.loc 1 115 52
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 115 17
	slli	a5,a5,2
	ld	a4,-40(s0)
	add	a5,a4,a5
	.loc 1 115 78
	ld	a4,-48(s0)
	lw	a4,12(a4)
	.loc 1 115 73
	sw	a4,0(a5)
	j	.L38
.L44:
	.loc 1 113 17
	nop
.L38:
	.loc 1 111 53 discriminator 2
	lw	a5,-24(s0)
	addiw	a5,a5,1
	sw	a5,-24(s0)
.L35:
	.loc 1 111 42 discriminator 1
	ld	a5,-48(s0)
	lbu	a5,8(a5)
	sext.w	a5,a5
	.loc 1 111 37 discriminator 1
	lw	a4,-24(s0)
	sext.w	a4,a4
	blt	a4,a5,.L39
	j	.L34
.L43:
.LBE3:
	.loc 1 109 13
	nop
.L34:
	.loc 1 107 49 discriminator 2
	lw	a5,-20(s0)
	addiw	a5,a5,1
	sw	a5,-20(s0)
.L31:
	.loc 1 107 38 discriminator 1
	ld	a5,-48(s0)
	lbu	a5,9(a5)
	sext.w	a5,a5
	.loc 1 107 33 discriminator 1
	lw	a4,-20(s0)
	sext.w	a4,a4
	blt	a4,a5,.L40
	j	.L27
.L41:
.LBE2:
	.loc 1 100 9
	nop
	j	.L27
.L42:
	.loc 1 103 9
	nop
.L27:
	.loc 1 118 1
	ld	ra,40(sp)
	.cfi_restore 1
	ld	s0,32(sp)
.LCFI22:
	.cfi_restore 8
	.cfi_def_cfa 2, 48
	addi	sp,sp,48
.LCFI23:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE5:
	.size	draw_window, .-draw_window
	.align	2
	.globl	start_beacon
	.type	start_beacon, @function
start_beacon:
.LFB6:
	.loc 1 127 1
	.cfi_startproc
	addi	sp,sp,-48
.LCFI24:
	.cfi_def_cfa_offset 48
	sd	ra,40(sp)
	sd	s0,32(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,48
.LCFI25:
	.cfi_def_cfa 8, 0
	sd	a0,-24(s0)
	mv	a0,a1
	mv	a1,a2
	mv	a2,a3
	mv	a3,a4
	mv	a4,a5
	mv	a5,a0
	sw	a5,-28(s0)
	mv	a5,a1
	sw	a5,-32(s0)
	mv	a5,a2
	sb	a5,-33(s0)
	mv	a5,a3
	sh	a5,-36(s0)
	mv	a5,a4
	sh	a5,-38(s0)
	.loc 1 128 24
	lla	a5,skyline_beacon
	ld	a4,-24(s0)
	sd	a4,0(a5)
	.loc 1 129 22
	lla	a5,skyline_beacon
	lw	a4,-28(s0)
	sw	a4,8(a5)
	.loc 1 130 22
	lla	a5,skyline_beacon
	lw	a4,-32(s0)
	sw	a4,12(a5)
	.loc 1 131 24
	lla	a5,skyline_beacon
	lbu	a4,-33(s0)
	sb	a4,16(a5)
	.loc 1 132 27
	lla	a5,skyline_beacon
	lhu	a4,-36(s0)
	sh	a4,18(a5)
	.loc 1 133 27
	lla	a5,skyline_beacon
	lhu	a4,-38(s0)
	sh	a4,20(a5)
	.loc 1 134 1
	nop
	ld	ra,40(sp)
	.cfi_restore 1
	ld	s0,32(sp)
.LCFI26:
	.cfi_restore 8
	.cfi_def_cfa 2, 48
	addi	sp,sp,48
.LCFI27:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE6:
	.size	start_beacon, .-start_beacon
	.align	2
	.globl	draw_beacon
	.type	draw_beacon, @function
draw_beacon:
.LFB7:
	.loc 1 140 1
	.cfi_startproc
	addi	sp,sp,-80
.LCFI28:
	.cfi_def_cfa_offset 80
	sd	ra,72(sp)
	sd	s0,64(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	addi	s0,sp,80
.LCFI29:
	.cfi_def_cfa 8, 0
	sd	a0,-56(s0)
	sd	a1,-64(s0)
	sd	a2,-72(s0)
	.loc 1 141 8
	ld	a5,-72(s0)
	beq	a5,zero,.L63
	.loc 1 144 8
	ld	a5,-56(s0)
	beq	a5,zero,.L64
	.loc 1 154 12
	ld	a5,-72(s0)
	ld	a5,0(a5)
	.loc 1 154 8
	beq	a5,zero,.L65
	.loc 1 154 32 discriminator 1
	ld	a5,-72(s0)
	lhu	a5,18(a5)
	.loc 1 154 26 discriminator 1
	beq	a5,zero,.L65
	.loc 1 154 52 discriminator 2
	ld	a5,-72(s0)
	lbu	a5,16(a5)
	.loc 1 154 46 discriminator 2
	beq	a5,zero,.L65
	.loc 1 159 16
	ld	a5,-72(s0)
	lhu	a5,18(a5)
	mv	a4,a5
	.loc 1 159 11
	ld	a5,-64(s0)
	remu	a5,a5,a4
	.loc 1 159 31
	ld	a4,-72(s0)
	lhu	a4,20(a4)
	.loc 1 159 8
	bgeu	a5,a4,.L66
	.loc 1 162 12
	sw	zero,-20(s0)
	.loc 1 162 5
	j	.L53
.L62:
	.loc 1 163 35
	ld	a5,-72(s0)
	lw	a5,12(a5)
	.loc 1 163 39
	lw	a4,-20(s0)
	addw	a5,a4,a5
	sext.w	a4,a5
	.loc 1 163 12
	li	a5,479
	bgt	a4,a5,.L67
	.loc 1 163 51 discriminator 2
	ld	a5,-72(s0)
	lw	a5,12(a5)
	.loc 1 163 55 discriminator 2
	lw	a4,-20(s0)
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 163 44 discriminator 2
	blt	a5,zero,.L67
	.loc 1 166 21
	ld	a5,-72(s0)
	ld	a4,0(a5)
	.loc 1 166 36
	ld	a5,-72(s0)
	lbu	a5,16(a5)
	sext.w	a5,a5
	.loc 1 166 31
	lw	a3,-20(s0)
	mulw	a5,a3,a5
	sext.w	a5,a5
	.loc 1 166 27
	slli	a5,a5,2
	.loc 1 166 16
	add	a5,a4,a5
	sd	a5,-32(s0)
	.loc 1 167 26
	lw	a5,-20(s0)
	mv	a4,a5
	mv	a5,a4
	slliw	a5,a5,2
	addw	a5,a5,a4
	slliw	a5,a5,7
	sext.w	a5,a5
	.loc 1 167 22
	slli	a5,a5,2
	.loc 1 167 15
	ld	a4,-56(s0)
	add	a5,a4,a5
	sd	a5,-40(s0)
	.loc 1 168 21
	ld	a5,-72(s0)
	lw	a4,8(a5)
	.loc 1 168 30
	ld	a5,-72(s0)
	lw	a5,12(a5)
	.loc 1 168 34
	mv	a3,a5
	mv	a5,a3
	slliw	a5,a5,2
	addw	a5,a5,a3
	slliw	a5,a5,7
	sext.w	a5,a5
	.loc 1 168 25
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 168 15
	slli	a5,a5,2
	ld	a4,-40(s0)
	add	a5,a4,a5
	sd	a5,-40(s0)
	.loc 1 170 16
	sw	zero,-24(s0)
	.loc 1 170 9
	j	.L57
.L61:
	.loc 1 171 38
	ld	a5,-72(s0)
	lw	a5,8(a5)
	.loc 1 171 42
	lw	a4,-24(s0)
	addw	a5,a4,a5
	sext.w	a4,a5
	.loc 1 171 16
	li	a5,639
	bgt	a4,a5,.L68
	.loc 1 171 54 discriminator 2
	ld	a5,-72(s0)
	lw	a5,8(a5)
	.loc 1 171 58 discriminator 2
	lw	a4,-24(s0)
	addw	a5,a4,a5
	sext.w	a5,a5
	.loc 1 171 47 discriminator 2
	blt	a5,zero,.L68
	.loc 1 174 31
	ld	a4,-32(s0)
	addi	a5,a4,4
	sd	a5,-32(s0)
	.loc 1 174 19
	ld	a5,-40(s0)
	addi	a3,a5,4
	sd	a3,-40(s0)
	.loc 1 174 24
	lw	a4,0(a4)
	.loc 1 174 22
	sw	a4,0(a5)
	j	.L60
.L68:
	.loc 1 172 17
	nop
.L60:
	.loc 1 170 36 discriminator 2
	lw	a5,-24(s0)
	addiw	a5,a5,1
	sw	a5,-24(s0)
.L57:
	.loc 1 170 28 discriminator 1
	ld	a5,-72(s0)
	lbu	a5,16(a5)
	sext.w	a5,a5
	.loc 1 170 23 discriminator 1
	lw	a4,-24(s0)
	sext.w	a4,a4
	blt	a4,a5,.L61
	j	.L56
.L67:
	.loc 1 164 13
	nop
.L56:
	.loc 1 162 32 discriminator 2
	lw	a5,-20(s0)
	addiw	a5,a5,1
	sw	a5,-20(s0)
.L53:
	.loc 1 162 24 discriminator 1
	ld	a5,-72(s0)
	lbu	a5,16(a5)
	sext.w	a5,a5
	.loc 1 162 19 discriminator 1
	lw	a4,-20(s0)
	sext.w	a4,a4
	blt	a4,a5,.L62
	j	.L46
.L63:
	.loc 1 142 9
	nop
	j	.L46
.L64:
	.loc 1 145 9
	nop
	j	.L46
.L65:
	.loc 1 155 9
	nop
	j	.L46
.L66:
	.loc 1 160 9
	nop
.L46:
	.loc 1 177 1
	ld	ra,72(sp)
	.cfi_restore 1
	ld	s0,64(sp)
.LCFI30:
	.cfi_restore 8
	.cfi_def_cfa 2, 80
	addi	sp,sp,80
.LCFI31:
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE7:
	.size	draw_beacon, .-draw_beacon
.Letext0:
	.file 2 "/opt/toolchains/riscv/lib/gcc/riscv64-unknown-elf/14.2.0/include/stdint-gcc.h"
	.file 3 "skyline.h"
	.file 4 "/opt/toolchains/riscv/lib/gcc/riscv64-unknown-elf/14.2.0/include/stddef.h"
	.file 5 "heap.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.4byte	0x5d4
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x8
	.uleb128 0x1
	.4byte	.LASF579
	.byte	0xc
	.4byte	.LASF580
	.string	"."
	.8byte	.Ltext0
	.8byte	.Letext0
	.4byte	.Ldebug_line0
	.4byte	.Ldebug_macro0
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF538
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF539
	.uleb128 0x3
	.4byte	.LASF541
	.byte	0x2
	.byte	0x28
	.byte	0x18
	.4byte	0x49
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF540
	.uleb128 0x3
	.4byte	.LASF542
	.byte	0x2
	.byte	0x2e
	.byte	0x18
	.4byte	0x63
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF543
	.uleb128 0x3
	.4byte	.LASF544
	.byte	0x2
	.byte	0x31
	.byte	0x19
	.4byte	0x76
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF545
	.uleb128 0x3
	.4byte	.LASF546
	.byte	0x2
	.byte	0x34
	.byte	0x19
	.4byte	0x8e
	.uleb128 0x5
	.4byte	0x7d
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF547
	.uleb128 0x3
	.4byte	.LASF548
	.byte	0x2
	.byte	0x37
	.byte	0x19
	.4byte	0xa1
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF549
	.uleb128 0x6
	.4byte	.LASF552
	.byte	0x18
	.byte	0x3
	.byte	0xd
	.byte	0x8
	.4byte	0xee
	.uleb128 0x7
	.4byte	.LASF550
	.byte	0x3
	.byte	0xe
	.byte	0x1b
	.4byte	0xf3
	.byte	0x2
	.byte	0x23
	.uleb128 0
	.uleb128 0x8
	.string	"x"
	.byte	0x3
	.byte	0xf
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x23
	.uleb128 0x8
	.uleb128 0x8
	.string	"y"
	.byte	0x3
	.byte	0x10
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x23
	.uleb128 0xc
	.uleb128 0x7
	.4byte	.LASF551
	.byte	0x3
	.byte	0x11
	.byte	0xe
	.4byte	0x7d
	.byte	0x2
	.byte	0x23
	.uleb128 0x10
	.byte	0
	.uleb128 0x5
	.4byte	0xa8
	.uleb128 0x9
	.byte	0x8
	.4byte	0xa8
	.uleb128 0x6
	.4byte	.LASF553
	.byte	0x10
	.byte	0x3
	.byte	0x14
	.byte	0x8
	.4byte	0x14a
	.uleb128 0x8
	.string	"x"
	.byte	0x3
	.byte	0x15
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x23
	.uleb128 0
	.uleb128 0x8
	.string	"y"
	.byte	0x3
	.byte	0x16
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x23
	.uleb128 0x4
	.uleb128 0x8
	.string	"w"
	.byte	0x3
	.byte	0x17
	.byte	0xd
	.4byte	0x57
	.byte	0x2
	.byte	0x23
	.uleb128 0x8
	.uleb128 0x8
	.string	"h"
	.byte	0x3
	.byte	0x18
	.byte	0xd
	.4byte	0x57
	.byte	0x2
	.byte	0x23
	.uleb128 0x9
	.uleb128 0x7
	.4byte	.LASF551
	.byte	0x3
	.byte	0x19
	.byte	0xe
	.4byte	0x7d
	.byte	0x2
	.byte	0x23
	.uleb128 0xc
	.byte	0
	.uleb128 0x5
	.4byte	0xf9
	.uleb128 0x6
	.4byte	.LASF554
	.byte	0x18
	.byte	0x3
	.byte	0x1c
	.byte	0x8
	.4byte	0x1b3
	.uleb128 0x8
	.string	"img"
	.byte	0x3
	.byte	0x1d
	.byte	0x16
	.4byte	0x1b8
	.byte	0x2
	.byte	0x23
	.uleb128 0
	.uleb128 0x8
	.string	"x"
	.byte	0x3
	.byte	0x1e
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x23
	.uleb128 0x8
	.uleb128 0x8
	.string	"y"
	.byte	0x3
	.byte	0x1f
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x23
	.uleb128 0xc
	.uleb128 0x8
	.string	"dia"
	.byte	0x3
	.byte	0x20
	.byte	0xd
	.4byte	0x57
	.byte	0x2
	.byte	0x23
	.uleb128 0x10
	.uleb128 0x7
	.4byte	.LASF555
	.byte	0x3
	.byte	0x21
	.byte	0xe
	.4byte	0x6a
	.byte	0x2
	.byte	0x23
	.uleb128 0x12
	.uleb128 0x7
	.4byte	.LASF556
	.byte	0x3
	.byte	0x22
	.byte	0xe
	.4byte	0x6a
	.byte	0x2
	.byte	0x23
	.uleb128 0x14
	.byte	0
	.uleb128 0x5
	.4byte	0x14f
	.uleb128 0x9
	.byte	0x8
	.4byte	0x89
	.uleb128 0xa
	.4byte	.LASF557
	.byte	0x3
	.byte	0x28
	.byte	0x1e
	.4byte	0xf3
	.byte	0x1
	.byte	0x1
	.uleb128 0xb
	.4byte	0xf9
	.4byte	0x1dd
	.uleb128 0xc
	.4byte	0xa1
	.2byte	0xf9f
	.byte	0
	.uleb128 0xa
	.4byte	.LASF558
	.byte	0x3
	.byte	0x2a
	.byte	0x1e
	.4byte	0x1cc
	.byte	0x1
	.byte	0x1
	.uleb128 0xa
	.4byte	.LASF559
	.byte	0x3
	.byte	0x2b
	.byte	0x11
	.4byte	0x6a
	.byte	0x1
	.byte	0x1
	.uleb128 0xa
	.4byte	.LASF554
	.byte	0x3
	.byte	0x2d
	.byte	0x1e
	.4byte	0x14f
	.byte	0x1
	.byte	0x1
	.uleb128 0x3
	.4byte	.LASF560
	.byte	0x4
	.byte	0xd6
	.byte	0x17
	.4byte	0xa1
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF561
	.uleb128 0x2
	.byte	0x10
	.byte	0x4
	.4byte	.LASF562
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF563
	.uleb128 0xd
	.byte	0x1
	.4byte	.LASF581
	.byte	0x5
	.byte	0x2b
	.byte	0xd
	.byte	0x1
	.byte	0x1
	.4byte	0x23d
	.uleb128 0xe
	.4byte	0x23d
	.byte	0
	.uleb128 0xf
	.byte	0x8
	.uleb128 0x10
	.byte	0x1
	.4byte	.LASF582
	.byte	0x5
	.byte	0x1c
	.byte	0xf
	.byte	0x1
	.4byte	0x23d
	.byte	0x1
	.4byte	0x258
	.uleb128 0xe
	.4byte	0x207
	.byte	0
	.uleb128 0x11
	.byte	0x1
	.4byte	.LASF567
	.byte	0x1
	.byte	0x88
	.byte	0x6
	.byte	0x1
	.8byte	.LFB7
	.8byte	.LFE7
	.4byte	.LLST7
	.byte	0x1
	.4byte	0x2e0
	.uleb128 0x12
	.4byte	.LASF564
	.byte	0x1
	.byte	0x89
	.byte	0x10
	.4byte	0x2e0
	.byte	0x2
	.byte	0x91
	.sleb128 -56
	.uleb128 0x13
	.string	"t"
	.byte	0x1
	.byte	0x8a
	.byte	0xe
	.4byte	0x95
	.byte	0x2
	.byte	0x91
	.sleb128 -64
	.uleb128 0x13
	.string	"bcn"
	.byte	0x1
	.byte	0x8b
	.byte	0x23
	.4byte	0x2e6
	.byte	0x3
	.byte	0x91
	.sleb128 -72
	.uleb128 0x14
	.string	"i"
	.byte	0x1
	.byte	0x93
	.byte	0x9
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 -20
	.uleb128 0x14
	.string	"j"
	.byte	0x1
	.byte	0x94
	.byte	0x9
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x15
	.4byte	.LASF565
	.byte	0x1
	.byte	0x95
	.byte	0x16
	.4byte	0x1b8
	.byte	0x2
	.byte	0x91
	.sleb128 -32
	.uleb128 0x15
	.4byte	.LASF566
	.byte	0x1
	.byte	0x96
	.byte	0x10
	.4byte	0x2e0
	.byte	0x2
	.byte	0x91
	.sleb128 -40
	.byte	0
	.uleb128 0x9
	.byte	0x8
	.4byte	0x7d
	.uleb128 0x9
	.byte	0x8
	.4byte	0x1b3
	.uleb128 0x11
	.byte	0x1
	.4byte	.LASF568
	.byte	0x1
	.byte	0x78
	.byte	0x6
	.byte	0x1
	.8byte	.LFB6
	.8byte	.LFE6
	.4byte	.LLST6
	.byte	0x1
	.4byte	0x366
	.uleb128 0x13
	.string	"img"
	.byte	0x1
	.byte	0x79
	.byte	0x16
	.4byte	0x1b8
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x13
	.string	"x"
	.byte	0x1
	.byte	0x7a
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -28
	.uleb128 0x13
	.string	"y"
	.byte	0x1
	.byte	0x7b
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -32
	.uleb128 0x13
	.string	"dia"
	.byte	0x1
	.byte	0x7c
	.byte	0xd
	.4byte	0x57
	.byte	0x2
	.byte	0x91
	.sleb128 -33
	.uleb128 0x12
	.4byte	.LASF555
	.byte	0x1
	.byte	0x7d
	.byte	0xe
	.4byte	0x6a
	.byte	0x2
	.byte	0x91
	.sleb128 -36
	.uleb128 0x12
	.4byte	.LASF556
	.byte	0x1
	.byte	0x7e
	.byte	0xe
	.4byte	0x6a
	.byte	0x2
	.byte	0x91
	.sleb128 -38
	.byte	0
	.uleb128 0x11
	.byte	0x1
	.4byte	.LASF569
	.byte	0x1
	.byte	0x62
	.byte	0x6
	.byte	0x1
	.8byte	.LFB5
	.8byte	.LFE5
	.4byte	.LLST5
	.byte	0x1
	.4byte	0x3ea
	.uleb128 0x12
	.4byte	.LASF564
	.byte	0x1
	.byte	0x62
	.byte	0x1d
	.4byte	0x2e0
	.byte	0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x13
	.string	"win"
	.byte	0x1
	.byte	0x62
	.byte	0x41
	.4byte	0x3ea
	.byte	0x2
	.byte	0x91
	.sleb128 -48
	.uleb128 0x16
	.8byte	.LBB2
	.8byte	.LBE2
	.uleb128 0x15
	.4byte	.LASF570
	.byte	0x1
	.byte	0x6b
	.byte	0xe
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 -20
	.uleb128 0x16
	.8byte	.LBB3
	.8byte	.LBE3
	.uleb128 0x15
	.4byte	.LASF571
	.byte	0x1
	.byte	0x6f
	.byte	0x12
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x9
	.byte	0x8
	.4byte	0x14a
	.uleb128 0x11
	.byte	0x1
	.4byte	.LASF572
	.byte	0x1
	.byte	0x4c
	.byte	0x6
	.byte	0x1
	.8byte	.LFB4
	.8byte	.LFE4
	.4byte	.LLST4
	.byte	0x1
	.4byte	0x459
	.uleb128 0x13
	.string	"x"
	.byte	0x1
	.byte	0x4c
	.byte	0x1c
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -52
	.uleb128 0x13
	.string	"y"
	.byte	0x1
	.byte	0x4c
	.byte	0x27
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -56
	.uleb128 0x15
	.4byte	.LASF573
	.byte	0x1
	.byte	0x4d
	.byte	0x1d
	.4byte	0x459
	.byte	0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x14
	.string	"win"
	.byte	0x1
	.byte	0x4e
	.byte	0x1d
	.4byte	0x459
	.byte	0x2
	.byte	0x91
	.sleb128 -32
	.uleb128 0x14
	.string	"i"
	.byte	0x1
	.byte	0x4f
	.byte	0x9
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 -20
	.byte	0
	.uleb128 0x9
	.byte	0x8
	.4byte	0xf9
	.uleb128 0x11
	.byte	0x1
	.4byte	.LASF574
	.byte	0x1
	.byte	0x37
	.byte	0x6
	.byte	0x1
	.8byte	.LFB3
	.8byte	.LFE3
	.4byte	.LLST3
	.byte	0x1
	.4byte	0x4d5
	.uleb128 0x13
	.string	"x"
	.byte	0x1
	.byte	0x38
	.byte	0xd
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -36
	.uleb128 0x13
	.string	"y"
	.byte	0x1
	.byte	0x38
	.byte	0x18
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x13
	.string	"w"
	.byte	0x1
	.byte	0x39
	.byte	0xd
	.4byte	0x57
	.byte	0x2
	.byte	0x91
	.sleb128 -41
	.uleb128 0x13
	.string	"h"
	.byte	0x1
	.byte	0x39
	.byte	0x18
	.4byte	0x57
	.byte	0x2
	.byte	0x91
	.sleb128 -42
	.uleb128 0x12
	.4byte	.LASF551
	.byte	0x1
	.byte	0x3a
	.byte	0xe
	.4byte	0x7d
	.byte	0x2
	.byte	0x91
	.sleb128 -48
	.uleb128 0x14
	.string	"win"
	.byte	0x1
	.byte	0x3c
	.byte	0x1d
	.4byte	0x459
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.byte	0
	.uleb128 0x11
	.byte	0x1
	.4byte	.LASF575
	.byte	0x1
	.byte	0x2b
	.byte	0x6
	.byte	0x1
	.8byte	.LFB2
	.8byte	.LFE2
	.4byte	.LLST2
	.byte	0x1
	.4byte	0x517
	.uleb128 0x12
	.4byte	.LASF564
	.byte	0x1
	.byte	0x2b
	.byte	0x1b
	.4byte	0x2e0
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x12
	.4byte	.LASF576
	.byte	0x1
	.byte	0x2b
	.byte	0x3d
	.4byte	0x517
	.byte	0x2
	.byte	0x91
	.sleb128 -32
	.byte	0
	.uleb128 0x9
	.byte	0x8
	.4byte	0xee
	.uleb128 0x17
	.byte	0x1
	.4byte	.LASF577
	.byte	0x1
	.byte	0x1b
	.byte	0x6
	.byte	0x1
	.8byte	.LFB1
	.8byte	.LFE1
	.4byte	.LLST1
	.byte	0x1
	.4byte	0x579
	.uleb128 0x13
	.string	"x"
	.byte	0x1
	.byte	0x1b
	.byte	0x1a
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -36
	.uleb128 0x13
	.string	"y"
	.byte	0x1
	.byte	0x1b
	.byte	0x25
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x15
	.4byte	.LASF578
	.byte	0x1
	.byte	0x1c
	.byte	0x1c
	.4byte	0x579
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x15
	.4byte	.LASF576
	.byte	0x1
	.byte	0x1d
	.byte	0x1b
	.4byte	0xf3
	.byte	0x2
	.byte	0x91
	.sleb128 -32
	.byte	0
	.uleb128 0x9
	.byte	0x8
	.4byte	0xf3
	.uleb128 0x18
	.byte	0x1
	.4byte	.LASF583
	.byte	0x1
	.byte	0xa
	.byte	0x6
	.byte	0x1
	.8byte	.LFB0
	.8byte	.LFE0
	.4byte	.LLST0
	.byte	0x1
	.uleb128 0x13
	.string	"x"
	.byte	0x1
	.byte	0xa
	.byte	0x17
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -36
	.uleb128 0x13
	.string	"y"
	.byte	0x1
	.byte	0xa
	.byte	0x22
	.4byte	0x3d
	.byte	0x2
	.byte	0x91
	.sleb128 -40
	.uleb128 0x12
	.4byte	.LASF551
	.byte	0x1
	.byte	0xa
	.byte	0x2e
	.4byte	0x7d
	.byte	0x2
	.byte	0x91
	.sleb128 -44
	.uleb128 0x15
	.4byte	.LASF576
	.byte	0x1
	.byte	0xb
	.byte	0x1b
	.4byte	0xf3
	.byte	0x2
	.byte	0x91
	.sleb128 -24
	.byte	0
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0x8
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x6
	.uleb128 0x2119
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x2117
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x2116
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x2116
	.uleb128 0xc
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loc,"",@progbits
.Ldebug_loc0:
.LLST7:
	.8byte	.LFB7-.Ltext0
	.8byte	.LCFI28-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI28-.Ltext0
	.8byte	.LCFI29-.Ltext0
	.2byte	0x3
	.byte	0x72
	.sleb128 80
	.8byte	.LCFI29-.Ltext0
	.8byte	.LCFI30-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI30-.Ltext0
	.8byte	.LCFI31-.Ltext0
	.2byte	0x3
	.byte	0x72
	.sleb128 80
	.8byte	.LCFI31-.Ltext0
	.8byte	.LFE7-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST6:
	.8byte	.LFB6-.Ltext0
	.8byte	.LCFI24-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI24-.Ltext0
	.8byte	.LCFI25-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI25-.Ltext0
	.8byte	.LCFI26-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI26-.Ltext0
	.8byte	.LCFI27-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI27-.Ltext0
	.8byte	.LFE6-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST5:
	.8byte	.LFB5-.Ltext0
	.8byte	.LCFI20-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI20-.Ltext0
	.8byte	.LCFI21-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI21-.Ltext0
	.8byte	.LCFI22-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI22-.Ltext0
	.8byte	.LCFI23-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI23-.Ltext0
	.8byte	.LFE5-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST4:
	.8byte	.LFB4-.Ltext0
	.8byte	.LCFI16-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI16-.Ltext0
	.8byte	.LCFI17-.Ltext0
	.2byte	0x3
	.byte	0x72
	.sleb128 64
	.8byte	.LCFI17-.Ltext0
	.8byte	.LCFI18-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI18-.Ltext0
	.8byte	.LCFI19-.Ltext0
	.2byte	0x3
	.byte	0x72
	.sleb128 64
	.8byte	.LCFI19-.Ltext0
	.8byte	.LFE4-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST3:
	.8byte	.LFB3-.Ltext0
	.8byte	.LCFI12-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI12-.Ltext0
	.8byte	.LCFI13-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI13-.Ltext0
	.8byte	.LCFI14-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI14-.Ltext0
	.8byte	.LCFI15-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI15-.Ltext0
	.8byte	.LFE3-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST2:
	.8byte	.LFB2-.Ltext0
	.8byte	.LCFI8-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI8-.Ltext0
	.8byte	.LCFI9-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 32
	.8byte	.LCFI9-.Ltext0
	.8byte	.LCFI10-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI10-.Ltext0
	.8byte	.LCFI11-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 32
	.8byte	.LCFI11-.Ltext0
	.8byte	.LFE2-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST1:
	.8byte	.LFB1-.Ltext0
	.8byte	.LCFI4-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI4-.Ltext0
	.8byte	.LCFI5-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI5-.Ltext0
	.8byte	.LCFI6-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI6-.Ltext0
	.8byte	.LCFI7-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI7-.Ltext0
	.8byte	.LFE1-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
.LLST0:
	.8byte	.LFB0-.Ltext0
	.8byte	.LCFI0-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	.LCFI0-.Ltext0
	.8byte	.LCFI1-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI1-.Ltext0
	.8byte	.LCFI2-.Ltext0
	.2byte	0x2
	.byte	0x78
	.sleb128 0
	.8byte	.LCFI2-.Ltext0
	.8byte	.LCFI3-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 48
	.8byte	.LCFI3-.Ltext0
	.8byte	.LFE0-.Ltext0
	.2byte	0x2
	.byte	0x72
	.sleb128 0
	.8byte	0
	.8byte	0
	.section	.debug_aranges,"",@progbits
	.4byte	0x2c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x8
	.byte	0
	.2byte	0
	.2byte	0
	.8byte	.Ltext0
	.8byte	.Letext0-.Ltext0
	.8byte	0
	.8byte	0
	.section	.debug_macro,"",@progbits
.Ldebug_macro0:
	.2byte	0x4
	.byte	0x2
	.4byte	.Ldebug_line0
	.byte	0x7
	.4byte	.Ldebug_macro2
	.byte	0x3
	.uleb128 0
	.uleb128 0x1
	.byte	0x3
	.uleb128 0x4
	.uleb128 0x3
	.byte	0x5
	.uleb128 0x5
	.4byte	.LASF335
	.file 6 "/opt/toolchains/riscv/lib/gcc/riscv64-unknown-elf/14.2.0/include/stdint.h"
	.byte	0x3
	.uleb128 0x7
	.uleb128 0x6
	.byte	0x3
	.uleb128 0xb
	.uleb128 0x2
	.byte	0x7
	.4byte	.Ldebug_macro3
	.byte	0x4
	.byte	0x5
	.uleb128 0xd
	.4byte	.LASF459
	.byte	0x4
	.byte	0x7
	.4byte	.Ldebug_macro4
	.byte	0x4
	.byte	0x3
	.uleb128 0x5
	.uleb128 0x5
	.byte	0x5
	.uleb128 0x8
	.4byte	.LASF463
	.byte	0x3
	.uleb128 0xa
	.uleb128 0x4
	.byte	0x7
	.4byte	.Ldebug_macro5
	.byte	0x4
	.file 7 "/opt/toolchains/riscv/riscv64-unknown-elf/include/memory.h"
	.byte	0x3
	.uleb128 0xb
	.uleb128 0x7
	.byte	0x5
	.uleb128 0x2
	.4byte	.LASF521
	.file 8 "./string.h"
	.byte	0x3
	.uleb128 0x3
	.uleb128 0x8
	.byte	0x5
	.uleb128 0x9
	.4byte	.LASF522
	.byte	0x3
	.uleb128 0xb
	.uleb128 0x4
	.byte	0x4
	.file 9 "/opt/toolchains/riscv/lib/gcc/riscv64-unknown-elf/14.2.0/include/stdarg.h"
	.byte	0x3
	.uleb128 0xc
	.uleb128 0x9
	.byte	0x7
	.4byte	.Ldebug_macro6
	.byte	0x4
	.byte	0x4
	.byte	0x4
	.byte	0x5
	.uleb128 0x10
	.4byte	.LASF537
	.byte	0x4
	.byte	0x3
	.uleb128 0x7
	.uleb128 0x4
	.byte	0x4
	.byte	0x4
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.0.c87dae90d63d5d145f9074c23200a943,comdat
.Ldebug_macro2:
	.2byte	0x4
	.byte	0
	.byte	0x5
	.uleb128 0
	.4byte	.LASF0
	.byte	0x5
	.uleb128 0
	.4byte	.LASF1
	.byte	0x5
	.uleb128 0
	.4byte	.LASF2
	.byte	0x5
	.uleb128 0
	.4byte	.LASF3
	.byte	0x5
	.uleb128 0
	.4byte	.LASF4
	.byte	0x5
	.uleb128 0
	.4byte	.LASF5
	.byte	0x5
	.uleb128 0
	.4byte	.LASF6
	.byte	0x5
	.uleb128 0
	.4byte	.LASF7
	.byte	0x5
	.uleb128 0
	.4byte	.LASF8
	.byte	0x5
	.uleb128 0
	.4byte	.LASF9
	.byte	0x5
	.uleb128 0
	.4byte	.LASF10
	.byte	0x5
	.uleb128 0
	.4byte	.LASF11
	.byte	0x5
	.uleb128 0
	.4byte	.LASF12
	.byte	0x5
	.uleb128 0
	.4byte	.LASF13
	.byte	0x5
	.uleb128 0
	.4byte	.LASF14
	.byte	0x5
	.uleb128 0
	.4byte	.LASF15
	.byte	0x5
	.uleb128 0
	.4byte	.LASF16
	.byte	0x5
	.uleb128 0
	.4byte	.LASF17
	.byte	0x5
	.uleb128 0
	.4byte	.LASF18
	.byte	0x5
	.uleb128 0
	.4byte	.LASF19
	.byte	0x5
	.uleb128 0
	.4byte	.LASF20
	.byte	0x5
	.uleb128 0
	.4byte	.LASF21
	.byte	0x5
	.uleb128 0
	.4byte	.LASF22
	.byte	0x5
	.uleb128 0
	.4byte	.LASF23
	.byte	0x5
	.uleb128 0
	.4byte	.LASF24
	.byte	0x5
	.uleb128 0
	.4byte	.LASF25
	.byte	0x5
	.uleb128 0
	.4byte	.LASF26
	.byte	0x5
	.uleb128 0
	.4byte	.LASF27
	.byte	0x5
	.uleb128 0
	.4byte	.LASF28
	.byte	0x5
	.uleb128 0
	.4byte	.LASF29
	.byte	0x5
	.uleb128 0
	.4byte	.LASF30
	.byte	0x5
	.uleb128 0
	.4byte	.LASF31
	.byte	0x5
	.uleb128 0
	.4byte	.LASF32
	.byte	0x5
	.uleb128 0
	.4byte	.LASF33
	.byte	0x5
	.uleb128 0
	.4byte	.LASF34
	.byte	0x5
	.uleb128 0
	.4byte	.LASF35
	.byte	0x5
	.uleb128 0
	.4byte	.LASF36
	.byte	0x5
	.uleb128 0
	.4byte	.LASF37
	.byte	0x5
	.uleb128 0
	.4byte	.LASF38
	.byte	0x5
	.uleb128 0
	.4byte	.LASF39
	.byte	0x5
	.uleb128 0
	.4byte	.LASF40
	.byte	0x5
	.uleb128 0
	.4byte	.LASF41
	.byte	0x5
	.uleb128 0
	.4byte	.LASF42
	.byte	0x5
	.uleb128 0
	.4byte	.LASF43
	.byte	0x5
	.uleb128 0
	.4byte	.LASF44
	.byte	0x5
	.uleb128 0
	.4byte	.LASF45
	.byte	0x5
	.uleb128 0
	.4byte	.LASF46
	.byte	0x5
	.uleb128 0
	.4byte	.LASF47
	.byte	0x5
	.uleb128 0
	.4byte	.LASF48
	.byte	0x5
	.uleb128 0
	.4byte	.LASF49
	.byte	0x5
	.uleb128 0
	.4byte	.LASF50
	.byte	0x5
	.uleb128 0
	.4byte	.LASF51
	.byte	0x5
	.uleb128 0
	.4byte	.LASF52
	.byte	0x5
	.uleb128 0
	.4byte	.LASF53
	.byte	0x5
	.uleb128 0
	.4byte	.LASF54
	.byte	0x5
	.uleb128 0
	.4byte	.LASF55
	.byte	0x5
	.uleb128 0
	.4byte	.LASF56
	.byte	0x5
	.uleb128 0
	.4byte	.LASF57
	.byte	0x5
	.uleb128 0
	.4byte	.LASF58
	.byte	0x5
	.uleb128 0
	.4byte	.LASF59
	.byte	0x5
	.uleb128 0
	.4byte	.LASF60
	.byte	0x5
	.uleb128 0
	.4byte	.LASF61
	.byte	0x5
	.uleb128 0
	.4byte	.LASF62
	.byte	0x5
	.uleb128 0
	.4byte	.LASF63
	.byte	0x5
	.uleb128 0
	.4byte	.LASF64
	.byte	0x5
	.uleb128 0
	.4byte	.LASF65
	.byte	0x5
	.uleb128 0
	.4byte	.LASF66
	.byte	0x5
	.uleb128 0
	.4byte	.LASF67
	.byte	0x5
	.uleb128 0
	.4byte	.LASF68
	.byte	0x5
	.uleb128 0
	.4byte	.LASF69
	.byte	0x5
	.uleb128 0
	.4byte	.LASF70
	.byte	0x5
	.uleb128 0
	.4byte	.LASF71
	.byte	0x5
	.uleb128 0
	.4byte	.LASF72
	.byte	0x5
	.uleb128 0
	.4byte	.LASF73
	.byte	0x5
	.uleb128 0
	.4byte	.LASF74
	.byte	0x5
	.uleb128 0
	.4byte	.LASF75
	.byte	0x5
	.uleb128 0
	.4byte	.LASF76
	.byte	0x5
	.uleb128 0
	.4byte	.LASF77
	.byte	0x5
	.uleb128 0
	.4byte	.LASF78
	.byte	0x5
	.uleb128 0
	.4byte	.LASF79
	.byte	0x5
	.uleb128 0
	.4byte	.LASF80
	.byte	0x5
	.uleb128 0
	.4byte	.LASF81
	.byte	0x5
	.uleb128 0
	.4byte	.LASF82
	.byte	0x5
	.uleb128 0
	.4byte	.LASF83
	.byte	0x5
	.uleb128 0
	.4byte	.LASF84
	.byte	0x5
	.uleb128 0
	.4byte	.LASF85
	.byte	0x5
	.uleb128 0
	.4byte	.LASF86
	.byte	0x5
	.uleb128 0
	.4byte	.LASF87
	.byte	0x5
	.uleb128 0
	.4byte	.LASF88
	.byte	0x5
	.uleb128 0
	.4byte	.LASF89
	.byte	0x5
	.uleb128 0
	.4byte	.LASF90
	.byte	0x5
	.uleb128 0
	.4byte	.LASF91
	.byte	0x5
	.uleb128 0
	.4byte	.LASF92
	.byte	0x5
	.uleb128 0
	.4byte	.LASF93
	.byte	0x5
	.uleb128 0
	.4byte	.LASF94
	.byte	0x5
	.uleb128 0
	.4byte	.LASF95
	.byte	0x5
	.uleb128 0
	.4byte	.LASF96
	.byte	0x5
	.uleb128 0
	.4byte	.LASF97
	.byte	0x5
	.uleb128 0
	.4byte	.LASF98
	.byte	0x5
	.uleb128 0
	.4byte	.LASF99
	.byte	0x5
	.uleb128 0
	.4byte	.LASF100
	.byte	0x5
	.uleb128 0
	.4byte	.LASF101
	.byte	0x5
	.uleb128 0
	.4byte	.LASF102
	.byte	0x5
	.uleb128 0
	.4byte	.LASF103
	.byte	0x5
	.uleb128 0
	.4byte	.LASF104
	.byte	0x5
	.uleb128 0
	.4byte	.LASF105
	.byte	0x5
	.uleb128 0
	.4byte	.LASF106
	.byte	0x5
	.uleb128 0
	.4byte	.LASF107
	.byte	0x5
	.uleb128 0
	.4byte	.LASF108
	.byte	0x5
	.uleb128 0
	.4byte	.LASF109
	.byte	0x5
	.uleb128 0
	.4byte	.LASF110
	.byte	0x5
	.uleb128 0
	.4byte	.LASF111
	.byte	0x5
	.uleb128 0
	.4byte	.LASF112
	.byte	0x5
	.uleb128 0
	.4byte	.LASF113
	.byte	0x5
	.uleb128 0
	.4byte	.LASF114
	.byte	0x5
	.uleb128 0
	.4byte	.LASF115
	.byte	0x5
	.uleb128 0
	.4byte	.LASF116
	.byte	0x5
	.uleb128 0
	.4byte	.LASF117
	.byte	0x5
	.uleb128 0
	.4byte	.LASF118
	.byte	0x5
	.uleb128 0
	.4byte	.LASF119
	.byte	0x5
	.uleb128 0
	.4byte	.LASF120
	.byte	0x5
	.uleb128 0
	.4byte	.LASF121
	.byte	0x5
	.uleb128 0
	.4byte	.LASF122
	.byte	0x5
	.uleb128 0
	.4byte	.LASF123
	.byte	0x5
	.uleb128 0
	.4byte	.LASF124
	.byte	0x5
	.uleb128 0
	.4byte	.LASF125
	.byte	0x5
	.uleb128 0
	.4byte	.LASF126
	.byte	0x5
	.uleb128 0
	.4byte	.LASF127
	.byte	0x5
	.uleb128 0
	.4byte	.LASF128
	.byte	0x5
	.uleb128 0
	.4byte	.LASF129
	.byte	0x5
	.uleb128 0
	.4byte	.LASF130
	.byte	0x5
	.uleb128 0
	.4byte	.LASF131
	.byte	0x5
	.uleb128 0
	.4byte	.LASF132
	.byte	0x5
	.uleb128 0
	.4byte	.LASF133
	.byte	0x5
	.uleb128 0
	.4byte	.LASF134
	.byte	0x5
	.uleb128 0
	.4byte	.LASF135
	.byte	0x5
	.uleb128 0
	.4byte	.LASF136
	.byte	0x5
	.uleb128 0
	.4byte	.LASF137
	.byte	0x5
	.uleb128 0
	.4byte	.LASF138
	.byte	0x5
	.uleb128 0
	.4byte	.LASF139
	.byte	0x5
	.uleb128 0
	.4byte	.LASF140
	.byte	0x5
	.uleb128 0
	.4byte	.LASF141
	.byte	0x5
	.uleb128 0
	.4byte	.LASF142
	.byte	0x5
	.uleb128 0
	.4byte	.LASF143
	.byte	0x5
	.uleb128 0
	.4byte	.LASF144
	.byte	0x5
	.uleb128 0
	.4byte	.LASF145
	.byte	0x5
	.uleb128 0
	.4byte	.LASF146
	.byte	0x5
	.uleb128 0
	.4byte	.LASF147
	.byte	0x5
	.uleb128 0
	.4byte	.LASF148
	.byte	0x5
	.uleb128 0
	.4byte	.LASF149
	.byte	0x5
	.uleb128 0
	.4byte	.LASF150
	.byte	0x5
	.uleb128 0
	.4byte	.LASF151
	.byte	0x5
	.uleb128 0
	.4byte	.LASF152
	.byte	0x5
	.uleb128 0
	.4byte	.LASF153
	.byte	0x5
	.uleb128 0
	.4byte	.LASF154
	.byte	0x5
	.uleb128 0
	.4byte	.LASF155
	.byte	0x5
	.uleb128 0
	.4byte	.LASF156
	.byte	0x5
	.uleb128 0
	.4byte	.LASF157
	.byte	0x5
	.uleb128 0
	.4byte	.LASF158
	.byte	0x5
	.uleb128 0
	.4byte	.LASF159
	.byte	0x5
	.uleb128 0
	.4byte	.LASF160
	.byte	0x5
	.uleb128 0
	.4byte	.LASF161
	.byte	0x5
	.uleb128 0
	.4byte	.LASF162
	.byte	0x5
	.uleb128 0
	.4byte	.LASF163
	.byte	0x5
	.uleb128 0
	.4byte	.LASF164
	.byte	0x5
	.uleb128 0
	.4byte	.LASF165
	.byte	0x5
	.uleb128 0
	.4byte	.LASF166
	.byte	0x5
	.uleb128 0
	.4byte	.LASF167
	.byte	0x5
	.uleb128 0
	.4byte	.LASF168
	.byte	0x5
	.uleb128 0
	.4byte	.LASF169
	.byte	0x5
	.uleb128 0
	.4byte	.LASF170
	.byte	0x5
	.uleb128 0
	.4byte	.LASF171
	.byte	0x5
	.uleb128 0
	.4byte	.LASF172
	.byte	0x5
	.uleb128 0
	.4byte	.LASF173
	.byte	0x5
	.uleb128 0
	.4byte	.LASF174
	.byte	0x5
	.uleb128 0
	.4byte	.LASF175
	.byte	0x5
	.uleb128 0
	.4byte	.LASF176
	.byte	0x5
	.uleb128 0
	.4byte	.LASF177
	.byte	0x5
	.uleb128 0
	.4byte	.LASF178
	.byte	0x5
	.uleb128 0
	.4byte	.LASF179
	.byte	0x5
	.uleb128 0
	.4byte	.LASF180
	.byte	0x5
	.uleb128 0
	.4byte	.LASF181
	.byte	0x5
	.uleb128 0
	.4byte	.LASF182
	.byte	0x5
	.uleb128 0
	.4byte	.LASF183
	.byte	0x5
	.uleb128 0
	.4byte	.LASF184
	.byte	0x5
	.uleb128 0
	.4byte	.LASF185
	.byte	0x5
	.uleb128 0
	.4byte	.LASF186
	.byte	0x5
	.uleb128 0
	.4byte	.LASF187
	.byte	0x5
	.uleb128 0
	.4byte	.LASF188
	.byte	0x5
	.uleb128 0
	.4byte	.LASF189
	.byte	0x5
	.uleb128 0
	.4byte	.LASF190
	.byte	0x5
	.uleb128 0
	.4byte	.LASF191
	.byte	0x5
	.uleb128 0
	.4byte	.LASF192
	.byte	0x5
	.uleb128 0
	.4byte	.LASF193
	.byte	0x5
	.uleb128 0
	.4byte	.LASF194
	.byte	0x5
	.uleb128 0
	.4byte	.LASF195
	.byte	0x5
	.uleb128 0
	.4byte	.LASF196
	.byte	0x5
	.uleb128 0
	.4byte	.LASF197
	.byte	0x5
	.uleb128 0
	.4byte	.LASF198
	.byte	0x5
	.uleb128 0
	.4byte	.LASF199
	.byte	0x5
	.uleb128 0
	.4byte	.LASF200
	.byte	0x5
	.uleb128 0
	.4byte	.LASF201
	.byte	0x5
	.uleb128 0
	.4byte	.LASF202
	.byte	0x5
	.uleb128 0
	.4byte	.LASF203
	.byte	0x5
	.uleb128 0
	.4byte	.LASF204
	.byte	0x5
	.uleb128 0
	.4byte	.LASF205
	.byte	0x5
	.uleb128 0
	.4byte	.LASF206
	.byte	0x5
	.uleb128 0
	.4byte	.LASF207
	.byte	0x5
	.uleb128 0
	.4byte	.LASF208
	.byte	0x5
	.uleb128 0
	.4byte	.LASF209
	.byte	0x5
	.uleb128 0
	.4byte	.LASF210
	.byte	0x5
	.uleb128 0
	.4byte	.LASF211
	.byte	0x5
	.uleb128 0
	.4byte	.LASF212
	.byte	0x5
	.uleb128 0
	.4byte	.LASF213
	.byte	0x5
	.uleb128 0
	.4byte	.LASF214
	.byte	0x5
	.uleb128 0
	.4byte	.LASF215
	.byte	0x5
	.uleb128 0
	.4byte	.LASF216
	.byte	0x5
	.uleb128 0
	.4byte	.LASF217
	.byte	0x5
	.uleb128 0
	.4byte	.LASF218
	.byte	0x5
	.uleb128 0
	.4byte	.LASF219
	.byte	0x5
	.uleb128 0
	.4byte	.LASF220
	.byte	0x5
	.uleb128 0
	.4byte	.LASF221
	.byte	0x5
	.uleb128 0
	.4byte	.LASF222
	.byte	0x5
	.uleb128 0
	.4byte	.LASF223
	.byte	0x5
	.uleb128 0
	.4byte	.LASF224
	.byte	0x5
	.uleb128 0
	.4byte	.LASF225
	.byte	0x5
	.uleb128 0
	.4byte	.LASF226
	.byte	0x5
	.uleb128 0
	.4byte	.LASF227
	.byte	0x5
	.uleb128 0
	.4byte	.LASF228
	.byte	0x5
	.uleb128 0
	.4byte	.LASF229
	.byte	0x5
	.uleb128 0
	.4byte	.LASF230
	.byte	0x5
	.uleb128 0
	.4byte	.LASF231
	.byte	0x5
	.uleb128 0
	.4byte	.LASF232
	.byte	0x5
	.uleb128 0
	.4byte	.LASF233
	.byte	0x5
	.uleb128 0
	.4byte	.LASF234
	.byte	0x5
	.uleb128 0
	.4byte	.LASF235
	.byte	0x5
	.uleb128 0
	.4byte	.LASF236
	.byte	0x5
	.uleb128 0
	.4byte	.LASF237
	.byte	0x5
	.uleb128 0
	.4byte	.LASF238
	.byte	0x5
	.uleb128 0
	.4byte	.LASF239
	.byte	0x5
	.uleb128 0
	.4byte	.LASF240
	.byte	0x5
	.uleb128 0
	.4byte	.LASF241
	.byte	0x5
	.uleb128 0
	.4byte	.LASF242
	.byte	0x5
	.uleb128 0
	.4byte	.LASF243
	.byte	0x5
	.uleb128 0
	.4byte	.LASF244
	.byte	0x5
	.uleb128 0
	.4byte	.LASF245
	.byte	0x5
	.uleb128 0
	.4byte	.LASF246
	.byte	0x5
	.uleb128 0
	.4byte	.LASF247
	.byte	0x5
	.uleb128 0
	.4byte	.LASF248
	.byte	0x5
	.uleb128 0
	.4byte	.LASF249
	.byte	0x5
	.uleb128 0
	.4byte	.LASF250
	.byte	0x5
	.uleb128 0
	.4byte	.LASF251
	.byte	0x5
	.uleb128 0
	.4byte	.LASF252
	.byte	0x5
	.uleb128 0
	.4byte	.LASF253
	.byte	0x5
	.uleb128 0
	.4byte	.LASF254
	.byte	0x5
	.uleb128 0
	.4byte	.LASF255
	.byte	0x5
	.uleb128 0
	.4byte	.LASF256
	.byte	0x5
	.uleb128 0
	.4byte	.LASF257
	.byte	0x5
	.uleb128 0
	.4byte	.LASF258
	.byte	0x5
	.uleb128 0
	.4byte	.LASF259
	.byte	0x5
	.uleb128 0
	.4byte	.LASF260
	.byte	0x5
	.uleb128 0
	.4byte	.LASF261
	.byte	0x5
	.uleb128 0
	.4byte	.LASF262
	.byte	0x5
	.uleb128 0
	.4byte	.LASF263
	.byte	0x5
	.uleb128 0
	.4byte	.LASF264
	.byte	0x5
	.uleb128 0
	.4byte	.LASF265
	.byte	0x5
	.uleb128 0
	.4byte	.LASF266
	.byte	0x5
	.uleb128 0
	.4byte	.LASF267
	.byte	0x5
	.uleb128 0
	.4byte	.LASF268
	.byte	0x5
	.uleb128 0
	.4byte	.LASF269
	.byte	0x5
	.uleb128 0
	.4byte	.LASF270
	.byte	0x5
	.uleb128 0
	.4byte	.LASF271
	.byte	0x5
	.uleb128 0
	.4byte	.LASF272
	.byte	0x5
	.uleb128 0
	.4byte	.LASF273
	.byte	0x5
	.uleb128 0
	.4byte	.LASF274
	.byte	0x5
	.uleb128 0
	.4byte	.LASF275
	.byte	0x5
	.uleb128 0
	.4byte	.LASF276
	.byte	0x5
	.uleb128 0
	.4byte	.LASF277
	.byte	0x5
	.uleb128 0
	.4byte	.LASF278
	.byte	0x5
	.uleb128 0
	.4byte	.LASF279
	.byte	0x5
	.uleb128 0
	.4byte	.LASF280
	.byte	0x5
	.uleb128 0
	.4byte	.LASF281
	.byte	0x5
	.uleb128 0
	.4byte	.LASF282
	.byte	0x5
	.uleb128 0
	.4byte	.LASF283
	.byte	0x5
	.uleb128 0
	.4byte	.LASF284
	.byte	0x5
	.uleb128 0
	.4byte	.LASF285
	.byte	0x5
	.uleb128 0
	.4byte	.LASF286
	.byte	0x5
	.uleb128 0
	.4byte	.LASF287
	.byte	0x5
	.uleb128 0
	.4byte	.LASF288
	.byte	0x5
	.uleb128 0
	.4byte	.LASF289
	.byte	0x5
	.uleb128 0
	.4byte	.LASF290
	.byte	0x5
	.uleb128 0
	.4byte	.LASF291
	.byte	0x5
	.uleb128 0
	.4byte	.LASF292
	.byte	0x5
	.uleb128 0
	.4byte	.LASF293
	.byte	0x5
	.uleb128 0
	.4byte	.LASF294
	.byte	0x5
	.uleb128 0
	.4byte	.LASF295
	.byte	0x5
	.uleb128 0
	.4byte	.LASF296
	.byte	0x5
	.uleb128 0
	.4byte	.LASF297
	.byte	0x5
	.uleb128 0
	.4byte	.LASF298
	.byte	0x5
	.uleb128 0
	.4byte	.LASF299
	.byte	0x5
	.uleb128 0
	.4byte	.LASF300
	.byte	0x5
	.uleb128 0
	.4byte	.LASF301
	.byte	0x5
	.uleb128 0
	.4byte	.LASF302
	.byte	0x5
	.uleb128 0
	.4byte	.LASF303
	.byte	0x5
	.uleb128 0
	.4byte	.LASF304
	.byte	0x5
	.uleb128 0
	.4byte	.LASF305
	.byte	0x5
	.uleb128 0
	.4byte	.LASF306
	.byte	0x5
	.uleb128 0
	.4byte	.LASF307
	.byte	0x5
	.uleb128 0
	.4byte	.LASF308
	.byte	0x5
	.uleb128 0
	.4byte	.LASF309
	.byte	0x5
	.uleb128 0
	.4byte	.LASF310
	.byte	0x5
	.uleb128 0
	.4byte	.LASF311
	.byte	0x5
	.uleb128 0
	.4byte	.LASF312
	.byte	0x5
	.uleb128 0
	.4byte	.LASF313
	.byte	0x5
	.uleb128 0
	.4byte	.LASF314
	.byte	0x5
	.uleb128 0
	.4byte	.LASF315
	.byte	0x5
	.uleb128 0
	.4byte	.LASF316
	.byte	0x5
	.uleb128 0
	.4byte	.LASF317
	.byte	0x5
	.uleb128 0
	.4byte	.LASF318
	.byte	0x5
	.uleb128 0
	.4byte	.LASF319
	.byte	0x5
	.uleb128 0
	.4byte	.LASF320
	.byte	0x5
	.uleb128 0
	.4byte	.LASF321
	.byte	0x5
	.uleb128 0
	.4byte	.LASF322
	.byte	0x5
	.uleb128 0
	.4byte	.LASF323
	.byte	0x5
	.uleb128 0
	.4byte	.LASF324
	.byte	0x5
	.uleb128 0
	.4byte	.LASF325
	.byte	0x5
	.uleb128 0
	.4byte	.LASF326
	.byte	0x5
	.uleb128 0
	.4byte	.LASF327
	.byte	0x5
	.uleb128 0
	.4byte	.LASF328
	.byte	0x5
	.uleb128 0
	.4byte	.LASF329
	.byte	0x5
	.uleb128 0
	.4byte	.LASF330
	.byte	0x5
	.uleb128 0
	.4byte	.LASF331
	.byte	0x5
	.uleb128 0
	.4byte	.LASF332
	.byte	0x5
	.uleb128 0
	.4byte	.LASF333
	.byte	0x5
	.uleb128 0
	.4byte	.LASF334
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.stdintgcc.h.29.6d480f4ba0f60596e88234283d42444f,comdat
.Ldebug_macro3:
	.2byte	0x4
	.byte	0
	.byte	0x5
	.uleb128 0x1d
	.4byte	.LASF336
	.byte	0x6
	.uleb128 0x64
	.4byte	.LASF337
	.byte	0x5
	.uleb128 0x65
	.4byte	.LASF338
	.byte	0x6
	.uleb128 0x66
	.4byte	.LASF339
	.byte	0x5
	.uleb128 0x67
	.4byte	.LASF340
	.byte	0x6
	.uleb128 0x6a
	.4byte	.LASF341
	.byte	0x5
	.uleb128 0x6b
	.4byte	.LASF342
	.byte	0x6
	.uleb128 0x6e
	.4byte	.LASF343
	.byte	0x5
	.uleb128 0x6f
	.4byte	.LASF344
	.byte	0x6
	.uleb128 0x70
	.4byte	.LASF345
	.byte	0x5
	.uleb128 0x71
	.4byte	.LASF346
	.byte	0x6
	.uleb128 0x74
	.4byte	.LASF347
	.byte	0x5
	.uleb128 0x75
	.4byte	.LASF348
	.byte	0x6
	.uleb128 0x78
	.4byte	.LASF349
	.byte	0x5
	.uleb128 0x79
	.4byte	.LASF350
	.byte	0x6
	.uleb128 0x7a
	.4byte	.LASF351
	.byte	0x5
	.uleb128 0x7b
	.4byte	.LASF352
	.byte	0x6
	.uleb128 0x7e
	.4byte	.LASF353
	.byte	0x5
	.uleb128 0x7f
	.4byte	.LASF354
	.byte	0x6
	.uleb128 0x82
	.4byte	.LASF355
	.byte	0x5
	.uleb128 0x83
	.4byte	.LASF356
	.byte	0x6
	.uleb128 0x84
	.4byte	.LASF357
	.byte	0x5
	.uleb128 0x85
	.4byte	.LASF358
	.byte	0x6
	.uleb128 0x88
	.4byte	.LASF359
	.byte	0x5
	.uleb128 0x89
	.4byte	.LASF360
	.byte	0x6
	.uleb128 0x8c
	.4byte	.LASF361
	.byte	0x5
	.uleb128 0x8d
	.4byte	.LASF362
	.byte	0x6
	.uleb128 0x8e
	.4byte	.LASF363
	.byte	0x5
	.uleb128 0x8f
	.4byte	.LASF364
	.byte	0x6
	.uleb128 0x90
	.4byte	.LASF365
	.byte	0x5
	.uleb128 0x91
	.4byte	.LASF366
	.byte	0x6
	.uleb128 0x92
	.4byte	.LASF367
	.byte	0x5
	.uleb128 0x93
	.4byte	.LASF368
	.byte	0x6
	.uleb128 0x94
	.4byte	.LASF369
	.byte	0x5
	.uleb128 0x95
	.4byte	.LASF370
	.byte	0x6
	.uleb128 0x96
	.4byte	.LASF371
	.byte	0x5
	.uleb128 0x97
	.4byte	.LASF372
	.byte	0x6
	.uleb128 0x98
	.4byte	.LASF373
	.byte	0x5
	.uleb128 0x99
	.4byte	.LASF374
	.byte	0x6
	.uleb128 0x9a
	.4byte	.LASF375
	.byte	0x5
	.uleb128 0x9b
	.4byte	.LASF376
	.byte	0x6
	.uleb128 0x9c
	.4byte	.LASF377
	.byte	0x5
	.uleb128 0x9d
	.4byte	.LASF378
	.byte	0x6
	.uleb128 0x9e
	.4byte	.LASF379
	.byte	0x5
	.uleb128 0x9f
	.4byte	.LASF380
	.byte	0x6
	.uleb128 0xa0
	.4byte	.LASF381
	.byte	0x5
	.uleb128 0xa1
	.4byte	.LASF382
	.byte	0x6
	.uleb128 0xa2
	.4byte	.LASF383
	.byte	0x5
	.uleb128 0xa3
	.4byte	.LASF384
	.byte	0x6
	.uleb128 0xa5
	.4byte	.LASF385
	.byte	0x5
	.uleb128 0xa6
	.4byte	.LASF386
	.byte	0x6
	.uleb128 0xa7
	.4byte	.LASF387
	.byte	0x5
	.uleb128 0xa8
	.4byte	.LASF388
	.byte	0x6
	.uleb128 0xa9
	.4byte	.LASF389
	.byte	0x5
	.uleb128 0xaa
	.4byte	.LASF390
	.byte	0x6
	.uleb128 0xab
	.4byte	.LASF391
	.byte	0x5
	.uleb128 0xac
	.4byte	.LASF392
	.byte	0x6
	.uleb128 0xad
	.4byte	.LASF393
	.byte	0x5
	.uleb128 0xae
	.4byte	.LASF394
	.byte	0x6
	.uleb128 0xaf
	.4byte	.LASF395
	.byte	0x5
	.uleb128 0xb0
	.4byte	.LASF396
	.byte	0x6
	.uleb128 0xb1
	.4byte	.LASF397
	.byte	0x5
	.uleb128 0xb2
	.4byte	.LASF398
	.byte	0x6
	.uleb128 0xb3
	.4byte	.LASF399
	.byte	0x5
	.uleb128 0xb4
	.4byte	.LASF400
	.byte	0x6
	.uleb128 0xb5
	.4byte	.LASF401
	.byte	0x5
	.uleb128 0xb6
	.4byte	.LASF402
	.byte	0x6
	.uleb128 0xb7
	.4byte	.LASF403
	.byte	0x5
	.uleb128 0xb8
	.4byte	.LASF404
	.byte	0x6
	.uleb128 0xb9
	.4byte	.LASF405
	.byte	0x5
	.uleb128 0xba
	.4byte	.LASF406
	.byte	0x6
	.uleb128 0xbb
	.4byte	.LASF407
	.byte	0x5
	.uleb128 0xbc
	.4byte	.LASF408
	.byte	0x6
	.uleb128 0xbf
	.4byte	.LASF409
	.byte	0x5
	.uleb128 0xc0
	.4byte	.LASF410
	.byte	0x6
	.uleb128 0xc1
	.4byte	.LASF411
	.byte	0x5
	.uleb128 0xc2
	.4byte	.LASF412
	.byte	0x6
	.uleb128 0xc5
	.4byte	.LASF413
	.byte	0x5
	.uleb128 0xc6
	.4byte	.LASF414
	.byte	0x6
	.uleb128 0xc9
	.4byte	.LASF415
	.byte	0x5
	.uleb128 0xca
	.4byte	.LASF416
	.byte	0x6
	.uleb128 0xcb
	.4byte	.LASF417
	.byte	0x5
	.uleb128 0xcc
	.4byte	.LASF418
	.byte	0x6
	.uleb128 0xcd
	.4byte	.LASF419
	.byte	0x5
	.uleb128 0xce
	.4byte	.LASF420
	.byte	0x6
	.uleb128 0xd2
	.4byte	.LASF421
	.byte	0x5
	.uleb128 0xd3
	.4byte	.LASF422
	.byte	0x6
	.uleb128 0xd4
	.4byte	.LASF423
	.byte	0x5
	.uleb128 0xd5
	.4byte	.LASF424
	.byte	0x6
	.uleb128 0xd7
	.4byte	.LASF425
	.byte	0x5
	.uleb128 0xd8
	.4byte	.LASF426
	.byte	0x6
	.uleb128 0xd9
	.4byte	.LASF427
	.byte	0x5
	.uleb128 0xda
	.4byte	.LASF428
	.byte	0x6
	.uleb128 0xdc
	.4byte	.LASF429
	.byte	0x5
	.uleb128 0xdd
	.4byte	.LASF430
	.byte	0x6
	.uleb128 0xdf
	.4byte	.LASF431
	.byte	0x5
	.uleb128 0xe0
	.4byte	.LASF432
	.byte	0x6
	.uleb128 0xe1
	.4byte	.LASF433
	.byte	0x5
	.uleb128 0xe2
	.4byte	.LASF434
	.byte	0x6
	.uleb128 0xe4
	.4byte	.LASF435
	.byte	0x5
	.uleb128 0xe5
	.4byte	.LASF436
	.byte	0x6
	.uleb128 0xe6
	.4byte	.LASF437
	.byte	0x5
	.uleb128 0xe7
	.4byte	.LASF438
	.byte	0x6
	.uleb128 0xef
	.4byte	.LASF439
	.byte	0x5
	.uleb128 0xf0
	.4byte	.LASF440
	.byte	0x6
	.uleb128 0xf1
	.4byte	.LASF441
	.byte	0x5
	.uleb128 0xf2
	.4byte	.LASF442
	.byte	0x6
	.uleb128 0xf3
	.4byte	.LASF443
	.byte	0x5
	.uleb128 0xf4
	.4byte	.LASF444
	.byte	0x6
	.uleb128 0xf5
	.4byte	.LASF445
	.byte	0x5
	.uleb128 0xf6
	.4byte	.LASF446
	.byte	0x6
	.uleb128 0xf7
	.4byte	.LASF447
	.byte	0x5
	.uleb128 0xf8
	.4byte	.LASF448
	.byte	0x6
	.uleb128 0xf9
	.4byte	.LASF449
	.byte	0x5
	.uleb128 0xfa
	.4byte	.LASF450
	.byte	0x6
	.uleb128 0xfb
	.4byte	.LASF451
	.byte	0x5
	.uleb128 0xfc
	.4byte	.LASF452
	.byte	0x6
	.uleb128 0xfd
	.4byte	.LASF453
	.byte	0x5
	.uleb128 0xfe
	.4byte	.LASF454
	.byte	0x6
	.uleb128 0xff
	.4byte	.LASF455
	.byte	0x5
	.uleb128 0x100
	.4byte	.LASF456
	.byte	0x6
	.uleb128 0x101
	.4byte	.LASF457
	.byte	0x5
	.uleb128 0x102
	.4byte	.LASF458
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.skyline.h.9.bfa69acc18c55695f7f429964d135e93,comdat
.Ldebug_macro4:
	.2byte	0x4
	.byte	0
	.byte	0x5
	.uleb128 0x9
	.4byte	.LASF460
	.byte	0x5
	.uleb128 0xa
	.4byte	.LASF461
	.byte	0x5
	.uleb128 0xb
	.4byte	.LASF462
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.stddef.h.39.0dc9006b34572d4d9cae4c8b422c4971,comdat
.Ldebug_macro5:
	.2byte	0x4
	.byte	0
	.byte	0x5
	.uleb128 0x27
	.4byte	.LASF464
	.byte	0x5
	.uleb128 0x28
	.4byte	.LASF465
	.byte	0x5
	.uleb128 0x2a
	.4byte	.LASF466
	.byte	0x5
	.uleb128 0x84
	.4byte	.LASF467
	.byte	0x5
	.uleb128 0x85
	.4byte	.LASF468
	.byte	0x5
	.uleb128 0x86
	.4byte	.LASF469
	.byte	0x5
	.uleb128 0x87
	.4byte	.LASF470
	.byte	0x5
	.uleb128 0x88
	.4byte	.LASF471
	.byte	0x5
	.uleb128 0x89
	.4byte	.LASF472
	.byte	0x5
	.uleb128 0x8a
	.4byte	.LASF473
	.byte	0x5
	.uleb128 0x8b
	.4byte	.LASF474
	.byte	0x5
	.uleb128 0x8c
	.4byte	.LASF475
	.byte	0x5
	.uleb128 0x8d
	.4byte	.LASF476
	.byte	0x6
	.uleb128 0x9e
	.4byte	.LASF477
	.byte	0x5
	.uleb128 0xb9
	.4byte	.LASF478
	.byte	0x5
	.uleb128 0xba
	.4byte	.LASF479
	.byte	0x5
	.uleb128 0xbb
	.4byte	.LASF480
	.byte	0x5
	.uleb128 0xbc
	.4byte	.LASF481
	.byte	0x5
	.uleb128 0xbd
	.4byte	.LASF482
	.byte	0x5
	.uleb128 0xbe
	.4byte	.LASF483
	.byte	0x5
	.uleb128 0xbf
	.4byte	.LASF484
	.byte	0x5
	.uleb128 0xc0
	.4byte	.LASF485
	.byte	0x5
	.uleb128 0xc1
	.4byte	.LASF486
	.byte	0x5
	.uleb128 0xc2
	.4byte	.LASF487
	.byte	0x5
	.uleb128 0xc3
	.4byte	.LASF488
	.byte	0x5
	.uleb128 0xc4
	.4byte	.LASF489
	.byte	0x5
	.uleb128 0xc5
	.4byte	.LASF490
	.byte	0x5
	.uleb128 0xc6
	.4byte	.LASF491
	.byte	0x5
	.uleb128 0xc7
	.4byte	.LASF492
	.byte	0x5
	.uleb128 0xc8
	.4byte	.LASF493
	.byte	0x5
	.uleb128 0xc9
	.4byte	.LASF494
	.byte	0x5
	.uleb128 0xd0
	.4byte	.LASF495
	.byte	0x6
	.uleb128 0xed
	.4byte	.LASF496
	.byte	0x5
	.uleb128 0x10b
	.4byte	.LASF497
	.byte	0x5
	.uleb128 0x10c
	.4byte	.LASF498
	.byte	0x5
	.uleb128 0x10d
	.4byte	.LASF499
	.byte	0x5
	.uleb128 0x10e
	.4byte	.LASF500
	.byte	0x5
	.uleb128 0x10f
	.4byte	.LASF501
	.byte	0x5
	.uleb128 0x110
	.4byte	.LASF502
	.byte	0x5
	.uleb128 0x111
	.4byte	.LASF503
	.byte	0x5
	.uleb128 0x112
	.4byte	.LASF504
	.byte	0x5
	.uleb128 0x113
	.4byte	.LASF505
	.byte	0x5
	.uleb128 0x114
	.4byte	.LASF506
	.byte	0x5
	.uleb128 0x115
	.4byte	.LASF507
	.byte	0x5
	.uleb128 0x116
	.4byte	.LASF508
	.byte	0x5
	.uleb128 0x117
	.4byte	.LASF509
	.byte	0x5
	.uleb128 0x118
	.4byte	.LASF510
	.byte	0x5
	.uleb128 0x119
	.4byte	.LASF511
	.byte	0x5
	.uleb128 0x11a
	.4byte	.LASF512
	.byte	0x6
	.uleb128 0x127
	.4byte	.LASF513
	.byte	0x6
	.uleb128 0x15d
	.4byte	.LASF514
	.byte	0x6
	.uleb128 0x18f
	.4byte	.LASF515
	.byte	0x5
	.uleb128 0x194
	.4byte	.LASF516
	.byte	0x6
	.uleb128 0x19a
	.4byte	.LASF517
	.byte	0x6
	.uleb128 0x19f
	.4byte	.LASF518
	.byte	0x5
	.uleb128 0x1a0
	.4byte	.LASF519
	.byte	0x5
	.uleb128 0x1a5
	.4byte	.LASF520
	.byte	0
	.section	.debug_macro,"G",@progbits,wm4.stdarg.h.31.f7f4f3bfddce9ed034956076d59396f7,comdat
.Ldebug_macro6:
	.2byte	0x4
	.byte	0
	.byte	0x5
	.uleb128 0x1f
	.4byte	.LASF523
	.byte	0x5
	.uleb128 0x20
	.4byte	.LASF524
	.byte	0x6
	.uleb128 0x22
	.4byte	.LASF525
	.byte	0x5
	.uleb128 0x27
	.4byte	.LASF526
	.byte	0x5
	.uleb128 0x32
	.4byte	.LASF527
	.byte	0x5
	.uleb128 0x34
	.4byte	.LASF528
	.byte	0x5
	.uleb128 0x35
	.4byte	.LASF529
	.byte	0x5
	.uleb128 0x38
	.4byte	.LASF530
	.byte	0x5
	.uleb128 0x3a
	.4byte	.LASF531
	.byte	0x5
	.uleb128 0x6d
	.4byte	.LASF532
	.byte	0x5
	.uleb128 0x70
	.4byte	.LASF533
	.byte	0x5
	.uleb128 0x73
	.4byte	.LASF534
	.byte	0x5
	.uleb128 0x76
	.4byte	.LASF535
	.byte	0x5
	.uleb128 0x79
	.4byte	.LASF536
	.byte	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF206:
	.string	"__FLT16_NORM_MAX__ 6.55040000000000000000000000000000000e+4F16"
.LASF485:
	.string	"_SIZE_T_ "
.LASF244:
	.string	"__FLT64_HAS_QUIET_NAN__ 1"
.LASF201:
	.string	"__FLT16_MIN_10_EXP__ (-4)"
.LASF227:
	.string	"__FLT32_HAS_INFINITY__ 1"
.LASF463:
	.string	"_HEAP_H_ "
.LASF312:
	.string	"__GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1"
.LASF234:
	.string	"__FLT64_MAX_EXP__ 1024"
.LASF289:
	.string	"__FLT64X_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966F64x"
.LASF510:
	.string	"_GCC_WCHAR_T "
.LASF266:
	.string	"__FLT32X_MAX_EXP__ 1024"
.LASF131:
	.string	"__INT_FAST16_WIDTH__ 32"
.LASF575:
	.string	"draw_star"
.LASF369:
	.string	"INT_LEAST16_MIN"
.LASF288:
	.string	"__FLT64X_EPSILON__ 1.92592994438723585305597794258492732e-34F64x"
.LASF176:
	.string	"__DBL_DENORM_MIN__ ((double)4.94065645841246544176568792868221372e-324L)"
.LASF432:
	.string	"WCHAR_MAX __WCHAR_MAX__"
.LASF368:
	.string	"INT_LEAST16_MAX __INT_LEAST16_MAX__"
.LASF140:
	.string	"__INTPTR_MAX__ 0x7fffffffffffffffL"
.LASF214:
	.string	"__FLT32_MANT_DIG__ 24"
.LASF507:
	.string	"_WCHAR_T_H "
.LASF302:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1"
.LASF181:
	.string	"__LDBL_MANT_DIG__ 113"
.LASF135:
	.string	"__INT_FAST64_WIDTH__ 64"
.LASF327:
	.string	"__riscv_cmodel_medany 1"
.LASF268:
	.string	"__FLT32X_DECIMAL_DIG__ 17"
.LASF167:
	.string	"__DBL_MIN_EXP__ (-1021)"
.LASF134:
	.string	"__INT_FAST64_MAX__ 0x7fffffffffffffffL"
.LASF305:
	.string	"__GCC_ATOMIC_CHAR16_T_LOCK_FREE 2"
.LASF524:
	.string	"_ANSI_STDARG_H_ "
.LASF269:
	.string	"__FLT32X_MAX__ 1.79769313486231570814527423731704357e+308F32x"
.LASF318:
	.string	"__SIZEOF_WINT_T__ 4"
.LASF564:
	.string	"fbuf"
.LASF480:
	.string	"_SIZE_T "
.LASF397:
	.string	"INT_FAST32_MAX"
.LASF392:
	.string	"INT_FAST16_MAX __INT_FAST16_MAX__"
.LASF6:
	.string	"__GNUC_MINOR__ 2"
.LASF222:
	.string	"__FLT32_NORM_MAX__ 3.40282346638528859811704183484516925e+38F32"
.LASF347:
	.string	"UINT16_MAX"
.LASF373:
	.string	"INT_LEAST32_MAX"
.LASF337:
	.string	"INT8_MAX"
.LASF107:
	.string	"__UINT64_MAX__ 0xffffffffffffffffUL"
.LASF547:
	.string	"unsigned int"
.LASF550:
	.string	"next"
.LASF420:
	.string	"UINTMAX_MAX __UINTMAX_MAX__"
.LASF239:
	.string	"__FLT64_MIN__ 2.22507385850720138309023271733240406e-308F64"
.LASF87:
	.string	"__LONG_LONG_WIDTH__ 64"
.LASF477:
	.string	"__need_ptrdiff_t"
.LASF160:
	.string	"__FLT_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F"
.LASF42:
	.string	"__CHAR16_TYPE__ short unsigned int"
.LASF378:
	.string	"UINT_LEAST32_MAX __UINT_LEAST32_MAX__"
.LASF25:
	.string	"__SIZEOF_SIZE_T__ 8"
.LASF342:
	.string	"UINT8_MAX __UINT8_MAX__"
.LASF128:
	.string	"__INT_FAST8_MAX__ 0x7fffffff"
.LASF29:
	.string	"__ORDER_BIG_ENDIAN__ 4321"
.LASF528:
	.string	"va_end(v) __builtin_va_end(v)"
.LASF535:
	.string	"_VA_LIST_T_H "
.LASF484:
	.string	"__SIZE_T "
.LASF127:
	.string	"__UINT64_C(c) c ## UL"
.LASF165:
	.string	"__DBL_MANT_DIG__ 53"
.LASF74:
	.string	"__INT_MAX__ 0x7fffffff"
.LASF12:
	.string	"__ATOMIC_RELEASE 3"
.LASF339:
	.string	"INT8_MIN"
.LASF199:
	.string	"__FLT16_DIG__ 3"
.LASF298:
	.string	"__CHAR_UNSIGNED__ 1"
.LASF541:
	.string	"int32_t"
.LASF46:
	.string	"__INT16_TYPE__ short int"
.LASF213:
	.string	"__FLT16_IS_IEC_60559__ 1"
.LASF522:
	.string	"_STRING_H_ "
.LASF416:
	.string	"INTMAX_MAX __INTMAX_MAX__"
.LASF442:
	.string	"INT16_C(c) __INT16_C(c)"
.LASF475:
	.string	"_PTRDIFF_T_DECLARED "
.LASF282:
	.string	"__FLT64X_MAX_EXP__ 16384"
.LASF10:
	.string	"__ATOMIC_SEQ_CST 5"
.LASF21:
	.string	"__SIZEOF_SHORT__ 2"
.LASF52:
	.string	"__UINT64_TYPE__ long unsigned int"
.LASF580:
	.string	"mp1.c"
.LASF53:
	.string	"__INT_LEAST8_TYPE__ signed char"
.LASF513:
	.string	"_BSD_WCHAR_T_"
.LASF59:
	.string	"__UINT_LEAST32_TYPE__ unsigned int"
.LASF311:
	.string	"__GCC_ATOMIC_LLONG_LOCK_FREE 2"
.LASF31:
	.string	"__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__"
.LASF191:
	.string	"__LDBL_MIN__ 3.36210314311209350626267781732175260e-4932L"
.LASF217:
	.string	"__FLT32_MIN_10_EXP__ (-37)"
.LASF428:
	.string	"SIG_ATOMIC_MIN __SIG_ATOMIC_MIN__"
.LASF56:
	.string	"__INT_LEAST64_TYPE__ long int"
.LASF78:
	.string	"__WCHAR_MIN__ (-__WCHAR_MAX__ - 1)"
.LASF3:
	.string	"__STDC_UTF_32__ 1"
.LASF111:
	.string	"__INT_LEAST16_MAX__ 0x7fff"
.LASF72:
	.string	"__SCHAR_MAX__ 0x7f"
.LASF193:
	.string	"__LDBL_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966L"
.LASF34:
	.string	"__GNUC_EXECUTION_CHARSET_NAME \"UTF-8\""
.LASF556:
	.string	"ontime"
.LASF231:
	.string	"__FLT64_DIG__ 15"
.LASF370:
	.string	"INT_LEAST16_MIN (-INT_LEAST16_MAX - 1)"
.LASF9:
	.string	"__ATOMIC_RELAXED 0"
.LASF410:
	.string	"INTPTR_MAX __INTPTR_MAX__"
.LASF197:
	.string	"__LDBL_IS_IEC_60559__ 1"
.LASF523:
	.string	"_STDARG_H "
.LASF275:
	.string	"__FLT32X_HAS_INFINITY__ 1"
.LASF259:
	.string	"__FLT128_HAS_INFINITY__ 1"
.LASF57:
	.string	"__UINT_LEAST8_TYPE__ unsigned char"
.LASF316:
	.string	"__SIZEOF_INT128__ 16"
.LASF403:
	.string	"INT_FAST64_MAX"
.LASF527:
	.string	"va_start(v,l) __builtin_va_start(v,l)"
.LASF109:
	.string	"__INT8_C(c) c"
.LASF483:
	.string	"_T_SIZE "
.LASF253:
	.string	"__FLT128_MAX__ 1.18973149535723176508575932662800702e+4932F128"
.LASF94:
	.string	"__UINTMAX_MAX__ 0xffffffffffffffffUL"
.LASF55:
	.string	"__INT_LEAST32_TYPE__ int"
.LASF113:
	.string	"__INT_LEAST16_WIDTH__ 16"
.LASF482:
	.string	"_T_SIZE_ "
.LASF546:
	.string	"uint32_t"
.LASF459:
	.string	"_GCC_WRAP_STDINT_H "
.LASF393:
	.string	"INT_FAST16_MIN"
.LASF505:
	.string	"_WCHAR_T_DEFINED_ "
.LASF245:
	.string	"__FLT64_IS_IEC_60559__ 1"
.LASF208:
	.string	"__FLT16_EPSILON__ 9.76562500000000000000000000000000000e-4F16"
.LASF511:
	.string	"_WCHAR_T_DECLARED "
.LASF226:
	.string	"__FLT32_HAS_DENORM__ 1"
.LASF299:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1"
.LASF415:
	.string	"INTMAX_MAX"
.LASF574:
	.string	"add_window"
.LASF280:
	.string	"__FLT64X_MIN_EXP__ (-16381)"
.LASF457:
	.string	"UINTMAX_C"
.LASF18:
	.string	"__SIZEOF_INT__ 4"
.LASF486:
	.string	"_BSD_SIZE_T_ "
.LASF445:
	.string	"INT64_C"
.LASF150:
	.string	"__FLT_DIG__ 6"
.LASF130:
	.string	"__INT_FAST16_MAX__ 0x7fffffff"
.LASF198:
	.string	"__FLT16_MANT_DIG__ 11"
.LASF425:
	.string	"SIG_ATOMIC_MAX"
.LASF142:
	.string	"__UINTPTR_MAX__ 0xffffffffffffffffUL"
.LASF386:
	.string	"INT_FAST8_MAX __INT_FAST8_MAX__"
.LASF248:
	.string	"__FLT128_MIN_EXP__ (-16381)"
.LASF228:
	.string	"__FLT32_HAS_QUIET_NAN__ 1"
.LASF489:
	.string	"_BSD_SIZE_T_DEFINED_ "
.LASF122:
	.string	"__UINT_LEAST16_MAX__ 0xffff"
.LASF139:
	.string	"__UINT_FAST64_MAX__ 0xffffffffffffffffUL"
.LASF5:
	.string	"__GNUC__ 14"
.LASF182:
	.string	"__LDBL_DIG__ 33"
.LASF120:
	.string	"__UINT_LEAST8_MAX__ 0xff"
.LASF340:
	.string	"INT8_MIN (-INT8_MAX - 1)"
.LASF38:
	.string	"__WCHAR_TYPE__ int"
.LASF100:
	.string	"__INT8_MAX__ 0x7f"
.LASF517:
	.string	"__need_NULL"
.LASF250:
	.string	"__FLT128_MAX_EXP__ 16384"
.LASF148:
	.string	"__FLT_RADIX__ 2"
.LASF273:
	.string	"__FLT32X_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F32x"
.LASF372:
	.string	"UINT_LEAST16_MAX __UINT_LEAST16_MAX__"
.LASF508:
	.string	"___int_wchar_t_h "
.LASF24:
	.string	"__SIZEOF_LONG_DOUBLE__ 16"
.LASF179:
	.string	"__DBL_HAS_QUIET_NAN__ 1"
.LASF293:
	.string	"__FLT64X_IS_IEC_60559__ 1"
.LASF345:
	.string	"INT16_MIN"
.LASF427:
	.string	"SIG_ATOMIC_MIN"
.LASF61:
	.string	"__INT_FAST8_TYPE__ int"
.LASF243:
	.string	"__FLT64_HAS_INFINITY__ 1"
.LASF429:
	.string	"SIZE_MAX"
.LASF185:
	.string	"__LDBL_MAX_EXP__ 16384"
.LASF40:
	.string	"__INTMAX_TYPE__ long int"
.LASF577:
	.string	"remove_star"
.LASF69:
	.string	"__INTPTR_TYPE__ long int"
.LASF309:
	.string	"__GCC_ATOMIC_INT_LOCK_FREE 2"
.LASF521:
	.string	"_MEMORY_H "
.LASF576:
	.string	"star"
.LASF119:
	.string	"__INT_LEAST64_WIDTH__ 64"
.LASF379:
	.string	"INT_LEAST64_MAX"
.LASF285:
	.string	"__FLT64X_MAX__ 1.18973149535723176508575932662800702e+4932F64x"
.LASF246:
	.string	"__FLT128_MANT_DIG__ 113"
.LASF28:
	.string	"__ORDER_LITTLE_ENDIAN__ 1234"
.LASF400:
	.string	"INT_FAST32_MIN (-INT_FAST32_MAX - 1)"
.LASF551:
	.string	"color"
.LASF319:
	.string	"__SIZEOF_PTRDIFF_T__ 8"
.LASF405:
	.string	"INT_FAST64_MIN"
.LASF424:
	.string	"PTRDIFF_MIN (-PTRDIFF_MAX - 1)"
.LASF408:
	.string	"UINT_FAST64_MAX __UINT_FAST64_MAX__"
.LASF470:
	.string	"__PTRDIFF_T "
.LASF567:
	.string	"draw_beacon"
.LASF47:
	.string	"__INT32_TYPE__ int"
.LASF279:
	.string	"__FLT64X_DIG__ 33"
.LASF290:
	.string	"__FLT64X_HAS_DENORM__ 1"
.LASF349:
	.string	"INT32_MAX"
.LASF530:
	.string	"va_copy(d,s) __builtin_va_copy(d,s)"
.LASF560:
	.string	"size_t"
.LASF558:
	.string	"skyline_windows"
.LASF417:
	.string	"INTMAX_MIN"
.LASF209:
	.string	"__FLT16_DENORM_MIN__ 5.96046447753906250000000000000000000e-8F16"
.LASF332:
	.string	"__riscv_a 2001000"
.LASF557:
	.string	"skyline_star_list"
.LASF493:
	.string	"_GCC_SIZE_T "
.LASF281:
	.string	"__FLT64X_MIN_10_EXP__ (-4931)"
.LASF525:
	.string	"__need___va_list"
.LASF559:
	.string	"skyline_win_cnt"
.LASF136:
	.string	"__UINT_FAST8_MAX__ 0xffffffffU"
.LASF509:
	.string	"__INT_WCHAR_T_H "
.LASF515:
	.string	"NULL"
.LASF251:
	.string	"__FLT128_MAX_10_EXP__ 4932"
.LASF255:
	.string	"__FLT128_MIN__ 3.36210314311209350626267781732175260e-4932F128"
.LASF233:
	.string	"__FLT64_MIN_10_EXP__ (-307)"
.LASF172:
	.string	"__DBL_MAX__ ((double)1.79769313486231570814527423731704357e+308L)"
.LASF520:
	.string	"_GCC_MAX_ALIGN_T "
.LASF448:
	.string	"UINT8_C(c) __UINT8_C(c)"
.LASF62:
	.string	"__INT_FAST16_TYPE__ int"
.LASF200:
	.string	"__FLT16_MIN_EXP__ (-13)"
.LASF220:
	.string	"__FLT32_DECIMAL_DIG__ 9"
.LASF247:
	.string	"__FLT128_DIG__ 33"
.LASF50:
	.string	"__UINT16_TYPE__ short unsigned int"
.LASF189:
	.string	"__LDBL_MAX__ 1.18973149535723176508575932662800702e+4932L"
.LASF88:
	.string	"__WCHAR_WIDTH__ 32"
.LASF304:
	.string	"__GCC_ATOMIC_CHAR_LOCK_FREE 2"
.LASF469:
	.string	"_T_PTRDIFF "
.LASF456:
	.string	"INTMAX_C(c) __INTMAX_C(c)"
.LASF76:
	.string	"__LONG_LONG_MAX__ 0x7fffffffffffffffLL"
.LASF534:
	.string	"_VA_LIST_DEFINED "
.LASF58:
	.string	"__UINT_LEAST16_TYPE__ short unsigned int"
.LASF154:
	.string	"__FLT_MAX_10_EXP__ 38"
.LASF192:
	.string	"__LDBL_EPSILON__ 1.92592994438723585305597794258492732e-34L"
.LASF272:
	.string	"__FLT32X_EPSILON__ 2.22044604925031308084726333618164062e-16F32x"
.LASF202:
	.string	"__FLT16_MAX_EXP__ 16"
.LASF297:
	.string	"__NO_INLINE__ 1"
.LASF455:
	.string	"INTMAX_C"
.LASF488:
	.string	"_SIZE_T_DEFINED "
.LASF92:
	.string	"__INTMAX_MAX__ 0x7fffffffffffffffL"
.LASF361:
	.string	"INT_LEAST8_MAX"
.LASF236:
	.string	"__FLT64_DECIMAL_DIG__ 17"
.LASF301:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1"
.LASF468:
	.string	"_T_PTRDIFF_ "
.LASF216:
	.string	"__FLT32_MIN_EXP__ (-125)"
.LASF36:
	.string	"__SIZE_TYPE__ long unsigned int"
.LASF435:
	.string	"WINT_MAX"
.LASF286:
	.string	"__FLT64X_NORM_MAX__ 1.18973149535723176508575932662800702e+4932F64x"
.LASF137:
	.string	"__UINT_FAST16_MAX__ 0xffffffffU"
.LASF66:
	.string	"__UINT_FAST16_TYPE__ unsigned int"
.LASF156:
	.string	"__FLT_MAX__ 3.40282346638528859811704183484516925e+38F"
.LASF168:
	.string	"__DBL_MIN_10_EXP__ (-307)"
.LASF563:
	.string	"char"
.LASF398:
	.string	"INT_FAST32_MAX __INT_FAST32_MAX__"
.LASF184:
	.string	"__LDBL_MIN_10_EXP__ (-4931)"
.LASF383:
	.string	"UINT_LEAST64_MAX"
.LASF215:
	.string	"__FLT32_DIG__ 6"
.LASF336:
	.string	"_GCC_STDINT_H "
.LASF71:
	.string	"__GXX_ABI_VERSION 1019"
.LASF242:
	.string	"__FLT64_HAS_DENORM__ 1"
.LASF542:
	.string	"uint8_t"
.LASF145:
	.string	"__FLT_EVAL_METHOD__ 0"
.LASF274:
	.string	"__FLT32X_HAS_DENORM__ 1"
.LASF464:
	.string	"_STDDEF_H "
.LASF443:
	.string	"INT32_C"
.LASF93:
	.string	"__INTMAX_C(c) c ## L"
.LASF65:
	.string	"__UINT_FAST8_TYPE__ unsigned int"
.LASF355:
	.string	"INT64_MAX"
.LASF532:
	.string	"_VA_LIST_ "
.LASF307:
	.string	"__GCC_ATOMIC_WCHAR_T_LOCK_FREE 2"
.LASF329:
	.string	"__riscv_arch_test 1"
.LASF479:
	.string	"__SIZE_T__ "
.LASF321:
	.string	"__riscv_atomic 1"
.LASF117:
	.string	"__INT_LEAST64_MAX__ 0x7fffffffffffffffL"
.LASF110:
	.string	"__INT_LEAST8_WIDTH__ 8"
.LASF104:
	.string	"__UINT8_MAX__ 0xff"
.LASF533:
	.string	"_VA_LIST "
.LASF536:
	.string	"__va_list__ "
.LASF105:
	.string	"__UINT16_MAX__ 0xffff"
.LASF581:
	.string	"kfree"
.LASF362:
	.string	"INT_LEAST8_MAX __INT_LEAST8_MAX__"
.LASF497:
	.string	"__wchar_t__ "
.LASF64:
	.string	"__INT_FAST64_TYPE__ long int"
.LASF207:
	.string	"__FLT16_MIN__ 6.10351562500000000000000000000000000e-5F16"
.LASF106:
	.string	"__UINT32_MAX__ 0xffffffffU"
.LASF96:
	.string	"__INTMAX_WIDTH__ 64"
.LASF143:
	.string	"__GCC_IEC_559 0"
.LASF81:
	.string	"__PTRDIFF_MAX__ 0x7fffffffffffffffL"
.LASF54:
	.string	"__INT_LEAST16_TYPE__ short int"
.LASF218:
	.string	"__FLT32_MAX_EXP__ 128"
.LASF431:
	.string	"WCHAR_MAX"
.LASF388:
	.string	"INT_FAST8_MIN (-INT_FAST8_MAX - 1)"
.LASF514:
	.string	"__need_wchar_t"
.LASF439:
	.string	"INT8_C"
.LASF166:
	.string	"__DBL_DIG__ 15"
.LASF512:
	.string	"__DEFINED_wchar_t "
.LASF108:
	.string	"__INT_LEAST8_MAX__ 0x7f"
.LASF421:
	.string	"PTRDIFF_MAX"
.LASF17:
	.string	"__LP64__ 1"
.LASF310:
	.string	"__GCC_ATOMIC_LONG_LOCK_FREE 2"
.LASF263:
	.string	"__FLT32X_DIG__ 15"
.LASF262:
	.string	"__FLT32X_MANT_DIG__ 53"
.LASF16:
	.string	"_LP64 1"
.LASF402:
	.string	"UINT_FAST32_MAX __UINT_FAST32_MAX__"
.LASF569:
	.string	"draw_window"
.LASF502:
	.string	"__WCHAR_T "
.LASF561:
	.string	"long long int"
.LASF26:
	.string	"__CHAR_BIT__ 8"
.LASF433:
	.string	"WCHAR_MIN"
.LASF174:
	.string	"__DBL_MIN__ ((double)2.22507385850720138309023271733240406e-308L)"
.LASF441:
	.string	"INT16_C"
.LASF391:
	.string	"INT_FAST16_MAX"
.LASF79:
	.string	"__WINT_MAX__ 0xffffffffU"
.LASF303:
	.string	"__GCC_ATOMIC_BOOL_LOCK_FREE 2"
.LASF15:
	.string	"__FINITE_MATH_ONLY__ 0"
.LASF367:
	.string	"INT_LEAST16_MAX"
.LASF237:
	.string	"__FLT64_MAX__ 1.79769313486231570814527423731704357e+308F64"
.LASF478:
	.string	"__size_t__ "
.LASF346:
	.string	"INT16_MIN (-INT16_MAX - 1)"
.LASF394:
	.string	"INT_FAST16_MIN (-INT_FAST16_MAX - 1)"
.LASF412:
	.string	"INTPTR_MIN (-INTPTR_MAX - 1)"
.LASF363:
	.string	"INT_LEAST8_MIN"
.LASF126:
	.string	"__UINT_LEAST64_MAX__ 0xffffffffffffffffUL"
.LASF387:
	.string	"INT_FAST8_MIN"
.LASF322:
	.string	"__riscv_mul 1"
.LASF271:
	.string	"__FLT32X_MIN__ 2.22507385850720138309023271733240406e-308F32x"
.LASF70:
	.string	"__UINTPTR_TYPE__ long unsigned int"
.LASF291:
	.string	"__FLT64X_HAS_INFINITY__ 1"
.LASF112:
	.string	"__INT16_C(c) c"
.LASF314:
	.string	"__GCC_HAVE_DWARF2_CFI_ASM 1"
.LASF22:
	.string	"__SIZEOF_FLOAT__ 4"
.LASF132:
	.string	"__INT_FAST32_MAX__ 0x7fffffff"
.LASF292:
	.string	"__FLT64X_HAS_QUIET_NAN__ 1"
.LASF162:
	.string	"__FLT_HAS_INFINITY__ 1"
.LASF211:
	.string	"__FLT16_HAS_INFINITY__ 1"
.LASF579:
	.string	"GNU C17 14.2.0 -mcmodel=medany -mabi=lp64 -mno-relax -mno-riscv-attribute -mtune=rocket -misa-spec=20191213 -march=rv64ima_zicsr -ggdb3 -gdwarf-2 -fno-omit-frame-pointer -fno-pie -fno-common -ffreestanding -fno-asynchronous-unwind-tables"
.LASF506:
	.string	"_WCHAR_T_DEFINED "
.LASF565:
	.string	"imgcur"
.LASF169:
	.string	"__DBL_MAX_EXP__ 1024"
.LASF19:
	.string	"__SIZEOF_LONG__ 8"
.LASF14:
	.string	"__ATOMIC_CONSUME 1"
.LASF467:
	.string	"_PTRDIFF_T "
.LASF133:
	.string	"__INT_FAST32_WIDTH__ 32"
.LASF414:
	.string	"UINTPTR_MAX __UINTPTR_MAX__"
.LASF278:
	.string	"__FLT64X_MANT_DIG__ 113"
.LASF39:
	.string	"__WINT_TYPE__ unsigned int"
.LASF531:
	.string	"__va_copy(d,s) __builtin_va_copy(d,s)"
.LASF101:
	.string	"__INT16_MAX__ 0x7fff"
.LASF83:
	.string	"__SCHAR_WIDTH__ 8"
.LASF125:
	.string	"__UINT32_C(c) c ## U"
.LASF333:
	.string	"__riscv_zicsr 2000000"
.LASF23:
	.string	"__SIZEOF_DOUBLE__ 8"
.LASF7:
	.string	"__GNUC_PATCHLEVEL__ 0"
.LASF121:
	.string	"__UINT8_C(c) c"
.LASF384:
	.string	"UINT_LEAST64_MAX __UINT_LEAST64_MAX__"
.LASF204:
	.string	"__FLT16_DECIMAL_DIG__ 5"
.LASF80:
	.string	"__WINT_MIN__ 0U"
.LASF376:
	.string	"INT_LEAST32_MIN (-INT_LEAST32_MAX - 1)"
.LASF399:
	.string	"INT_FAST32_MIN"
.LASF157:
	.string	"__FLT_NORM_MAX__ 3.40282346638528859811704183484516925e+38F"
.LASF2:
	.string	"__STDC_UTF_16__ 1"
.LASF229:
	.string	"__FLT32_IS_IEC_60559__ 1"
.LASF356:
	.string	"INT64_MAX __INT64_MAX__"
.LASF401:
	.string	"UINT_FAST32_MAX"
.LASF325:
	.string	"__riscv_xlen 64"
.LASF375:
	.string	"INT_LEAST32_MIN"
.LASF472:
	.string	"_BSD_PTRDIFF_T_ "
.LASF195:
	.string	"__LDBL_HAS_INFINITY__ 1"
.LASF144:
	.string	"__GCC_IEC_559_COMPLEX 0"
.LASF452:
	.string	"UINT32_C(c) __UINT32_C(c)"
.LASF461:
	.string	"SKYLINE_HEIGHT 480"
.LASF190:
	.string	"__LDBL_NORM_MAX__ 1.18973149535723176508575932662800702e+4932L"
.LASF317:
	.string	"__SIZEOF_WCHAR_T__ 4"
.LASF249:
	.string	"__FLT128_MIN_10_EXP__ (-4931)"
.LASF32:
	.string	"__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__"
.LASF300:
	.string	"__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1"
.LASF411:
	.string	"INTPTR_MIN"
.LASF225:
	.string	"__FLT32_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F32"
.LASF496:
	.string	"__need_size_t"
.LASF98:
	.string	"__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)"
.LASF451:
	.string	"UINT32_C"
.LASF102:
	.string	"__INT32_MAX__ 0x7fffffff"
.LASF430:
	.string	"SIZE_MAX __SIZE_MAX__"
.LASF11:
	.string	"__ATOMIC_ACQUIRE 2"
.LASF123:
	.string	"__UINT16_C(c) c"
.LASF444:
	.string	"INT32_C(c) __INT32_C(c)"
.LASF358:
	.string	"INT64_MIN (-INT64_MAX - 1)"
.LASF284:
	.string	"__FLT64X_DECIMAL_DIG__ 36"
.LASF562:
	.string	"long double"
.LASF544:
	.string	"uint16_t"
.LASF41:
	.string	"__UINTMAX_TYPE__ long unsigned int"
.LASF210:
	.string	"__FLT16_HAS_DENORM__ 1"
.LASF366:
	.string	"UINT_LEAST8_MAX __UINT_LEAST8_MAX__"
.LASF343:
	.string	"INT16_MAX"
.LASF494:
	.string	"_SIZET_ "
.LASF404:
	.string	"INT_FAST64_MAX __INT_FAST64_MAX__"
.LASF500:
	.string	"_T_WCHAR_ "
.LASF159:
	.string	"__FLT_EPSILON__ 1.19209289550781250000000000000000000e-7F"
.LASF99:
	.string	"__SIG_ATOMIC_WIDTH__ 32"
.LASF454:
	.string	"UINT64_C(c) __UINT64_C(c)"
.LASF335:
	.string	"_SKYLINE_H_ "
.LASF73:
	.string	"__SHRT_MAX__ 0x7fff"
.LASF30:
	.string	"__ORDER_PDP_ENDIAN__ 3412"
.LASF155:
	.string	"__FLT_DECIMAL_DIG__ 9"
.LASF235:
	.string	"__FLT64_MAX_10_EXP__ 308"
.LASF183:
	.string	"__LDBL_MIN_EXP__ (-16381)"
.LASF89:
	.string	"__WINT_WIDTH__ 32"
.LASF287:
	.string	"__FLT64X_MIN__ 3.36210314311209350626267781732175260e-4932F64x"
.LASF382:
	.string	"INT_LEAST64_MIN (-INT_LEAST64_MAX - 1)"
.LASF449:
	.string	"UINT16_C"
.LASF294:
	.string	"__REGISTER_PREFIX__ "
.LASF224:
	.string	"__FLT32_EPSILON__ 1.19209289550781250000000000000000000e-7F32"
.LASF406:
	.string	"INT_FAST64_MIN (-INT_FAST64_MAX - 1)"
.LASF330:
	.string	"__riscv_i 2001000"
.LASF539:
	.string	"short int"
.LASF583:
	.string	"add_star"
.LASF97:
	.string	"__SIG_ATOMIC_MAX__ 0x7fffffff"
.LASF75:
	.string	"__LONG_MAX__ 0x7fffffffffffffffL"
.LASF260:
	.string	"__FLT128_HAS_QUIET_NAN__ 1"
.LASF440:
	.string	"INT8_C(c) __INT8_C(c)"
.LASF460:
	.string	"SKYLINE_WIDTH 640"
.LASF265:
	.string	"__FLT32X_MIN_10_EXP__ (-307)"
.LASF540:
	.string	"long int"
.LASF446:
	.string	"INT64_C(c) __INT64_C(c)"
.LASF453:
	.string	"UINT64_C"
.LASF95:
	.string	"__UINTMAX_C(c) c ## UL"
.LASF43:
	.string	"__CHAR32_TYPE__ unsigned int"
.LASF491:
	.string	"__DEFINED_size_t "
.LASF283:
	.string	"__FLT64X_MAX_10_EXP__ 4932"
.LASF529:
	.string	"va_arg(v,l) __builtin_va_arg(v,l)"
.LASF264:
	.string	"__FLT32X_MIN_EXP__ (-1021)"
.LASF407:
	.string	"UINT_FAST64_MAX"
.LASF141:
	.string	"__INTPTR_WIDTH__ 64"
.LASF381:
	.string	"INT_LEAST64_MIN"
.LASF422:
	.string	"PTRDIFF_MAX __PTRDIFF_MAX__"
.LASF323:
	.string	"__riscv_div 1"
.LASF115:
	.string	"__INT32_C(c) c"
.LASF196:
	.string	"__LDBL_HAS_QUIET_NAN__ 1"
.LASF45:
	.string	"__INT8_TYPE__ signed char"
.LASF295:
	.string	"__USER_LABEL_PREFIX__ "
.LASF419:
	.string	"UINTMAX_MAX"
.LASF423:
	.string	"PTRDIFF_MIN"
.LASF471:
	.string	"_PTRDIFF_T_ "
.LASF474:
	.string	"_GCC_PTRDIFF_T "
.LASF537:
	.string	"HEAP_ALLOC_MAX 4000"
.LASF548:
	.string	"uint64_t"
.LASF51:
	.string	"__UINT32_TYPE__ unsigned int"
.LASF138:
	.string	"__UINT_FAST32_MAX__ 0xffffffffU"
.LASF256:
	.string	"__FLT128_EPSILON__ 1.92592994438723585305597794258492732e-34F128"
.LASF371:
	.string	"UINT_LEAST16_MAX"
.LASF20:
	.string	"__SIZEOF_LONG_LONG__ 8"
.LASF573:
	.string	"last"
.LASF49:
	.string	"__UINT8_TYPE__ unsigned char"
.LASF84:
	.string	"__SHRT_WIDTH__ 16"
.LASF353:
	.string	"UINT32_MAX"
.LASF350:
	.string	"INT32_MAX __INT32_MAX__"
.LASF238:
	.string	"__FLT64_NORM_MAX__ 1.79769313486231570814527423731704357e+308F64"
.LASF578:
	.string	"starp"
.LASF328:
	.string	"__riscv_misaligned_slow 1"
.LASF518:
	.string	"offsetof"
.LASF33:
	.string	"__SIZEOF_POINTER__ 8"
.LASF240:
	.string	"__FLT64_EPSILON__ 2.22044604925031308084726333618164062e-16F64"
.LASF526:
	.string	"__GNUC_VA_LIST "
.LASF77:
	.string	"__WCHAR_MAX__ 0x7fffffff"
.LASF149:
	.string	"__FLT_MANT_DIG__ 24"
.LASF85:
	.string	"__INT_WIDTH__ 32"
.LASF252:
	.string	"__FLT128_DECIMAL_DIG__ 36"
.LASF313:
	.string	"__GCC_ATOMIC_POINTER_LOCK_FREE 2"
.LASF377:
	.string	"UINT_LEAST32_MAX"
.LASF187:
	.string	"__DECIMAL_DIG__ 36"
.LASF326:
	.string	"__riscv_float_abi_soft 1"
.LASF1:
	.string	"__STDC_VERSION__ 201710L"
.LASF549:
	.string	"long unsigned int"
.LASF306:
	.string	"__GCC_ATOMIC_CHAR32_T_LOCK_FREE 2"
.LASF458:
	.string	"UINTMAX_C(c) __UINTMAX_C(c)"
.LASF499:
	.string	"_WCHAR_T "
.LASF571:
	.string	"x_iter"
.LASF8:
	.string	"__VERSION__ \"14.2.0\""
.LASF492:
	.string	"___int_size_t_h "
.LASF4:
	.string	"__STDC_HOSTED__ 0"
.LASF365:
	.string	"UINT_LEAST8_MAX"
.LASF91:
	.string	"__SIZE_WIDTH__ 64"
.LASF473:
	.string	"___int_ptrdiff_t_h "
.LASF501:
	.string	"_T_WCHAR "
.LASF364:
	.string	"INT_LEAST8_MIN (-INT_LEAST8_MAX - 1)"
.LASF554:
	.string	"skyline_beacon"
.LASF212:
	.string	"__FLT16_HAS_QUIET_NAN__ 1"
.LASF86:
	.string	"__LONG_WIDTH__ 64"
.LASF498:
	.string	"__WCHAR_T__ "
.LASF103:
	.string	"__INT64_MAX__ 0x7fffffffffffffffL"
.LASF153:
	.string	"__FLT_MAX_EXP__ 128"
.LASF13:
	.string	"__ATOMIC_ACQ_REL 4"
.LASF466:
	.string	"_ANSI_STDDEF_H "
.LASF543:
	.string	"unsigned char"
.LASF351:
	.string	"INT32_MIN"
.LASF48:
	.string	"__INT64_TYPE__ long int"
.LASF158:
	.string	"__FLT_MIN__ 1.17549435082228750796873653722224568e-38F"
.LASF390:
	.string	"UINT_FAST8_MAX __UINT_FAST8_MAX__"
.LASF582:
	.string	"kmalloc"
.LASF223:
	.string	"__FLT32_MIN__ 1.17549435082228750796873653722224568e-38F32"
.LASF147:
	.string	"__DEC_EVAL_METHOD__ 2"
.LASF503:
	.string	"_WCHAR_T_ "
.LASF188:
	.string	"__LDBL_DECIMAL_DIG__ 36"
.LASF516:
	.string	"NULL ((void *)0)"
.LASF324:
	.string	"__riscv_muldiv 1"
.LASF116:
	.string	"__INT_LEAST32_WIDTH__ 32"
.LASF35:
	.string	"__GNUC_WIDE_EXECUTION_CHARSET_NAME \"UTF-32LE\""
.LASF67:
	.string	"__UINT_FAST32_TYPE__ unsigned int"
.LASF359:
	.string	"UINT64_MAX"
.LASF504:
	.string	"_BSD_WCHAR_T_ "
.LASF60:
	.string	"__UINT_LEAST64_TYPE__ long unsigned int"
.LASF434:
	.string	"WCHAR_MIN __WCHAR_MIN__"
.LASF338:
	.string	"INT8_MAX __INT8_MAX__"
.LASF82:
	.string	"__SIZE_MAX__ 0xffffffffffffffffUL"
.LASF465:
	.string	"_STDDEF_H_ "
.LASF232:
	.string	"__FLT64_MIN_EXP__ (-1021)"
.LASF180:
	.string	"__DBL_IS_IEC_60559__ 1"
.LASF308:
	.string	"__GCC_ATOMIC_SHORT_LOCK_FREE 2"
.LASF380:
	.string	"INT_LEAST64_MAX __INT_LEAST64_MAX__"
.LASF462:
	.string	"SKYLINE_WIN_MAX 4000"
.LASF277:
	.string	"__FLT32X_IS_IEC_60559__ 1"
.LASF152:
	.string	"__FLT_MIN_10_EXP__ (-37)"
.LASF164:
	.string	"__FLT_IS_IEC_60559__ 1"
.LASF447:
	.string	"UINT8_C"
.LASF146:
	.string	"__FLT_EVAL_METHOD_TS_18661_3__ 0"
.LASF178:
	.string	"__DBL_HAS_INFINITY__ 1"
.LASF426:
	.string	"SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__"
.LASF118:
	.string	"__INT64_C(c) c ## L"
.LASF63:
	.string	"__INT_FAST32_TYPE__ int"
.LASF163:
	.string	"__FLT_HAS_QUIET_NAN__ 1"
.LASF37:
	.string	"__PTRDIFF_TYPE__ long int"
.LASF418:
	.string	"INTMAX_MIN (-INTMAX_MAX - 1)"
.LASF320:
	.string	"__riscv 1"
.LASF173:
	.string	"__DBL_NORM_MAX__ ((double)1.79769313486231570814527423731704357e+308L)"
.LASF437:
	.string	"WINT_MIN"
.LASF396:
	.string	"UINT_FAST16_MAX __UINT_FAST16_MAX__"
.LASF519:
	.string	"offsetof(TYPE,MEMBER) __builtin_offsetof (TYPE, MEMBER)"
.LASF490:
	.string	"_SIZE_T_DECLARED "
.LASF331:
	.string	"__riscv_m 2000000"
.LASF538:
	.string	"signed char"
.LASF555:
	.string	"period"
.LASF276:
	.string	"__FLT32X_HAS_QUIET_NAN__ 1"
.LASF170:
	.string	"__DBL_MAX_10_EXP__ 308"
.LASF354:
	.string	"UINT32_MAX __UINT32_MAX__"
.LASF545:
	.string	"short unsigned int"
.LASF385:
	.string	"INT_FAST8_MAX"
.LASF296:
	.string	"__GNUC_STDC_INLINE__ 1"
.LASF409:
	.string	"INTPTR_MAX"
.LASF27:
	.string	"__BIGGEST_ALIGNMENT__ 16"
.LASF438:
	.string	"WINT_MIN __WINT_MIN__"
.LASF257:
	.string	"__FLT128_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966F128"
.LASF481:
	.string	"_SYS_SIZE_T_H "
.LASF344:
	.string	"INT16_MAX __INT16_MAX__"
.LASF129:
	.string	"__INT_FAST8_WIDTH__ 32"
.LASF151:
	.string	"__FLT_MIN_EXP__ (-125)"
.LASF0:
	.string	"__STDC__ 1"
.LASF495:
	.string	"__size_t "
.LASF254:
	.string	"__FLT128_NORM_MAX__ 1.18973149535723176508575932662800702e+4932F128"
.LASF334:
	.string	"__ELF__ 1"
.LASF341:
	.string	"UINT8_MAX"
.LASF348:
	.string	"UINT16_MAX __UINT16_MAX__"
.LASF205:
	.string	"__FLT16_MAX__ 6.55040000000000000000000000000000000e+4F16"
.LASF357:
	.string	"INT64_MIN"
.LASF124:
	.string	"__UINT_LEAST32_MAX__ 0xffffffffU"
.LASF552:
	.string	"skyline_star"
.LASF267:
	.string	"__FLT32X_MAX_10_EXP__ 308"
.LASF476:
	.string	"__DEFINED_ptrdiff_t "
.LASF177:
	.string	"__DBL_HAS_DENORM__ 1"
.LASF203:
	.string	"__FLT16_MAX_10_EXP__ 4"
.LASF270:
	.string	"__FLT32X_NORM_MAX__ 1.79769313486231570814527423731704357e+308F32x"
.LASF360:
	.string	"UINT64_MAX __UINT64_MAX__"
.LASF241:
	.string	"__FLT64_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F64"
.LASF450:
	.string	"UINT16_C(c) __UINT16_C(c)"
.LASF219:
	.string	"__FLT32_MAX_10_EXP__ 38"
.LASF395:
	.string	"UINT_FAST16_MAX"
.LASF570:
	.string	"y_iter"
.LASF90:
	.string	"__PTRDIFF_WIDTH__ 64"
.LASF114:
	.string	"__INT_LEAST32_MAX__ 0x7fffffff"
.LASF568:
	.string	"start_beacon"
.LASF68:
	.string	"__UINT_FAST64_TYPE__ long unsigned int"
.LASF572:
	.string	"remove_window"
.LASF186:
	.string	"__LDBL_MAX_10_EXP__ 4932"
.LASF221:
	.string	"__FLT32_MAX__ 3.40282346638528859811704183484516925e+38F32"
.LASF175:
	.string	"__DBL_EPSILON__ ((double)2.22044604925031308084726333618164062e-16L)"
.LASF315:
	.string	"__PRAGMA_REDEFINE_EXTNAME 1"
.LASF44:
	.string	"__SIG_ATOMIC_TYPE__ int"
.LASF258:
	.string	"__FLT128_HAS_DENORM__ 1"
.LASF374:
	.string	"INT_LEAST32_MAX __INT_LEAST32_MAX__"
.LASF261:
	.string	"__FLT128_IS_IEC_60559__ 1"
.LASF194:
	.string	"__LDBL_HAS_DENORM__ 1"
.LASF487:
	.string	"_SIZE_T_DEFINED_ "
.LASF436:
	.string	"WINT_MAX __WINT_MAX__"
.LASF553:
	.string	"skyline_window"
.LASF413:
	.string	"UINTPTR_MAX"
.LASF352:
	.string	"INT32_MIN (-INT32_MAX - 1)"
.LASF230:
	.string	"__FLT64_MANT_DIG__ 53"
.LASF389:
	.string	"UINT_FAST8_MAX"
.LASF161:
	.string	"__FLT_HAS_DENORM__ 1"
.LASF566:
	.string	"fbcur"
.LASF171:
	.string	"__DBL_DECIMAL_DIG__ 17"
	.ident	"GCC: (g04696df09-dirty) 14.2.0"
	.section	.note.GNU-stack,"",@progbits
