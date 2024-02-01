//////////////////////////////////////////////////////////////////////////
// File : pibus_simple_ram.h
// Author : Alain Greiner
// Date : 29/01/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
//////////////////////////////////////////////////////////////////////////
// This component implements a  multi-segment SRAM, with PIBUS interface.
// It uses the SoCLib loader to load binary code in a given memory segment.
// This component can contain up to 16 segments.
// Each segment is defined by a BASE address and a SIZE
// (the SIZE is a number of bytes)
// Both the BASE and SIZE must be multiple of 4 bytes.
// Each segment is implemented by a table of "int" dynamically
// allocated by the RAM constructor.
// This component checks address for segmentation violation.
// In case of burst, all addresses must be in the same segment,
// and it is forbidden to mix read and write accesses in
// a single burst.
// The number of wait cycles at the beginning of a transaction 
// is a parameter (The value can be 0).
///////////////////////////////////////////////////////////////////////// 
// This component has 4 "generator" parameters
// - sc_module_name		name    : instance name
// - unsigned int  		index   : target index      
// - pibusSegmentTable		segmap  : segment table
// - int			latency	: number of wait cycles
// - soclib::common::Loader	loader  : loader
/////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_SIMPLE_RAM_H
#define PIBUS_SIMPLE_RAM_H

#include <systemc>
#include <stdio.h>
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"
#include "loader.h"

#define MAXSEG 	16

namespace soclib { namespace caba {

class PibusSimpleRam : sc_core::sc_module {

   //  REGISTERS
    sc_register<int>		r_fsm_state;		// FSM state
    sc_register<uint32_t>	r_counter;		// Latency counter
    sc_register<size_t>		r_index;		// Selected segment index
    sc_register<uint32_t>	r_address;		// PIBUS address
    sc_register<int>		r_opc;			// PIBUS codop 
    uint32_t*			r_buf[MAXSEG];		// segment buffers

    //  STRUCTURAL PARAMETERS
    const char*			m_name;			// instance name
    const uint32_t    	 	m_tgtid;		// target index
    size_t      		m_nbseg;		// segment number
    uint32_t    		m_segsize[MAXSEG];	// segment sizes
    uint32_t   			m_segbase[MAXSEG];	// segment bases
    const char*			m_segname[MAXSEG];	// segment names
    const uint32_t		m_latency;		// intrinsic latency
    soclib::common::Loader	m_loader;		// loader
    char			m_fsm_str[6][20];	// FSM states names
    bool			m_monitor_ok;		// monitor activated
    uint32_t			m_monitor_base;		// monitored segment base
    uint32_t			m_monitor_length; 	// monitored segment length

    // FSM states
    enum {
	FSM_IDLE	= 0,
	FSM_READ_WAIT	= 1,
	FSM_READ_OK	= 2,
	FSM_WRITE_WAIT	= 3,
	FSM_WRITE_OK	= 4,
	FSM_ERROR	= 5
    };

protected:

    SC_HAS_PROCESS(PibusSimpleRam);

public:

    // IO PORTS
    sc_core::sc_in<bool> 		p_ck;
    sc_core::sc_in<bool> 		p_resetn;
    sc_core::sc_in<bool>		p_sel;
    sc_core::sc_in<uint32_t>		p_a;
    sc_core::sc_in<bool>		p_read;
    sc_core::sc_in<uint32_t>		p_opc;
    sc_core::sc_out<uint32_t>		p_ack;
    sc_core::sc_inout<uint32_t>		p_d;
    sc_core::sc_in<bool>		p_tout;

    // constructor
    PibusSimpleRam (sc_core::sc_module_name		name,
		uint32_t	         		tgtid,
		soclib::common::PibusSegmentTable	&segtab,
		uint32_t				latency, 	
                const soclib::common::Loader  		&loader = soclib::common::Loader() );
    // methods
    void transition();
    void genMoore();
    void printTrace(uint32_t address = 0);
    void startMonitor(uint32_t base, uint32_t length);
    void stopMonitor();

};  // end class PibusSimpleRam

}} // end name spaces

#endif

