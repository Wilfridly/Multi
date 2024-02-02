#include <stdarg.h>
#include <stdio.h>

#define SYSCALL_PROCID          0x00
#define SYSCALL_PROCTIME        0x01
#define SYSCALL_TTY_WRITE       0x02
#define SYSCALL_TTY_READ        0x03
#define SYSCALL_TIMER_WRITE     0x04
#define SYSCALL_TIMER_READ      0x05
#define SYSCALL_GCD_WRITE       0x06
#define SYSCALL_GCD_READ        0x07
#define SYSCALL_TTY_READ_IRQ    0x0A
#define SYSCALL_TTY_WRITE_IRQ   0x0B
#define SYSCALL_CTX_SWITCH      0x0D
#define SYSCALL_EXIT            0x0E
#define SYSCALL_PROCNUMBER      0x0F
#define SYSCALL_FB_SYNC_WRITE   0x10
#define SYSCALL_FB_SYNC_READ    0x11
#define SYSCALL_FB_WRITE        0x12
#define SYSCALL_FB_READ         0x13
#define SYSCALL_FB_COMPLETED    0x14
#define SYSCALL_IOC_WRITE       0x15
#define SYSCALL_IOC_READ        0x16
#define SYSCALL_IOC_COMPLETED   0x17
#define SYSCALL_BARRIER_INIT    0x18
#define SYSCALL_BARRIER_WAIT    0x19

/*
 * sys_call()
 *
 * This generic C function is used to implement all system calls.
 */
static inline unsigned int sys_call(unsigned int call_no,
        unsigned int arg_0, unsigned int arg_1, unsigned int arg_2, unsigned int arg_3)
{
    register unsigned int reg_no_and_output asm("v0") = call_no;
    register unsigned int reg_a0 asm("a0") = arg_0;
    register unsigned int reg_a1 asm("a1") = arg_1;
    register unsigned int reg_a2 asm("a2") = arg_2;
    register unsigned int reg_a3 asm("a3") = arg_3;

    asm volatile(
            "syscall"
            : "=r" (reg_no_and_output)  /* output argument */
            : "r" (reg_a0),             /* input arguments */
            "r" (reg_a1),
            "r" (reg_a2),
            "r" (reg_a3),
            "r" (reg_no_and_output)
            : "memory",
            /* These persistant registers will be saved on the stack by the
             * compiler only if they contain relevant data. */
            "at",
            "v1",
            "ra",
            "t0",
            "t1",
            "t2",
            "t3",
            "t4",
            "t5",
            "t6",
            "t7",
            "t8",
            "t9"
               );
    return reg_no_and_output;
}

/*
 * *******************************
 * MIPS32 related system calls
 * *******************************
 */

/*
 * procid()
 *
 * This function returns the processor identifier.
 */
unsigned int procid()
{
    return sys_call(SYSCALL_PROCID, 0, 0, 0, 0);
}

/*
 * proctime()
 *
 * This function returns the local processor time (elapsed clock cycles since
 * bootup).
 */
unsigned int proctime()
{
    return sys_call(SYSCALL_PROCTIME, 0, 0, 0, 0);
}

/*
 * procnumber()
 *
 * This function returns the number of processors controlled by the system.
 */
unsigned int procnumber()
{
    return sys_call(SYSCALL_PROCNUMBER, 0, 0, 0, 0);
}

/*
 * *******************************
 * TTY device related system calls
 * *******************************
 */

/*
 * tty_putc()
 *
 * This function displays a single ascii character on a terminal.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by the task ID in case of multi-tasking).
 * - It doesn't use the TTY_PUT_IRQ interrupt, and the associated kernel
 *   buffer.
 * - Returns 1 if the character has been written, 0 otherwise.
 */
unsigned int tty_putc(char byte)
{
    return sys_call(SYSCALL_TTY_WRITE,
            (unsigned int)(&byte),
            1,
            0,0);
}

/*
 * tty_puts()
 *
 * This function displays a string on a terminal.
 * - The terminal index is implicitely defined by the processor identifer (and
 *   by the task ID in case of multi-tasking).
 * - The string must be terminated by a NUL character.
 * - It doesn't use the TTY_PUT_IRQ interrupt, and the associated kernel
 *   buffer.
 * - Returns the number of written characters.
 */
unsigned int tty_puts(char *buf)
{
    unsigned int length = 0;
    while (buf[length] != 0)
    {
        length++;
    }
    return sys_call(SYSCALL_TTY_WRITE,
            (unsigned int)buf,
            length,
            0,0);
}

