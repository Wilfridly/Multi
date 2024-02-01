/////////////////////////////////////////////////////////////////////////////////////
// File : pibus_block_device.h
// Date : 22/03/2010
// author : Alain Greiner 
// This program is released under the GNU public license
// Copyright UPMC - LIP6
//////////////////////////////////////////////////////////////////////////////////////
// This component is a simplified disk controller with a PIBUS interface.
// This component can perform data transfers between one single file belonging 
// to the host system and a buffer in the memory of the virtual system. 
// The file name is an argument of the constructor, 
// as well as the block size (bytes), and the access latency (cycles). 
// This component has a DMA capability, and is both a target and an initiator.
// Both read and write transfers are supported. An IRQ is optionally
// asserted when the transfer is completed. 
//
// As a target this block device controler contains 8 memory mapped registers,
// taking 32 bytes in the address space.
// - BLOCK_DEVICE_BUFFER        0x00 (read/write)    Memory buffer base address.
// - BLOCK_DEVICE_COUNT         0x04 (read/write)    Number of blocks to be transfered.
// - BLOCK_DEVICE_LBA           0x08 (read/write)    Index of first block in the file.
// - BLOCK_DEVICE_OP            0x0C (write-only)    Writing here starts the operation.
// - BLOCK_DEVICE_STATUS        0x10 (read-only)     Block Device status.
// - BLOCK_DEVICE_IRQ_ENABLE    0x14 (read/write)    IRQ enabled if non zero.
// - BLOCK_DEVICE_SIZE          0x18 (read-only)     Number of addressable blocks.
// - BLOCK_DEVICE_BLOCK_SIZE    0x1C (read_only)     Block size in bytes.
//
// The following operations codes are supported: 
// - BLOCK_DEVICE_NOOP          No operation
// - BLOCK_DEVICE_READ          From block device to memory
// - BLOCK_DEVICE_WRITE         From memory to block device 
//
// The BLOCK_DEVICE_STATUS is actually defined by the master FSM state.
// The following values are defined for device status:
// -BLOCK_DEVICE_IDLE 		0
// -BLOCK_DEVICE_BUSY 		1
// -BLOCK_DEVICE_READ_SUCCESS 	2
// -BLOCK_DEVICE_WRITE_SUCCESS	3
// -BLOCK_DEVICE_READ_ERROR	4
// -BLOCK_DEVICE_WRITE_ERROR	5
//
// In the 4 states READ_ERROR, READ_SUCCESS, WRITE_ERROR, WRITE_SUCCESS,
// the IRQ is asserted (if it is enabled).
// A read access to the BLOCK_DEVICE_STATUS in these 4 states reset 
// the master FSM state to IDLE, and acknowledge the IRQ.
// Any write access to registers BUFFER, COUNT, LBA, OP is ignored
// if the device is not IDLE.
///////////////////////////////////////////////////////////////////////////
// This component has 6 "constructor" parameters :
// - sc_module_name 	name	    : instance name
// - unsigned int	tgtid	    : target index
// - PibusSegmentTable 	segtab	    : segment table
// - string             file_name   : file name on the host processor
// - unsigned int	block_size  : number of bytes (128/256/512/1024)
// - unsigned int	latency	    : access latency
////////////////////////////////////////////////////////////////////////////

#ifndef SOCLIB_VCI_BLOCK_DEVICE_H
#define SOCLIB_VCI_BLOCK_DEVICE_H

#include <systemc.h>
#include "pibus_mnemonics.h"
#include "pibus_segment_table.h"

namespace soclib { namespace caba {

class PibusBlockDevice : sc_module {

    // REGISTERS
    sc_register<int>      	r_target_fsm;	// target fsm state register
    sc_register<int>      	r_master_fsm;	// master fsm state register
    sc_register<bool>      	r_irq_enable;	// default value is true
    sc_register<uint32_t>       r_nblocks;	// number of blocks to be transfered
    sc_register<uint32_t> 	r_buf_address;	// memory buffer address
    sc_register<uint32_t> 	r_lba;		// first block index
    sc_register<bool>       	r_read;         // requested operation
    sc_register<uint32_t> 	r_word_count;	// byte counter (in a block)
    sc_register<uint32_t> 	r_block_count;	// block counter (in a transfer)
    sc_register<bool> 		r_go;         	// transmit command from T_FSM to M_FSM
    sc_register<uint32_t>	r_latency_count;// latency access (for each block)
   
