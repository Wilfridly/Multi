/*
 * The following global parameters must be defined in a config.h file:
 *
 * - NB_PROCS    : number of processors in the platform
 * - NB_MAXTASKS : max number of tasks per processor
 * - NO_HARD_CC  : No hardware cache coherence
 *
 * The following base addresses must be defined in the ldscript file:
 *
 * - seg_icu_base
 * - seg_timer_base
 * - seg_tty_base
 * - seg_gcd_base
 * - seg_dma_base
 * - seg_fb_base
 * - seg_ioc_base
 */

#include <config.h>
#include <drivers.h>
#include <common.h>
#include <hwr_mapping.h>
#include <ctx_handler.h>

/* Here, we perform some static parameters checking */
#if !defined(NB_PROCS) 
# error You must define NB_PROCS in 'config.h' file!
#endif

#if NB_PROCS > 8
# error GIET currently supports a maximum of 8 processors
#endif

#if !defined(NB_MAXTASKS)
# error You must define NB_MAXTASKS in 'config.h' file!
#endif

#if NB_MAXTASKS > 4
# error GIET currently supports a maximum of 4 tasks/processor
#endif

#if !defined(NO_HARD_CC)
# error You must define NO_HARD_CC in 'config.h' file!
#endif

/*
 * Global (uncachable) variables for interaction with ISR
 *
 * _ioc_lock variable must be integer because 'll/sc' locking mechanism works
 * on integer only.
 */

#define in_unckdata __attribute__((section (".unckdata")))

in_unckdata volatile unsigned int _dma_status[NB_PROCS];
in_unckdata volatile unsigned char _dma_busy[NB_PROCS] = {
    [0 ... NB_PROCS-1] = 0
};

in_unckdata volatile unsigned char _ioc_status;
in_unckdata volatile unsigned char _ioc_done = 0;
in_unckdata volatile unsigned int _ioc_lock = 0;

in_unckdata volatile unsigned char _tty_get_buf[NB_PROCS*NB_MAXTASKS];
in_unckdata volatile unsigned char _tty_get_full[NB_PROCS*NB_MAXTASKS] = {
    [0 ... NB_PROCS*NB_MAXTASKS-1] = 0
};

/* *************
 * Mips32 driver
 * *************
 */

/*
 * _procid()
 *
 * Access CP0 and returns current processor's identifier.
 */
unsigned int _procid()
{
    unsigned int ret;
    asm volatile("mfc0 %0, $15, 1" : "=r"(ret));
    return (ret & 0x3FF);
}

/*
 * _proctime()
 *
 * Access CP0 and returns current processor's elapsed clock cycles (since
 * boot-up).
 */
unsigned int _proctime()
{
    unsigned int ret;
    asm volatile("mfc0 %0, $9" : "=r"(ret));
    return ret;
}

/* ********************
 * VciMultiTimer driver
 * ********************
 *
 * - The total number of timers is equal to NB_PROCS. There is one timer per
 *   processor.
 * - These two functions give access in read/write mode any internal
 *   configuration register with 'register_index' of the timer associated to
 *   the running processor.
 */

/*
 * _timer_write()
 *
 * Write a 32-bit word in a memory mapped register of a timer device. The base
 * address is deduced by the proc_id.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _timer_write(unsigned int register_index, unsigned int value)
{
    volatile unsigned int *timer_address;
    unsigned int proc_id;

    /* parameters checking */
    if (register_index >= TIMER_SPAN)
        return 1;

    proc_id = _procid();
    timer_address = (unsigned int*)&seg_timer_base + (proc_id * TIMER_SPAN);
    timer_address[register_index] = value; /* write word */

    return 0;
}

