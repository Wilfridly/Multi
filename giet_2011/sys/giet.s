/*
 * GIET: Interruption/Exception/Trap Handler for MIPS32 processor
 *
 * The base address of the segment containing this code MUST be 0x80000000, in
 * order to have the entry point at address 0x80000180!!! All messages are
 * printed on the TTY corresponding to the task&processor identifiers.
 *
 * It uses two arrays of functions:
 * - the _cause_vector[16] array defines the 16 causes to enter the GIET
 *   it is initialized in th exc_handler.c file
 * - the _syscall_vector[32] array defines the 32 system calls entry points
 *   it is initialised in the sys_handler.c file 
 */

    .section .giet, "ax", @progbits
    .space 0x180

/*
 * GIET Entry point (at address 0x80000180)
 */

    .func   _giet
    .type   _giet, %function

_giet:
    mfc0    $27,    $13                 /* $27 <= Cause register */
    la      $26,    _cause_vector       /* $26 <= _cause_vector */
    andi    $27,    $27,    0x3c        /* $27 <= XCODE*4 */
    addu    $26,    $26,    $27         /* $26 <= &_cause_vector[XCODE] */
    lw      $26,    ($26)               /* $26 <=  _cause_vector[XCODE] */
    jr      $26                         /* Jump indexed by XCODE */

    .endfunc
    .size _giet, .-_giet

/*
 * *** System Call Handler ***
 *
 * A system call is handled as a special function call.
 *  - $2 contains the system call index (< 16).
 *  - $3 is used to store the syscall address
 *  - $4, $5, $6, $7 contain the arguments values.
 *  - The return address (EPC) and the SR are saved in the stack.
 *  - Interrupts are enabled before branching to the syscall.
 *  - All syscalls must return to the syscall handler.
 *  - $2, $3, $4, $5, $6, $7 as well as $26 & $27 can be modified.
 *
 * In case of undefined system call, an error message displays the value of EPC
 * on the TTY corresponding to the processor, and the user program is killed.
 */

    .globl  _sys_handler
    .func   _sys_handler
    .type   _sys_handler, %function

_sys_handler:
    addiu   $29,    $29,    -24     /* 2 slots for SR&EPC, 4 slots for args passing */
    mfc0    $26,    $12             /* load SR */
    sw      $26,    16($29)         /* save it in the stack */
    mfc0    $27,    $14             /* load EPC */
    addiu   $27,    $27,    4       /* increment EPC for return address */
    sw      $27,    20($29)         /* save it in the stack */

    andi    $26,    $2,     0x1F    /* $26 <= syscall index (i < 32) */
    sll     $26,    $26,    2       /* $26 <= index * 4 */
    la      $27,    _syscall_vector /* $27 <= &_syscall_vector[0] */
    addu    $27,    $27,    $26     /* $27 <= &_syscall_vector[i] */
    lw      $3,     0($27)          /* $3  <= syscall address */

    li      $27,    0xFFFFFFED      /* Mask for UM & EXL bits */
    mfc0    $26,    $12             /* $26 <= SR */
    and     $26,    $26,    $27     /* UM = 0 / EXL = 0 */
    mtc0    $26,    $12             /* interrupt enabled */
    jalr    $3                      /* jump to the proper syscall */
    mtc0    $0,     $12             /* interrupt disbled */

    lw      $26,    16($29)         /* load SR from stack */
    mtc0    $26,    $12             /* restore SR */
    lw      $26,    20($29)         /* load EPC from stack */
    mtc0    $26,    $14             /* restore EPC */
    addiu   $29,    $29,     24     /* restore stack pointer */
    eret                            /* exit GIET */

    .endfunc
    .size _sys_handler, .-_sys_handler

/*
 * *** Interrupt Handler ***
 *
 * This simple interrupt handler cannot be interrupted.
 *
 * All non persistant registers, such as $1 to $15, and $24 to $25, as well as
 * register $31, HI, LO and EPC, are saved in the interrupted program stack, before
 * calling the Interrupt Service Routine. These registers can be used by the
 * ISR code.
 */

    .globl  _int_handler
    .func   _int_handler
    .type   _int_handler, %function

