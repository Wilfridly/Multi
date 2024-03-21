#################################################################################
#	File : reset.s
#	Author : Alain Greiner
#	Date : 25/12/2011
#################################################################################
#       This is a boot code for a mono-processor architecture.
#       - initializes the interrupt vector for DMA and TTY.
#       - initializes the ICU MASK register for DMA and TTY.
#       - initializes the Status Register.
#       - initializes the stack pointer.
#       - initializes the EPC register, and jumps to the user code.
#################################################################################
		
	.section .reset,"ax",@progbits

	.extern	seg_stack_base
	.extern	seg_data_base
	.extern	seg_icu_base

	.func	reset
	.type   reset, %function

reset:
       	.set noreorder

	# get the processor id
    # mfc0	$27,	$15,	1		# get the proc_id
    # andi	$27,	$27,	0x1		# no more than 2 processors
    # bne	$27,	$0,	proc1
	
proc0:
	# initialises interrupt vector 
	la	$26,	_interrupt_vector
	la	$27,	_isr_dma
	sw	$27,	0($26)			# _interrupt_vector[0] <= _isr_dma
	la	$27,	_isr_tty_get
	sw	$27,	12($26)			# _interrupt_vector[3] <= _isr_tty_get

	#initializes the ICU MASK[0] register
	la $12, seg_icu_base
	addiu $8, $0, 0x5
	sw $8, 0x8($12)
	# nor $8, $8, $8
	# sw $8, 0xC($12)

	mfc0  $27,    $15, 1
	addiu $27,    $27, 1
	li    $28,    0x40000
	mult  $27,    $28
	mflo  $27 

	# initializes stack pointer 
	la	$29,	seg_stack_base
	addu	$29,	$29,	$27    		# $29 <= seg_stack_base + 64K

	# initializes SR register
	li	$26,	0x0000FF13	
	mtc0	$26,	$12			# SR <= 0x0000FF13

	# jump to main in user mode
	la	$26,	seg_data_base
	lw	$26,	0($26)			# $26 <= main[0]
	mtc0	$26,	$14			# write it in EPC register
	eret

# proc1:
# 	# initialises interrupt vector 
# 	la	$26,	_interrupt_vector
# 	la	$27,	_isr_dma
# 	sw	$27,	0($26)			# _interrupt_vector[0] <= _isr_dma
# 	la	$27,	_isr_tty_get
# 	sw	$27,	28($26)			# _interrupt_vector[3] <= _isr_tty_get

# 	#initializes the ICU MASK[0] register
# 	la $12, seg_icu_base
# 	addiu $8, $0, 0x10
# 	sw $8, 0x28($12)
# 	nor $8, $8, $8
# 	sw $8, 0x2C($12)

# 	mfc0  $27,    $15, 1
# 	addiu $27,    $27, 1
# 	li    $28,         0x40000
# 	mult  $27,    $28
# 	mflo  $27 

# 	# initializes stack pointer 
# 	la	$29,	seg_stack_base
# 	addu	$29,	$29,	$27    		# $29 <= seg_stack_base + 64K

# 	# initializes SR register
# 	li	$26,	0x0000FF13	
# 	mtc0	$26,	$12			# SR <= 0x0000FF13

# 	# jump to main in user mode
# 	la	$26,	seg_data_base
# 	lw	$26,	0($26)			# $26 <= main[0]
# 	mtc0	$26,	$14			# write it in EPC register
# 	eret


	.set reorder
	.endfunc
	.size	reset, .-reset