/*
 * tty_putw()
 *
 * This function displays the value of a 32-bit word with decimal characters.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by pthe task ID in case of multi-tasking).
 * - It doesn't use the TTY_PUT_IRQ interrupt, and the associated kernel
 *   buffer.
 * - Returns the number of written characters (should be equal to ten).
 */
unsigned int tty_putw(unsigned int val)
{
    char buf[10];
    unsigned int i;
    for (i = 0; i < 10; i++)
    {
        buf[9-i] = (val % 10) + 0x30;
        val = val / 10;
    }
    return sys_call(SYSCALL_TTY_WRITE,
            (unsigned int)buf,
            10,
            0,0);
}

/*
 * tty_getc()
 *
 * This blocking function fetches a single ascii character from a terminal.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by the task ID in case of multi-tasking)
 * - It doesn't use the IRQ_GET interrupt, and the associated kernel buffer.
 * - Returns necessarily 0 when completed.
 */
unsigned int tty_getc(char *byte)
{
    unsigned int ret = 0;
    while (ret == 0)
    {
        ret = sys_call(SYSCALL_TTY_READ,
                (unsigned int)byte,
                1,
                0,0);
    }
    return 0;
}

/*
 * tty_getc_irq()
 *
 * This blocking function fetches a single ascii character from a terminal.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by the task ID in case of multi-tasking).
 * - It uses the IRQ_GET interrupt, and the associated kernel buffer.
 * - Returns necessarily 0 when completed.
 */
unsigned int tty_getc_irq(char *byte)
{
    unsigned int ret = 0;
    while (ret == 0)
    {
        ret = sys_call(SYSCALL_TTY_READ_IRQ,
                (unsigned int)byte,
                1,
                0,0);
    }
    return 0;
}

/*
 * tty_gets_irq()
 *
 * This blocking function fetches a string from a terminal to a bounded length
 * buffer.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by the task ID in case of multi-tasking)
 * - It uses the TTY_GET_IRQ interrupt, anf the associated kernel buffer.
 * - Returns necessarily 0 when completed.
 *
 * - Up to (bufsize - 1) characters (including the non printable characters)
 *   will be copied into buffer, and the string is always completed by a NUL
 *   character.
 * - The <LF> character is interpreted, as the function close the string with a
 *   NUL character if <LF> is read.
 * - The <DEL> character is interpreted, and the corresponding character(s) are
 *   removed from the target buffer.
 */
unsigned int tty_gets_irq(char *buf, unsigned int bufsize)
{
    unsigned int ret;
    unsigned char byte;
    unsigned int index = 0;

    while (index < (bufsize - 1))
    {
        do {
            ret = sys_call(SYSCALL_TTY_READ_IRQ,
                    (unsigned int)(&byte),
                    1,
                    0,0);
        } while (ret != 1);

        if ( byte == 0x0A )
            break; /* LF */
        else if ((byte == 0x7F) && (index > 0))
            index--; /* DEL */
        else
        {
            buf[index] = byte;
            index++;
        }
    }
    buf[index] = 0;
    return 0;
}

/*
 * tty_getw_irq()
 *
 * This blocking function fetches a string of decimal characters (most
 * significant digit first) to build a 32-bit unsigned integer.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by the task ID in case of multi-tasking).
 * - It uses the TTY_GET_IRQ interrupt, anf the associated kernel buffer.
 * - Returns necessarily 0 when completed.
 *
 * - The non-blocking system function _tty_read_irq is called several times,
 *   and the decimal characters are written in a 32 characters buffer until a
 *   <LF> character is read.
 * - The <DEL> character is interpreted, and previous characters can be
 *   cancelled. All others characters are ignored.
 * - When the <LF> character is received, the string is converted to an
 *   unsigned int value. If the number of decimal digit is too large for the 32
 *   bits range, the zero value is returned.
 */
