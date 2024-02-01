//////////////////////////////////////////////////////////////////////////
// File : pibus_icu.h
// Author : A.Greiner 
// Date : 10/04/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
/////////////////////////////////////////////////////////////////////////

#include "pibus_icu.h"
#include "alloc_elems.h"

using namespace sc_core;
using namespace soclib::common;

namespace soclib { namespace caba {

///////////////////////////////////////////////////////
PibusIcu::PibusIcu(sc_module_name		insname, 
	   		size_t			tgtid, 
	   		PibusSegmentTable	&segtab,
			size_t			nirq,
			size_t			nproc)
    : m_name(insname),
      m_tgtid(tgtid),
      m_nirq(nirq),
      m_nproc(nproc),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_sel("p_sel"),
      p_a("p_a"),
      p_read("p_read"),
      p_opc("p_opc"),
      p_ack("p_ack"),
      p_d("p_d"),
      p_tout("p_tout"),
      p_irq_in(soclib::common::alloc_elems<sc_in<bool> >("p_irq_in",nirq)),
      p_irq_out(soclib::common::alloc_elems<sc_out<bool> >("p_irq_out",nproc))
{	
    SC_METHOD (transition);
    sensitive << p_ck.pos();

    SC_METHOD (genMoore);
    sensitive << p_ck.neg();
	
    SC_METHOD(genMealy);
    for(size_t i = 0 ; i < m_nirq ; i++)  sensitive << p_irq_in[i]; 
    sensitive << p_ck.neg();

    strcpy (m_fsm_str[0], "IDLE");
    strcpy (m_fsm_str[1], "READ_VECTOR");
    strcpy (m_fsm_str[2], "READ_IRQS");
    strcpy (m_fsm_str[3], "READ_MASK");
    strcpy (m_fsm_str[4], "SET_MASK");
    strcpy (m_fsm_str[5], "RESET_MASK");
    strcpy (m_fsm_str[6], "ERROR");

    // get the base address & segment size
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();

    if ((m_nproc < 1) || (m_nproc > 8))
    {
        printf(" ERROR in PibusIcu component : %s\n", m_name);
        printf(" The number of output IRQs cannot be larger than 8 !\n");
        exit(1);
    }
    if ((m_nirq < 1) || (m_nirq > 32))
    {
        printf(" ERROR in PibusIcu component : %s\n", m_name);
        printf(" The number of input IRQs cannot be larger than 32 !\n");
        exit(1);
    }
    if ((m_segbase & 0x1F) != 0)
    {
        printf(" ERROR in PibusIcu component : %s\n", m_name);
        printf(" The base adress must be multiple of 32 !\n");
        exit(1);
    }
    if (m_segsize < m_nproc*32)
    {
        printf(" ERROR in PibusIcu component : %s\n",m_name);
        printf(" The segment size must be at least nproc * 32 bytes !\n");
        exit(1);
    }

    std::cout << std::endl << "Instanciation of PibusIcu : " << m_name << std::endl;
    std::cout << "    irq_in  = " << m_nirq << std::endl;
    std::cout << "    irq_out = " << m_nproc << std::endl;
    std::cout << "    segment " << m_segname << std::hex
                  << " | base = 0x" << m_segbase
                  << " | size = 0x" << m_segsize << std::endl;
} //end constructor

///////////////////////////
void PibusIcu::transition()
{
    if(p_resetn == false) 
    {
	r_fsm_state = FSM_IDLE;
        for(size_t i=0 ; i<m_nproc ; i++) r_mask[i] = 0x00000000;
	return;	
    }

    switch(r_fsm_state) {
    case FSM_IDLE : 
	if(p_sel == true) 
        {
	    uint32_t address = (uint32_t)p_a.read() & 0xFFFFFFFC;
            r_index = (address>>5) & 0x7;
            if((address < m_segbase) || (address > (m_segbase+m_segsize))) 	r_fsm_state = FSM_ERROR;  
            else if( p_read.read() &&  ((address & 0x1F) == ICU_IT_VECTOR))     r_fsm_state = FSM_READ_VECTOR; 
            else if( p_read.read() &&  ((address & 0x1F) == ICU_INT))           r_fsm_state = FSM_READ_IRQS; 
            else if( p_read.read() &&  ((address & 0x1F) == ICU_MASK))          r_fsm_state = FSM_READ_MASK;  
            else if( !p_read.read() && ((address & 0x1F) == ICU_MASK_SET))      r_fsm_state = FSM_SET_MASK; 
            else if( !p_read.read() && ((address & 0x1F) == ICU_MASK_CLEAR))    r_fsm_state = FSM_RESET_MASK; 
            else                                                                r_fsm_state = FSM_ERROR; 
	}
        break;
    case FSM_SET_MASK :
	r_fsm_state = FSM_IDLE;
	r_mask[r_index] = r_mask[r_index] | (uint32_t)p_d.read();
        break;
    case FSM_RESET_MASK :
	r_fsm_state = FSM_IDLE;
	r_mask[r_index] = r_mask[r_index] & ~(uint32_t)p_d.read();
        break;
    default :
	r_fsm_state = FSM_IDLE;
    break;
    } // end switch
} //end transition

/////////////////////////
void PibusIcu::genMoore()
{

    switch(r_fsm_state) {
    case FSM_IDLE : 
        break; 
    case FSM_ERROR :
	p_ack.write(PIBUS_ACK_ERROR);
        break;
    case FSM_READ_VECTOR :
    {
        uint32_t index = 32;
        for(size_t n = m_nirq ; n > 0 ; n--) 
        {
            size_t	i = n-1;
            if( p_irq_in[i].read() && (((r_mask[r_index]>>i) & 0x1) == 0x1) ) index = i; 
	} 
	p_ack.write(PIBUS_ACK_READY);
	p_d.write(index);
        break;
    }
    case FSM_READ_IRQS :
    {
        uint32_t val = 0;
        for(size_t n = 0 ; n < m_nirq ; n++)
        {
            if( p_irq_in[n].read() && (((r_mask[r_index]>>n) & 0x1) == 0x1) ) val |= 1<<n;
        }
	p_ack.write(PIBUS_ACK_READY);
	p_d.write(val);
        break;
    }
    case FSM_READ_MASK :
	p_ack.write(PIBUS_ACK_READY);
	p_d.write(r_mask[r_index]);
        break;
    case FSM_SET_MASK :
    case FSM_RESET_MASK :
	p_ack.write(PIBUS_ACK_READY);
        break;
    } // end switch FSM
} // end genMoore()

/////////////////////////
void PibusIcu::genMealy()
{
    for(size_t i=0 ; i<m_nproc ; i++)
    {
        bool it = false;	
        for(size_t n=0 ; n<m_nirq ; n++) 
        {
	    it = it || (p_irq_in[n].read() && (((r_mask[i]>>n)&0x1) == 0x1));
        }
        p_irq_out[i].write(it);
    }
} // end genMealy

///////////////////////////
void PibusIcu::printTrace()
{
    std::cout << m_name << " : " << m_fsm_str[r_fsm_state] << " | " ;
    std::cout << "index = " << r_index << " | " << std::hex ;
    for(size_t i=0 ; i<m_nproc ; i++) std::cout << "mask[" << i << "] = " << r_mask[i] << "  " ;
    std::cout << std::endl;
}

}} // end namespace
