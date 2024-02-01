///////////////////////////////////////////////////////////////// 
// File  : pibus_simple_master.cpp
// Date  : 29/01/2010
// author:  Alain Greiner 
// Copyright  UPMC - LIP6
// This program is released under the GNU public license
/////////////////////////////////////////////////////////////////

#include "pibus_simple_master.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::common;
using namespace soclib::caba;

/////////////////////////////////////
void PibusSimpleMaster::transition()
{
    if(p_resetn.read() == false) 
    {
	r_fsm_state == FSM_INIT;
	return;
    }

    switch(r_fsm_state) {
    case FSM_INIT: 
    {
	r_count = 0;
	r_fsm_state = FSM_RAM_REQ;
        break;
    } 

    //// Lecture 4 mots de 32 bits en mémoire

    case FSM_RAM_REQ: 
    {
	if(p_gnt.read() == true) r_fsm_state = FSM_RAM_A0;
        break;
    } 
    case FSM_RAM_A0:
    {
	r_fsm_state = FSM_RAM_A1_D0;
        break;
    } 
    case FSM_RAM_A1_D0: 
    {
	if(p_ack.read() == PIBUS_ACK_READY) 
	{
            r_fsm_state = FSM_RAM_A2_D1;
            r_buf[0] = (uint32_t)p_d.read();
	}
        else if ( (p_ack.read() == PIBUS_ACK_ERROR) ||
                  (p_ack.read() == PIBUS_ACK_RETRY) ||
                  (p_tout == true) )
        {
            std::cout << "error detected by the PibusSimpleMaster component" << std::endl;
            std::cout << "during a read access to the PibuSimpleRam component" << std::endl;
            exit(0);    
        }
        break;
    } 
    case FSM_RAM_A2_D1: 
    {
	if(p_ack.read() == PIBUS_ACK_READY) 
	{
            r_fsm_state = FSM_RAM_A3_D2;
            r_buf[1] = (uint32_t)p_d.read();
	}
        else if ( (p_ack.read() == PIBUS_ACK_ERROR) ||
                  (p_ack.read() == PIBUS_ACK_RETRY) ||
                  (p_tout == true) )
        {
            std::cout << "error detected by the PibusSimpleMaster component" << std::endl;
            std::cout << "during a read access to the PibuSimpleRam component" << std::endl;
            exit(0);    
        }
        break;
    } 
    case FSM_RAM_A3_D2: 
    {
	if(p_ack.read() == PIBUS_ACK_READY) 
	{
            r_fsm_state = FSM_RAM_D3;
            r_buf[2] = (uint32_t)p_d.read();
	}
        else if ( (p_ack.read() == PIBUS_ACK_ERROR) ||
                  (p_ack.read() == PIBUS_ACK_RETRY) ||
                  (p_tout == true) )
        {
            std::cout << "error detected by the PibusSimpleMaster component" << std::endl;
            std::cout << "during a read access to the PibuSimpleRam component" << std::endl;
            exit(0);    
        }
        break;
    } 
    case  FSM_RAM_D3: 
    {
	if(p_ack.read() == PIBUS_ACK_READY) 
	{
            r_fsm_state = FSM_WRITE_REQ;
            r_buf[3] = (uint32_t)p_d.read();
	}
        else if ( (p_ack.read() == PIBUS_ACK_ERROR) ||
                  (p_ack.read() == PIBUS_ACK_RETRY) ||
                  (p_tout == true) )
        {
            std::cout << "error detected by the PibusSimpleMaster component" << std::endl;
            std::cout << "during a read access to the PibuSimpleRam component" << std::endl;
            exit(0);    
        }
        break;
    } 

    //// affichage chaîne de caractère Rsur le TTY

    case FSM_WRITE_REQ:
    {
	if(p_gnt.read() == true) r_fsm_state = FSM_WRITE_AD;
        break;
    } 
    case FSM_WRITE_AD:
    {
	r_fsm_state = FSM_WRITE_DT;
        break;
    } 
    case FSM_WRITE_DT:
    {
	if(p_ack.read() == PIBUS_ACK_READY) 
	{
            if( r_count == 15 ) 	r_fsm_state = FSM_STS_REQ;
            else 	 		r_fsm_state = FSM_WRITE_REQ;
            r_count = r_count + 1;
        }
        else if ( (p_ack.read() == PIBUS_ACK_ERROR) ||
                  (p_ack.read() == PIBUS_ACK_RETRY) ||
                  (p_tout == true) )
        {
            std::cout << "error detected by the PibusSimpleMaster component" << std::endl;
            std::cout << "during a write access to the PibusMultiTty component" << std::endl;
            exit(0);    
        } 
        break;
    }

    //// lecture registre status du TTY

    case FSM_STS_REQ: 
    {
	if(p_gnt.read() == true) r_fsm_state = FSM_STS_AD;
        break;
    } 
    case FSM_STS_AD: 
    {
	r_fsm_state = FSM_STS_DT;
        break;
    } 
    case FSM_STS_DT: 
    {
	if(p_ack.read() == PIBUS_ACK_READY) 
	{
            if(p_d.read() == 0) 	r_fsm_state = FSM_STS_REQ;
            else			r_fsm_state = FSM_BUF_REQ;
	}
        else if ( (p_ack.read() == PIBUS_ACK_ERROR) ||
                  (p_ack.read() == PIBUS_ACK_RETRY) ||
                  (p_tout == true) )
        {
            std::cout << "error detected by the PibusSimpleMaster component" << std::endl;
            std::cout << "during a read status access to the PibuMultiTty component" << std::endl;
            exit(0);    
        } 
        break;
    } 

    //// lecture registre data du TTY

    case FSM_BUF_REQ: 
    {
	if(p_gnt.read() == true) r_fsm_state = FSM_BUF_AD;
        break;
    } 
    case FSM_BUF_AD: 
    {
	r_fsm_state = FSM_BUF_DT;
        break;
    } 
    case FSM_BUF_DT: 
    {
	if(p_ack.read() == PIBUS_ACK_READY) r_fsm_state = FSM_INIT;
        break;
    }
    } // end switch
} // end transition()