unsigned int tty_getw_irq(unsigned int *val)
{
    unsigned char buf[32];
    unsigned char byte;
    unsigned int save = 0;
    unsigned int dec = 0;
    unsigned int done = 0;
    unsigned int overflow = 0;
    unsigned int max = 0;
    unsigned int i;
    unsigned int ret;

    while (done == 0)
    {
        do {
            ret = sys_call(SYSCALL_TTY_READ_IRQ,
                    (unsigned int)(&byte),
                    1,
                    0,0);
        } while (ret != 1);

        if ((byte > 0x2F) && (byte < 0x3A)) /* decimal character */
        {
            buf[max] = byte;
            max++;
            tty_putc(byte);
        }
        else if ((byte == 0x0A) || (byte == 0x0D)) /* LF or CR character */
        {
            done = 1;
        }
        else if (byte == 0x7F) /* DEL character */
        {
            if (max > 0)
            {
                max--; /* cancel the character */
                tty_putc(0x08);
                tty_putc(0x20);
                tty_putc(0x08);
            }
        }
        if (max == 32) /* decimal string overflow */
        {
            for (i = 0; i < max; i++) /* cancel the string */
            {
                tty_putc(0x08);
                tty_putc(0x20);
                tty_putc(0x08);
            }
            tty_putc(0x30);
            *val = 0; /* return 0 value */
            return 0;
        }
    }

    /* string conversion */
    for (i = 0; i < max; i++)
    {
        dec = dec * 10 + (buf[i] - 0x30);
        if (dec < save)
            overflow = 1;
        save = dec;
    }

    /* check overflow */
    if (overflow == 0)
    {
        *val = dec; /* return decimal value */
    }
    else
    {
        for (i = 0; i < max; i++) /* cancel the string */
        {
            tty_putc(0x08);
            tty_putc(0x20);
            tty_putc(0x08);
        }
        tty_putc(0x30);
        *val = 0; /* return 0 value */
    }
    return 0;
}

/*
 * tty_printf()
 *
 * This function is a simplified version of the mutek_printf() function.
 * - The terminal index is implicitely defined by the processor identifier (and
 *   by the task ID in case of multi-tasking).
 * - It doesn't use the IRQ_PUT interrupt, anf the associated kernel buffer.
 * - Only a limited number of formats are supported:
 *  - %d : signed decimal
 *  - %u : unsigned decimal
 *  - %x : hexadecimal
 *  - %c : char
 *  - %s : string
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int tty_printf(char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    unsigned int ret;

printf_text:

    while (*format) {
        unsigned int i;
        for (i = 0; format[i] && format[i] != '%'; i++)
            ;
        if (i) {
            ret = sys_call(SYSCALL_TTY_WRITE,
                    (unsigned int)format,
                    i,
                    0,0);
            if (ret != i)
                return 1; /* return error */
            format += i;
        }
        if (*format == '%') {
            format++;
            goto printf_arguments;
        }
    }

    va_end(ap);
    return 0;

printf_arguments:

    {
        int         val = va_arg(ap, long);
        char            buf[20];
        char*           pbuf;
        unsigned int        len = 0;
        static const char   HexaTab[] = "0123456789ABCDEF";
        unsigned int        i;

        switch (*format++) {
            case ('c'):             /* char conversion */
                len = 1;
                buf[0] = val;
                pbuf = buf;
                break;
            case ('d'):             /* decimal signed integer */
                if (val < 0) {
                    val = -val;
                    ret = sys_call(SYSCALL_TTY_WRITE,
                            (unsigned int)"-",
                            1,
                            0,0);
                    if (ret != 1)
                        return 1; /* return error */
                }
            case ('u'):             /* decimal unsigned integer */
                for( i=0 ; i<10 ; i++) {
                    buf[9-i] = HexaTab[val % 10];
                    if (!(val /= 10)) break;
                }
                len =  i+1;
                pbuf = &buf[9-i];
                break;
            case ('x'):             /* hexadecimal integer */
                ret = sys_call(SYSCALL_TTY_WRITE,
                        (unsigned int)"0x",
                        2,
                        0,0);
                if (ret != 2)
                    return 1; /* return error */
                for( i=0 ; i<8 ; i++) {
                    buf[7-i] = HexaTab[val % 16U];
                    if (!(val /= 16U)) break;
                }
                len =  i+1;
                pbuf = &buf[7-i];
                break;
            case ('s'):             /* string */
                {
                    char *str = (char*)val;
                    while ( str[len] ) len++;
                    pbuf = (char*)val;
                }
                break;
            default:
                goto printf_text;
        }

        ret = sys_call(SYSCALL_TTY_WRITE,
                (unsigned int)pbuf,
                len,
                0,0);
        if (ret != len)
            return 1;
        goto printf_text;
    }
}

/*
 * ************************************
 * Timer device related system calls
 * ************************************
 */

#define TIMER_VALUE     0
#define TIMER_MODE      1
#define TIMER_PERIOD    2
#define TIMER_RESETIRQ  3

