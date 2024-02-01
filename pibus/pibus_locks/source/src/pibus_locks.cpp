//////////////////////////////////////////////////////////////////////////
// File : pibus_locks.h
// Author : A.Greiner 
// Date : 08/04/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
/////////////////////////////////////////////////////////////////////////

#include "pibus_locks.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::caba;
using namespace soclib::common;

//////////////////////////////////////////////////////
PibusLocks::PibusLocks(sc_module_name		name, 
  	     		size_t			tgtid,	
     			PibusSegmentTable	&segtab,
			uint32_t		nlocks)	
    : m_name(name),
      m_tgtid(tgtid),
      m_nlocks(nlocks),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_sel("p_sel"),
      p_a("p_a"),
      p_read("p_read"),
      p_opc("p_opc"),
      p_ack("p_ack"),
      p_d("p_d"),
      p_tout("p_tout")
{
    SC_METHOD (transition);
    sensitive_pos << p_ck;

    SC_METHOD (genMoore);
    sensitive_neg << p_ck;

    // segment definition
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();

    if((m_segbase & 0x00000003) != 0x0)
    {
        printf("ERROR in component PibusLocks %s\n", m_name);
        printf("The segment base address must be word aligned\n");
        exit(1);
    }
    if(m_segsize < nlocks*4)
    {
        printf("ERROR in component PibusLocks %s\n", m_name);
        printf("The segment size must be at least : 4 * nlocks\n");
        exit(1);
    }

    // Lock array allocation
    r_locks = new bool[nlocks];

    strcpy(m_fsm_str[0], "IDLE");
    strcpy(m_fsm_str[1], "READ");
    strcpy(m_fsm_str[2], "WRITE");
    strcpy(m_fsm_str[3], "ERROR");

    std::cout << std::endl << "Instanciation of PibusLocks : " << m_name << std::endl;
    std::cout << "    nlocks = " << nlocks << std::endl;
    std::cout << "    segment " << m_segname << std::hex
              << " | base = 0x" << m_segbase
              << " | size = 0x" << m_segsize << std::endl;
} // end constructor

/////////////////////////////
void PibusLocks::transition()
{
    if (p_resetn.read() == false) 
    {
        r_fsm_state = LOCKS_IDLE;
        for (size_t i = 0 ; i < m_nlocks ; i++) r_locks[i] = false; 
        return;
    } // end reset

    switch (r_fsm_state) {       	
    case LOCKS_IDLE :
        if (p_sel.read() == true) 
        {
            uint32_t address = (uint32_t)p_a.read() & 0xfffffffc;
            if ((address >= m_segbase) && (address <  m_segbase + m_segsize)) 
            { 
                r_index = (address - m_segbase)  >> 2;
                if(p_read.read() == true) r_fsm_state = LOCKS_READ;
                else			  r_fsm_state = LOCKS_WRITE;
            } 
            else 			  r_fsm_state = LOCKS_ERROR;
	}		
        break;
    case LOCKS_READ :
	r_locks[r_index] = true;	
	r_fsm_state	= LOCKS_IDLE;
        break;
    case LOCKS_WRITE :
	r_locks[r_index] = false;
	r_fsm_state	= LOCKS_IDLE;
        break;
    case LOCKS_ERROR : 	
	r_fsm_state	= LOCKS_IDLE;
        break;
    } // end switch r_fsm_state
} // end transition()

////////////////////////////
void PibusLocks::genMoore()
{
    switch(r_fsm_state) {
    case LOCKS_IDLE :
        break;
    case LOCKS_READ :
	p_ack = PIBUS_ACK_READY;
        if(r_locks[r_index] == false) p_d = 0;
        else                  	      p_d = 1;
        break;
    case LOCKS_WRITE :
	p_ack = PIBUS_ACK_READY;
        break;
    case LOCKS_ERROR :
	p_ack = PIBUS_ACK_ERROR;
        break;
    } // end switch r_fsm_state
} // end genMoore()

/////////////////////////////////
void PibusLocks::printTrace()
{
    std::cout << m_name << " : " << m_fsm_str[r_fsm_state] << std::endl;
} // end print()

}} // end namespaces
