#################################################################################
#    File : reset.s
#    Author : Alain Greiner
#    Date : 15/12/2011
#################################################################################
#    - It initializes the Status Register (SR) 
#    - It defines the stack size  and initializes the stack pointer ($29) 
#    - It initializes the EPC register, and jumps to user code.
#################################################################################

.section .reset,"ax",@progbits

.extern    seg_stack_base
.extern    seg_data_base

.func    reset
.type   reset, %function

reset:
.set noreorder

# initializes stack pointer
mfc0  $27,    $15, 1
addiu $27,    $27, 1
sll   $27,    $27, 16
# mfc0  $27,    $15, 1
# addiu $27,    $27, 1
# li    $28,         0x10000
# mult  $27,    $28
# mflo  $27

la    $29,    seg_stack_base
addu  $29,    $29,    $27  # stack size = 64 Kbytes

# initializes SR register
li    $26,    0x0000FF13    
mtc0  $26,    $12            # SR <= 0x0000FF13

# jump to main in user mode
la    $26,    seg_data_base
lw    $26,    0($26)         # get the user code entry point 
mtc0  $26,    $14            # write it in EPC register
eret

.set reorder

.endfunc
.size    reset, .-reset

