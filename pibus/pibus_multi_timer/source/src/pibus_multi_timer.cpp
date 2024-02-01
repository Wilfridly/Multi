///////////////////////////////////////////////////////////////////////////////
// File : pibus_multi_timer.h
// Date : 05/03/2010
// Author : A.Greiner 
// It is released under the GNU Public License.
// Copyright : UPMC-LIP6
////////////////////////////////////////////////////////////////////////////////

#include "pibus_multi_timer.h"
#include "alloc_elems.h"

using namespace sc_core;
using namespace soclib::common;

namespace soclib { namespace caba {

//////////////////////////////////////////////////////////////
PibusMultiTimer::PibusMultiTimer(sc_module_name		name,	
				 uint32_t		tgtid,	
				 PibusSegmentTable	&segtab,
				 uint32_t	        ntimer)
    : m_name(name),
      m_tgtid(tgtid),
      m_ntimer(ntimer),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_sel("p_sel"),
      p_a("p_a"),
      p_read("p_read"),
      p_opc("p_opc"),
      p_ack("p_ack"),
      p_d("p_d"),
      p_tout("p_tout"),
      p_irq(soclib::common::alloc_elems<sc_out<bool> >("p_irq",ntimer))
{
    SC_METHOD (transition);
    sensitive << p_ck.pos();
	      
    SC_METHOD (genMoore);
    sensitive << p_ck.neg();
  
    strcpy (m_fsm_str[0], "IDLE");
    strcpy (m_fsm_str[1], "READ");
    strcpy (m_fsm_str[2], "WRITE");
    strcpy (m_fsm_str[3], "ERROR");

    // get the base address and segment size 
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();

    if ((m_ntimer < 1) || (m_ntimer > 32))
    {
        printf(" ERROR in PibusMultiTimer component : %s\n", m_name);
        printf(" The number of timers cannot be larger than 32 !\n");
        exit(1);
    }
    if ((m_segbase & 0xF) != 0)
    {
        printf(" ERROR in PibusMultiTimer component : %s\n",m_name);
        printf(" The base adress must be multiple of 16 !\n");
        exit(1);
    }
    if (m_segsize < (m_ntimer<<4))
    {
        printf(" ERROR in PibusMultiTimer component : %s\n",m_name);
        printf(" The segment size must be at least 16*ntimer bytes !\n");
        exit(1);
    }
                                      
    // register allocation 
    r_irq	= new 	sc_register<bool>[ntimer];
    r_running	= new 	sc_register<bool>[ntimer];
    r_value	= new	sc_register<uint32_t>[ntimer];
    r_period	= new	sc_register<uint32_t>[ntimer];
    r_counter	= new	sc_register<uint32_t>[ntimer];
		
    std::cout << std::endl << "Instanciation of PibusMultiTimer : " << m_name << std::endl;
    std::cout << "    ntimer = " << m_ntimer << std::endl;
    std::cout << "    segment " << m_segname << std::hex
                  << " | base = 0x" << m_segbase
                  << " | size = 0x" << m_segsize << std::endl;
} // end constructor

///////////////////////////////////
void PibusMultiTimer::transition() 
{
    if(p_resetn == false) 
    {
	r_fsm_state = FSM_IDLE;
	for(size_t i = 0 ; i < m_ntimer ; i++) 
        {
            r_running[i] = false;
            r_irq[i] = false;
	}
	return;
    }
		
    switch(r_fsm_state) {
    case FSM_IDLE :
	if(p_sel == true) 
        {			
            uint32_t address = (uint32_t)p_a.read() & 0xFFFFFFFC;
            if ((address < m_segbase) || (address >= (m_segbase + m_segsize))) 
            { 
                r_fsm_state = FSM_ERROR;
            } 
            else 
            {
                r_cell   = (address - m_segbase) & 0x0000000C;
                r_index  = ((address - m_segbase) & 0x000001F0) >> 4;
		if (p_read.read()) r_fsm_state = FSM_READ;
                else	           r_fsm_state = FSM_WRITE;
            }
        }
        break;
    case FSM_WRITE :
	if      (r_cell == VALUE_ADDRESS)     	r_value[r_index] = (uint32_t)p_d.read();
	else if (r_cell == IRQ_ADDRESS)  	r_irq[r_index] = ((uint32_t)p_d.read() != 0);
        else if (r_cell == RUNNING_ADDRESS)   	r_running[r_index] = ((uint32_t)p_d.read() != 0);
	else if (r_cell == PERIOD_ADDRESS) 
        {
						r_period[r_index] = (uint32_t)p_d.read();
						r_counter[r_index] = (uint32_t)p_d.read();
						r_running[r_index] = false;
	}
	r_fsm_state = FSM_IDLE;
        break;
    case FSM_READ :
	r_fsm_state = FSM_IDLE;
        break;
    case FSM_ERROR :
	r_fsm_state = FSM_IDLE; 
        break;
    } // end switch TARGET FSM

    // Increment r_value[i], decrement r_counter[i] & Set r_irq[i]

    for(size_t i = 0 ; i < m_ntimer ; i++) 
    {
	r_value[i] = r_value[i] + 1;
	if (r_running[i]  == true) 
        { 
            if ( r_counter[i] > 0) 
            {
                r_counter[i] = r_counter[i] - 1;
            } 
            else 
            {
                r_counter[i] = r_period[i];
                r_irq[i] = true; 
            }
	} // end if timer running
    } // end for
} // end transition()

////////////////////////////////
void PibusMultiTimer::genMoore()
{
    // PIBUS signals 
	switch (r_fsm_state) {
	case FSM_IDLE :
	    break;
	case FSM_WRITE :
	    p_ack = PIBUS_ACK_READY;
            break;
	case FSM_READ :
	    p_ack = PIBUS_ACK_READY;
	    if      (r_cell == VALUE_ADDRESS)    p_d.write((uint32_t)r_value[r_index]); 
            else if (r_cell == PERIOD_ADDRESS)   p_d.write((uint32_t)r_period[r_index]);
            else if (r_cell == RUNNING_ADDRESS)  p_d.write((uint32_t)r_running[r_index]); 
            else if (r_cell == IRQ_ADDRESS)      p_d.write((uint32_t)r_irq[r_index]); 
            break;
	case FSM_ERROR:
            p_ack = PIBUS_ACK_ERROR;
            break;
	} // end switch FSM

    // IRQ[i]
    for (size_t i = 0 ; i < m_ntimer ; i++) p_irq[i] = r_irq[i]  && r_running[i];

} // end genMoore()
	
//////////////////////////////////
void PibusMultiTimer::printTrace()
{
    std::cout << m_name << " : " << m_fsm_str[r_fsm_state] 
              << "   period[0] = " << r_period[0] 
              << "   running[0] = " << r_running[0] << std::endl;
}

}} // end namespace
