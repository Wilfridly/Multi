#ifndef _IRQ_HANDLER_H
#define _IRQ_HANDLER_H

/*
 * Interrupt Vector Table (indexed by interrupt index)
 *
 * 32 entries corresponding to 32 ISR addresses
 */

typedef void (*_isr_func_t)(void);
extern _isr_func_t _interrupt_vector[32];

/*
 * Prototypes of the Interrupt Service Routines (ISRs) supported by the GIET.
 * - they must be installed in reset.s
 */

void _isr_default();

void _isr_dma();

void _isr_ioc();

void _isr_timer0();
void _isr_timer1();
void _isr_timer2();
void _isr_timer3();

void _isr_tty_get();
void _isr_tty_get_task0();
void _isr_tty_get_task1();
void _isr_tty_get_task2();
void _isr_tty_get_task3();

void _isr_switch();

#endif
