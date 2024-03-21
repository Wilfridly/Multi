
/**********************************************************
        File : app.ld
        Author : Alain Greiner
        Date : December 2011 
**********************************************************/

INCLUDE seg.ld

/* Grouping sections into segments for the link editor.
The applcation segments containing binary code are seg_code, seg_data */


SECTIONS
{
    . = seg_code_base;
    seg_code : 
    {
        *(.mycode)
        *(.text)
    }
    . = seg_data_base;
    seg_data : 
    {
        *(.ctors)
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
}