_int_handler:
    addiu   $29,    $29,    -25*4   /* stack space reservation (19 registers to
                                       save and 4 free words to call function) */
    .set noat
    sw      $1,     4*4($29)        /* save $1 */
    .set at
    sw      $2,     5*4($29)        /* save $2 */
    sw      $3,     6*4($29)        /* save $3 */
    sw      $4,     7*4($29)        /* save $4 */
    sw      $5,     8*4($29)        /* save $5 */
    sw      $6,     9*4($29)        /* save $6 */
    sw      $7,     10*4($29)       /* save $7 */
    sw      $8,     11*4($29)       /* save $8 */
    sw      $9,     12*4($29)       /* save $9 */
    sw      $10,    13*4($29)       /* save $10 */
    sw      $11,    14*4($29)       /* save $11 */
    sw      $12,    15*4($29)       /* save $12 */
    sw      $13,    16*4($29)       /* save $13 */
    sw      $14,    17*4($29)       /* save $14 */
    sw      $15,    18*4($29)       /* save $15 */
    sw      $24,    19*4($29)       /* save $24 */
    sw      $25,    20*4($29)       /* save $25 */
    sw      $31,    21*4($29)       /* save $31 */
    mflo    $26
    sw      $26,    22*4($29)       /* save LO */
    mfhi    $26
    sw      $26,    23*4($29)       /* save HI */
    mfc0    $27,    $14
    sw      $27,    24*4($29)       /* save EPC */

    la      $26,    _int_demux
    jalr    $26                     /* jump to a C function to find the proper ISR */

restore:
    .set noat
    lw      $1,     4*4($29)        /* restore $1 */
    .set at
    lw      $2,     4*5($29)        /* restore $2 */
    lw      $3,     4*6($29)        /* restore $3 */
    lw      $4,     4*7($29)        /* restore $4 */
    lw      $5,     4*8($29)        /* restore $5 */
    lw      $6,     4*9($29)        /* restore $6 */
    lw      $7,     4*10($29)       /* restore $7 */
    lw      $8,     4*11($29)       /* restore $8 */
    lw      $9,     4*12($29)       /* restore $9 */
    lw      $10,    4*13($29)       /* restore $10 */
    lw      $11,    4*14($29)       /* restore $11 */
    lw      $12,    4*15($29)       /* restore $12 */
    lw      $13,    4*16($29)       /* restore $13 */
    lw      $14,    4*17($29)       /* restore $14 */
    lw      $15,    4*18($29)       /* restore $15 */
    lw      $24,    4*19($29)       /* restore $24 */
    lw      $25,    4*20($29)       /* restore $25 */
    lw      $31,    4*21($29)       /* restore $31 */
    lw      $26,    4*22($29)
    mtlo    $26                     /* restore LO */
    lw      $26,    4*23($29)
    mthi    $26                     /* restore HI */
    lw      $27,    4*24($29)       /* return address (EPC) */
    addiu   $29,    $29,    25*4    /* restore stack pointer */
    mtc0    $27,    $14             /* restore EPC */
    eret                            /* exit GIET */

    .endfunc
    .size _int_handler, .-_int_handler

/*
 * *** _task_switch ***
 *
 * A task context is an array of 64 words = 256 bytes. It aims at containing
 * copies of all the processor registers except HI and LO which need not be saved.
 * As much as possible a register is stored at the index defined by its number
 * (for example, $8 is saved in ctx[8]).
 * The exception are :
 * - $0 is not saved since always 0.
 * - $26, $27 are not saved since not used by the task (they are system
 * registers).
 *
 * 0*4(ctx) SR    8*4(ctx) $8    16*4(ctx) $16   24*4(ctx) $24      32*4(ctx) EPC
 * 1*4(ctx) $1    9*4(ctx) $9    17*4(ctx) $17   25*4(ctx) $25      33*4(ctx) CR
 * 2*4(ctx) $2   10*4(ctx) $10   18*4(ctx) $18   26*4(ctx) reserved 34*4(ctx) tty_id + 0x80000000
 * 3*4(ctx) $3   11*4(ctx) $11   19*4(ctx) $19   27*4(ctx) reserved 35*4(ctx) reserved
 * 4*4(ctx) $4   12*4(ctx) $12   20*4(ctx) $20   28*4(ctx) $28      36*4(ctx) reserved
 * 5*4(ctx) $5   13*4(ctx) $13   21*4(ctx) $21   29*4(ctx) $29      37*4(ctx) reserved
 * 6*4(ctx) $6   14*4(ctx) $14   22*4(ctx) $22   30*4(ctx) $30      38*4(ctx) reserved
 * 7*4(ctx) $7   15*4(ctx) $15   23*4(ctx) $23   31*4(ctx) $31      39*4(ctx) reserved
 *
 * The return address contained in $31 is saved in the _current task context
 * (in the ctx[31] slot), and the function actually returns to the address
 * contained in the ctx[31] slot of the new task context.
 *
 * This function receives two arguments representing addresses of task
 * contexts, respectively for the current running task to be descheduled and
 * for the next task to be scheduled.
 */

    .globl  _task_switch
    .func   _task_switch
    .type   _task_switch, %function

