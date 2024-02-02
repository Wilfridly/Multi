/*
 * Commonly used functions
 */

#ifndef _COMMON_H
#define _COMMON_H

/*
 * Prototypes of common functions
 */
unsigned int _putk(const char *msg);
void _exit() __attribute__((noreturn));
void _dcache_buf_invalidate(const void *buffer, unsigned int size);

void _itoa_dec(unsigned int val, char* buf);
void _itoa_hex(unsigned int val, char* buf);

unsigned int _barrier_init(unsigned int index, unsigned int count);
unsigned int _barrier_wait(unsigned int index);

/*
 * memcpy function
 *
 * This function is likely not to be called directly but GCC can automatically
 * issue call to it during compilation so we must provide it. 'static inline'
 * so the function's code is directly included when used.
 *
 * Code taken from MutekH.
 */
static inline void *memcpy(void *_dst, const void *_src, unsigned int size)
{
    unsigned int *dst = _dst;
    const unsigned int *src = _src;

    /* if source and destination buffer are word-aligned,
     * then copy word-by-word */
    if (!((unsigned int)dst & 3) && !((unsigned int)src & 3))
        while (size > 3) {
            *dst++ = *src++;
            size -= 4;
        }

    unsigned char *cdst = (unsigned char*)dst;
    unsigned char *csrc = (unsigned char*)src;

    /* byte-by-byte copy */
    while (size--) {
        *cdst++ = *csrc++;
    }
    return _dst;
}

/*
 * ---
 * MIPS32 related helpers
 * ---
 */

/*
 * _get_epc()
 *
 * Access CP0 and returns EPC register.
 */
static inline unsigned int _get_epc()
{
    unsigned int ret;
    asm volatile("mfc0 %0, $14" : "=r"(ret));
    return ret;
}

/*
 * _get_bar()
 *
 * Access CP0 and returns BAR register.
 */
static inline unsigned int _get_bar()
{
    unsigned int ret;
    asm volatile("mfc0 %0, $8" : "=r"(ret));
    return ret;
}

/*
 * _get_cause()
 *
 * Access CP0 and returns CAUSE register.
 */
static inline unsigned int _get_cause()
{
    unsigned int ret;
    asm volatile("mfc0 %0, $13" : "=r"(ret));
    return ret;
}

#if 0
/*
 *  _it_mask()
 * Access CP0 and mask IRQs
 */

static inline void _it_mask()
{
    asm volatile(
            "mfc0  $2, $12     \n"
            "ori   $2, $2, 1   \n"
            "mtc0  $2, $12     \n"
            ::: "$2"
            );
}

/*
 *  _it_enable()
 * Access CP0 and enable IRQs
 */
static inline void _it_enable()
{
    asm volatile(
            "mfc0  $2, $12     \n"
            "addi  $2, $2, -1  \n"
            "mtc0  $2, $12     \n"
            ::: "$2"
            );
}
#endif

#endif
