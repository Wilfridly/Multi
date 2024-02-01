//////////////////////////////////////////////////////////////////////////// 
// File  : pibus_seg_bcu.cpp
// Date  : 11/01/2010
// author:  Alain Greiner 
// Copyright  UPMC - LIP6
// This program is released under the GNU public license
///////////////////////////////////////////////////////////////////////////

#include "pibus_seg_bcu.h"
#include "alloc_elems.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::caba;
using namespace soclib::common;

//////////////////////////////////////////////////////
PibusSegBcu::PibusSegBcu (	sc_module_name 			name,
                            PibusSegmentTable 	    &segtab,
                            size_t 					nb_master,
                            size_t 					nb_target,
                            uint32_t 				time_out)
	: m_name(name),
      m_target_table(segtab.getTargetTable()),
      m_msb_shift( 32 - segtab.getMSBnumber() ),
      m_nb_master(nb_master),
      m_nb_target(nb_target),
      m_time_out(time_out),
      r_fsm_state("r_fsm_state"),
      r_current_master("r_current_master"),
      r_tout_counter("r_tout_counter"),
      r_req_counter(soclib::common::alloc_elems<sc_signal<uint32_t> >("r_req_counter", nb_master)),
      r_wait_counter(soclib::common::alloc_elems<sc_signal<uint32_t> >("r_wait_counter", nb_master)),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_req(soclib::common::alloc_elems<sc_in<bool> >("p_req", nb_master)),
      p_gnt(soclib::common::alloc_elems<sc_out<bool> >("p_gnt", nb_master)),
      p_sel(soclib::common::alloc_elems<sc_out<bool> >("p_sel", nb_target)),
      p_a("p_a"),
      p_lock("p_lock"),
      p_ack("p_ack"),
      p_tout("p_tout"),
      p_avalid("p_avalid")
{
	SC_METHOD(transition);
	sensitive << p_ck.pos();

	SC_METHOD(genMoore);
	sensitive << p_ck.neg();

	SC_METHOD(genMealy_sel);
	sensitive << p_ck.neg();
	sensitive << p_a;

	SC_METHOD(genMealy_gnt);
	sensitive << p_ck.neg();
    sensitive << p_ack;
	for (size_t i = 0 ; i < m_nb_master; i++)
        sensitive << p_req[i];

    strcpy(m_fsm_str[0], "IDLE");
    strcpy(m_fsm_str[1], "AD");
    strcpy(m_fsm_str[2], "DTAD");
    strcpy(m_fsm_str[3], "DT");

	if (!segtab.isAllBelow( m_nb_target )) 
    {
	    std::cout << "ERROR in PibusSegBcu Component" << std::endl;
        std::cout << "Target index larger than the number of targets" << std::endl;
        exit(0);
    }

	if (time_out == 0) 
    {
	    std::cout << "ERROR in PibusSegBcu Component" << std::endl;
        std::cout << "Time_out argument cannot be 0" << std::endl;
        exit(0);
    }

    std::cout << std::endl << "Instanciation of PibuBcu : " << m_name << std::endl;
    std::cout << "    nb_master = " << m_nb_master << std::endl;
    std::cout << "    nb_target = " << m_nb_target << std::endl;
    std::cout << "    time_out  = " << m_time_out  << std::endl;

}

PibusSegBcu::~PibusSegBcu()
{
    soclib::common::dealloc_elems(p_req, m_nb_master);
    soclib::common::dealloc_elems(p_gnt, m_nb_master);
    soclib::common::dealloc_elems(p_sel, m_nb_target);
    soclib::common::dealloc_elems(r_req_counter, m_nb_master);
    soclib::common::dealloc_elems(r_wait_counter, m_nb_master);
}

