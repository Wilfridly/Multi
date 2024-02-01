//////////////////////////////////////////////////////////////////////////
// File : pibus_locks.h
// Author : A.Greiner 
// Date : 08/04/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
//////////////////////////////////////////////////////////////////////////
// This component implements a PIBUS peripheral for binary locks : 
// - All read requests are considered as "set" : 
// The read value is returned, and the memory cell is set to 1.
// - All write requests are considered as "reset" : 
// the memory cell is set to 0.
// Each binary lock corresponds to 4 bytes in the adress space.
// This component contains a single segment defined by 
// a BASE address and a SIZE.
// Both the BASE and SIZE must be multiple of 4 bytes.
// This component cheks address for segmentation violation,
// and can be used as a default target.
/////////////////////////////////////////////////////////////////////////
// This component has 4 "generator" parameters :
// - sc_module_name	name    : instance name
// - uint32_t	index   : target index
// - pibusSegmentTable	segmap  : segment table
// - uint32_t	nlocks	: number of locks
/////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_LOCKS_H
#define PIBUS_LOCKS_H

#include <systemc.h>
#include "pibus_mnemonics.h"
#include "pibus_segment_table.h"

namespace soclib { namespace caba {

class PibusLocks : sc_core::sc_module {

    //  REGISTERS
    sc_register<int>		r_fsm_state;
    sc_register<uint32_t>      	r_index;
    bool*			r_locks;

    //  STRUCTURAL PARAMETERS
    const char*			m_name;			// instance name
    const uint32_t		m_tgtid;		// target index
    uint32_t			m_segsize;		// segment size
    uint32_t			m_segbase;		// segment base
    const char*			m_segname;		// segment name
    uint32_t			m_nlocks;		// number of locks
    char			m_fsm_str[4][20];	// FSM states names

    // FSM states
    enum{
        LOCKS_IDLE    = 0,
        LOCKS_READ    = 1,
        LOCKS_WRITE   = 2,
        LOCKS_ERROR   = 3,
        };

protected:

    SC_HAS_PROCESS(PibusLocks);

public:

    // IO PORTS
    sc_core::sc_in<bool>		p_ck;
    sc_core::sc_in<bool>		p_resetn;
    sc_core::sc_in<bool>		p_sel;
    sc_core::sc_in<uint32_t>		p_a;
    sc_core::sc_in<bool>		p_read; 
    sc_core::sc_in<uint32_t>		p_opc;
    sc_core::sc_out<uint32_t>		p_ack;
    sc_core::sc_inout<uint32_t>		p_d;
    sc_core::sc_in<bool>    		p_tout;

    //	constructor
    PibusLocks(sc_core::sc_module_name		name, 
  	     size_t 				tgtid,	
	     soclib::common::PibusSegmentTable	&segtab,	
  	     uint32_t				nlocks);

    //	methods
    void transition();
    void genMoore();
    void printTrace();

};  // end class PibusLocks

}} // end name spaces

#endif

