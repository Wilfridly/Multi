///////////////////////////////////////////////////////////
// File : pibus_simple_ram.cpp
// Author : Alain Greiner
// Date : 29/01/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
///////////////////////////////////////////////////////////

#include "pibus_simple_ram.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::caba;
using namespace soclib::common;

////////////////////////////////////////////////////////////
PibusSimpleRam::PibusSimpleRam (sc_module_name	 	name,
				uint32_t		tgtid,
				PibusSegmentTable	&segtab,
				uint32_t		latency,
				const Loader  		&loader)
    : m_name(name),
      m_tgtid(tgtid),
      m_latency(latency),
      m_loader(loader),
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

    // segments allocation
    m_nbseg = 0;
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    std::list<SegmentTableEntry>::iterator iter;
  
    for (iter = seglist.begin() ; iter != seglist.end() ; ++iter) 
    {
    	uint32_t base = (*iter).getBase();
    	uint32_t size = (*iter).getSize();
        if(m_nbseg == MAXSEG) 
        {
	  	printf("ERROR in component PibusSimpleRam %s\n", m_name);
	  	printf("The number of segments cannot be larger than %d\n",MAXSEG);
		exit(1);
	}
	if((base & 0x00000003) != 0x0) 
        {
		printf("ERROR in component PibusSimpleRam %s\n", m_name);
		printf("The segbase address must be word aligned\n");
		exit(1);
	}
	if((size & 0x00000003) != 0x0) 
        {
		printf("ERROR in component PibusSimpleRam %s\n", m_name);
		printf("The m_size parameter must be multiple of 4\n");
		exit(1);
	}
	m_segname[m_nbseg]   = (*iter).getName(); 
	m_segsize[m_nbseg]   = size;
	m_segbase[m_nbseg]   = base;
	r_buf[m_nbseg]       = new uint32_t[size >> 2];
	m_nbseg              = m_nbseg+1;
    } 

    strcpy(m_fsm_str[0], "IDLE");
    strcpy(m_fsm_str[1], "READ_WAIT");
    strcpy(m_fsm_str[2], "READ_OK");
    strcpy(m_fsm_str[3], "WRITE_WAIT");
    strcpy(m_fsm_str[4], "WRITE_OK");
    strcpy(m_fsm_str[5], "ERROR");

    std::cout << std::endl << "Instanciation of PibusSimpleRam : " << m_name << std::endl;
    std::cout << "    latency = " << latency << std::endl;
    for(uint32_t i = 0 ; i < m_nbseg ; i++) 
 	std::cout << "    segment " << m_segname[i] << std::hex
                  << " | base = 0x" << m_segbase[i]
                  << " | size = 0x" << m_segsize[i] << std::endl;

} // end constructor

//////////////////////////////////////////////////////
//	Functions used to manage the possible
//	big-endianness of the simulation processor
bool IsBigEndian()
{
    short int 	word = 0x0001;
    char 	*byte = (char *) &word;
    return(byte[0] ? false : true); 
}

int swap_bytes (int LE)
{
    int BE;
    BE =   ((LE & 0xFF000000) >> 24 |
            (LE & 0x00FF0000) >> 8  |
            (LE & 0x0000FF00) << 8  |
            (LE & 0x000000FF) << 24 ); 
    return BE;
}

////////////////////////////////////////////////////////////////////////
void write_seg(uint32_t* tab, size_t index, uint32_t data, uint32_t opc)
{
    // The memory organisation is litle endian
    switch (opc) {
    case PIBUS_OPC_BY0 :  // write byte 0 
	tab[index] = (tab[index] & 0xFFFFFF00) | (data & 0x000000FF);
        break;
    case PIBUS_OPC_BY1 :  // write byte 1 
	tab[index] = (tab[index] & 0xFFFF00FF) | (data & 0x0000FF00);
        break;
    case PIBUS_OPC_BY2 :  // write byte 2 
	tab[index] = (tab[index] & 0xFF00FFFF) | (data & 0x00FF0000);
        break;
    case PIBUS_OPC_BY3 :  // write byte 3 
	tab[index] = (tab[index] & 0x00FFFFFF) | (data & 0xFF000000);
        break;
    case PIBUS_OPC_HW0 :  // write lower half  
	tab[index] = (tab[index] & 0xFFFF0000) | (data & 0x0000FFFF);
        break;
    case PIBUS_OPC_HW1 :  // write upper half 
	tab[index] = (tab[index] & 0x0000FFFF) | (data & 0xFFFF0000);
        break;
    case PIBUS_OPC_WDU :  // write word
	tab[index] = data;
        break;
    case PIBUS_OPC_NOP :  // no write  
        break;
    default :
	printf("ERROR in PibusSimpleRam\n");
	printf("illegal value of the PIBUS OPC field for a WRITE : %0x\n", opc);
	printf("the supported values are : BY0/BY1/BY2/BY3\n");
	printf("                           HW0/HW1/WDU/NOP\n");
	exit(1);
	break;
    } // end switch 
} // end write_seg()