/*
 * _timer_read()
 *
 * Read a 32-bit word in a memory mapped register of a timer device. The base
 * address is deduced by the proc_id.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _timer_read(unsigned int register_index, unsigned int *buffer)
{
    volatile unsigned int *timer_address;
    unsigned int proc_id;

    /* parameters checking */
    if (register_index >= TIMER_SPAN)
        return 1;

    proc_id = _procid();
    timer_address = (unsigned int*)&seg_timer_base + (proc_id * TIMER_SPAN);
    *buffer = timer_address[register_index]; /* read word */

    return 0;
}

/*
 * ******************
 * VciMultiTty driver
 * ******************
 *
 * - The max number of TTYs is equal to NB_PROCS * NB_MAXTASKS.
 *   (one private TTY per task).
 * - For each task, the tty_id is stored in the context of the task (slot 34),
 *   and can be explicitely defined by the system designer in the boot code,
 *   using the _tty_config() function. The actual stored value is (ty_id + 0x80000000)
 *   A 0 value means that the default tty_id must be used. It is computed as :
 *   tty_id = proc_id * NB_MAXTASKS + task_id.
 *
 *   Finally, the TTY address is always computed as : seg_tty_base + tty_id*TTY_SPAN
 */

/*
 * _tty_config()
 *
 * Initialize the tty_index associated to the task identified by (proc_id, task_id).
 * It returns 1 in case of success and 0 in case of error.
 */
unsigned int _tty_config(unsigned int tty_id, unsigned int proc_id, unsigned int task_id)
{
    if (task_id >= NB_MAXTASKS)  return 0;
    if (proc_id >= NB_PROCS)     return 0;
    _task_context_array[(proc_id*NB_MAXTASKS + task_id)*64 + 34] = tty_id + 0x80000000;
    return 1;
}

/*
 * _tty_write()
 *
 * Write one or several characters directly from a fixed-length user buffer to
 * the TTY_WRITE register of the TTY controler.
 *
 * It doesn't use the TTY_PUT_IRQ interrupt and the associated kernel buffer.
 * This is a non blocking call: it tests the TTY_STATUS register.
 * As soon as the TTY_STATUS[WRITE] bit is set, the transfer stops and the
 * function returns  the number of characters that have been actually written.
 */
unsigned int _tty_write(const char *buffer, unsigned int length)
{
    volatile unsigned int *tty_address;

    unsigned int proc_id;
    unsigned int task_id;
    unsigned int tty_id;

    unsigned int nwritten;

    proc_id = _procid();
    task_id = _current_task_array[proc_id];
    tty_id  = _task_context_array[(proc_id*NB_MAXTASKS + task_id)*64 + 34];
    if(tty_id == 0)  tty_id = proc_id*NB_MAXTASKS + task_id;
    else             tty_id = tty_id - 0x80000000;

    tty_address = (unsigned int*)&seg_tty_base + tty_id*TTY_SPAN;

    for (nwritten = 0; nwritten < length; nwritten++)
    {
        /* check tty's status */
        if ((tty_address[TTY_STATUS] & 0x2) == 0x2)
            break;
        else
            /* write character */
            tty_address[TTY_WRITE] = (unsigned int)buffer[nwritten];
    }

    return nwritten;
}

/*
 * _tty_read_irq()
 *
 * This non-blocking function uses the TTY_GET_IRQ interrupt and the associated
 * kernel buffer, that has been written by the ISR.
 *
 * It fetches one single character from the _tty_get_buf[tty_index] kernel
 * buffer, writes this character to the user buffer, and resets the
 * _tty_get_full[tty_index] buffer.
 *
 * - Returns 0 if the kernel buffer is empty, 1 if the buffer is full.
 */
unsigned int _tty_read_irq(char *buffer, unsigned int length)
{
    unsigned int proc_id;
    unsigned int task_id;
    unsigned int tty_id;

    proc_id = _procid();
    task_id = _current_task_array[proc_id];
    tty_id  = _task_context_array[(proc_id*NB_MAXTASKS + task_id)*64 + 34];
    if(tty_id == 0)  tty_id = proc_id*NB_MAXTASKS + task_id;
    else             tty_id = tty_id - 0x80000000;

    if (_tty_get_full[tty_id] == 0) return 0;

    *buffer = _tty_get_buf[tty_id];
    _tty_get_full[tty_id] = 0;
    return 1;
}

