/**********************************************************************
* File : tp2_top.cpp
* Date : 15/01/2011
* Author :  Alain Greiner
* UPMC - LIP6
* This program is released under the GNU public license
**********************************************************************
* This architecture contains 5 components :
*  - PibusBcu : PIBUS controler
*  - PibusSimpleRam : boot ROM
*  - PibusSimpleRam : static RAM
*  - PibusMultiTty : TTY controler
*  - PibusMips32Xcache : Mips32 processor 
**********************************************************************/

#include <systemc>

#include "pibus_simple_ram.h"
#include "pibus_mips32_xcache.h"
#include "pibus_multi_tty.h"
#include "pibus_seg_bcu.h"
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"
#include "loader.h"

#include <stdio.h>
#include <stdarg.h>

// segment definition
#define SEG_RESET_BASE  0xBFC00000
#define SEG_RESET_SIZE  4096

#define SEG_KCODE_BASE  0x80000000
#define SEG_KCODE_SIZE  16384

#define SEG_KDATA_BASE  0x82000000
#define SEG_KDATA_SIZE  65536

#define SEG_KUNC_BASE   0x81000000
#define SEG_KUNC_SIZE   4096

#define SEG_DATA_BASE   0x01000000
#define SEG_DATA_SIZE   16384

#define SEG_CODE_BASE   0x00400000
#define SEG_CODE_SIZE   16384

#define SEG_STACK_BASE  0x02000000
#define SEG_STACK_SIZE  16384

#define SEG_TTY_BASE    0x90000000
#define SEG_TTY_SIZE    16

// Taget indexes definition
#define ROM_INDEX	0
#define RAM_INDEX	1
#define TTY_INDEX	2

