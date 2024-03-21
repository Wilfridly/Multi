
/**********************************************************
        File : sys.ld
        Author : Alain Greiner
        Date : December 2011
**********************************************************/

INCLUDE seg.ld

/* Grouping sections into segments for the link editor.  
Kernel segments containing binary code are 
seg_reset, seg_kcode, seg_kdata, seg_kunc */


SECTIONS
{
    . = seg_reset_base;
    seg_reset : 
    {
        *(.reset)
    }
    . = seg_kcode_base;
    seg_kcode : 
    {
        *(.giet)
        *(.text)
    }
    . = seg_kdata_base;
    seg_kdata : 
    {
        *(.rodata)
        *(.rodata.*)
        *(.data)
        *(.lit8)
        *(.lit4)
        *(.sdata)
        *(.bss)
        *(COMMON)
        *(.sbss)
        *(.scommon)
    }
    . = seg_kunc_base;
    seg_kunc : 
    {
        *(.unckdata)
    }
}

