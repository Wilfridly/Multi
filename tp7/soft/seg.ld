
/**********************************************************
        File : seg.ld
        Author : Alain Greiner
        Date : December 2011
**********************************************************/

/* definition of the base address for all segments
The peripherals base addresses are referenced by the
software drivers and must be defined, even if these
peripherals are not present in the architecture */

seg_reset_base  = 0xBFC00000;

seg_kcode_base  = 0x80000000;
seg_kunc_base   = 0x81000000;
seg_kdata_base  = 0x82000000;

seg_code_base   = 0x00400000;
seg_data_base   = 0x01000000;
seg_stack_base  = 0x02000000;

seg_tty_base    = 0x90000000;
seg_timer_base  = 0x91000000;
seg_ioc_base    = 0x92000000;
seg_dma_base    = 0x93000000;
seg_gcd_base    = 0x95000000;
seg_fb_base     = 0x96000000;
seg_icu_base    = 0x9F000000;

