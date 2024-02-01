//////////////////////////////////////////////////////////////////////////// 
// File  : pibus_seg_bcu.h
// Date  : 11/01/2010
// author:  Alain Greiner 
// Copyright  UPMC - LIP6
// This program is released under the GNU public license
///////////////////////////////////////////////////////////////////////////
// This component is a simplified PIBUS controler.
// The Three basic functionnalities are :
// - arbitration between masters requests.
// - selection of the target by decoding the MSB address bits.
// - Time-out when the target does not complete the transaction.
// The simplifications and modifications are :
// - The default master mechanism is not supported.
// - Only four values are supported for the ACK signal:
//   READY, WAIT, ERROR, RETRY.
// - The arbitration policy between masters is round-robin.
// The bus is granted to a new master in the FSM_IDLE state 
// (the bus is not used), and in the FSM_DT state (last cycle 
// of a transaction) when the ACK signal is not PI_ACK-WAT.
// The COUNT_REQ[i] register counts the total number of transaction 
// requests for master i. The COUNT_WAIT[i] register counts the total
// number of wait cycles for master i.
// This component use the Segment Table to build the Target ROM table, 
// that decode the address MSB bits and gives the the selected target 
// index to generate the SEL[i] signals.
//////////////////////////////////////////////////////////////////////////
// This component has 5 "constructor" parameters :
// - sc_module_name	name		: instance name
// - pibusSegmentTable	segtab		: segment table
// - int 		nb_master       : number of PIBUS masters   
// - int 		nb_slave        : number of PIBUS slaves  
// - int 		time_out	: max wait cycles (default = 100)
//////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_BCU_H_
#define PIBUS_BCU_H_

#include <systemc>
#include <inttypes.h>
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"


namespace soclib { namespace caba {

////////////////////////////////////////
class PibusSegBcu : sc_core::sc_module {

	// 	FSM states
	enum fms_state_e 
        {
	FSM_IDLE	= 0,
	FSM_AD		= 1,
	FSM_DTAD	= 2,
	FSM_DT		= 3,
	};

	//	STRUCTURAL PARAMETERS
        const char*			m_name;			// instance name
	const size_t* 			m_target_table;		// MSB to tgtid trancoding ROM
	const size_t 			m_msb_shift;		// 32 - MSB_bits_number
	const size_t 			m_nb_master;		// number of connected masters
	const size_t 			m_nb_target;		// number of connected targets
	const uint32_t 			m_time_out;		// number of cycles before time-out
        char				m_fsm_str[4][20];	// FSM states names

	// 	REGISTERS
	sc_register<int> 		r_fsm_state;		// FSM state
	sc_register<size_t>		r_current_master;	// current master index
	sc_register<uint32_t>		r_tout_counter;		// time-out counter
	sc_register<uint32_t>*		r_req_counter;		// number of requests (per master)
	sc_register<uint32_t>*		r_wait_counter;		// number of wait cycles (per master)

protected:

	SC_HAS_PROCESS(PibusSegBcu);

public:

	//	I/O PORTS
	sc_core::sc_in<bool>  		p_ck;	 
	sc_core::sc_in<bool>  		p_resetn;  
	sc_core::sc_in<bool>*		p_req;
	sc_core::sc_out<bool>*		p_gnt;
	sc_core::sc_out<bool>*		p_sel;
	sc_core::sc_in<uint32_t>	p_a;
	sc_core::sc_in<bool>		p_lock;
	sc_core::sc_in<uint32_t>	p_ack;
	sc_core::sc_out<bool>		p_tout;
	sc_core::sc_out<bool>		p_avalid;

	//	CONSTRUCTOR
	PibusSegBcu (sc_core::sc_module_name 			name,
	             soclib::common::PibusSegmentTable       	&segtab,
		     size_t					nb_master,
		     size_t					nb_slave,
		     uint32_t					time_out = 1000000000);
	~PibusSegBcu();

	// 	METHODS
	void transition(); 
	void genMealy_gnt(); 
	void genMealy_sel();
	void genMoore();
        void printTrace();
        void printStatistics();

#ifdef SOCVIEW
        void registerDebug( SocviewDebugger db );
#endif

}; // end class PibusSegBcu

}} // end namespaces

#endif
