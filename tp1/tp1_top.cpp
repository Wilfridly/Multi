/**********************************************************************
* File : tp1_archi.cpp
* Date : 20/01/2011
* Author :  Alain Greiner
* UPMC - LIP6
* This program is released under the GNU public license
**********************************************************************
* This architecture contains 4 components :
*  - PibusSegBcu : PIBUS controler
*  - PibusSimpleRam : static RAM
*  - PibusMultiTty : TTY controler
*  - PibusSimpleMaster : wired FSM
**********************************************************************/

#include <systemc>

#include "pibus_simple_ram.h"
#include "pibus_simple_master.h"
#include "pibus_multi_tty.h"
#include "pibus_seg_bcu.h"
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"

#include <stdio.h>
#include <stdarg.h>

#define SEG_RAM_BASE	0x10000000
#define SEG_TTY_BASE	0xC0000000

int _main (int argc, char *argv[])
{
  using namespace sc_core;
  using namespace soclib::caba;
  using namespace soclib::common;

///////////////////////////////////////////////////////////////
//  Command line arguments
///////////////////////////////////////////////////////////////
    size_t  ncycles             = 1000000000;       // simulated cycles
    bool    trace_ok            = false;            // trace activated
    size_t  from_cycle          = 0;                // trace start cycle
    size_t  ram_latency		= 2;		    // ram  latency	

    std::cout << std::endl;
    std::cout << "********************************************************" << std::endl;
    std::cout << "******        tp1_multi                           ******" << std::endl;
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
            else if( (strcmp(argv[n],"-TRACE") == 0) && (n+1<argc) )
            {
                trace_ok = true;
                from_cycle = atoi(argv[n+1]);
            }
            else if( (strcmp(argv[n],"-LATENCY") == 0) && (n+1<argc) )
            {
                ram_latency = atoi(argv[n+1]);
            }
            else
            {
                std::cout << "   Arguments on the command line are (key,value) couples." << std::endl;
                std::cout << "   The order is not important." << std::endl;
                std::cout << "   Accepted arguments are :" << std::endl << std::endl;
                std::cout << "   -NCYCLES number_of_simulated_cycles" << std::endl;
                std::cout << "   -TRACE trace_start_cycle" << std::endl;
                std::cout << "   -LATENCY ram_latency_number_of_cycles" << std::endl;
                exit(0);

            }
        }
    }
                                                                             
///////////////////////////////////////////////////////
//	SIGNALS DECLARATION
//////////////////////////////////////////////////////

  sc_clock			signal_ck("ck");
  sc_signal<bool>		signal_resetn("resetn");
  	
  sc_signal<bool>		signal_req_master("req_master");
  sc_signal<bool>		signal_gnt_master("gnt_master");
  sc_signal<bool>		signal_sel_ram("sel_ram");
  sc_signal<bool>		signal_sel_tty("sel_tty");
	
  sc_signal<uint32_t> 		signal_pi_a("pi_a");
  sc_signal<bool>		signal_pi_lock("pi_lock");
  sc_signal<bool>		signal_pi_read("pi_read");
  sc_signal<uint32_t>		signal_pi_opc("pi_opc");
  sc_signal<uint32_t>		signal_pi_d("pi_d");
  sc_signal<uint32_t>		signal_pi_ack("pi_ack");
  sc_signal<bool>		signal_pi_tout("pi_tout");
  sc_signal<bool>		signal_pi_avalid("pi_avalid");
  
  sc_signal<bool>		signal_irq("irq");
  
////////////////////////////////////////////////////
//	SEGMENT TABLE DEFINITION
////////////////////////////////////////////////////
  
  PibusSegmentTable	segtable;

  segtable.setMSBnumber(4);

  segtable.addSegment("seg_ram" , SEG_RAM_BASE, 0x00000010, 0, false);
 
// TO BE COMPLETED : segment associated to the TTY
  segtable.addSegment("seg_tty", SEG_TTY_BASE, 0x00000010, 1, false);
/////////////////////////////////////////////////////////
//	INSTANCIATED  COMPONENTS
/////////////////////////////////////////////////////////

  Loader			loader	("string_file@0x10000000:D");

  PibusSegBcu			bcu	("bcu", segtable, 1 , 2, 100);
  PibusSimpleMaster		master	("master", SEG_RAM_BASE, SEG_TTY_BASE); //TO BE COMPLETED
  PibusSimpleRam		ram	("ram"  , 0, segtable, ram_latency, loader); //TO BE COMPLETED
  PibusMultiTty  		tty 	("tty"  ,  1, segtable, 1);

  std::cout << std::endl;

//////////////////////////////////////////////////////////
//	Net-List
//////////////////////////////////////////////////////////
 
  bcu.p_ck 			(signal_ck);
  bcu.p_resetn			(signal_resetn);
  bcu.p_req[0]			(signal_req_master);
  bcu.p_gnt[0]			(signal_gnt_master);
  bcu.p_sel[0]			(signal_sel_ram);
  bcu.p_sel[1]			(signal_sel_tty);
  bcu.p_a 			(signal_pi_a);
  bcu.p_lock			(signal_pi_lock);
  bcu.p_ack			(signal_pi_ack);
  bcu.p_tout			(signal_pi_tout);
  bcu.p_avalid			(signal_pi_avalid);
  
  tty.p_ck			(signal_ck);
  tty.p_resetn			(signal_resetn);
  tty.p_sel 			(signal_sel_tty);
  tty.p_a			(signal_pi_a);
  tty.p_read			(signal_pi_read);
  tty.p_opc			(signal_pi_opc);
  tty.p_ack			(signal_pi_ack);
  tty.p_d			(signal_pi_d);
  tty.p_tout			(signal_pi_tout);
  tty.p_irq_put[0]		(signal_irq);
  tty.p_irq_get[0]		(signal_irq);
  
TO BE COMPLETED : connect the ram (PibusSimpleRam) & master (PibusSimpleMaster) components
  
//////////////////////////////////////////////
//     simulation loop
/////////////////////////////////////////////
  
  signal_resetn = false;
  sc_start( sc_time( 1, SC_NS ) );

  signal_resetn = true;
  for ( size_t n = 0 ; n < ncycles ; n++) 
  {
      sc_start( sc_time( 1, SC_NS ) );

      if ( trace_ok && (n > from_cycle) )
      {
          std::cout << std::dec <<"*******  cycle = " << n << " *******" << std::endl;
          bcu.printTrace();
          master.printTrace();
          ram.printTrace();
          tty.printTrace();
          std::cout << "req     = " << signal_req_master.read() << std::endl
                    << "gnt     = " << signal_gnt_master.read() << std::endl
                    << "sel_ram = " << signal_sel_ram.read()    << std::endl
                    << "sel_tty = " << signal_sel_tty.read()    << std::endl
                    << "avalid  = " << signal_pi_avalid.read()  << std::endl
                    << "read    = " << signal_pi_read.read()    << std::endl
                    << "lock    = " << signal_pi_lock.read()    << std::endl << std::hex
                    << "address = " << signal_pi_a.read()       << std::endl
                    << "ack     = " << signal_pi_ack.read()     << std::endl
                    << "data    = " << signal_pi_d.read()       << std::endl;
       }
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