int _main (int argc, char *argv[])
{
    using namespace sc_core;
    using namespace soclib::common;
    using namespace soclib::caba;

    ///////////////////////////////////////////////////////////////////////////////////
    //   PARAMETERS
    ///////////////////////////////////////////////////////////////////////////////////
    size_t  ncycles             = 1000000000;       	// number of simulated cycles
    char    sys_path[256]       = "soft/sys.bin";  	// pathname for the system binary code
    char    app_path[256]       = "soft/app.bin";  	// pathname for the application binary code
    bool    debug_ok            = false;            	// debug activated
    size_t  from_cycle          = 0;                	// debug start cycle
    size_t  ram_latency		= 0;		    	// ram latency
    bool    snoop_active	= false;	    	// snoop activation
    size_t  icache_ways     	= 1;	// instruction cache number of ways
    size_t  icache_sets     	= 64;	// instruction cache number of sets
    size_t  icache_words    	= 4;	// instruction cache number of words per line
    size_t  dcache_ways     	= 1;	// data cache number of ways
    size_t  dcache_sets     	= 64;	// data cache number of sets
    size_t  dcache_words    	= 4;	// data cache number of words per line
    size_t  wbuf_depth      	= 8;	// write buffer depth
    bool    stats_ok		= false;	    	// statistics activation
    size_t  stats_period	= 0;    	    	// statistics display period 


    std::cout << std::endl;
    std::cout << "********************************************************" << std::endl;
    std::cout << "******        tp2_top                             ******" << std::endl;
    std::cout << "********************************************************" << std::endl;
    std::cout << std::endl;

    if (argc > 1)
    {
        for( int n=1 ; n<argc ; n=n+2 )
        {
            if( (strcmp(argv[n],"-NCYCLES") == 0) && (n+1<argc) )
            {
                ncycles = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-DEBUG") == 0) && (n+1<argc) )
            {
                debug_ok = true;
                from_cycle = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-SYS") == 0) && (n+1<argc) )
            {
                strcpy(sys_path, argv[n+1]) ;
            }
            else if( (strcmp(argv[n],"-APP") == 0) && (n+1<argc) )
            {
                strcpy(app_path, argv[n+1]) ;
            }
            else if( (strcmp(argv[n],"-LATENCY") == 0) && (n+1<argc) )
            {
                ram_latency = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-SNOOP") == 0) && (n+1<argc) )
            {
                snoop_active = (atoi(argv[n+1]) != 0);
            }

            else if( (strcmp(argv[n],"-IWORDS") == 0) && (n+1<argc) )
            {
                icache_words = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-ISETS") == 0) && (n+1<argc) )
            {
                icache_sets = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-IWAYS") == 0) && (n+1<argc) )
            {
                icache_ways = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-DWORDS") == 0) && (n+1<argc) )
            {
                dcache_words = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-DSETS") == 0) && (n+1<argc) )
            {
                dcache_sets = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-DWAYS") == 0) && (n+1<argc) )
            {
                dcache_ways = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-WBUF") == 0) && (n+1<argc) )
            {
                wbuf_depth = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-STATS") == 0) && (n+1<argc) )
            {
                stats_ok = true;
                stats_period = atoi(argv[n+1]);
            }
            else
            {
                std::cout << "   Arguments on the command line are (key,value) couples." << std::endl;
                std::cout << "   The order is not important." << std::endl;
                std::cout << "   Accepted arguments are :" << std::endl << std::endl;
                std::cout << "   -NCYCLES number_of_simulated_cycles" << std::endl;
                std::cout << "   -DEBUG debug_start_cycle" << std::endl;
                std::cout << "   -LATENCY ram_latency_value" << std::endl;
                std::cout << "   -SOFT file_name" << std::endl;
                std::cout << "   -SNOOP non_zero_value_to_activate" << std::endl;
                std::cout << "   -IWORDS number_of_words_per_line" << std::endl;
                std::cout << "   -ISETS number_of_sets" << std::endl;
                std::cout << "   -IWAYS number_of_ways" << std::endl;
                std::cout << "   -DWORDS number_of_words_per_line" << std::endl;
                std::cout << "   -DSETS number_of_sets" << std::endl;
                std::cout << "   -DWAYS number_of_ways" << std::endl;
                std::cout << "   -WBUF write_buffer_depth" << std::endl;
                std::cout << "   -STATS period" << std::endl;
                exit(0);
            }
        }
    }

    ///////////////////////////////////////////////////////
    //	SIGNALS DECLARATION
    //////////////////////////////////////////////////////

    sc_clock				signal_ck("signal_ck");
    sc_signal<bool>			signal_resetn("signal_resetn");
  	
    sc_signal<bool>			signal_req_proc("signal_req_proc");
    sc_signal<bool>			signal_gnt_proc("signal_gnt_proc");
    sc_signal<bool>			signal_sel_rom("signal_sel_rom");
    sc_signal<bool>			signal_sel_ram("signal_sel_ram");
    sc_signal<bool>			signal_sel_tty("signal_sel_tty");
  	
    sc_signal<uint32_t> 		signal_pi_a("signal_pi_a");
    sc_signal<bool>			signal_pi_lock("signal_pi_lock");
    sc_signal<bool>			signal_pi_read("signal_pi_read");
    sc_signal<uint32_t>			signal_pi_opc("signal_pi_opc");
    sc_signal<uint32_t>			signal_pi_d("signal_pi_d");
    sc_signal<uint32_t>			signal_pi_ack("signal_pi_ack");
    sc_signal<bool>			signal_pi_tout("signal_pi_tout");
    sc_signal<bool>			signal_pi_avalid("signal_pi_avalid");
  
    sc_signal<bool>			signal_unused("signal_unused");
    sc_signal<bool>			signal_null("signal_null");
  
    ////////////////////////////////////////////////////
    //	SEGMENT TABLE DEFINITION
    ////////////////////////////////////////////////////
  
    PibusSegmentTable	segtable;

    segtable.setMSBnumber(8);
  
    segtable.addSegment("seg_reset"  , SEG_RESET_BASE , SEG_RESET_SIZE , ROM_INDEX, true);
    segtable.addSegment("seg_kcode"  , SEG_KCODE_BASE , SEG_KCODE_SIZE , RAM_INDEX, true);                           
    segtable.addSegment("seg_kdata"  , SEG_KDATA_BASE , SEG_KDATA_SIZE , RAM_INDEX, true);                                   
    segtable.addSegment("seg_kunc"   , SEG_KUNC_BASE  , SEG_KUNC_SIZE  , RAM_INDEX, false);
    segtable.addSegment("seg_code"   , SEG_CODE_BASE  , SEG_CODE_SIZE  , RAM_INDEX, true);
    segtable.addSegment("seg_data"   , SEG_DATA_BASE  , SEG_DATA_SIZE  , RAM_INDEX, true);
    segtable.addSegment("seg_stack"  , SEG_STACK_BASE , SEG_STACK_SIZE , RAM_INDEX, true);                                    
    segtable.addSegment("seg_tty"    , SEG_TTY_BASE   , SEG_TTY_SIZE   , TTY_INDEX, false);                                    
    /////////////////////////////////////////////////////////
    //	INSTANCIATED  COMPONENTS
    /////////////////////////////////////////////////////////

    soclib::common::Loader	loader("sys.bin","app.bin");	

    PibusSegBcu			bcu	("bcu", segtable, 1 , 3, 100);
    PibusMips32Xcache		proc	("proc", segtable, 0, 
                                          icache_ways, icache_sets, icache_words,
                                          dcache_ways, dcache_sets, dcache_words, 
                                          wbuf_depth, snoop_active);
    PibusSimpleRam		rom	("rom"  , ROM_INDEX, segtable, 0, loader);
    PibusSimpleRam		ram	("ram"  , RAM_INDEX, segtable, ram_latency, loader);
    PibusMultiTty  		tty 	("tty"  , TTY_INDEX, segtable, 1);

    std::cout << std::endl;

    //////////////////////////////////////////////////////////
    //	NET-LIST
    //////////////////////////////////////////////////////////
 
  bcu.p_ck 			(signal_ck);
  bcu.p_resetn			(signal_resetn);
  bcu.p_req[0]			(signal_req_proc);
  bcu.p_gnt[0]			(signal_gnt_proc);
  bcu.p_sel[0]			(signal_sel_rom);
  bcu.p_sel[1]			(signal_sel_ram);
  bcu.p_sel[2]			(signal_sel_tty);
  bcu.p_a 			(signal_pi_a);
  bcu.p_lock			(signal_pi_lock);
  bcu.p_ack			(signal_pi_ack);
  bcu.p_tout			(signal_pi_tout);
  bcu.p_avalid			(signal_pi_avalid);
  
  std::cout << "bcu : connected" << std::endl;
  
  rom.p_ck			(signal_ck);
  rom.p_resetn			(signal_resetn);
  rom.p_sel 			(signal_sel_rom);
  rom.p_a			(signal_pi_a);
  rom.p_read			(signal_pi_read);
  rom.p_opc			(signal_pi_opc);
  rom.p_ack			(signal_pi_ack);
  rom.p_d			(signal_pi_d);
  rom.p_tout			(signal_pi_tout);
   
  std::cout << "rom : connected" << std::endl;
  
  ram.p_ck			(signal_ck);
  ram.p_resetn			(signal_resetn);
  ram.p_sel 			(signal_sel_ram);
  ram.p_a			(signal_pi_a);
  ram.p_read			(signal_pi_read);
  ram.p_opc			(signal_pi_opc);
  ram.p_ack			(signal_pi_ack);
  ram.p_d			(signal_pi_d);
  ram.p_tout			(signal_pi_tout);
   
  std::cout << "ram : connected" << std::endl;
  
  tty.p_ck			(signal_ck);
  tty.p_resetn			(signal_resetn);
  tty.p_sel 			(signal_sel_tty);
  tty.p_a			(signal_pi_a);
  tty.p_read			(signal_pi_read);
  tty.p_opc			(signal_pi_opc);
  tty.p_ack			(signal_pi_ack);
  tty.p_d			(signal_pi_d);
  tty.p_tout			(signal_pi_tout);
  tty.p_irq_put[0]		(signal_unused);
  tty.p_irq_get[0]		(signal_unused);
  
  std::cout << "tty : connected" << std::endl;
  
  proc.p_ck			(signal_ck);  
  proc.p_resetn         	(signal_resetn);  
  proc.p_req			(signal_req_proc);
  proc.p_gnt			(signal_gnt_proc);
  proc.p_lock			(signal_pi_lock);
  proc.p_read			(signal_pi_read);
  proc.p_opc			(signal_pi_opc);
  proc.p_a			(signal_pi_a);
  proc.p_d			(signal_pi_d);
  proc.p_ack			(signal_pi_ack);
  proc.p_tout			(signal_pi_tout);
  proc.p_avalid			(signal_pi_avalid);
  proc.p_irq			(signal_null);

  std::cout << "proc : connected" << std::endl;
  
    //////////////////////////////////////////////
    //     SIMULATION LOOP
    /////////////////////////////////////////////
  
    std::cout << std::endl;

    signal_null = false;

    signal_resetn = false;
    sc_start( sc_time( 1, SC_NS ) );

    signal_resetn = true;
    for ( size_t n = 0 ; n < ncycles ; n++) 
    {
        sc_start( sc_time( 1, SC_NS ) );

        if ( debug_ok && (n >= from_cycle) )
        {
        std::cout << std::dec <<"*******  cycle = " << n << " **************" << std::endl;
        bcu.printTrace();
        proc.printTrace();
        rom.printTrace();
        ram.printTrace();
        tty.printTrace();
        std::cout << "  -- pibus signals -- " << std::endl;
        std::cout << "req     = " << signal_req_proc.read()   << std::endl
                  << "gnt     = " << signal_gnt_proc.read()   << std::endl
                  << "sel_rom = " << signal_sel_rom.read()    << std::endl
                  << "sel_ram = " << signal_sel_ram.read()    << std::endl
                  << "sel_tty = " << signal_sel_tty.read()    << std::endl
                  << "avalid  = " << signal_pi_avalid.read()  << std::endl
                  << "read    = " << signal_pi_read.read()    << std::endl
                  << "lock    = " << signal_pi_lock.read()    << std::endl << std::hex
                  << "address = " << signal_pi_a.read()       << std::endl
                  << "ack     = " << signal_pi_ack.read()     << std::endl
                  << "data    = " << signal_pi_d.read()       << std::endl;
        }

        if ( stats_ok && ((n % stats_period) == 0) ) proc.printStatistics();
    }


    return EXIT_SUCCESS;

}; // end _main()

//////////////////////////////////////
int sc_main( int argc, char* argv[] )
{
    try 
    {
        return _main(argc, argv);
    }
    catch ( std::exception &error)
    {
        std::cout << error.what() << std::endl;
    }
    return 0;
} // end sc_main()
