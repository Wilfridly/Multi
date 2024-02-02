#include <exc_handler.h>
#include <drivers.h>
#include <common.h>

/*
 * Prototypes of exception handlers.
 */

static void _cause_ukn();
static void _cause_adel();
static void _cause_ades();
static void _cause_ibe();
static void _cause_dbe();
static void _cause_bp();
static void _cause_ri();
static void _cause_cpu();
static void _cause_ovf();

extern void _int_handler();
extern void _sys_handler();


/*
 * Initialize the exception vector according to CR code
 */
const _exc_func_t _cause_vector[16] = {
    &_int_handler,  /* 0000 : external interrupt */
    &_cause_ukn,    /* 0001 : undefined exception */
    &_cause_ukn,    /* 0010 : undefined exception */
    &_cause_ukn,    /* 0011 : undefined exception */
    &_cause_adel,   /* 0100 : illegal address read exception */
    &_cause_ades,   /* 0101 : illegal address write exception */
    &_cause_ibe,    /* 0110 : instruction bus error exception */
    &_cause_dbe,    /* 0111 : data bus error exception */
    &_sys_handler,  /* 1000 : system call */
    &_cause_bp,     /* 1001 : breakpoint exception */
    &_cause_ri,     /* 1010 : illegal codop exception */
    &_cause_cpu,    /* 1011 : illegal coprocessor access */
    &_cause_ovf,    /* 1100 : arithmetic overflow exception */
    &_cause_ukn,    /* 1101 : undefined exception */
    &_cause_ukn,    /* 1110 : undefined exception */
    &_cause_ukn,    /* 1111 : undefined exception */
};

static const char *exc_message_causes[] = {
    "\n\nException : strange unknown cause\n",
    "\n\nException : illegal read address \n",
    "\n\nException : illegal write address\n",
    "\n\nException : inst bus error       \n",
    "\n\nException : data bus error       \n",
    "\n\nException : breakpoint           \n",
    "\n\nException : reserved instruction \n",
    "\n\nException : illegal coproc access\n"
    "\n\nException : arithmetic overflow  \n",
};

static void _cause(unsigned int msg_cause)
{
    char *buf = "0x00000000";

    /* print the human readable cause */
    _putk(exc_message_causes[msg_cause]);

    /* print EPC value */
    _putk("\nEPC = ");

    unsigned int epc = _get_epc();
    _itoa_hex(epc, buf + 2);
    _putk(buf);

    /* print BAR value */
    _putk("\nBAR = ");

    unsigned int bar = _get_bar();
    _itoa_hex(bar, buf + 2);
    _putk(buf);

    /* print CAUSE value */
    _putk("\nCAUSE = ");

    unsigned int cause = _get_cause();
    _itoa_hex(cause, buf + 2);
    _putk(buf);

    /* exit forever */
    _exit();
}

static void _cause_ukn()  { _cause(0); }
static void _cause_adel() { _cause(1); }
static void _cause_ades() { _cause(2); }
static void _cause_ibe()  { _cause(3); }
static void _cause_dbe()  { _cause(4); }
static void _cause_bp()   { _cause(5); }
static void _cause_ri()   { _cause(6); }
static void _cause_cpu()  { _cause(7); }
static void _cause_ovf()  { _cause(8); }

