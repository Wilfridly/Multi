#ifndef _HWR_MAPPING_H
#define _HWR_MAPPING_H

/*
 * Registers mapping for the different peripherals
 */

/* IOC (block device) */
enum IOC_registers {
    BLOCK_DEVICE_BUFFER,
    BLOCK_DEVICE_LBA,
    BLOCK_DEVICE_COUNT,
    BLOCK_DEVICE_OP,
    BLOCK_DEVICE_STATUS,
    BLOCK_DEVICE_IRQ_ENABLE,
    BLOCK_DEVICE_SIZE,
    BLOCK_DEVICE_BLOCK_SIZE,
};
enum IOC_operations {
    BLOCK_DEVICE_NOOP,
    BLOCK_DEVICE_READ,
    BLOCK_DEVICE_WRITE,
};
enum IOC_status{
    BLOCK_DEVICE_IDLE,
    BLOCK_DEVICE_BUSY,
    BLOCK_DEVICE_READ_SUCCESS,
    BLOCK_DEVICE_WRITE_SUCCESS,
    BLOCK_DEVICE_READ_ERROR,
    BLOCK_DEVICE_WRITE_ERROR,
    BLOCK_DEVICE_ERROR,
};

/* DMA */
enum DMA_registers {
    DMA_SRC         = 0,
    DMA_DST         = 1,
    DMA_LEN         = 2,
    DMA_RESET       = 3,
    DMA_IRQ_DISABLE = 4,
    /**/
    DMA_END         = 5,
    DMA_SPAN        = 8,
};

/* GCD */
enum GCD_registers {
    GCD_OPA     = 0,
    GCD_OPB     = 1,
    GCD_START   = 2,
    GCD_STATUS  = 3,
    /**/
    GCD_END     = 4,
};

/* ICU */
enum ICU_registers {
    ICU_INT         = 0,
    ICU_MASK        = 1,
    ICU_MASK_SET    = 2,
    ICU_MASK_CLEAR  = 3,
    ICU_IT_VECTOR   = 4,
    /**/
    ICU_END         = 5,
    ICU_SPAN        = 8,
};

/* TIMER */
enum TIMER_registers {
    TIMER_VALUE     = 0,
    TIMER_MODE      = 1,
    TIMER_PERIOD    = 2,
    TIMER_RESETIRQ  = 3,
    /**/
    TIMER_SPAN      = 4,
};

/* TTY */
enum TTY_registers {
    TTY_WRITE   = 0,
    TTY_STATUS  = 1,
    TTY_READ    = 2,
    TTY_CONFIG  = 3,
    /**/
    TTY_SPAN    = 4,
};

#endif

