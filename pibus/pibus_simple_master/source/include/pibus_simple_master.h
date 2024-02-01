//////////////////////////////////////////////////////////////////////////// 
// File  : pibus_simple_master.h
// Date  : 15/01/2010
// author:  Alain Greiner 
// Copyright  UPMC - LIP6
// This program is released under the GNU public license
///////////////////////////////////////////////////////////////////////////
// This component is a very simple PIBUS master, that is only used 
// to demonstrate the PIBUS protocol.
// It contains a Finite State Machine executing an infinite loop.
// In this loop, the FSM executes 4 steps) :
// 1) It reads a burst of 4 words in memory, and stores those 4 words
// in an internal buffer.
// 2) It considers those 4 words as a character string, and print those 
// characters to the TTY terminal (one PIBUS transaction per character)
// until the first NUL character.
// 3) It reads the keyboard status register, waiting for a non zero value,
// that indicates an available character.
// 4) It reads the value of the character captured on the keyboard
// in order to clear the keyboard buffer.
// As the string must terminate with a NUL character, the maximal length
// of the printable string is 15 characters (including a possible 
// NewLine character).
// This coprocessor uses the "litle endian" convention for the translation
// from a string to an integer.
//////////////////////////////////////////////////////////////////////////
// This component has 3 "constructor" parameter) :
// - sc_module_m_name 	m_name   		: instance m_name
// - unsigned int 	ram_address   	: string address in memory
// - unsigned int 	tty_address	: TTY base address
///////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_SIMPLE_MASTER_H
#define PIBUS_SIMPLE_MASTER_H

#include <systemc>
#include <inttypes.h>
#include "pibus_mnemonics.h"

namespace soclib { namespace caba {

using namespace sc_core;

class PibusSimpleMaster : sc_module {

    // STRUCTURAL PARAMETERS
    const char*			m_name;
    const uint32_t		m_ram_address;
    const uint32_t		m_tty_address;
    char			m_fsm_str[16][20];

    // REGISTERS
    sc_register<int>		r_fsm_state;
    sc_register<size_t>		r_count;
    sc_register<uint32_t>	r_buf[4];

    // FSM states
    enum{
	FSM_INIT		= 0x0,
	FSM_RAM_REQ		= 0x1,
	FSM_RAM_A0		= 0x2,
	FSM_RAM_A1_D0		= 0x3,
	FSM_RAM_A2_D1		= 0x4,
	FSM_RAM_A3_D2		= 0x5,
	FSM_RAM_D3		= 0x6,
	FSM_WRITE_REQ		= 0x7,
	FSM_WRITE_AD		= 0x8,
	FSM_WRITE_DT		= 0x9,
	FSM_STS_REQ		= 0xa,
	FSM_STS_AD		= 0xb,
	FSM_STS_DT		= 0xc,
	FSM_BUF_REQ		= 0xd,
	FSM_BUF_AD		= 0xe,
	FSM_BUF_DT		= 0xf};

protected:

    SC_HAS_PROCESS(PibusSimpleMaster);

public:

    // 	I/O PORTS
    sc_in<bool>			p_ck;
    sc_in<bool>			p_resetn;
    sc_in<bool>			p_gnt;
    sc_out<bool>		p_req;
    sc_out<uint32_t>		p_a;
    sc_out<uint32_t>		p_opc;
    sc_out<bool>		p_read;
    sc_out<bool>		p_lock;
    sc_inout<uint32_t>		p_d;
    sc_in<uint32_t>		p_ack;
    sc_in<bool>			p_tout;

    // Constructor
    PibusSimpleMaster(sc_module_name 	name, 		// instance name
			unsigned int 	ram_address, 	// RAM segment base address
			unsigned int 	tty_address);	// TTY@segment base address

    // Methods
    void transition();
    void genMoore();
    void printTrace();

#ifdef SOCVIEW
    void registerDebug( SocviewDebugger db );
#endif

};  // end class PibusSimpleMaster

}} // end namespaces

#endif