_task_switch:

    /* save _current task context */

    add     $27,    $4,     $0  /* $27 <= @_task_context_array[current_task_index] */

    mfc0    $26,    $12         /* $26 <= SR */
    sw      $26,    0*4($27)    /* ctx[0] <= SR */
    .set noat
    sw      $1,     1*4($27)    /* ctx[1] <= $1 */
    .set at
    sw      $2,     2*4($27)    /* ctx[2] <= $2 */
    sw      $3,     3*4($27)    /* ctx[3] <= $3 */
    sw      $4,     4*4($27)    /* ctx[4] <= $4 */
    sw      $5,     5*4($27)    /* ctx[5] <= $5 */
    sw      $6,     6*4($27)    /* ctx[6] <= $6 */
    sw      $7,     7*4($27)    /* ctx[7] <= $7 */
    sw      $8,     8*4($27)    /* ctx[8] <= $8 */
    sw      $9,     9*4($27)    /* ctx[9] <= $9 */
    sw      $10,    10*4($27)   /* ctx[10] <= $10 */
    sw      $11,    11*4($27)   /* ctx[11] <= $11 */
    sw      $12,    12*4($27)   /* ctx[12] <= $12 */
    sw      $13,    13*4($27)   /* ctx[13] <= $13 */
    sw      $14,    14*4($27)   /* ctx[14] <= $14 */
    sw      $15,    15*4($27)   /* ctx[15] <= $15 */
    sw      $16,    16*4($27)   /* ctx[16] <= $16 */
    sw      $17,    17*4($27)   /* ctx[17] <= $17 */
    sw      $18,    18*4($27)   /* ctx[18] <= $18 */
    sw      $19,    19*4($27)   /* ctx[19] <= $19 */
    sw      $20,    20*4($27)   /* ctx[20] <= $20 */
    sw      $21,    21*4($27)   /* ctx[21] <= $21 */
    sw      $22,    22*4($27)   /* ctx[22] <= $22 */
    sw      $23,    23*4($27)   /* ctx[23] <= $23 */
    sw      $24,    24*4($27)   /* ctx[24] <= $24 */
    sw      $25,    25*4($27)   /* ctx[25] <= $25 */
    sw      $28,    28*4($27)   /* ctx[28] <= $28 */
    sw      $29,    29*4($27)   /* ctx[29] <= $29 */
    sw      $30,    30*4($27)   /* ctx[30] <= $30 */
    sw      $31,    31*4($27)   /* ctx[31] <= $31 */
    mfc0    $26,    $14
    sw      $26,    32*4($27)   /* ctx[32] <= EPC */
    mfc0    $26,    $13
    sw      $26,    33*4($27)   /* ctx[33] <= CR */

    /* restore next task context */

    add     $27,    $5,     $0  /* $27<= @_task_context_array[next_task_index] */

    lw      $26,    0*4($27)
    mtc0    $26,    $12         /* restore SR */
    .set noat
    lw      $1,     1*4($27)    /* restore $1 */
    .set at
    lw      $2,     2*4($27)    /* restore $2 */
    lw      $3,     3*4($27)    /* restore $3 */
    lw      $4,     4*4($27)    /* restore $4 */
    lw      $5,     5*4($27)    /* restore $5 */
    lw      $6,     6*4($27)    /* restore $6 */
    lw      $7,     7*4($27)    /* restore $7 */
    lw      $8,     8*4($27)    /* restore $8 */
    lw      $9,     9*4($27)    /* restore $9 */
    lw      $10,    10*4($27)   /* restore $10 */
    lw      $11,    11*4($27)   /* restore $11 */
    lw      $12,    12*4($27)   /* restore $12 */
    lw      $13,    13*4($27)   /* restore $13 */
    lw      $14,    14*4($27)   /* restore $14 */
    lw      $15,    15*4($27)   /* restore $15 */
    lw      $16,    16*4($27)   /* restore $16 */
    lw      $17,    17*4($27)   /* restore $17 */
    lw      $18,    18*4($27)   /* restore $18 */
    lw      $19,    19*4($27)   /* restore $19 */
    lw      $20,    20*4($27)   /* restore $20 */
    lw      $21,    21*4($27)   /* restore $21 */
    lw      $22,    22*4($27)   /* restore $22 */
    lw      $23,    23*4($27)   /* restore $23 */
    lw      $24,    24*4($27)   /* restore $24 */
    lw      $25,    25*4($27)   /* restore $25 */
    lw      $28,    28*4($27)   /* restore $28 */
    lw      $29,    29*4($27)   /* restore $29 */
    lw      $30,    30*4($27)   /* restore $30 */
    lw      $31,    31*4($27)   /* restore $31 */
    lw      $26,    32*4($27)
    mtc0    $26,    $14         /* restore EPC */
    lw      $26,    33*4($27)
    mtc0    $26,    $13         /* restore CR */

    jr      $31                 /* returns to caller */

    .endfunc
    .size _task_switch, .-_task_switch