/*
 * _tty_read()
 *
 * This function fetches one character directly from the TTY_READ register of
 * the TTY controler controler, and writes this character to the user buffer.
 *
 * It doesn't use the TTY_GET_IRQ interrupt and the associated kernel buffer.
 * This is a non-blocking call: it tests the TTY_STATUS register.
 *
 * - Returns 0 if the register is empty, 1 if the register is full.
 */
unsigned int _tty_read(char *buffer, unsigned int length)
{
    volatile unsigned int *tty_address;

    unsigned int proc_id;
    unsigned int task_id;
    unsigned int tty_id;

    proc_id = _procid();
    task_id = _current_task_array[proc_id];
    tty_id  = _task_context_array[(proc_id*NB_MAXTASKS + task_id)*64 + 34];
    if(tty_id == 0)  tty_id = proc_id*NB_MAXTASKS + task_id;
    else             tty_id = tty_id - 0x80000000;

    tty_address = (unsigned int*)&seg_tty_base + tty_id*TTY_SPAN;

    if ((tty_address[TTY_STATUS] & 0x1) != 0x1) return 0;

    *buffer = (char)tty_address[TTY_READ];
    return 1;
}

/*
 * ******************
 * VciMultiIcu driver
 * ******************
 *
 * The total number of ICUs is equal to NB_PROCS. There is one ICU per
 * processor.
 */

/*
 * _icu_write()
 *
 * Write a 32-bit word in a memory mapped register of the ICU device. The
 * base address is deduced by the proc_id.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _icu_write(unsigned int register_index, unsigned int value)
{
    volatile unsigned int *icu_address;
    unsigned int proc_id;

    /* parameters checking */
    if (register_index >= ICU_END)
        return 1;

    proc_id = _procid();
    icu_address = (unsigned int*)&seg_icu_base + (proc_id * ICU_SPAN);
    icu_address[register_index] = value;   /* write word */
    return 0;
}

/*
 * _icu_read()
 *
 * Read a 32-bit word in a memory mapped register of the ICU device. The
 * ICU base address is deduced by the proc_id.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _icu_read(unsigned int register_index, unsigned int *buffer)
{
    volatile unsigned int *icu_address;
    unsigned int proc_id;

    /* parameters checking */
    if (register_index >= ICU_END)
        return 1;

    proc_id = _procid();
    icu_address = (unsigned int*)&seg_icu_base + (proc_id * ICU_SPAN);
    *buffer = icu_address[register_index]; /* read word */
    return 0;
}

/*
 * *************
 * VciGcd driver
 * *************
 */

/*
 * _gcd_write()
 *
 * Write a 32-bit word in a memory mapped register of the GCD coprocessor.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _gcd_write(unsigned int register_index, unsigned int value)
{
    volatile unsigned int *gcd_address;

    /* parameters checking */
    if (register_index >= GCD_END)
        return 1;

    gcd_address = (unsigned int*)&seg_gcd_base;
    gcd_address[register_index] = value; /* write word */
    return 0;
}

/*
 * _gcd_read()
 *
 * Read a 32-bit word in a memory mapped register of the GCD coprocessor.
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _gcd_read(unsigned int register_index, unsigned int *buffer)
{
    volatile unsigned int *gcd_address;

    /* parameters checking */
    if (register_index >= GCD_END)
        return 1;

    gcd_address = (unsigned int*)&seg_gcd_base;
    *buffer = gcd_address[register_index]; /* read word */
    return 0;
}

