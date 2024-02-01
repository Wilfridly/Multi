///////////////////////////////////////////////////////////////////////////////
// File : pibus_multi_timer.h
// Date : 05/03/2010
// Author : A.Greiner 
// It is released under the GNU Public License.
// Copyright : UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////
// This component is a generic timer : It contains up to 32 independant
// software controled timers.
// The timer index [i] is defined by the 5 bits ADDRESS[8:4].
// The TIMER_COUNT[i] registers used to generate periodic interrupts 
// are not directly addressables.
//
// Each timer defines 4 memory mapped registers :
// - TIMER_VALUE[i]	(0x0) 	(read/write)
// A read request returns the value contained in TIMER_VALUE[i].
// A write request sets a new value in TIMER_VALUE[i].
// - TIMER_RUNNING[i] 	(0x4)	(read/write)
// A write request of a zero value gives a false value to this register.
// A write request of a non-zero value gives a true value to this register.
// When this Boolean is true, the TIMER_COUNT[i] register is 
// decremented and interrupt IRQ[i] is enabled.
// - TIMER_PERIOD[i]	(0x8)	(read/write)
// A write request writes a new value in the TIMER_PERIOD[i] register, 
// and the new value is also  written in TIMER_COUNT[i]. 
// The TIMER_RUNNING[i] register is set to false.  
// A read request returns the value contained in the TIMER_PERIOD[i] register. 
// - TIMER_IRQ[i]	(0xc)	(read/write)
// A write request resets the TIMER_IRQ[i] register to false.
// A read request returns the 0 value if TIMER_IRQ[i] is false.
//
// This component cheks address for segmentation violation,
// and can be used as a default target.
///////////////////////////////////////////////////////////////////////////////
// This component has 4 "constructor" parameters :
// - sc_module_name	name		: instance name
// - uint32_t 		tgtid		: target index.
// - PibusSegmentTable 	segtab		: segment table.
// - uint32_t 		ntimer		: number of independant timers 
////////////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_MULTI_TIMER_H
#define PIBUS_MULTI_TIMER_H

#include <systemc>
#include "pibus_mnemonics.h"
#include "pibus_segment_table.h"

namespace soclib { namespace caba {

class PibusMultiTimer : sc_core::sc_module {

    // Structural parameters
    const char*                 m_name;                 // instance name
    size_t                      m_tgtid;                // target index
    size_t                      m_ntimer;               // number of timers
    uint32_t                    m_segbase;              // segment base address
    uint32_t                    m_segsize;              // segment size
    const char*                 m_segname;              // segment name
    char			m_fsm_str[4][20];	// FSM states names 

    //	Registers
    sc_register<int>		r_fsm_state;
    sc_register<uint32_t>	*r_value;
    sc_register<uint32_t>	*r_period;
    sc_register<uint32_t>	*r_counter;
    sc_register<bool>		*r_running;
    sc_register<bool>		*r_irq;
    sc_register<uint32_t>	r_index;
    sc_register<uint32_t>	r_cell;

    //	FSM states
    enum{
    FSM_IDLE     	= 0,
    FSM_READ     	= 1,	
    FSM_WRITE    	= 2,
    FSM_ERROR		= 3,
    };

    //	register mapping
    enum{
    VALUE_ADDRESS	= 0,
    RUNNING_ADDRESS	= 4,
    PERIOD_ADDRESS	= 8,
    IRQ_ADDRESS  	= 12, 
    };

protected:

    SC_HAS_PROCESS(PibusMultiTimer);

public:

    //	I/O Ports
    sc_core::sc_in<bool>                 p_ck;
    sc_core::sc_in<bool>                 p_resetn;
    sc_core::sc_in<bool>                 p_sel;
    sc_core::sc_in<uint32_t>             p_a;
    sc_core::sc_in<bool>                 p_read;
    sc_core::sc_in<uint32_t>             p_opc;
    sc_core::sc_out<uint32_t>            p_ack;
    sc_core::sc_inout<uint32_t>          p_d;
    sc_core::sc_in<bool>                 p_tout;
    sc_core::sc_out<bool>*               p_irq;

    //	Constructor
    PibusMultiTimer(sc_core::sc_module_name		name,	
		    uint32_t				tgtid,	
		    soclib::common::PibusSegmentTable	&segtab,
	            uint32_t				ntimer);

    //  Methods 
    void transition();
    void genMoore();
    void printTrace();

}; // end class PibusMultiTimer

}} // end namespace

#endif

