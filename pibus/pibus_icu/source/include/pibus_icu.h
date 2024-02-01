//////////////////////////////////////////////////////////////////////////
// File : pibus_icu.h
// Author : A.Greiner 
// Date : 10/04/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////////
// This component implements a vectorised interrupt controler and router,
// as a PIBUS target. It concentrates up to 32 input interrupt requests
// IRQ_IN[NIRQ] and controls up to 8 output interrupt signals IRQ_OUT[NPROC].
// The NIRQ parameter defines the number of input IRQs.
// The NPROC parameter defines the number of output IRQs.
// This component emulates NPROC independant single output ICUs.
// Each OUT_IRQ[i] is the logical OR of the 32 inputs IN_IRQ[k], with
// a specific 32 bits MASK[i] depending on the output IRQ.
// These 32 bits MASK registers allow the software to route the input IRQs
// to the proper output IRQ, i.e. to the proper processor. 
// IN_IRQ[i] is enabled when the corresponding mask bit is set to 1.
//
// This component takes 32 * NPROC bytes in the address space.
// Each single output ICU is seen as 5  memory mapped registers :
// - ICU_INT 	   	(0x00)	(Read-Only)   returns the the 32 input IRQs.
// - ICU_MASK 	  	(0x04)	(Read-Only)   returns the current mask value.
// - ICU_MASK_SET 	(0x08)	(Write-Only)  mask <= mask | wdata.
// - ICU_MASK_RESET	(0x0C)	(Write-Only)  mask <= mask & ~wdata.
// - ICU_IT_VECTOR	(0x10)	(Read-Only)   index of the smallest active IRQ.
// 	(if there is no active IRQ, the returned value is 32).
// 
// This component cheks address for segmentation violation,
// and can be used as a default target.
//////////////////////////////////////////////////////////////////////////////////
// This component has 5 "generator" parameters :
// - sc_module_name	name    : instance name
// - uint32_t		tgtid   : target index
// - pibusSegmentTable	segmap  : segment table
// - uint32_t		nirq	: number of input interrupt lines
// - uint32_t		nproc	: number of output interrupt lines (default = 1)
//////////////////////////////////////////////////////////////////////////////////

#ifndef PI_ICU_H
#define PI_ICU_H

#include <systemc.h>
#include "pibus_mnemonics.h"
#include "pibus_segment_table.h"

namespace soclib { namespace caba {

class PibusIcu : sc_module {

    // Structural parameters
    const char*                 m_name;                 // instance name
    size_t                      m_tgtid;                // target index
    size_t                      m_nirq;                 // number of input IRQs
    size_t                      m_nproc;                // number of output IRQs
    uint32_t                    m_segbase;              // segment base address
    uint32_t                    m_segsize;              // segment size
    const char*                 m_segname;              // segment name
    char			m_fsm_str[7][20];	// FSM states names

    // 	REGISTERS
    sc_register<uint32_t>	r_index;		// index of the selected output
    sc_register<uint32_t> 	r_mask[8];		// interrupt masks for the 8 outputs
    sc_register<int>		r_fsm_state;		// FSM State

    // FSM states
    enum {
    FSM_IDLE        	= 0x0,
    FSM_READ_VECTOR	= 0x1,
    FSM_READ_IRQS	= 0x2,
    FSM_READ_MASK	= 0x3,
    FSM_SET_MASK	= 0x4,
    FSM_RESET_MASK	= 0x5,
    FSM_ERROR         	= 0x6,
    };

    // Registers mapping
    enum {
    ICU_INT       	= 0x0,				// read only
    ICU_MASK          	= 0x4,				// read only
    ICU_MASK_SET      	= 0x8,				// write only
    ICU_MASK_CLEAR    	= 0xC,				// write only
    ICU_IT_VECTOR    	= 0x10,				// read_only
    };

protected:

    SC_HAS_PROCESS(PibusIcu);

public:

    //  I/O Ports
    sc_core::sc_in<bool>        p_ck;
    sc_core::sc_in<bool>        p_resetn;
    sc_core::sc_in<bool>        p_sel;
    sc_core::sc_in<uint32_t>    p_a;
    sc_core::sc_in<bool>        p_read;
    sc_core::sc_in<uint32_t>    p_opc;
    sc_core::sc_out<uint32_t>   p_ack;
    sc_core::sc_inout<uint32_t> p_d;
    sc_core::sc_in<bool>        p_tout;
    sc_core::sc_in<bool>*       p_irq_in;
    sc_core::sc_out<bool>*	p_irq_out;

    // constructor
    PibusIcu (sc_core::sc_module_name		insname, 
	      size_t				tgtid, 
	      soclib::common::PibusSegmentTable	&segtab,
	      size_t				nirq,
	      size_t				nproc = 1);

    // Methods
    void transition();
    void genMoore();
    void genMealy();
    void printTrace();

}; // end PibusIcu

}} // end namespace

#endif