//////////////////////////////
void PibusSegBcu::transition()
{
    if (p_resetn == false) 
    {
        r_fsm_state = FSM_IDLE;
        r_current_master = 0;
        for(size_t i = 0 ; i < m_nb_master ; i++) 
        {
            r_wait_counter[i] = 0;
            r_req_counter[i] = 0;
        }
        return;
    } // end p_resetn

    for(size_t i = 0 ; i < m_nb_master ; i++) 
    {
        if(p_req[i]) r_wait_counter[i] = r_wait_counter[i] + 1;
	}
	
    switch(r_fsm_state) {
	case FSM_IDLE:
    {
        r_tout_counter = m_time_out;
        for(size_t i = 0 ; i < m_nb_master ; i++) 
        {
            int j = (i + 1 + r_current_master) % m_nb_master;
            if (p_req[j]) 
            {
                r_current_master = j;
                r_req_counter[j] = r_req_counter[j] + 1;
                r_fsm_state = FSM_AD;
                break;
            }
        } 
        break;
    }
	case FSM_AD:
    {
        if(p_lock)   r_fsm_state = FSM_DTAD;  
        else	     r_fsm_state = FSM_DT; 
        break;
    }
	case FSM_DTAD:
    {
        if (r_tout_counter == 0) 
        {
            r_fsm_state = FSM_IDLE;
        } 
        else if ( (p_ack.read() != PIBUS_ACK_WAIT) and (p_lock == false) ) 
        {
            r_fsm_state = FSM_DT; 
        } 
        else 
        { 
            r_tout_counter = r_tout_counter - 1;
        }
        break;
    }
	case FSM_DT:
    {
        if(r_tout_counter == 0) 
        {
            r_fsm_state = FSM_IDLE;
        } 
        else if(p_ack.read() != PIBUS_ACK_WAIT)  // new allocation
        {
            r_tout_counter = m_time_out;
            bool found = false;
            for(size_t i = 0 ; (i < m_nb_master) and (found == false) ; i++) 
            {
                int j = (i + 1 + r_current_master) % m_nb_master;
                if( p_req[j] == true )
                {
                    r_current_master = j;
                    r_req_counter[j] = r_req_counter[j] + 1;
                    found = true;
                }
            } 
            if(found == true) r_fsm_state = FSM_AD; 
            else              r_fsm_state = FSM_IDLE; 
        } 
        else 
        { 
            r_tout_counter = r_tout_counter - 1;
        }
        break;
    }
    } // end switch FSM
} // end transition

////////////////////////////////
void PibusSegBcu::genMealy_gnt()
{
    bool	found = false;
    if( (r_fsm_state == FSM_IDLE) || ((r_fsm_state == FSM_DT) && (p_ack.read() != PIBUS_ACK_WAIT)) ) 
    {
        for(size_t i = 0 ; i < m_nb_master ; i++) 
        {
            int j = (i + 1 + r_current_master) % m_nb_master;
            if((p_req[j] == true) && (found == false)) 
            {
                p_gnt[j] = true;
                found = true;
            } 
            else 
            {
                p_gnt[j] = false;
            }
        } 
    } 
    else 
    {
        for (size_t i = 0 ; i < m_nb_master ; i++) 
        {
            p_gnt[i] = false;
        }
    }
} // end genMealy_gnt()

////////////////////////////////
void PibusSegBcu::genMealy_sel()
{
    if((r_fsm_state == FSM_AD) || (r_fsm_state == FSM_DTAD)) 
    {
        size_t index = m_target_table[p_a.read() >> m_msb_shift];
        for(size_t i = 0; i < m_nb_target ; i++) 
        {
            if(i == index)  	p_sel[i] = true;
            else			    p_sel[i] = false;
        } 
    } 
    else 
    {
        for(size_t i = 0 ; i < m_nb_target ; i++) 
        {
            p_sel[i] = false;
        }
    }
} // end genMealy_sel()

////////////////////////////
void PibusSegBcu::genMoore() 
{
    p_tout = (r_tout_counter == 0);
    p_avalid = (r_fsm_state == FSM_AD) || (r_fsm_state == FSM_DTAD);
}

//////////////////////////////
void PibusSegBcu::printTrace() 
{
    std::cout << m_name << " : fsm = " << m_fsm_str[r_fsm_state] << std::dec;

    if( (r_fsm_state == FSM_IDLE) || ((r_fsm_state == FSM_DT) && (p_ack.read() != PIBUS_ACK_WAIT)) ) 
    {
        bool found = false;
        size_t  index;
        for(size_t i=0 ; (i<m_nb_master) and (found == false) ; i++) 
        {
            index = (i + 1 + r_current_master) % m_nb_master;
            if( p_req[index] == true )
            {
                std::cout << " | granted master = " << index;
                found = true;
            }
        } 
    }
    if( (r_fsm_state == FSM_AD) || (r_fsm_state == FSM_DTAD) ) 
    {
        size_t index = m_target_table[p_a.read() >> m_msb_shift];
        std::cout << " | selected target = " << index; 
    }
    std::cout << std::endl;
}

///////////////////////////////////
void PibusSegBcu::printStatistics()
{
    std::cout << m_name << " : Statistics" << std::endl;
    for(size_t i = 0 ; i < m_nb_master ; i++) 
    {
        size_t req  = r_req_counter[i].read();
        size_t wait = r_wait_counter[i].read();
        std::cout << "master " << i << " : n_req = " << req << " , n_wait_cycles = " << wait
                  << " , access time = " <<  (float)wait/(float)req << std::endl;
    }
}

#ifdef SOCVIEW
///////////////////////////////////////////////
void PibusSegBcu::registerDebug(SocviewDebugger db)
{
    db.add(r_fsm_state     , m_name + ".r_fsm_state");
    db.add(r_current_master, m_name + ".r_current_master");
    db.add(r_tout_counter  , m_name + ".r_tout_counter");
    db.add(r_req_counter   , m_name + ".r_req_counter");
    db.add(r_wait_counter  , m_name + ".r_wait_counter");
}
#endif

}} // end namespaces


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4
