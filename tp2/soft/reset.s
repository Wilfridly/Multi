#################################################################################
#	File : reset.s
#	Author : Alain Greiner
#	Date : 15/12/2011
#################################################################################
#	- It initializes the Status Register (SR) 
#	- It defines the stack size  and initializes the stack pointer ($29) 
#	- It initializes the EPC register, and jumps to user code.
#################################################################################
		
	.section .reset,"ax",@progbits

	.extern	seg_stack_base
	.extern	seg_data_base

	.func	reset
	.type   reset, %function

reset:
       	.set noreorder

# initializes stack pointer
	la	$29,	TO BE COMPLETED		# $29 <= seg_stack_base
	addiu	$29,	TO BE COMPLETED		# stack size = 16 Kbytes

# initializes SR register
       	li	$26,	0x0000FF13	
       	mtc0	$26,	$12			# SR <= 0x0000FF13

# jump to main in user mode
	la	$26,	seg_data_base
        lw	$26,	0($26)			# get the user code entry point 
	mtc0	$26,	$14			# write it in EPC register
	eret

	.set reorder

	.endfunc
	.size	reset, .-reset

