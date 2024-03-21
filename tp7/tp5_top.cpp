/**********************************************************************
 * File : tp5_top.cpp
 * Date : 25/12/2011
 * Author :  Alain Greiner
 * UPMC - LIP6
 * This program is released under the GNU public license
 **********************************************************************
 * This architecture contains (nprocs + 9) components:
 *  - BCU        : PIBUS controler
 *  - RAM        : static RAM
 *  - ROM        : boot ROM
 *  - TTY        : TTY Display controller
 *  - FBF        : Frame Buffer controller
 *  - ICU        : Interrupt controller
 *  - TIMER      : programmable timer
 *  - DMA        : DMA controller
 *  - IOC        : Disk controller
 *  - PROC[i]    : MIPS32 processors 
 * Interupts are connected as follows:
 *  - IRQ_IN[0]    : DMA
 *  - IRQ_IN[1]    : IOC
 *  - IRQ_IN[2+2i] : TIMER[i]
 *  - IRQ_IN[3+2i] : TTY[i]
 **********************************************************************/

// Hardware parameters default values
// These values can be modified on the command Line

#define NPROCS       1    // number of processors
#define FB_NPIXEL    256    // Frame buffer width
#define FB_NLINE     256    // Frame buffer heigth
#define BLOCK_SIZE   512    // IOC block size
#define IOC_LATENCY  1000    // disk latency
#define RAM_LATENCY  0    // ram latency
#define ICACHE_WAYS  4       // instruction cache number of ways
#define ICACHE_SETS  128    // instruction cache number of sets
#define ICACHE_WORDS 8       // instruction cache number of words per line
#define DCACHE_WAYS  4       // data cache number of ways
#define DCACHE_SETS  128     // data cache number of sets
#define DCACHE_WORDS 8       // data cache number of words per line
#define WBUF_DEPTH   8       // cache write buffer depth
#define SNOOP        false    // cache snoop activation
#define DMA_BURST    16    // number of words in a DMA burst

#include <systemc.h>

#include "pibus_simple_ram.h"
#include "pibus_frame_buffer.h"
#include "pibus_icu.h"
#include "pibus_multi_timer.h"
#include "pibus_dma.h"
#include "pibus_mips32_xcache.h"
#include "pibus_multi_tty.h"
#include "pibus_seg_bcu.h"
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"
#include "pibus_block_device.h"
#include "loader.h"

#include <stdio.h>
#include <stdarg.h>

// segments definition

#define SEG_RESET_BASE  0xBFC00000
#define SEG_RESET_SIZE  0x00001000

#define SEG_KCODE_BASE  0x80000000
#define SEG_KCODE_SIZE  0x00004000

#define SEG_KDATA_BASE  0x82000000
#define SEG_KDATA_SIZE  0x00010000

#define SEG_KUNC_BASE   0x81000000
#define SEG_KUNC_SIZE   0x00010000

#define SEG_CODE_BASE   0x00400000
#define SEG_CODE_SIZE   0x00004000

#define SEG_DATA_BASE   0x01000000
#define SEG_DATA_SIZE   0x00080000

#define SEG_STACK_BASE  0x02000000
#define SEG_STACK_SIZE  0x00100000

#define SEG_TTY_BASE    0x90000000
#define SEG_TTY_SIZE    16*nprocs 

#define SEG_TIM_BASE    0x91000000
#define SEG_TIM_SIZE    16*nprocs 

#define SEG_IOC_BASE    0x92000000
#define SEG_IOC_SIZE    0x00000020

#define SEG_DMA_BASE    0x93000000
#define SEG_DMA_SIZE    0x00000020

#define SEG_FBF_BASE    0x96000000
#define SEG_FBF_SIZE    FB_NPIXEL*FB_NLINE

#define SEG_ICU_BASE    0x9F000000
#define SEG_ICU_SIZE    32*nprocs 

#define ROM_INDEX    0
#define RAM_INDEX    1
#define TTY_INDEX    2
#define FBF_INDEX    3
#define ICU_INDEX    4
#define TIM_INDEX    5
#define DMA_INDEX    6
#define IOC_INDEX    7

