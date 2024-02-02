
/**********************************************************
        File : seg.ld
        Author : Alain Greiner
        Date : December 2011
**********************************************************/

/* definition of the base address for all segments
The peripherals base addresses are referenced by the
software drivers and must be defined, even if these
peripherals are not present in the architecture */

seg_reset_base  = TO BE COMPLETED;

seg_kcode_base  = TO BE COMPLETED;
seg_kunc_base   = TO BE COMPLETED;
seg_kdata_base  = TO BE COMPLETED;

seg_code_base   = TO BE COMPLETED;
seg_data_base   = TO BE COMPLETED;
seg_stack_base  = TO BE COMPLETED;

seg_tty_base    = TO BE COMPLETED;
seg_timer_base  = 0x91000000;
seg_ioc_base    = 0x92000000;
seg_dma_base    = 0x93000000;
seg_gcd_base    = 0x95000000;
seg_fb_base     = 0x96000000;
seg_icu_base    = 0x9F000000;