    uint32_t*			m_local_buffer;	// capacity is one block 

    // STRUCTURAL PARAMETERS
    const char*		        m_name;		// instance name
    const uint32_t	        m_tgtid;	// target index
    const uint32_t	        m_latency;      // device latency
    uint32_t		        m_segbase;	// segment base address
    uint32_t		        m_segsize;	// segment size
    const char*		        m_segname;	// segment name
    int                        	m_fd;           // File descriptor
    uint64_t                   	m_device_size;  // Total number of blocks
    const uint32_t	        m_block_size;   // number of bytes in a block

    char	                m_master_str[17][20];	// master FSM states names
    char	                m_target_str[14][20];	// target FSM states names

    //  MASTER_FSM STATES
    enum {
    M_IDLE		= 0,
    M_READ_BLOCK	= 1,
    M_READ_REQ		= 2,
    M_READ_AD		= 3,
    M_READ_DTAD		= 4,
    M_READ_DT		= 5,
    M_READ_SUCCESS 	= 6,
    M_READ_ERROR	= 7,
    M_WRITE_REQ		= 8,
    M_WRITE_AD		= 9,
    M_WRITE_DTAD	= 10,
    M_WRITE_DT		= 11,
    M_WRITE_BLOCK	= 12,
    M_WRITE_SUCCESS	= 13,
    M_WRITE_ERROR	= 14,
    M_READ_TEST		= 15,
    M_WRITE_TEST 	= 16,
    };

    // TARGET FSM STATES
    enum {
    T_IDLE		= 0,
    T_WRITE_BUFFER	= 1,
    T_READ_BUFFER	= 2,
    T_WRITE_COUNT	= 3,
    T_READ_COUNT	= 4,
    T_WRITE_LBA		= 5,
    T_READ_LBA		= 6,
    T_WRITE_OP    	= 7,
    T_READ_STATUS	= 8,
    T_WRITE_IRQEN 	= 9,
    T_READ_IRQEN 	= 10,
    T_READ_SIZE 	= 11,
    T_READ_BLOCK 	= 12,
    T_ERROR		= 13,
    };

    // Addressable registers map
    enum {
    BLOCK_DEVICE_BUFFER = 0,
    BLOCK_DEVICE_LBA	= 1,
    BLOCK_DEVICE_COUNT	= 2,
    BLOCK_DEVICE_OP	= 3,
    BLOCK_DEVICE_STATUS	= 4,
    BLOCK_DEVICE_IRQEN	= 5,
    BLOCK_DEVICE_SIZE	= 6,
    BLOCK_DEVICE_BLOCK	= 7,
    };

    // Status values
    enum {
    BLOCK_DEVICE_IDLE		= 0,
    BLOCK_DEVICE_BUSY		= 1,
    BLOCK_DEVICE_READ_SUCCESS	= 2,
    BLOCK_DEVICE_WRITE_SUCCESS	= 3,
    BLOCK_DEVICE_READ_ERROR	= 4,
    BLOCK_DEVICE_WRITE_ERROR	= 5,
    };

    // Command values
    enum {
    BLOCK_DEVICE_NOOP,
    BLOCK_DEVICE_READ,
    BLOCK_DEVICE_WRITE,
    };

protected:

    SC_HAS_PROCESS(PibusBlockDevice);

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
    sc_core::sc_out<bool>	         p_irq;

    // METHODS
    void transition();
    void genMoore();
    void printTrace();

    // Constructor   
    PibusBlockDevice( sc_module_name                      name,
                      uint32_t                            tgtid,
		      soclib::common::PibusSegmentTable   &segtab,
                      char*                               filename,
                      uint32_t                            block_size = 512,
                      uint32_t                            latency = 0);

}; // end class PibusBlockDevice

}} // end namespace

#endif
