/*
 * Drivers for the following SoCLib hardware components:
 *
 * - mips32
 * - vci_multi_tty
 * - vci_multi_timer
 * - vci_multi_dma
 * - vci_multi_icu
 * - vci_gcd
 * - vci_frame_buffer
 * - vci_block_device
 */

#ifndef _DRIVERS_H_
#define _DRIVERS_H_

/*
 * For retrieving ldscript symbols corresponding to base addresses of
 * peripheral devices.
 */

typedef struct __ldscript_symbol_s __ldscript_symbol_t;

extern __ldscript_symbol_t seg_icu_base;
extern __ldscript_symbol_t seg_timer_base;
extern __ldscript_symbol_t seg_tty_base;
extern __ldscript_symbol_t seg_gcd_base;
extern __ldscript_symbol_t seg_dma_base;
extern __ldscript_symbol_t seg_fb_base;
extern __ldscript_symbol_t seg_ioc_base;

/*
 * Global variables for interaction with ISR
 */

extern volatile unsigned int _dma_status[];
extern volatile unsigned char _dma_busy[];

extern volatile unsigned char _ioc_status;
extern volatile unsigned char _ioc_done;
extern volatile unsigned int _ioc_lock;

extern volatile unsigned char _tty_get_buf[];
extern volatile unsigned char _tty_get_full[];

/*
 * Prototypes of the hardware drivers functions.
 */

unsigned int _procid();
unsigned int _proctime();

unsigned int _timer_write(unsigned int register_index, unsigned int value);
unsigned int _timer_read(unsigned int register_index, unsigned int *buffer);

unsigned int _tty_write(const char *buffer, unsigned int length);
unsigned int _tty_read(char *buffer, unsigned int length);
unsigned int _tty_read_irq(char *buffer, unsigned int length);

unsigned int _ioc_write(unsigned int lba, const void *buffer, unsigned int count);
unsigned int _ioc_read(unsigned int lba, void *buffer, unsigned int count);
unsigned int _ioc_completed();

unsigned int _icu_write(unsigned int register_index, unsigned int value);
unsigned int _icu_read(unsigned int register_index, unsigned int *buffer);

unsigned int _gcd_write(unsigned int register_index, unsigned int value);
unsigned int _gcd_read(unsigned int register_index, unsigned int *buffer);

unsigned int _fb_sync_write(unsigned int offset, const void *buffer, unsigned int length);
unsigned int _fb_sync_read(unsigned int offset, const void *buffer, unsigned int length);
unsigned int _fb_write(unsigned int offset, const void *buffer, unsigned int length);
unsigned int _fb_read(unsigned int offset, const void *buffer, unsigned int length);
unsigned int _fb_completed();

#endif

