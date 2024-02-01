////////////////////////////////////////////////////////////////////////////
// File : pibus_dma.h
// Date : 28/03/2010
// author : Alain Greiner 
// This program is released under the GNU public license
// Copyright UPMC - LIP6
////////////////////////////////////////////////////////////////////////////
// This component is a DMA controler with a PIBUS interface,
// acting both as a master and a target on the system bus.
//
// As a target the DMA controler contains  5 memory mapped registers,
// taking 32 bytes in the address space.
// (only the 5 less significant bits of the VCI address are decoded)
// - SOURCE		(0x00)	Read/Write	Source buffer base address
// - DEST		(0x04)  Read/Write	Destination buffer base address
// - LENGTH/STATUS	(0x08)	Read/Write	Transfer length (bytes) / Status
// - RESET  		(0x0C)	Write Only	Software reset & IRQ acknowledge
// - IRQ_DISABLED	(0x10)	Read/Write	IRQ disabled when non zeo
//
// A write access to register LENGTH/STATUS starts the DMA transfer, 
// with the addresses contained in registers SOURCE and DEST,
// A read access to register LENGTH/STATUS will return the DMA status,
// that is actually the master FSM state.
// The relevant values for the status are :
// - DMA_SUCCESS 	(0)
// - DMA_READ_ERROR 	(1)
// - DMA_WRITE_ERROR 	(2)
// - DMA_IDLE 		(3)	
// - DMA_RUNNING	(>3)
//
// The source and destination addresses must be word aligned,
// and the transfer length must be a multiple of 4 bytes.
// 
// A write access to the SOURCE, DEST or NWORDS registers is
// ignored if the DMA master FSM is not IDLE.
// Writing in register RESET stops gracefully an ongoing transfer 
// and forces the master FSM in IDLE state.
// The DMA controller asserts an IRQ when the transfer is completed
// (states SUCCESS, READ_ERROR, WRITE_ERROR). The IRQ is not asserted
// if the IRQ_DISABLED register contains a non-zero value.
// Writing in the RESET register is the normal way to acknowledge IRQ.
// The initiator FSM uses an internal buffer to store a burst.
///////////////////////////////////////////////////////////////////////////
// This component has 4 "constructor" parameters :
// - sc_module_name 	name	: instance name
// - unsigned int	tgtid	: target index
// - PibusSegmentTable 	segtab	: segment table
// - unsigned int	burst	: max number of words per burst
////////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_DMA_H
#define PIBUS_DMA_H

#include <systemc.h>
#include "pibus_mnemonics.h"
#include "pibus_segment_table.h"

namespace soclib { namespace caba {

class PibusDma : sc_module {

    // REGISTERS
    sc_register<int>      	r_target_fsm;		// target fsm state register
    sc_register<int>      	r_master_fsm;		// master fsm state register
    sc_register<uint32_t>      	r_source;		// source buffer base address
    sc_register<uint32_t>      	r_dest;  		// destination buffer base address
    sc_register<uint32_t>      	r_nwords;		// number of words to be transfered
    sc_register<uint32_t>      	r_irq_disable;		// default value is 0 (IRQ enable)
    sc_register<bool>      	r_stop;   		// soft reset request
    sc_register<uint32_t>	r_read_ptr;		// pointer on the read buffer
    sc_register<uint32_t>	r_write_ptr;		// pointer on the write buffer
    sc_register<uint32_t>	r_index;		// pointer on the local buffer
    sc_register<uint32_t>	r_max;			// max number of words in a burst
    sc_register<uint32_t>	r_count;		// number of words to be transfered
   
    uint32_t*			m_buf;			// local buffer

    // STRUCTURAL PARAMETERS
    const char*			m_name;			// instance name
    const uint32_t		m_tgtid;		// target index
    const uint32_t		m_burst;		// burst max number of words
    uint32_t			m_segbase;		// segment base address
    uint32_t			m_segsize;		// segment size
    const char*			m_segname;		// segment name
    char			m_master_str[12][20];	// master FSM states names
    char			m_target_str[11][20];	// target FSM states names

    //  MASTER_FSM STATES
    enum {
    DMA_SUCCESS		= 0,
    DMA_READ_ERROR	= 1,
    DMA_WRITE_ERROR	= 2,
    DMA_IDLE		= 3,
    DMA_READ_REQ	= 4,
    DMA_READ_AD		= 5,
    DMA_READ_DTAD	= 6,
    DMA_READ_DT		= 7,
    DMA_WRITE_REQ	= 8,
    DMA_WRITE_AD	= 9,
    DMA_WRITE_DTAD	= 10,
    DMA_WRITE_DT	= 11,
    };

    // TARGET FSM STATES
    enum{
    TGT_IDLE		= 0,
    TGT_WRITE_SOURCE	= 1,
    TGT_WRITE_DEST	= 2,
    TGT_WRITE_NWORDS	= 3,
    TGT_WRITE_RESET	= 4,
    TGT_WRITE_NOIRQ	= 5,
    TGT_READ_SOURCE	= 6,
    TGT_READ_DEST  	= 7,
    TGT_READ_STATUS	= 8,
    TGT_READ_NOIRQ	= 9,
    TGT_ERROR		= 10,
    };

    // Addressable registers map
    enum {
    DMA_SRC,
    DMA_DST,
    DMA_LEN,
    DMA_RST,
    DMA_IRQ,
    };

protected:

    SC_HAS_PROCESS(PibusDma);

public:

    // IO PORTS
    sc_core::sc_in<bool>                 p_ck;
    sc_core::sc_in<bool>                 p_resetn;
    sc_core::sc_out<bool>                p_req;
    sc_core::sc_in<bool>                 p_gnt;
    sc_core::sc_in<bool>                 p_sel;
    sc_core::sc_inout<uint32_t>          p_a;
    sc_core::sc_inout<bool>              p_read;
    sc_core::sc_inout<uint32_t>          p_opc;
    sc_core::sc_out<bool>                p_lock;
    sc_core::sc_inout<uint32_t>          p_ack;
    sc_core::sc_inout<uint32_t>          p_d;
    sc_core::sc_in<bool>                 p_tout;
    sc_core::sc_out<bool>		 p_irq;

    // METHODS
    void transition();
    void genMoore();
    void printTrace();

    // Constructor   
    PibusDma(sc_module_name			name, 
             uint32_t				tgtid,
             soclib::common::PibusSegmentTable	&segtab,
             uint32_t				burst);

}; // end class PibusDma

}} // end namespace

#endif