/*
 * timer_set_mode()
 *
 * This function defines the operation mode of a timer. The possible values for
 * this mode are:
 * - 0x0 : Timer not activated
 * - 0x1 : Timer activated, but no interrupt is generated
 * - 0x3 : Timer activarted and periodic interrupts generated
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int timer_set_mode(unsigned int val)
{
    return sys_call(SYSCALL_TIMER_WRITE,
            TIMER_MODE,
            val,
            0, 0);
}

/*
 * timer_set_period()
 *
 * This function defines the period value of a timer to enable a periodic
 * interrupt.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int timer_set_period(unsigned int val)
{
    return sys_call(SYSCALL_TIMER_WRITE,
            TIMER_PERIOD,
            val,
            0, 0);
}

/*
 * timer_reset_irq()
 *
 * This function resets the interrupt signal issued by a timer.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int timer_reset_irq()
{
    return sys_call(SYSCALL_TIMER_WRITE,
            TIMER_RESETIRQ,
            0, 0, 0);
}

/*
 * timer_get_time()
 *
 * This function returns the current timing value of a timer.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int timer_get_time(unsigned int *time)
{
    return sys_call(SYSCALL_TIMER_READ,
            TIMER_VALUE,
            (unsigned int)time,
            0, 0);
}

/*
 * ***********************************************************
 * GCD (Greatest Common Divisor) device related system calls
 * ***********************************************************
 */

#define GCD_OPA     0
#define GCD_OPB     1
#define GCD_START   2
#define GCD_STATUS  3

/*
 * gcd_set_opa()
 *
 * This function sets the operand A in the GCD coprocessor.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int gcd_set_opa(unsigned int val)
{
    return sys_call(SYSCALL_GCD_WRITE,
            GCD_OPA,
            val,
            0, 0);
}

/*
 * gcd_set_opb()
 *
 * This function sets operand B in the GCD coprocessor.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int gcd_set_opb(unsigned int val)
{
    return sys_call(SYSCALL_GCD_WRITE,
            GCD_OPB,
            val,
            0, 0);
}

/*
 * gcd_start()
 *
 * This function starts the computation in the GCD coprocessor.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int gcd_start()
{
    return sys_call(SYSCALL_GCD_WRITE,
            GCD_START,
            0, 0, 0);
}

/*
 * gcd_get_status()
 *
 * This function gets the status fromn the GCD coprocessor.
 * - The value is equal to 0 when the coprocessor is idle (computation
 *   completed).
 */
unsigned int gcd_get_status(unsigned int *val)
{
    return sys_call(SYSCALL_GCD_READ,
            GCD_STATUS,
            (unsigned int)val,
            0, 0);
}

/*
 * gcd_get_result()
 *
 * This function gets the result of the computation from the GCD coprocessor.
 */
unsigned int gcd_get_result(unsigned int *val)
{
    return sys_call(SYSCALL_GCD_READ,
            GCD_OPA,
            (unsigned int)val,
            0, 0);
}

/*
 * ***********************************
 * Block device related system calls
 * ***********************************
 */

/*
 * ioc_write()
 *
 * Transfer data from a memory buffer to a file on the block_device.
 * - lba    : Logical Block Address (first block index)
 * - buffer : base address of the memory buffer
 * - count  : number of blocks to be transfered
 *
 * - Returns 0 if success, > 0 if error (e.g. memory buffer not in user space).
 */
unsigned int ioc_write(unsigned int lba, void *buffer, unsigned int count)
{
    return sys_call(SYSCALL_IOC_WRITE,
            lba,
            (unsigned int)buffer,
            count,
            0);
}

/*
 * ioc_read()
 *
 * Transfer data from a file on the block_device to a memory buffer.
 * - lba    : Logical Block Address (first block index)
 * - buffer : base address of the memory buffer
 * - count  : number of blocks to be transfered
 *
 * - Returns 0 if success, > 0 if error (e.g. memory buffer not in user space).
 */
unsigned int ioc_read(unsigned int lba, void *buffer, unsigned int count)
{
    return sys_call(SYSCALL_IOC_READ,
            lba,
            (unsigned int)buffer,
            count,
            0);
}

/*
 * ioc_completed()
 *
 * This blocking function returns 0 when the I/O transfer is
 * successfully completed, and returns 1 if an address error
 * has been detected.
 */
unsigned int ioc_completed()
{
    return sys_call(SYSCALL_IOC_COMPLETED,
            0, 0, 0, 0);
}

/*
 * *******************************************
 * Frame buffer device related system calls
 * *******************************************
 */

/*
 * fb_sync_write()
 *
 * This blocking function use a memory copy strategy to transfer data from a
 * user buffer to the frame buffer device in kernel space.
 * - offset : offset (in bytes) in the frame buffer
 * - buffer : base address of the memory buffer
 * - length : number of bytes to be transfered
 *
 * - Returns 0 if success, > 0 if error (e.g. memory buffer not in user space).
 */
