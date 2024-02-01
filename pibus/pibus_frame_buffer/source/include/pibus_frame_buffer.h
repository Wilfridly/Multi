//////////////////////////////////////////////////////////////////////////
// File : pibus_frame_buffer.h
// Author : Alain Greiner
// Date : 29/01/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
//////////////////////////////////////////////////////////////////////////
// This component implements a PIBUS compliant frame buffer.
// It use the generic SoCLib fb_controler that contains 
// the buffer itself, and supports both read & write accesses.
//////////////////////////////////////////////////////////////////////////
// This component has 7 constructor parameters
// - sc_module_name		name    : instance name
// - unsigned int  		index   : target index      
// - pibusSegmentTable		segmap  : segment table
// - unsigned int		latency	: number of wait cycles
// - unsigned int		width	: number of pixels per line
// - unsigned int		height  : number of lines
// - unsigned int		subsampling : default = 420
//////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_FRAME_BUFFER_H
#define PIBUS_FRAME_BUFFER_H

#include <systemc>
#include <stdio.h>
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"
#include "fb_controller.h"
#include "process_wrapper.h"

namespace soclib { namespace caba {

class PibusFrameBuffer : sc_core::sc_module {

   //  REGISTERS
    sc_register<int>			r_fsm_state;		// FSM state
    sc_register<uint32_t>		r_counter;		// Latency counter
    sc_register<uint32_t>		r_display;		// display counter
    sc_register<size_t>			r_word;			// word index in the frame buffer
    sc_register<int>			r_opc;			// PIBUS codop 

    //  STRUCTURAL PARAMETERS
    const char*				m_name;			// instance name
    const uint32_t			m_tgtid;		// target index
    const uint32_t			m_latency;		// intrinsic latency
    uint32_t				m_segbase;		// segment base address
    uint32_t				m_segsize;   		// segment size
    const char*				m_segname;		// segment name
    soclib::common::FbController        m_fb_controller;	// generic controller
    char				m_fsm_str[6][20];	// FSM states names

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

    SC_HAS_PROCESS(PibusFrameBuffer);

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
    PibusFrameBuffer (sc_core::sc_module_name		name, 			// instance name
		uint32_t				tgtid,			// target index 
		soclib::common::PibusSegmentTable	&segtab,		// segment table
		uint32_t				latency, 		// access latency
		uint32_t				width, 			// frame width
		uint32_t				height, 		// frame height
		int					subsampling = 420); 	// pixel format
    // methods
    void transition();
    void genMoore();
    void printTrace();

#ifdef SOCVIEW
    void registerDebug( SocviewDebugger db );
#endif

};  // end class PibusFrameBuffer

}} // end name spaces

#endif

