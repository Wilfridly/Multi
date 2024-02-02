#include <common.h>
#include <drivers.h>

/*
 * _putk()
 *
 * Print a message with _tty_write after calculating its length.
 */
unsigned int _putk(const char *msg)
{
    unsigned int len = 0;
    const char *tmp = msg;

    while (*tmp++)
        len++;

    return _tty_write(msg, len);
}

/*
 * _exit()
 *
 * Exit (suicide) after printing a death message on a terminal.
 */
void _exit()
{
    char buf[40] = "\n\n!!! Exit Processor 0x___ !!!\n";

    unsigned int proc_id = _procid();

    /* proc_id can be up to 0x3FF so display three digits */
    buf[23] = (char)((proc_id >> 8) & 0xF) + 0x30;
    buf[24] = (char)((proc_id >> 4) & 0xF) + 0x30;
    buf[25] = (char)((proc_id >> 0) & 0xF) + 0x30;

    _putk(buf);

    /* infinite loop */
    while (1)
        asm volatile("nop");
}

/*
 * _dcache_buf_invalidate()
 *
 * Invalidate all data cache lines corresponding to a memory buffer (identified
 * by an address and a size).
 */
void _dcache_buf_invalidate(const void *buffer, unsigned int size)
{
    unsigned int i;
    unsigned int tmp;
    unsigned int line_size;

    /*
     * compute data cache line size based on config register (bits 12:10)
     */
    asm volatile("mfc0 %0, $16, 1" : "=r"(tmp));
    tmp = ((tmp>>10) & 0x7);
    line_size = 2 << tmp;

    /* iterate on cache lines to invalidate each one of them */
    for (i = 0; i < size; i += line_size)
    {
        asm volatile(
                " cache %0, %1"
                ::"i" (0x11), "R" (*((unsigned char*)buffer+i))
                );
    }
}

/*
 * _itoa_dec()
 *
 * Convert a 32-bit unsigned integer to a string of ten decimal characters.
 */
void _itoa_dec(unsigned int val, char *buf)
{
    const static char dectab[] = "0123456789";
    unsigned int i;

    for (i = 0; i < 10; i++)
    {
        if ((val != 0) || (i == 0))
            buf[9-i] = dectab[val % 10];
        else
            buf[9-i] = 0x20;
        val /= 10;
    }
}

/*
 * _itoa_hex()
 *
 * Convert a 32-bit unsigned integer to a string of height hexadecimal
 * characters.
 */
void _itoa_hex(unsigned int val, char *buf)
{
    const static char hexatab[] = "0123456789ABCD";
    unsigned int i;

    for (i = 0; i < 8; i++)
    {
        buf[7-i] = hexatab[val % 16];
        val /= 16;
    }
}

/*
 * Barrier related uncachable variables
 */

#define in_unckdata __attribute__((section (".unckdata")))

#define MAX_BARRIER_COUNT 8

in_unckdata unsigned int volatile _barrier_initial_value[MAX_BARRIER_COUNT] = {
    [0 ... MAX_BARRIER_COUNT-1] = 0
};
in_unckdata unsigned int volatile _barrier_count[MAX_BARRIER_COUNT] = {
    [0 ... MAX_BARRIER_COUNT-1] = 0
};

/*
 * _barrier_init()
 *
 * This function makes a cooperative initialisation of the barrier: several
 * tasks can try to initialize the barrier, but the initialisation is done by
 * only one task, using LL/SC instructions.
 */

unsigned int _barrier_init(unsigned int index, unsigned int value)
{
    /* check the index */
    if (index >= MAX_BARRIER_COUNT)
        return 1;

    unsigned int* pinit = (unsigned int*)&_barrier_initial_value[index];
    unsigned int* pcount = (unsigned int*)&_barrier_count[index];

    /* parallel initialisation using atomic instructions LL/SC */
    asm volatile ("_barrier_init_test:                  \n"
                  "ll   $2,     0(%0)                   \n" /* read initial value */
                  "bnez $2,     _barrier_init_done      \n"
                  "move $3,     %2                      \n"
                  "sc   $3,     0(%0)                   \n" /* try to write initial value */
                  "beqz $3,     _barrier_init_test      \n"
                  "move $3,     %2                      \n"
                  "sw   $3,     0(%1)                   \n" /* write count */
                  "_barrier_init_done:                  \n"
                  :: "r"(pinit), "r"(pcount), "r"(value)
                  : "$2", "$3");
    return 0 ;
}

/*
 * _barrier_wait()
 *
 * This blocking function decrements a barrier's counter and then uses a
 * busy_wait mechanism for synchronization, because the GIET does not support
 * dynamic scheduling/descheduling of tasks.
 *
 * There is at most MAX_BARRIER_COUNT independant barriers, and an error is
 * returned if the barrier index is larger than MAX_BARRIER_COUNT.
 */
unsigned int _barrier_wait(unsigned int index)
{
    if (index >= MAX_BARRIER_COUNT)
        return 1;

    unsigned int *pcount = (unsigned int*)&_barrier_count[index];
    unsigned int maxcount = _barrier_initial_value[index];
    unsigned int count;

    /* parallel decrement barrier counter using atomic instructions LL/SC
     * - input : pointer on the barrier counter
     * - output : counter value
     */
    asm volatile ("_barrier_decrement:          \n"
                  "ll   %0, 0(%1)               \n"
                  "addi $3, %0,     -1          \n"
                  "sc   $3, 0(%1)               \n"
                  "beqz $3, _barrier_decrement  \n"
                  : "=&r"(count)
                  : "r"(pcount)
                  : "$2", "$3");

    /* the last task re-initializes the barrier counter to the max value,
     * waking up all other waiting tasks
     */

    if (count == 1)
        /* last task */
        *pcount = maxcount;
    else
        /* other tasks busy-wait for the re-initialization */
        while (*pcount != maxcount);

    return 0;
}