int _main (int argc, char * argv[]) {
    using namespace sc_core;
    using namespace soclib::common;
    using namespace soclib::caba;

    ///////////////////////////////////////////////////////////////////////////////////
    //   Hardware parameters (can be redefined on the command line)
    ///////////////////////////////////////////////////////////////////////////////////
    size_t  ncycles             = 1000000000;          // number of simulated cycles
    char    sys_path[256]       = "soft/sys.bin";      // pathname for system binary code
    char    app_path[256]       = "soft/app.bin";      // pathname for application binary code
    char    disk_path[256]      = "soft/Makefile";          // pathname for the disk_image
    bool    trace_ok            = false;               // debug activated
    size_t  from_cycle          = 0;                   // debug start cycle
    size_t  ram_latency         = RAM_LATENCY;         // ram latency
    size_t  ioc_latency         = IOC_LATENCY;         // disk latency
    size_t  nprocs              = NPROCS;              // number of processors 
    size_t  icache_ways         = ICACHE_WAYS;         // instruction cache number of ways
    size_t  icache_sets         = ICACHE_SETS;         // instruction cache number of sets
    size_t  icache_words        = ICACHE_WORDS;        // instruction cache number of words per line
    size_t  dcache_ways         = DCACHE_WAYS;         // data cache number of ways
    size_t  dcache_sets         = DCACHE_SETS;         // data cache number of sets
    size_t  dcache_words        = DCACHE_WORDS;        // data cache number of words per line
    size_t  wbuf_depth          = WBUF_DEPTH;          // write buffer depth
    bool    stats_ok            = false;               // statistics activation
    size_t  stats_period        = 0;                   // statistics display period 
    size_t  dma_burst           = DMA_BURST;           // DMA burst length (number of words)
    bool    snoop_active        = SNOOP;               // snoop activation

    std::cout << std::endl;
    std::cout << "********************************************************" << std::endl;
    std::cout << "******        tp5_top                             ******" << std::endl;
    std::cout << "********************************************************" << std::endl;
    std::cout << std::endl;

    if (argc > 1) {
        for (int n = 1; n < argc; n = n + 2) {
            if ((strcmp(argv[n], "-NCYCLES") == 0) && (n + 1 < argc)) {
                ncycles = atoi(argv[n + 1]);
            }
            else if ((strcmp(argv[n], "-NPROCS") == 0) && (n + 1 < argc)) {
                nprocs = atoi(argv[n + 1]);
            }
            else if ((strcmp(argv[n], "-TRACE") == 0) && (n + 1 < argc)) {
                trace_ok = true;
                from_cycle = atoi(argv[n + 1]);
            }
            else if ((strcmp(argv[n],"-SYS") == 0) && (n + 1 < argc)) {
                strcpy(sys_path, argv[n+1]) ;
            }
            else if ((strcmp(argv[n], "-APP") == 0) && (n + 1 < argc)) {
                strcpy(app_path, argv[n + 1]) ;
            }
            else if ((strcmp(argv[n], "-DISK") == 0) && (n + 1 < argc)) {
                strcpy(disk_path, argv[n + 1]) ;
            }
            else if ((strcmp(argv[n], "-RAMLATENCY") == 0) && (n + 1 < argc)) {
                ram_latency = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-IOCLATENCY") == 0) && (n + 1 < argc)) {
                ram_latency = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-SNOOP") == 0) && (n + 1 < argc)) {
                snoop_active = (atoi(argv[n+1]) != 0);
            }
            else if ((strcmp(argv[n], "-IWORDS") == 0) && (n + 1 < argc)) {
                icache_words = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-ISETS") == 0) && (n + 1 < argc)) {
                icache_sets = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-IWAYS") == 0) && (n + 1 < argc)) {
                icache_ways = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-DWORDS") == 0) && (n + 1 < argc)) {
                dcache_words = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-DSETS") == 0) && (n + 1 < argc)) {
                dcache_sets = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-DWAYS") == 0) && (n + 1 < argc)) {
                dcache_ways = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-WBUF") == 0) && (n + 1 < argc)) {
                wbuf_depth = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-STATS") == 0) && (n + 1 < argc)) {
                stats_ok = true;
                stats_period = atoi(argv[n+1]);
            }
            else if ((strcmp(argv[n], "-DMABURST") == 0) && (n + 1 < argc)) {
                dma_burst = atoi(argv[n+1]);
            }
            else
            {
                std::cout << "   Arguments on the command line are (key,value) couples." << std::endl;
                std::cout << "   The order is not important." << std::endl;
                std::cout << "   Accepted arguments are :" << std::endl << std::endl;
                std::cout << "   -NCYCLES number_of_simulated_cycles" << std::endl;
                std::cout << "   -NPROCS number_of_processors" << std::endl;
                std::cout << "   -TRACE debug_start_cycle" << std::endl;
                std::cout << "   -RAMLATENCY ram_latency_value" << std::endl;
                std::cout << "   -IOCLATENCY ioc_latency_value" << std::endl;
                std::cout << "   -SYS system_code_path_name" << std::endl;
                std::cout << "   -APP application_code_path_name" << std::endl;
                std::cout << "   -DISK disk_image_path_name" << std::endl;
                std::cout << "   -SNOOP non_zero_value_to_activate" << std::endl;
                std::cout << "   -IWORDS number_of_words_per_line" << std::endl;
                std::cout << "   -ISETS number_of_sets" << std::endl;
                std::cout << "   -IWAYS number_of_ways" << std::endl;
                std::cout << "   -DWORDS number_of_words_per_line" << std::endl;
                std::cout << "   -DSETS number_of_sets" << std::endl;
                std::cout << "   -DWAYS number_of_ways" << std::endl;
                std::cout << "   -WBUF write_buffer_depth" << std::endl;
                std::cout << "   -STATS period" << std::endl;
                std::cout << "   -DMABURST number_of_words_in_a_burst" << std::endl;
                exit(0);
            }
        }
    }

    //////////////////////////////////////////////////////
    //      SIGNALS DECLARATION
    //////////////////////////////////////////////////////

    sc_clock                signal_ck("signal_ck");
    sc_signal<bool>         signal_resetn("signal_resetn");

    sc_signal<bool>         signal_req_proc[nprocs];
    sc_signal<bool>         signal_gnt_proc[nprocs];

    sc_signal<bool>         signal_req_dma("req_dma");
    sc_signal<bool>         signal_gnt_dma("gnt_dma");

    sc_signal<bool>         signal_req_ioc("req_ioc");
    sc_signal<bool>         signal_gnt_ioc("gnt_ioc");

    sc_signal<bool>         signal_sel_rom("sel_rom");
    sc_signal<bool>         signal_sel_ram("sel_ram");
    sc_signal<bool>         signal_sel_tty("sel_tty");
    sc_signal<bool>         signal_sel_fbf("sel_fbf");
    sc_signal<bool>         signal_sel_icu("sel_icu");
    sc_signal<bool>         signal_sel_tim("sel_tim");
    sc_signal<bool>         signal_sel_dma("sel_dma");
    sc_signal<bool>         signal_sel_ioc("sel_ioc");

    sc_signal<uint32_t>     signal_pi_a("pi_a");
    sc_signal<bool>         signal_pi_lock("pi_lock");
    sc_signal<bool>         signal_pi_read("pi_read");
    sc_signal<uint32_t>     signal_pi_opc("pi_opc");
    sc_signal<uint32_t>     signal_pi_d("pi_d");
    sc_signal<uint32_t>     signal_pi_ack("pi_ack");
    sc_signal<bool>         signal_pi_tout("pi_tout");
    sc_signal<bool>         signal_pi_avalid("pi_avalid");

    sc_signal<bool>         signal_irq_proc[nprocs];
    sc_signal<bool>         signal_irq_tim[nprocs];
    sc_signal<bool>         signal_irq_tty_get[nprocs];
    sc_signal<bool>         signal_irq_tty_put[nprocs];
    sc_signal<bool>         signal_irq_dma("signal_irq_dma");
    sc_signal<bool>         signal_irq_ioc("signal_irq_ioc");

    ////////////////////////////////////////////////////
    //    SEGMENT_TABLE DEFINITION
    ////////////////////////////////////////////////////

    PibusSegmentTable    segtable;

    segtable.setMSBnumber(8);

    segtable.addSegment("seg_reset", SEG_RESET_BASE,  SEG_RESET_SIZE, ROM_INDEX, true);
    segtable.addSegment("seg_kcode", SEG_KCODE_BASE,  SEG_KCODE_SIZE, RAM_INDEX, true);
    segtable.addSegment("seg_kdata", SEG_KDATA_BASE,  SEG_KDATA_SIZE, RAM_INDEX, true);
    segtable.addSegment("seg_kunc" , SEG_KUNC_BASE ,  SEG_KUNC_SIZE , RAM_INDEX, false);
    segtable.addSegment("seg_code" , SEG_CODE_BASE ,  SEG_CODE_SIZE , RAM_INDEX, true);
    segtable.addSegment("seg_stack", SEG_STACK_BASE,  SEG_STACK_SIZE, RAM_INDEX, true);
    segtable.addSegment("seg_data" , SEG_DATA_BASE ,  SEG_DATA_SIZE , RAM_INDEX, true);
    segtable.addSegment("seg_fbf"  , SEG_FBF_BASE  ,  SEG_FBF_SIZE  , FBF_INDEX, false);
    segtable.addSegment("seg_tty"  , SEG_TTY_BASE  ,  SEG_TTY_SIZE  , TTY_INDEX, false);
    segtable.addSegment("seg_icu"  , SEG_ICU_BASE  ,  SEG_ICU_SIZE  , ICU_INDEX, false);
    segtable.addSegment("seg_tim"  , SEG_TIM_BASE  ,  SEG_TIM_SIZE  , TIM_INDEX, false);
    segtable.addSegment("seg_dma"  , SEG_DMA_BASE  ,  SEG_DMA_SIZE  , DMA_INDEX, false);
    segtable.addSegment("seg_ioc"  , SEG_IOC_BASE  ,  SEG_IOC_SIZE  , IOC_INDEX, false);

    segtable.print();
    std::cout << std::endl;

    /////////////////////////////////////////////////////////
    //    INSTANCIATED  COMPONENTS
    /////////////////////////////////////////////////////////

    Loader        loader(sys_path, app_path);

    PibusSegBcu      bcu("bcu", segtable,  nprocs + 2, 8, 100);
    PibusSimpleRam   rom("rom", ROM_INDEX, segtable, 0, loader);
    PibusSimpleRam   ram("ram", RAM_INDEX, segtable, ram_latency, loader);
    PibusMultiTty    tty("tty", TTY_INDEX, segtable, nprocs);
    PibusFrameBuffer fbf("fbf", FBF_INDEX, segtable, 0, FB_NPIXEL, FB_NLINE);
    PibusIcu         icu("icu", ICU_INDEX, segtable, 2 * nprocs + 2, nprocs);
    PibusMultiTimer  tim("tim", TIM_INDEX, segtable, nprocs);
    PibusDma         dma("dma", DMA_INDEX, segtable, dma_burst);
    PibusBlockDevice ioc("ioc", IOC_INDEX, segtable, disk_path, BLOCK_SIZE, ioc_latency);


    PibusMips32Xcache * proc[nprocs];
    char * name[nprocs];
    for (size_t i = 0; i < nprocs; i++) {
        name[i] = new char[16];
        sprintf(name[i], "proc[%d]", i);
        proc[i] = new PibusMips32Xcache( name[i] , segtable, i, icache_ways, icache_sets, icache_words,
                dcache_ways, dcache_sets, dcache_words, 
                wbuf_depth, snoop_active);
    }

    std::cout << std::endl;

    //////////////////////////////////////////////////////////
    //    Net-List
    //////////////////////////////////////////////////////////

    bcu.p_ck             (signal_ck);
    bcu.p_resetn         (signal_resetn);
    bcu.p_sel[ROM_INDEX] (signal_sel_rom);
    bcu.p_sel[RAM_INDEX] (signal_sel_ram);
    bcu.p_sel[TTY_INDEX] (signal_sel_tty);
    bcu.p_sel[FBF_INDEX] (signal_sel_fbf);
    bcu.p_sel[ICU_INDEX] (signal_sel_icu);
    bcu.p_sel[TIM_INDEX] (signal_sel_tim);
    bcu.p_sel[DMA_INDEX] (signal_sel_dma);
    bcu.p_sel[IOC_INDEX] (signal_sel_ioc);
    bcu.p_a              (signal_pi_a);
    bcu.p_lock           (signal_pi_lock);
    bcu.p_ack            (signal_pi_ack);
    bcu.p_tout           (signal_pi_tout);
    bcu.p_avalid         (signal_pi_avalid);
    for (size_t i = 0; i < nprocs; i += 1) {
        bcu.p_req[i]     (signal_req_proc[i]);
        bcu.p_gnt[i]     (signal_gnt_proc[i]);
    }
    bcu.p_req[nprocs]    (signal_req_dma);
    bcu.p_gnt[nprocs]    (signal_gnt_dma);
    bcu.p_req[nprocs + 1](signal_req_ioc);
    bcu.p_gnt[nprocs + 1](signal_gnt_ioc);

    std::cout << "bcu : connected" << std::endl;

    ram.p_ck             (signal_ck);
    ram.p_resetn         (signal_resetn);
    ram.p_sel            (signal_sel_ram);
    ram.p_a              (signal_pi_a);
    ram.p_read           (signal_pi_read);
    ram.p_opc            (signal_pi_opc);
    ram.p_ack            (signal_pi_ack);
    ram.p_d              (signal_pi_d);
    ram.p_tout           (signal_pi_tout);

    std::cout << "ram : connected" << std::endl;

    rom.p_ck             (signal_ck);
    rom.p_resetn         (signal_resetn);
    rom.p_sel            (signal_sel_rom);
    rom.p_a              (signal_pi_a);
    rom.p_read           (signal_pi_read);
    rom.p_opc            (signal_pi_opc);
    rom.p_ack            (signal_pi_ack);
    rom.p_d              (signal_pi_d);
    rom.p_tout           (signal_pi_tout);

    std::cout << "rom : connected" << std::endl;

    tty.p_ck             (signal_ck);
    tty.p_resetn         (signal_resetn);
    tty.p_sel            (signal_sel_tty);
    tty.p_a              (signal_pi_a);
    tty.p_read           (signal_pi_read);
    tty.p_opc            (signal_pi_opc);
    tty.p_ack            (signal_pi_ack);
    tty.p_d              (signal_pi_d);
    tty.p_tout           (signal_pi_tout);
    for (size_t i = 0; i < nprocs; i += 1) {
        tty.p_irq_get[i] (signal_irq_tty_get[i]);
        tty.p_irq_put[i] (signal_irq_tty_put[i]);
    }

    std::cout << "tty : connected" << std::endl;

    tim.p_ck             (signal_ck);
    tim.p_resetn         (signal_resetn);
    tim.p_sel            (signal_sel_tim);
    tim.p_a              (signal_pi_a);
    tim.p_read           (signal_pi_read);
    tim.p_opc            (signal_pi_opc);
    tim.p_ack            (signal_pi_ack);
    tim.p_d              (signal_pi_d);
    tim.p_tout           (signal_pi_tout);
    for (size_t i = 0; i < nprocs; i++) {
        tim.p_irq[i]     (signal_irq_tim[i]);
    }

    std::cout << "tim : connected" << std::endl;

    fbf.p_ck             (signal_ck);
    fbf.p_resetn         (signal_resetn);
    fbf.p_sel            (signal_sel_fbf);
    fbf.p_a              (signal_pi_a);
    fbf.p_read           (signal_pi_read);
    fbf.p_opc            (signal_pi_opc);
    fbf.p_ack            (signal_pi_ack);
    fbf.p_d              (signal_pi_d);
    fbf.p_tout           (signal_pi_tout);

    std::cout << "fbf : connected" << std::endl;

    icu.p_ck             (signal_ck);
    icu.p_resetn         (signal_resetn);
    icu.p_sel            (signal_sel_icu);
    icu.p_a              (signal_pi_a);
    icu.p_read           (signal_pi_read);
    icu.p_opc            (signal_pi_opc);
    icu.p_ack            (signal_pi_ack);
    icu.p_d              (signal_pi_d);
    icu.p_tout           (signal_pi_tout);
    icu.p_irq_in[0]      (signal_irq_dma);
    icu.p_irq_in[1]      (signal_irq_ioc);
    for (size_t i = 0 ; i < nprocs; i += 1) {
        icu.p_irq_in[2 + 2 * i](signal_irq_tim[i]);
        icu.p_irq_in[3 + 2 * i](signal_irq_tty_get[i]);
        icu.p_irq_out[i] (signal_irq_proc[i]);
    }

    std::cout << "icu : connected" << std::endl;

    dma.p_ck             (signal_ck);
    dma.p_resetn         (signal_resetn);
    dma.p_req            (signal_req_dma);
    dma.p_gnt            (signal_gnt_dma);
    dma.p_sel            (signal_sel_dma);
    dma.p_a              (signal_pi_a);
    dma.p_read           (signal_pi_read);
    dma.p_opc            (signal_pi_opc);
    dma.p_lock           (signal_pi_lock);
    dma.p_ack            (signal_pi_ack);
    dma.p_d              (signal_pi_d);
    dma.p_tout           (signal_pi_tout);
    dma.p_irq            (signal_irq_dma);

    std::cout << "dma : connected" << std::endl;

    ioc.p_ck             (signal_ck);
    ioc.p_resetn         (signal_resetn);
    ioc.p_req            (signal_req_ioc);
    ioc.p_gnt            (signal_gnt_ioc);
    ioc.p_sel            (signal_sel_ioc);
    ioc.p_a              (signal_pi_a);
    ioc.p_read           (signal_pi_read);
    ioc.p_opc            (signal_pi_opc);
    ioc.p_lock           (signal_pi_lock);
    ioc.p_ack            (signal_pi_ack);
    ioc.p_d              (signal_pi_d);
    ioc.p_tout           (signal_pi_tout);
    ioc.p_irq            (signal_irq_ioc);

    std::cout << "ioc : connected" << std::endl;

    for (size_t i = 0; i < nprocs; i++) {
        proc[i]->p_ck     (signal_ck);  
        proc[i]->p_resetn (signal_resetn);  
        proc[i]->p_req    (signal_req_proc[i]);
        proc[i]->p_gnt    (signal_gnt_proc[i]);
        proc[i]->p_lock   (signal_pi_lock);
        proc[i]->p_read   (signal_pi_read);
        proc[i]->p_opc    (signal_pi_opc);
        proc[i]->p_a      (signal_pi_a);
        proc[i]->p_d      (signal_pi_d);
        proc[i]->p_ack    (signal_pi_ack);
        proc[i]->p_tout   (signal_pi_tout);
        proc[i]->p_avalid (signal_pi_avalid);
        proc[i]->p_irq    (signal_irq_proc[i]);
    }

    std::cout << "procs : connected" << std::endl;

    std::cout << std::endl;

    //////////////////////////////////////////////
    //     simulation loop
    /////////////////////////////////////////////

    signal_resetn = false;

    sc_start(sc_time(1, SC_NS));

    signal_resetn = true;

    for (size_t n = 1; n < ncycles; n++) {
        sc_start(sc_time(1, SC_NS));

        if (stats_ok && (n % stats_period == 0)) {
            proc[0]->printStatistics();
            bcu.printStatistics();
        }

        if (trace_ok && (n > from_cycle)) {
            std::cout << std::dec <<"*******************  cycle = " << n 
                << " ***************************************" << std::endl;
            proc[0]->printTrace();
            bcu.printTrace();
            rom.printTrace();
            ram.printTrace();
            tty.printTrace();
            fbf.printTrace();
            icu.printTrace();
            tim.printTrace();
            dma.printTrace();
            ioc.printTrace();

            std::cout << "  -- select signals --" << std::dec << std::endl;
            std::cout << "sel_rom     = " << signal_sel_rom.read()           << std::endl;
            std::cout << "sel_ram     = " << signal_sel_ram.read()           << std::endl;
            std::cout << "sel_tty     = " << signal_sel_tty.read()           << std::endl;
            std::cout << "sel_fbf     = " << signal_sel_fbf.read()           << std::endl;
            std::cout << "sel_icu     = " << signal_sel_icu.read()           << std::endl;
            std::cout << "sel_tim     = " << signal_sel_tim.read()           << std::endl;
            std::cout << "sel_dma     = " << signal_sel_dma.read()           << std::endl;
            std::cout << "sel_ioc     = " << signal_sel_ioc.read()           << std::endl;

            std::cout << "  -- pibus signals --" << std::hex << std::endl;
            std::cout << "avalid      = " << signal_pi_avalid.read()         << std::endl;
            std::cout << "read        = " << signal_pi_read.read()           << std::endl;
            std::cout << "lock        = " << signal_pi_lock.read()           << std::endl;
            std::cout << "address     = " << signal_pi_a.read()              << std::endl;
            std::cout << "ack         = " << signal_pi_ack.read()            << std::endl;
            std::cout << "data        = " << signal_pi_d.read()              << std::endl;

            std::cout << "  -- IRQ signals --" << std::dec << std::endl;
            std::cout << "tim_irq[0]  = " << signal_irq_tim[0].read()        << std::endl;
            std::cout << "tty_irq[0]  = " << signal_irq_tty_get[0].read()    << std::endl;
            std::cout << "dma_irq     = " << signal_irq_dma.read()           << std::endl;
            std::cout << "ioc_irq     = " << signal_irq_ioc.read()           << std::endl;
            std::cout << "proc_irq[0] = " << signal_irq_proc[0].read()       << std::endl;
        }
    }
    return EXIT_SUCCESS;

} // end _main

/////////////////////////////////////
int sc_main(int argc, char * argv[]) {
    try {
        return _main(argc, argv);
    }
    catch (std::exception &error) {
        std::cout << error.what() << std::endl;
    }
    return 0;
} // end sc_main()

