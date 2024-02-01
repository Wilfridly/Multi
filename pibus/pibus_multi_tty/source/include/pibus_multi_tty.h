/////////////////////////////////////////////////////////////////////
// File: pibus_multi_tty.h
// Authors : Alain Greiner 
// Date : 11/01/2010 
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
/////////////////////////////////////////////////////////////////////
// This component is a TTY controler. 
// It controls up to 16 terminals emulated as XTERM windows.
// Each terminal is acting  both as a character display
// and a keyboard controler.
//
//  Each terminal is seen as 4 memory mapped registers,
// - TTY_WRITE 	(0x0)	(write) character to display
// - TTY_STATUS (0x4)	(read)  bit0 : read buffer / bit1 : write buffer
// - TTY_READ   (0x8)	(read)  the key-board character 
// - TTY_CONFIG (0xc)	(write) unused 
//
// As a keyboard controler, it contains a TTY_READ register
// to store the character corresponding to the stroken key.
// Bit 0 of the TTY_STATUS register is 1 when TTY_READ is full. 
// It must be tested by the softwre before reading a character.
// If Bit 0 of TTY_STATUS register is 0, any read request
// in register TTY_READ will return an undefined value.
// If Bit 0 of TTY_STATUS register is 1, any new stroken key will be ignored.
// Bit 0 of TTY_STATUS is forced to 1 and the IRQ_GET line is activated
// when a key is stroken.
// Bit 0 of TTY_STATUS is forced to 0, and the IRQ_GET line is
// de-activated by a read request to the TTY_READ register.
// 
// As a display controler, it contains a TTY_WRITE register 
// to store the character that must be diplayed. 
// Bit 1 of the TTY_STATUS register is 1 when TTY_WRITE is full.
// In principle this bit must be tested before writing in TTY_WRITE register:
// If Bit 1 of TTY_STATUS register is 1, any write request to TTY_WRITE 
// register will be ignored.
// Bit 1 of TTY_STATUS  is forced to 1 and the IRQ_PUT line is 
// de-activated by a write request to the TTY_WRITE register.
// Bit 1 of TTY_STATUS is forced to 0, and the IRQ_PUT line is
// activated when the character is actually displayed.
//
// Implementation note : In the present implementation,
// the display buffer is supposed infinite. Therefore,
// the IRQ_PUT interrupt is not used, the Bit1 of TTY_STATUS
// is not used, and the associated flow-control mechanism.
//
// The constructor creates as many UNIX XTERM processes as
// the number of emulated terminals. It creates a PTY pseudo-terminal 
// for each XTERM supporting bi-directional inter-process communication.
/////////////////////////////////////////////////////////////////////
// This component has 4 "constructor" parameters :
// - sc_module_name	name		: instance name  
// - unsigned int	tgtid		: target index  
// - PibusSegmentTable  segtab		: segment table
// - unsigned int	ntty		: number of terminals
/////////////////////////////////////////////////////////////////////

#ifndef PIBUS_MULTI_TTY_H
#define PIBUS_MULTI_TTY_H
	
#include <systemc>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include "pibus_segment_table.h"
#include "pibus_mnemonics.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::common;

class PibusMultiTty : sc_module {

    //	STRUTURAL PARAMETERS
    const char*			m_name;			// instance name
    size_t			m_tgtid;		// target index
    size_t       		m_ntty;			// number of terminals
    uint32_t    		m_segbase;		// segment base address
    uint32_t    		m_segsize;		// segment size
    const char*			m_segname;		// segment name
    pid_t			m_pid[16];		// Process ID table for XTERMs
    int				m_pty[16];		// File Descriptor table for PTYs
    char			m_fsm_str[6][20];	// FSM states names

    //	REGISTERS
    sc_register<int>		r_fsm_state;		// FSM state
    sc_register<size_t>		r_index;		// index of the addressed terminal
    sc_register<bool>		r_keyboard_sts[16];	// Keyboard Status Register (false when empty)
    sc_register<bool>		r_keyboard_msk[16];	// Keyboard Status Register (true when IRQ enable)
    sc_register<bool>		r_display_sts[16];	// Display Status Register (false when empty)
    sc_register<bool>		r_display_msk[16];	// Display Status Register (true when IRQ enable)
    sc_register<uint32_t>	r_keyboard_buf[16];	// Keyboard Character Buffer (ASCII code)

protected:

    SC_HAS_PROCESS(PibusMultiTty);

public:

    // Register Map
    enum {
	TTY_WRITE	= 0x0,
	TTY_STATUS	= 0x4,
	TTY_READ	= 0x8,
	TTY_CONFIG	= 0xC,
    };

    // FSM STATES
    enum { 
        FSM_IDLE   	= 0x0,
        FSM_DISPLAY 	= 0x1,
        FSM_STATUS	= 0x2,
        FSM_KEYBOARD  	= 0x3,
        FSM_CONFIG    	= 0x4,
        FSM_ERROR    	= 0x5,
    };

    //	IO PORTS
    sc_in<bool>			p_ck;
    sc_in<bool>			p_resetn;
    sc_in<bool>			p_sel; 
    sc_in<uint32_t>		p_a; 
    sc_in<bool>			p_read; 
    sc_in<uint32_t>		p_opc; 
    sc_out<uint32_t>		p_ack; 	
    sc_inout<uint32_t>		p_d; 
    sc_in<bool>			p_tout; 
    sc_out<bool>*		p_irq_get;
    sc_out<bool>*		p_irq_put;

    //	constructor 
    PibusMultiTty(sc_module_name 	name,
		uint32_t        	tgtid, 
		PibusSegmentTable	&segtab,
		uint32_t    		ntty);

    ~PibusMultiTty();

    //  Methods()
    void transition();
    void genMoore();
    void printTrace();

#ifdef SOCVIEW
    void registerDebug( SocviewDebugger db);
#endif

}; // end class PibusMultiTty

}} // end namespaces

#endif