//////////////////////////////////
void PibusSimpleMaster::genMoore()
{
    // REQ Signal
    if((r_fsm_state == FSM_RAM_REQ) || (r_fsm_state == FSM_WRITE_REQ) ||
       (r_fsm_state == FSM_STS_REQ) || (r_fsm_state == FSM_BUF_REQ))	p_req = true;
    else								p_req = false;

    // A, OPC, READ & LOCK signals
    if(r_fsm_state == FSM_RAM_A0) 
    {
	p_a = (uint32_t)m_ram_address;
	p_opc = (uint32_t)PIBUS_OPC_WDU;
	p_read = true;
	p_lock = true;
    } 
    else if(r_fsm_state == FSM_RAM_A1_D0)
    {
	p_a = (uint32_t)(m_ram_address + 4);
	p_opc = (uint32_t)PIBUS_OPC_WDU;
	p_read = true;
	p_lock = true;
    } 
    else if(r_fsm_state == FSM_RAM_A2_D1) 
    {
	p_a = (uint32_t)(m_ram_address + 8);
	p_opc = (uint32_t)PIBUS_OPC_WDU;
	p_read = true;
	p_lock = true;
    } 
    else if(r_fsm_state == FSM_RAM_A3_D2) 
    {
	p_a = (uint32_t)(m_ram_address + 12);
	p_opc = (uint32_t)PIBUS_OPC_WDU;
	p_read = true;
	p_lock = false;
    } 
    else if(r_fsm_state == FSM_WRITE_AD) 
    {
	p_a = (uint32_t)m_tty_address;
	p_opc = (uint32_t)PIBUS_OPC_WDU;
	p_read = false;
	p_lock = false;
    } 
    else if(r_fsm_state == FSM_STS_AD) 
    {
	p_a = (uint32_t)(m_tty_address + 4);
	p_opc = (uint32_t)PIBUS_OPC_WDU;
	p_read = true;
	p_lock = false;
    } 
    else if(r_fsm_state == FSM_BUF_AD) 
    {
	p_a    = (uint32_t)(m_tty_address + 8);
	p_opc  = (uint32_t)PIBUS_OPC_WDU;
	p_read = true;
	p_lock = false;
    }
    // DT signal
    if(r_fsm_state == FSM_WRITE_DT) 
    {
	int word = r_count >> 2;
	int byte = r_count % 4;
        if     (byte ==  0)	p_d = (uint32_t)((r_buf[word]      ) & 0x000000FF);
        else if(byte ==  1) 	p_d = (uint32_t)((r_buf[word] >>  8) & 0x000000FF);
        else if(byte ==  2) 	p_d = (uint32_t)((r_buf[word] >> 16) & 0x000000FF);
        else                	p_d = (uint32_t)((r_buf[word] >> 24) & 0x000000FF);
    }
} // end genMoore()