/////////////////////////////////
void PibusSimpleRam::transition()
{
    if (p_resetn == false) 
    {
        m_monitor_ok = false;
        r_fsm_state  = FSM_IDLE;
        for ( size_t seg = 0 ; seg < m_nbseg ; seg++ )
        {
            memset( &r_buf[seg][0], 0, m_segsize[seg] );
            m_loader.load( &r_buf[seg][0], m_segbase[seg], m_segsize[seg] );
            if (IsBigEndian())  
            {
                for( size_t word = 0; word < (m_segsize[seg] >> 2); word++) 
                  r_buf[seg][word] = swap_bytes(r_buf[seg][word]);
            }
        }
        return;
    } // end p_resetn

    switch (r_fsm_state) {
    case FSM_IDLE :
    {
        if (p_sel == true) 
        {
            uint32_t address = ((uint32_t)p_a.read()) & 0xfffffffc; 
            bool error = true;
            for (size_t i = 0 ; ((i < m_nbseg) && (error == true)) ; i++) 
            { 
                if ((address >= m_segbase[i]) && (address < m_segbase[i] + m_segsize[i])) 
                { 
                    error=false;
                    r_index = i;
                    r_address  = address;
                    r_opc   = (int) p_opc.read();
                    
                }  
            }
            if(error == false) 
            {
                r_counter = m_latency;
                if((p_read == true)  && (m_latency == 0))  r_fsm_state = FSM_READ_OK; 
                if((p_read == true)  && (m_latency != 0))  r_fsm_state = FSM_READ_WAIT; 
                if((p_read == false) && (m_latency == 0))  r_fsm_state = FSM_WRITE_OK; 
                if((p_read == false) && (m_latency != 0))  r_fsm_state = FSM_WRITE_WAIT; 
            } 
            else 
            {
                r_fsm_state = FSM_ERROR;
            }
        }
        break;
    }
    case FSM_ERROR :
    {
	r_fsm_state = FSM_IDLE;
        break;
    }
    case FSM_READ_WAIT :
    {
	r_counter = r_counter - 1;
	if(r_counter == 1)  r_fsm_state = FSM_READ_OK; 
        break;
    }
    case FSM_READ_OK :
    {
	if (p_sel == true) 
        {
            uint32_t address = ((uint32_t)p_a.read()) & 0xfffffffc; 
            if ((address < m_segbase[r_index]) ||
                ((address >= (m_segbase[r_index] + m_segsize[r_index])) ||
                (p_read == false))) 
            { 
                r_fsm_state = FSM_ERROR;
            } 
            else 
            {
                r_address = address;	
            } 
        } 
        else 
        {
            r_fsm_state = FSM_IDLE;
        }
        break;
    }
    case FSM_WRITE_WAIT :
    {
        r_counter = r_counter - 1;
        if(r_counter == 1)  r_fsm_state = FSM_WRITE_OK; 
        break;
    }
    case FSM_WRITE_OK :   
    {
	uint32_t data     = (uint32_t)p_d.read(); 
        uint32_t address  = r_address.read();
        uint32_t word     = (r_address.read() - m_segbase[r_index.read()]) >> 2;

        if ( m_monitor_ok )
        {
            if ( (address >= m_monitor_base) and
                 (address <  m_monitor_base + m_monitor_length) )
            {
                std::cout << " RAM Change : address = " << std::hex << address
                          << " / data = " << data << std::endl;
            }
        } 

  	write_seg(r_buf[r_index], word, data, r_opc);
	if (p_sel == true) 
        { 
	    uint32_t address = ((uint32_t)p_a.read()) & 0xfffffffc; 
	    if ((address < m_segbase[r_index]) || 
		    (address >= m_segbase[r_index] + m_segsize[r_index]) ||
		    (p_read == true)) 
            { 
                r_fsm_state = FSM_ERROR;	
            } 
            else 
            {
                r_address = address;
            } 
	} 
        else 
        {	
            r_fsm_state = FSM_IDLE;
	}
        break;
    }
    } // end switch r_fsm_state
} // end transition()

///////////////////////////////
void PibusSimpleRam::genMoore()
{
    switch(r_fsm_state) {
    case FSM_IDLE :  
        break;
    case FSM_ERROR : 
        p_ack = PIBUS_ACK_ERROR;
        break;
    case FSM_READ_WAIT :
        p_ack = PIBUS_ACK_WAIT;
        p_d = 0;
        break;
    case FSM_READ_OK :
        p_ack = PIBUS_ACK_READY;
        p_d = r_buf[r_index][(r_address.read() - m_segbase[r_index]) >> 2];
        break;
    case FSM_WRITE_WAIT :
        p_ack = PIBUS_ACK_WAIT;
        break;
    case FSM_WRITE_OK :
        p_ack = PIBUS_ACK_READY;
        break;
    } 
} // end genMoore()

/////////////////////////////////////////////////
void PibusSimpleRam::printTrace(uint32_t address)
{
    bool error = true;
    size_t index;

    if ( address )
    {
        for (size_t i = 0 ; ((i < m_nbseg) && (error == true)) ; i++) 
        { 
            if ((address >= m_segbase[i]) && (address < m_segbase[i] + m_segsize[i])) 
            { 
                error = false;
                index = i;
                break;
            }  
        }
        if ( not error )
        {
            uint32_t data = r_buf[index][(address - m_segbase[index]) >> 2];
            std::cout << m_name << " : address = " << std::hex << address
                      << " data = " <<  data << std::endl;
        }
        else
        {
            std::cout << " ERROR IN RAM : monitored address out of segment" << std::endl;
        }
    }
    else
    {
        std::cout << m_name << " : " << m_fsm_str[r_fsm_state] << std::endl;
    }
} 

/////////////////////////////////////////////////////////////////
void PibusSimpleRam::startMonitor(uint32_t base, uint32_t length)
{
    m_monitor_ok	= true;
    m_monitor_base	= base;
    m_monitor_length	= length;
}
//////////////////////////////////
void PibusSimpleRam::stopMonitor()
{
    m_monitor_ok	= false;
}

}} // end namespaces