/*
 * *********************
 * VciBlockDevice driver
 * *********************
 *
 * The three functions below use the three variables _ioc_lock _ioc_done,  and
 * _ioc_status for synchronsation.
 * - As the IOC component can be used by several programs running in parallel,
 *   the _ioc_lock variable guaranties exclusive access to the device.  The
 *   _ioc_read() and _ioc_write() functions use atomic LL/SC to get the lock.
 *   and set _ioc_lock to a non zero value.  The _ioc_write() and _ioc_read()
 *   functions are blocking, polling the _ioc_lock variable until the device is
 *   available.
 * - When the tranfer is completed, the ISR routine activated by the IOC IRQ
 *   set the _ioc_done variable to a non-zero value. Possible address errors
 *   detected by the IOC peripheral are reported by the ISR in the _ioc_status
 *   variable.
 * The _ioc_completed() function is polling the _ioc_done variable, waiting for
 * tranfer conpletion. When the completion is signaled, the _ioc_completed()
 * function reset the _ioc_done variable to zero, and releases the _ioc_lock
 * variable.
 *
 * In a multi-processing environment, this polling policy should be replaced by
 * a descheduling policy for the requesting process.
 */

/*
 * _ioc_get_lock()
 *
 * This blocking helper is used by '_ioc_read()' and '_ioc_write()' functions
 * to get _ioc_lock using atomic LL/SC.
 */
static inline void _ioc_get_lock()
{
    register unsigned int *plock = (unsigned int*)&_ioc_lock;

    asm volatile (
            "_ioc_llsc:             \n"
            "ll   $2,    0(%0)      \n" /* $2 <= _ioc_lock current value */
            "bnez $2,    _ioc_llsc  \n" /* retry if _ioc_lock already taken */
            "li   $3,    1          \n" /* $3 <= argument for sc */
            "sc   $3,    0(%0)      \n" /* try to set _ioc_lock */
            "beqz $3,    _ioc_llsc  \n" /* exit if atomic */
            :
            :"r"(plock)
            :"$2", "$3");
}

/*
 *  _ioc_write()
 *
 * Transfer data from a memory buffer to a file on the block_device. The source
 * memory buffer must be in user address space.
 * - lba    : first block index on the disk.
 * - buffer : base address of the memory buffer.
 * - count  : number of blocks to be transfered.
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _ioc_write(unsigned int lba, const void *buffer, unsigned int count)
{
    volatile unsigned int *ioc_address;

    ioc_address = (unsigned int*)&seg_ioc_base;

    /* parameters checking */
    /* buffer must be in user space */
    unsigned int block_size = ioc_address[BLOCK_DEVICE_BLOCK_SIZE];

    if (((unsigned int)buffer >= 0x80000000)
            || (((unsigned int)buffer + block_size*count) >= 0x80000000))
        return 1;

    /* get the lock on ioc device */
    _ioc_get_lock();

    /* block_device configuration for the write transfer */
    ioc_address[BLOCK_DEVICE_BUFFER] = (unsigned int)buffer;
    ioc_address[BLOCK_DEVICE_COUNT] = count;
    ioc_address[BLOCK_DEVICE_LBA] = lba;
    ioc_address[BLOCK_DEVICE_IRQ_ENABLE] = 1;
    ioc_address[BLOCK_DEVICE_OP] = BLOCK_DEVICE_WRITE;
    return 0;
}

/*
 * _ioc_read()
 *
 * Transfer data from a file on the block device to a memory buffer. The destination
 * memory buffer must be in user address space.
 * - lba    : first block index on the disk.
 * - buffer : base address of the memory buffer.
 * - count  : number of blocks to be transfered.
 *
 * - Returns 0 if success, > 0 if error.
 *
 * Note: all cache lines corresponding to the the target buffer are invalidated
 * for cache coherence.
 */
