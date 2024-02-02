#include <sys_handler.h>
#include <drivers.h>
#include <ctx_handler.h>
#include <common.h>
#include <config.h>

/*
 * Local syscall handlers prototypes
 */
static void _sys_ukn();
static unsigned int _procnumber();

/*
 * Initialize the syscall vector with syscall handlers
 */
const void *_syscall_vector[32] = {
    &_procid,           /* 0x00 */
    &_proctime,         /* 0x01 */
    &_tty_write,        /* 0x02 */
    &_tty_read,         /* 0x03 */
    &_timer_write,      /* 0x04 */
    &_timer_read,       /* 0x05 */
    &_gcd_write,        /* 0x06 */
    &_gcd_read,         /* 0x07 */
    &_sys_ukn,          /* 0x08 */
    &_sys_ukn,          /* 0x09 */
    &_tty_read_irq,     /* 0x0A */
    &_sys_ukn,          /* 0x0B */
    &_sys_ukn,          /* 0x0C */
    &_ctx_switch,       /* 0x0D */
    &_exit,             /* 0x0E */
    &_procnumber,       /* 0x0F */
    &_fb_sync_write,    /* 0x10 */
    &_fb_sync_read,     /* 0x11 */
    &_fb_write,         /* 0x12 */
    &_fb_read,          /* 0x13 */
    &_fb_completed,     /* 0x14 */
    &_ioc_write,        /* 0x15 */
    &_ioc_read,         /* 0x16 */
    &_ioc_completed,    /* 0x17 */
    &_barrier_init,     /* 0x18 */
    &_barrier_wait,     /* 0x19 */
    &_sys_ukn,          /* 0x1A */
    &_sys_ukn,          /* 0x1B */
    &_sys_ukn,          /* 0x1C */
    &_sys_ukn,          /* 0x1D */
    &_sys_ukn,          /* 0x1E */
    &_sys_ukn,          /* 0x1F */
};

static void _sys_ukn()
{
    /* print the human readable cause */
    _putk("\n\n!!! Undefined System Call !!!\n");

    /* print EPC value */
    _putk("\nEPC = ");

    char *buf = "0x00000000";
    unsigned int epc = _get_epc();
    _itoa_hex(epc, buf + 2);
    _putk(buf);

    /* exit forever */
    _exit();
}

static unsigned int _procnumber()
{
    return NB_PROCS;
}

