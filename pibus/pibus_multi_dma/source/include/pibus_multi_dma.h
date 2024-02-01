////////////////////////////////////////////////////////////////////////////
// File : pibus_multi_dma.h
// Date : 01/02/2012
// author : Alain Greiner 
// This program is released under the GNU public license
// Copyright UPMC - LIP6
////////////////////////////////////////////////////////////////////////////
// This component is a multi-channels DMA controler with a PIBUS interface,
// acting both as a master and a target on the system bus.
//
// The max number of channels is 16.
// Each channel occupies an aligned 4K bytes segment in the address space:
// The total segment size is 4K * NB_CHANNELS.
// All DMA channels share the same master port on the PIBUS, and the
// allocation policy (in case of simultaneous transferts) is round-robin
// with a PIBUS transaction granularity.

// For each channel, this DMA controler contains  5 memory mapped registers
// (only the 5 less significant bits of the VCI address are decoded)
// - SOURCE		(0x00)	Read/Write	Source buffer base address
// - DEST		(0x04)  Read/Write	Destination buffer base address
// - LENGTH/STATUS	(0x08)	Read/Write	Transfer length (bytes) / Status
// - RESET  		(0x0C)	Write Only	Software reset & IRQ acknowledge
// - NOIRQ       	(0x10)	Read/Write	IRQ disabled when non zeo
//
// Both the source and destination address must be word aligned, and
// and the transfer length must be a multiple of 4 bytes.
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
// ignored if the corresponding channel is not IDLE.
// Writing in register RESET stops gracefully an ongoing transfer 
// and forces the corresponding channel in IDLE state.
// The DMA controller asserts an IRQ when the transfer is completed
// (states SUCCESS, READ_ERROR, WRITE_ERROR). The IRQ is not asserted
// if the NOIRQ register contains a non-zero value.
// Writing in the RESET register is the normal way to acknowledge IRQ.
// Each DMA channel contains a private buffer to store a burst.
///////////////////////////////////////////////////////////////////////////
// Implementation note:
// This component contains NB_CHANNELS + 2 FSMs:
// - the CHANNEL_FSM[k] define the global state for channel (k)
// - the TARGET_FSM handles the configuration commands and activate
//   or desactivate the CHANNEL_FSM[k]
// - the MASTER_FSM is a server handling the PIBUS transactions requested
//   by the CHANNEL_FSM[k]
///////////////////////////////////////////////////////////////////////////
// This component has 5 "constructor" parameters :
// - sc_module_name 	name		: instance name
// - unsigned int	tgtid		: target index
// - PibusSegmentTable 	segtab		: segment table
// - unsigned int	burst		: burst length number of bytes
// - unsigned int	channels	: number of channels
////////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_MULTI_DMA_H
#define PIBUS_MULTI_DMA_H

#include <systemc.h>
#include "pibus_mnemonics.h"
#include "pibus_segment_table.h"

namespace soclib { namespace caba {

class PibusMultiDma : sc_module {

    // REGISTERS
    sc_register<int>      	r_target_fsm;		// target fsm state register
    sc_register<uint32_t>       r_target_index;		// selected channel

    sc_register<int>      	r_master_fsm;		// master fsm state register
    sc_register<uint32_t>       r_master_index;		// selected channel
    sc_register<uint32_t>       r_master_count;		// word counter in a burst
    sc_register<uint32_t>       r_master_burst;		// actual burst length (words)

    sc_register<int>*		r_channel_fsm;		// channel fsm state registers [channel]
    sc_register<uint32_t>*     	r_channel_source;	// source buffer base address [channel]
    sc_register<uint32_t>*     	r_channel_dest;  	// destination buffer base address [channel]
    sc_register<uint32_t>*     	r_channel_length;	// number of bytes to be transfered [channel]
    sc_register<bool>*     	r_channel_noirq;	// IRQ disabled [channel]
    sc_register<bool>*     	r_channel_active;	// channel activation [channel]
    sc_register<bool>*     	r_channel_done;		// bus transaction completed [channel]
    sc_register<bool>*     	r_channel_error;	// bus error reported [channel]
    uint32_t**			r_channel_buf;		// local buffer [channels][burst]
    
    // STRUCTURAL PARAMETERS
    const char*			m_name;			// instance name
    const uint32_t		m_tgtid;		// target index
    const uint32_t		m_burst;		// burst max number of words
    const uint32_t		m_channels;		// number of channels
    uint32_t			m_segbase;		// segment base address
    uint32_t			m_segsize;		// segment size
    const char*			m_segname;		// segment name
    char			m_master_str[9][20];	// master FSM states names
    char			m_target_str[11][20];	// target FSM states names
    char			m_channel_str[8][20];	// channel FSM states names

    //  CHANNEL_FSM STATES
    enum {
    CHANNEL_DONE	= 0,
    CHANNEL_READ_ERROR	= 1,
    CHANNEL_IDLE	= 2,
    CHANNEL_WRITE_ERROR	= 3,
    CHANNEL_READ_REQ	= 4,
    CHANNEL_READ_WAIT	= 5,
    CHANNEL_WRITE_REQ 	= 6,
    CHANNEL_WRITE_WAIT	= 7,
    };

    // MASTER FSM STATES
    enum {
    MST_IDLE		= 0,
    MST_READ_REQ	= 1,
    MST_READ_AD		= 2,
    MST_READ_DTAD	= 3,
    MST_READ_DT		= 4,
    MST_WRITE_REQ	= 5,
    MST_WRITE_AD	= 6,
    MST_WRITE_DTAD	= 7,
    MST_WRITE_DT	= 8,
    };
    
    // TARGET FSM STATES
    enum{
    TGT_IDLE		= 0,
    TGT_WRITE_SOURCE	= 1,
    TGT_WRITE_DEST	= 2,
    TGT_WRITE_LENGTH	= 3,
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
    DMA_SRC = 0,
    DMA_DST = 1,
    DMA_LEN = 2,
    DMA_RST = 3,
    DMA_IRQ = 4,

    DMA_SPAN = 8,
    };

protected:

    SC_HAS_PROCESS(PibusMultiDma);

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
    sc_core::sc_out<bool>*		 p_irq;

    // METHODS
    void transition();
    void genMoore();
    void printTrace();

    // Constructor   
    PibusMultiDma(sc_module_name			name, 
                  uint32_t				tgtid,
                  soclib::common::PibusSegmentTable	&segtab,
                  size_t				burst,
                  size_t				channels);

}; // end class PibusMultiDma

}} // end namespace

#endif
