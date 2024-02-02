#ifndef _STDIO_H
#define _STDIO_H

/*
 * These functions implements a minimal C library
 */

/* MIPS32 related functions */
unsigned int procid();
unsigned int proctime();
unsigned int procnumber();

/* TTY device related functions */
unsigned int tty_putc(char byte);
unsigned int tty_puts(char *buf);
unsigned int tty_putw(unsigned int val);
unsigned int tty_getc(char *byte);
unsigned int tty_getc_irq(char *byte);
unsigned int tty_gets_irq(char *buf, unsigned int bufsize);
unsigned int tty_getw_irq(unsigned int *val);
unsigned int tty_printf(char *format,...);

/* Timer device related functions */
unsigned int timer_set_mode(unsigned int mode);
unsigned int timer_set_period(unsigned int period);
unsigned int timer_reset_irq();
unsigned int timer_get_time(unsigned int *time);

/* GCD coprocessor related functions */
unsigned int gcd_set_opa(unsigned int val);
unsigned int gcd_set_opb(unsigned int val);
unsigned int gcd_start();
unsigned int gcd_get_result(unsigned int *val);
unsigned int gcd_get_status(unsigned int *val);

/* Block device related functions */
unsigned int ioc_read(unsigned int lba, void *buffer, unsigned int count);
unsigned int ioc_write(unsigned int lba, void *buffer, unsigned int count);
unsigned int ioc_completed();

/* Frame buffer device related functions */
unsigned int fb_sync_read(unsigned int offset, void *buffer, unsigned int length);
unsigned int fb_sync_write(unsigned int offset, void *buffer, unsigned int length);
unsigned int fb_read(unsigned int offset, void *buffer, unsigned int length);
unsigned int fb_write(unsigned int offset, void *buffer, unsigned int length);
unsigned int fb_completed();

/* Software barrier related functions */
unsigned int barrier_init(unsigned int index, unsigned int count);
unsigned int barrier_wait(unsigned int index);

/* Misc */
void exit();
unsigned int rand();
unsigned int ctx_switch();

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

#endif