unsigned int _ioc_read(unsigned int lba, void *buffer, unsigned int count)
{
    volatile unsigned int *ioc_address;

    ioc_address = (unsigned int*)&seg_ioc_base;

    /* parameters checking */
    /* buffer must be in user space */
    unsigned int block_size = ioc_address[BLOCK_DEVICE_BLOCK_SIZE];

    if (((unsigned int)buffer >= 0x80000000)
            || (((unsigned int)buffer + block_size*count) >= 0x80000000))
        return 1;

    /* get the lock on ioc device */
    _ioc_get_lock();

    /* invalidation of data cache */
    if( NO_HARD_CC ) _dcache_buf_invalidate(buffer, block_size*count);

    /* block_device configuration for the read transfer */
    ioc_address[BLOCK_DEVICE_BUFFER] = (unsigned int)buffer;
    ioc_address[BLOCK_DEVICE_COUNT] = count;
    ioc_address[BLOCK_DEVICE_LBA] = lba;
    ioc_address[BLOCK_DEVICE_IRQ_ENABLE] = 1;
    ioc_address[BLOCK_DEVICE_OP] = BLOCK_DEVICE_READ;


    return 0;
}

/*
 * _ioc_completed()
 *
 * This function checks completion of an I/O transfer and reports errors. As it
 * is a blocking call, the processor is stalled until the next interrupt.
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _ioc_completed()
{
    unsigned int ret;

    /* busy waiting */
    while (_ioc_done == 0)
        asm volatile("nop");

    /* test IOC status */
    if ((_ioc_status != BLOCK_DEVICE_READ_SUCCESS)
            && (_ioc_status != BLOCK_DEVICE_WRITE_SUCCESS)) 	ret = 1;	/* error */
    else							ret = 0;	/* success */

    /* reset synchronization variables */
    _ioc_done =0;
    _ioc_lock =0;

    return ret;
}

/*
 * *********************
 * VciFrameBuffer driver
 * *********************
 *
 * The '_fb_sync_write' and '_fb_sync_read' functions use a memcpy strategy to
 * implement the transfer between a data buffer (user space) and the frame
 * buffer (kernel space). They are blocking until completion of the transfer.
 * ---
 * The '_fb_write()', '_fb_read()' and '_fb_completed()' functions use the DMA
 * coprocessor to transfer data between the user buffer and the frame buffer.
 *
 * Quite similarly to the block device, these three functions use a polling
 * policy to test the global variables _dma_busy[i] and detect the transfer
 * completion.  As each processor has its private DMA, there is up to NB_PROCS
 * _dma_busy locks, that are indexed by the proc_id.
 * A _dma_busy variable is reset by the ISR associated to the DMA device IRQ.
 */

/*
 * _fb_sync_write()
 *
 * Transfer data from an memory buffer to the frame_buffer device with a
 * memcpy. The source memory buffer must be in user address space.
 * - offset : offset (in bytes) in the frame buffer.
 * - buffer : base address of the memory buffer.
 * - length : number of bytes to be transfered.
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _fb_sync_write(unsigned offset, const void *buffer, unsigned int length)
{
    volatile unsigned char *fb_address;

    /* parameters checking */
    /* buffer must be in user space */
    if (((unsigned int)buffer >= 0x80000000)
            || (((unsigned int)buffer + length ) >= 0x80000000 ))
        return 1;

    fb_address = (unsigned char*)&seg_fb_base + offset;

    /* buffer copy */
    memcpy((void*)fb_address, (void*)buffer, length);

    return 0;
}

/*
 * _fb_sync_read()
 *
 * Transfer data from the frame_buffer device to an memory buffer with a
 * memcpy. The destination memory buffer must be in user address space.
 * - offset : offset (in bytes) in the frame buffer.
 * - buffer : base address of the memory buffer.
 * - length : number of bytes to be transfered.
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _fb_sync_read(unsigned int offset, const void *buffer, unsigned int length)
{
    volatile unsigned char *fb_address;

    /* parameters checking */
    /* buffer must be in user space */
    if (((unsigned int)buffer >= 0x80000000)
            || (((unsigned int)buffer + length ) >= 0x80000000 ))
        return 1;

    fb_address = (unsigned char*)&seg_fb_base + offset;

    /* buffer copy */
    memcpy((void*)buffer, (void*)fb_address, length);

    return 0;
}