////////////////////////////////////////////////////////
PibusSimpleMaster::PibusSimpleMaster(sc_module_name name, 
					unsigned int ram_address, 
					unsigned int tty_address)
    : m_name(name),
      m_ram_address(ram_address),
      m_tty_address(tty_address),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_gnt("p_gnt"),
      p_req("p_req"),
      p_a("p_a"),
      p_opc("p_opc"),
      p_read("p_read"),
      p_lock("p_lock"),
      p_d("p_d"),
      p_ack("p_ack"),
      p_tout("p_tout")
{

    SC_METHOD(transition);
    sensitive_pos << p_ck;

    SC_METHOD(genMoore);
    sensitive_neg << p_ck;

    strcpy(m_fsm_str[0], "INIT");
    strcpy(m_fsm_str[1], "RAM_REQ");
    strcpy(m_fsm_str[2], "RAM_A0");
    strcpy(m_fsm_str[3], "RAM_A1_D0");
    strcpy(m_fsm_str[4], "RAM_A2_D1");
    strcpy(m_fsm_str[5], "RAM_A3_D2");
    strcpy(m_fsm_str[6], "RAM_D3");
    strcpy(m_fsm_str[7], "WRITE_REQ");
    strcpy(m_fsm_str[8], "WRITE_AD");
    strcpy(m_fsm_str[9], "WRITE_DT");
    strcpy(m_fsm_str[10], "STS_REQ");
    strcpy(m_fsm_str[11], "STS_AD");
    strcpy(m_fsm_str[12], "STS_DT");
    strcpy(m_fsm_str[13], "BUF_REQ");
    strcpy(m_fsm_str[14], "BUF_AD");
    strcpy(m_fsm_str[15], "BUF_DT");

// checking parameters

if((ram_address & 0x00000003) != 0) {
	perror("Error in the PibusSimpleMaster component\n");
	perror("The ram_address parameter must be multiple of 4\n");
	exit(1);
}
if((tty_address & 0x00000003) != 0) {
	perror("Error in the PibusSimpleMaster component\n");
	perror("The tty_address parameter must be multiple of 4\n");
	exit(1);
}

    std::cout << std::endl << "Instanciation of PibusSimpleMaster : " <<  m_name << std::endl;
    std::cout << "    tty base address = 0x" << std::hex << tty_address << std::endl;
    std::cout << "    ram base address = 0x" << std::hex << ram_address << std::endl;

} // end constructor

////////////////////////////////////
void PibusSimpleMaster::printTrace()
{
    std::cout << m_name << " : state = " << m_fsm_str[r_fsm_state] << std::endl;
} // end print()

#ifdef SOCVIEW
////////////////////////////////////////////////////
PibusSimpleMaster::registerDebug(SocviewDebugger db)
{
   db.add(r_fsm_state, m_name + ".r_fsm_state"); 
   db.add(r_count    , m_name + ".r_count"); 
   db.add(r_buf      , m_name + ".r_buf"  );
} // end 
#endif

}} // end namespaces
