///////////////////////////////////////////////////////////
// File : pibus_frame_buffer.cpp
// Author : Alain Greiner
// Date : 26/02/2010
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
///////////////////////////////////////////////////////////

#include "pibus_frame_buffer.h"

namespace soclib { namespace caba {

using namespace sc_core;
using namespace soclib::caba;
using namespace soclib::common;

////////////////////////////////////////////////////////////////
PibusFrameBuffer::PibusFrameBuffer (sc_module_name	name,
				uint32_t		tgtid,
				PibusSegmentTable	&segtab,
				uint32_t		latency,
				uint32_t		width,
				uint32_t		height,
				int			subsampling)
    : m_name(name),
      m_tgtid(tgtid),
      m_latency(latency),
      m_fb_controller((const char*)name, width, height, subsampling),
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
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();
    
    if((m_segbase & 0x00000003) != 0x0) 
    {
	printf("ERROR in component PibusFrameBuffer %s\n", m_name);
	printf("The segment base address must be word aligned\n");
	exit(1);
    }
    if(m_segsize < width*height)
    {
	printf("ERROR in component PibusFrameBuffer %s\n", m_name);
	printf("The segment size must be at least : width * height\n");
	exit(1);
    }

    strcpy(m_fsm_str[0], "IDLE");
    strcpy(m_fsm_str[1], "READ_WAIT");
    strcpy(m_fsm_str[2], "READ_OK");
    strcpy(m_fsm_str[3], "WRITE_WAIT");
    strcpy(m_fsm_str[4], "WRITE_OK");
    strcpy(m_fsm_str[5], "ERROR");

    std::cout << std::endl << "Instanciation of PibusFrameBuffer : " << m_name << std::endl;
    std::cout << "    latency = " << latency << std::endl;
    std::cout << "    segment " << m_segname << std::hex
              << " | base = 0x" << m_segbase
              << " | size = 0x" << m_segsize << std::endl;

} // end constructor

////////////////////////////////////////////////////////////////////////
void write_buf(uint32_t* buf, size_t index, uint32_t data, uint32_t opc)
{
      
    // The memory organisation is litle endian
    switch (opc) {
    case PIBUS_OPC_BY0 :  // write byte 0 
	buf[index] = (buf[index] & 0xFFFFFF00) | (data & 0x000000FF);
        break;
    case PIBUS_OPC_BY1 :  // write byte 1 
	buf[index] = (buf[index] & 0xFFFF00FF) | (data & 0x0000FF00);
        break;
    case PIBUS_OPC_BY2 :  // write byte 2 
	buf[index] = (buf[index] & 0xFF00FFFF) | (data & 0x00FF0000);
        break;
    case PIBUS_OPC_BY3 :  // write byte 3 
	buf[index] = (buf[index] & 0x00FFFFFF) | (data & 0xFF000000);
        break;
    case PIBUS_OPC_HW0 :  // write lower half  
	buf[index] = (buf[index] & 0xFFFF0000) | (data & 0x0000FFFF);
        break;
    case PIBUS_OPC_HW1 :  // write upper half 
	buf[index] = (buf[index] & 0x0000FFFF) | (data & 0xFFFF0000);
        break;
    case PIBUS_OPC_WDU :  // write word
	buf[index] = data;
        break;
    case PIBUS_OPC_NOP :  // no write  
        break;
    default :
	printf("ERROR in PibusFrameBuffer\n");
	printf("illegal value of the PIBUS OPC field for a WRITE : %0x\n", opc);
	printf("the supported values are : BY0/BY1/BY2/BY3\n");
	printf("                           HW0/HW1/WDU/NOP\n");
	exit(1);
	break;
    } // end switch 
} // end write_buf()

/////////////////////////////////
void PibusFrameBuffer::transition()
{
    if (p_resetn == false) 
    {
        r_fsm_state = FSM_IDLE;
        r_display = 0;
        return;
    } // end p_resetn

    switch (r_fsm_state) {
    case FSM_IDLE :
    {
        if (p_sel == true) 
        {
            uint32_t address = ((uint32_t)p_a.read()) & 0xfffffffc; 
            if ((address >= m_segbase) && (address < m_segbase + m_segsize)) 
                { 
                r_word  = (address - m_segbase) >> 2;
                r_opc   = (int) p_opc.read();
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
            if ((address < m_segbase) ||
                ((address >= (m_segbase + m_segsize)) ||
                (p_read == false))) 
            { 
                r_fsm_state = FSM_ERROR;
            } 
            else 
            {
                r_word = (address - m_segbase) >> 2;	
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
        uint32_t* buf     = m_fb_controller.surface();
  	write_buf(buf, r_word, data, r_opc);
	if (p_sel == true) 
        { 
	    uint32_t address = ((uint32_t)p_a.read()) & 0xfffffffc; 
	    if ((address < m_segbase) || 
		    (address >= m_segbase + m_segsize) ||
		    (p_read == true)) 
            { 
                r_fsm_state = FSM_ERROR;	
            } 
            else 
            {
                r_word = (address - m_segbase) >> 2;
            } 
	} 
        else 
        {	
            r_fsm_state = FSM_IDLE;
	}
        break;
    }
    } // end switch r_fsm_state

    if(r_display == 0)
    {
        m_fb_controller.update();
        r_display = 1000;
    }
    else
    {
        r_display = r_display - 1;
    }
} // end transition()

///////////////////////////////
void PibusFrameBuffer::genMoore()
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
    {
        p_ack = PIBUS_ACK_READY;
        uint32_t* buf = m_fb_controller.surface();
        p_d = buf[r_word];
        break;
    }
    case FSM_WRITE_WAIT :
        p_ack = PIBUS_ACK_WAIT;
        break;
    case FSM_WRITE_OK :
        p_ack = PIBUS_ACK_READY;
        break;
    } 
} // end genMoore()

/////////////////////////////////
void PibusFrameBuffer::printTrace()
{
    std::cout << m_name << " : " << m_fsm_str[r_fsm_state] << std::endl;
} // end print()

#ifdef SOCVIEW

/////////////////////////////////////////////////////
void PibusFrameBuffer::registerDebug(SocviewDebugger db)
{
db.add(r_fsm_state  , m_name + ".r_fsm_state");
db.add(r_counter    , m_name + ".r_counter");
db.add(r_index      , m_name + ".r_index");
db.add(r_word       , m_name + ".r_word");
db.add(r_opc        , m_name + ".r_opc");
}

#endif

}} // end namespaces