/*
 * _fb_write()
 *
 * Transfer data from an memory buffer to the frame_buffer device using a DMA.
 * The source memory buffer must be in user address space.
 * - offset : offset (in bytes) in the frame buffer.
 * - buffer : base address of the memory buffer.
 * - length : number of bytes to be transfered.
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _fb_write(unsigned int offset, const void *buffer, unsigned int length)
{
    volatile unsigned char *fb_address;
    volatile unsigned int *dma;

    unsigned int proc_id;

    unsigned int delay;
    unsigned int i;

    /* parameters checking */
    /* buffer must be in user space */
    if (((unsigned int)buffer >= 0x80000000)
            || (((unsigned int)buffer + length ) >= 0x80000000 ))
        return 1;

    proc_id = _procid();
    fb_address = (unsigned char*)&seg_fb_base + offset;
    dma = (unsigned int*)&seg_dma_base + (proc_id * DMA_SPAN);

    /* waiting until DMA device is available */
    while (_dma_busy[proc_id] != 0)
    {
        /* if the lock failed, busy wait with a pseudo random delay between bus
         * accesses */
        delay = (_proctime() & 0xF) << 4;
        for (i = 0; i < delay; i++)
            asm volatile("nop");
    }
    _dma_busy[proc_id] = 1;

    /* DMA configuration for write transfer */
    dma[DMA_IRQ_DISABLE] = 0;
    dma[DMA_SRC] = (unsigned int)buffer;
    dma[DMA_DST] = (unsigned int)fb_address;
    dma[DMA_LEN] = (unsigned int)length;
    return 0;
}

/*
 * _fb_read()
 *
 * Transfer data from the frame_buffer device to an memory buffer using a DMA.
 * The destination memory buffer must be in user address space.
 * - offset : offset (in bytes) in the frame buffer.
 * - buffer : base address of the memory buffer.
 * - length : number of bytes to be transfered.
 *
 * - Returns 0 if success, > 0 if error.
 *
 * Note: all cache lines corresponding to the the target buffer are invalidated
 * for cache coherence.
 */
unsigned int _fb_read(unsigned int offset, const void *buffer, unsigned int length)
{
    volatile unsigned char *fb_address;
    volatile unsigned int *dma;

    unsigned int proc_id;

    unsigned int delay;
    unsigned int i;

    /* parameters checking */
    /* buffer must be in user space */
    if (((unsigned int)buffer >= 0x80000000)
            || (((unsigned int)buffer + length ) >= 0x80000000 ))
        return 1;

    proc_id = _procid();
    fb_address = (unsigned char*)&seg_fb_base + offset;
    dma = (unsigned int*)&seg_dma_base + (proc_id * DMA_SPAN);

    /* waiting until DMA device is available */
    while (_dma_busy[proc_id] != 0)
    {
        /* if the lock failed, busy wait with a pseudo random delay between bus
         * accesses */
        delay = (_proctime() & 0xF) << 4;
        for (i = 0; i < delay; i++)
            asm volatile("nop");
    }
    _dma_busy[proc_id] = 1;

    /* DMA configuration for write transfer */
    dma[DMA_IRQ_DISABLE] = 0;
    dma[DMA_SRC] = (unsigned int)fb_address;
    dma[DMA_DST] = (unsigned int)buffer;
    dma[DMA_LEN] = (unsigned int)length;

    /* invalidation of data cache */
//    _dcache_buf_invalidate(buffer, length);

    return 0;
}

/*
 * _fb_completed()
 *
 * This function checks completion of a DMA transfer to or fom the frame buffer.
 *
 * As it is a blocking call, the processor is stalled until the next interrupt.
 *
 * - Returns 0 if success, > 0 if error.
 */
unsigned int _fb_completed()
{
    unsigned int proc_id;

    proc_id = _procid();

    while (_dma_busy[proc_id] != 0)
        asm volatile("nop");

    if (_dma_status[proc_id] != 0)
        return 1;

    return 0;
}