unsigned int fb_sync_write(unsigned int offset, void *buffer, unsigned int length)
{
    return sys_call(SYSCALL_FB_SYNC_WRITE,
            offset,
            (unsigned int)buffer,
            length,
            0);
}

/*
 * fb_sync_read()
 *
 * This blocking function use a memory copy strategy to transfer data from the
 * frame buffer device in kernel space to an user buffer.
 * - offset : offset (in bytes) in the frame buffer
 * - buffer : base address of the user buffer
 * - length : number of bytes to be transfered
 *
 * - Returns 0 if success, > 0 if error (e.g. memory buffer not in user space).
 */
unsigned int fb_sync_read(unsigned int offset, void *buffer, unsigned int length)
{
    return sys_call(SYSCALL_FB_SYNC_READ,
            offset,
            (unsigned int)buffer,
            length,
            0);
}

/*
 * fb_write()
 *
 * This non-blocking function use the DMA coprocessor to transfer data from a
 * user buffer to the frame buffer device in kernel space.
 * - offset : offset (in bytes) in the frame buffer
 * - buffer : base address of the user buffer
 * - length : number of bytes to be transfered
 *
 * - Returns 0 if success, > 0 if error (e.g. memory buffer not in user space).
 *
 * The transfer completion is signaled by an IRQ, and must be tested by the
 * fb_completed() function.
 */
unsigned int fb_write(unsigned int offset, void *buffer, unsigned int length)
{
    return sys_call(SYSCALL_FB_WRITE,
            offset,
            (unsigned int)buffer,
            length,
            0);
}

/*
 * fb_read()
 *
 * This non-blocking function use the DMA coprocessor to transfer data from the
 * frame buffer device in kernel space to an user buffer.
 * - offset : offset (in bytes) in the frame buffer
 * - buffer : base address of the memory buffer
 * - length : number of bytes to be transfered
 *
 * - Returns 0 if success, > 0 if error (e.g. memory buffer not in user space).
 *
 * The transfer completion is signaled by an IRQ, and must be tested by the
 * fb_completed() function.
 */
unsigned int fb_read(unsigned int offset, void *buffer, unsigned int length)
{
    return sys_call(SYSCALL_FB_READ,
            offset,
            (unsigned int)buffer,
            length,
            0);
}

/*
 * fb_completed()
 *
 * This blocking function returns when the transfer is completed.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int fb_completed()
{
    return sys_call(SYSCALL_FB_COMPLETED,
            0, 0, 0, 0);
}

/*
 * ***************************************
 * Software barrier related system calls
 * ***************************************
 */

/*
 * barrier_init()
 *
 * This function initializes the counter for barrier[index].
 * - index  : index of the barrier (between 0 & 7)
 * - count  : number of tasks to be synchronized.
 * The GIET supports up to 8 independant barriers.
 *
 * - Returns 0 if success, > 0 if error (e.g. index >= 8).
 */
unsigned int barrier_init(unsigned int index, unsigned int count)
{
    return sys_call(SYSCALL_BARRIER_INIT,
            index,
            count,
            0, 0);
}

/*
 * barrier_wait()
 *
 * This blocking function use a busy waiting policy, and returns only when all
 * synchonized asks have reached the barrier.
 * - index  : index of the barrier (between 0 & 7)
 * The GIET supports up to 8 independant barriers.
 *
 * - Returns 0 if success, > 0 if error (e.g. index >= 8).
 */
unsigned int barrier_wait(unsigned int index)
{
    return sys_call(SYSCALL_BARRIER_WAIT,
            index,
            0, 0, 0);
}

/*
 * ****************************
 * Miscellaneous system calls
 * ****************************
 */

/*
 * exit()
 *
 * This function exits the program with a TTY message, 
 * and enter an infinite loop.
 * The task is blocked until the next RESET,
 * but it still consume processor cycles ...
 */
void exit()
{
    unsigned int proc_index = procid();
    sys_call(SYSCALL_EXIT, proc_index, 0, 0, 0);
}

/*
 * rand()
 *
 * This function returns a pseudo-random value derived from the processor cycle
 * count. This value is comprised between 0 & 65535.
 */
unsigned int rand()
{
    unsigned int x = sys_call(SYSCALL_PROCTIME, 0, 0, 0, 0);
    if((x & 0xF) > 7)
        return (x*x & 0xFFFF);
    else
        return (x*x*x & 0xFFFF);
}

/*
 * ctx_switch()
 *
 * The user task calling this function is descheduled and
 * the processor is allocated to another task.
*/
unsigned int ctx_switch()
{
    return sys_call(SYSCALL_CTX_SWITCH, 0, 0, 0, 0);
}

