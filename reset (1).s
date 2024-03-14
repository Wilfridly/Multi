#################################################################################
#	File : reset.s
#	Author : Alain Greiner
#	Date : 25/12/2011
#################################################################################
#       This is an improved boot code for a bi-processor architecture.
#	Depending on the proc_id, each processor
#       - initializes the interrupt vector.
#       - initializes the ICU MASK registers.
#       - initializes the TIMER .
#       - initializes the Status Register.
#       - initializes the stack pointer.
#       - initializes the EPC register, and jumps to the user code.
#################################################################################
		
	.section .reset,"ax",@progbits

	.extern	seg_stack_base
	.extern	seg_data_base
        .extern	_interrupt_vector
        .extern	_isr_dma
        .extern	_isr_ioc
        .extern	_isr_timer
        .extern	r_fsm_state
        .extern	seg_timer_base
        .extern seg_icu_base

	.func	reset
	.type   reset, %function

reset:
       	.set noreorder

# get the processor id
        mfc0	$27,	$15,	1		# get the proc_id
        andi	$27,	$27,	0x1		# no more than 2 processors
        bne	$27,	$0,	proc1

proc0:
        # initialises interrupt vector entries for PROC[0]
        la $12, _interrupt_vector
        la $13, _isr_timer
        sw $13, 8($12)

        # initializes the ICU[0] MASK register

        la $12, seg_icu_base
        addiu $8, $0, 0x4
        sw $8, 0x8($12)
        nor $8, $8, $8
        sw $8, 0xC($12)

        # initializes TIMER[0] PERIOD and RUNNING registers
        # load base segment + set la period
        la $12, seg_timer_base
        li $13, 50000
        sw $13, 8($12)
        sw $13, 4($12)

        # initializes stack pointer for PROC[0]
	la	$29,	seg_stack_base
        li	$27,	0x10000			# stack size = 64K
	addu	$29,	$29,	$27    		# $29 <= seg_stack_base + 64K

        # initializes SR register for PROC[0]
       	li	$26,	0x0000FF13	
       	mtc0	$26,	$12			# SR <= 0x0000FF13

        # jump to main in user mode: main[0]
	la	$26,	seg_data_base
        lw	$26,	0($26)			# $26 <= main[0]
	mtc0	$26,	$14			# write it in EPC register
	eret

proc1:
        # initialises interrupt vector entries for PROC[1]
        la $12, _interrupt_vector        
        la $13, _isr_timer
        sw $13, 16($12)

        # initializes the ICU[1] MASK register
        la $12, seg_icu_base
        addiu $8, $0, 0x10
        sw $8, 0x28($12)
        nor $8, $8, $8
        sw $8, 0x2C($12)

        # initializes TIMER[1] PERIOD and RUNNING registers
        # load base segment + set la period
        la $12, seg_timer_base
        li $13, 100000
        sw $13, 0x18($12)
        sw $13, 0x14($12)


        # initializes stack pointer for PROC[1]
        la	$29,	seg_stack_base
        li	$27,	 0x20000			# stack size = 64K
	addu	$29,	$29,	$27    		# $29 <= seg_stack_base + 64K


        # initializes SR register for PROC[1]
        li	$26,	0x0000FF13	
       	mtc0	$26,	$12			# SR <= 0x0000FF13

        # jump to main in user mode: main[1]
        la	$26,	seg_data_base
        lw	$26,	4($26)			# $26 <= main[0]
	mtc0	$26,	$14			# write it in EPC register
	eret

	.set reorder

	.set reorder

	.endfunc
	.size	reset, .-reset

