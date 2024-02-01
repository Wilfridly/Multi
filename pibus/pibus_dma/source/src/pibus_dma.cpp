////////////////////////////////////////////////////////////////////////////
// File : pibus_dma.cpp
// Date : 28/03/2010
// author : Alain Greiner 
// This program is released under the GNU public license
// Copyright UPMC - LIP6
////////////////////////////////////////////////////////////////////////////

#include "pibus_dma.h"

namespace soclib { namespace caba {

using namespace soclib::caba;
using namespace soclib::common;

///////////////////////////
void PibusDma::transition()
{
    if(p_resetn.read() == false) 
    {
 	r_master_fsm  = DMA_IDLE;
	r_target_fsm  = TGT_IDLE;
	r_stop        = true;
	r_irq_disable = 0;
	return;
    } 

    // The Target FSM controls the following registers:
    // r_target_fsm , r_source , r_dest , r_nwords, r_stop 

    switch(r_target_fsm) {
    case TGT_IDLE:
        if(p_sel.read() == true) 
        { 
	    uint32_t address = (uint32_t)p_a.read();
            bool     read    = p_read.read();
	    if( (address < m_segbase) || (address >= m_segbase + m_segsize) ) 	r_target_fsm = TGT_ERROR;
            else if( !read && ((address & 0x1F) == (DMA_SRC << 2)) ) 		r_target_fsm = TGT_WRITE_SOURCE;
            else if( !read && ((address & 0x1F) == (DMA_DST << 2)) ) 		r_target_fsm = TGT_WRITE_DEST;
            else if( !read && ((address & 0x1F) == (DMA_LEN << 2)) ) 		r_target_fsm = TGT_WRITE_NWORDS;
            else if( !read && ((address & 0x1F) == (DMA_RST << 2)) ) 		r_target_fsm = TGT_WRITE_RESET;
            else if( !read && ((address & 0x1F) == (DMA_IRQ << 2)) ) 		r_target_fsm = TGT_WRITE_NOIRQ;
            else if(  read && ((address & 0x1F) == (DMA_SRC << 2)) ) 		r_target_fsm = TGT_READ_SOURCE;
            else if(  read && ((address & 0x1F) == (DMA_DST << 2)) ) 		r_target_fsm = TGT_READ_DEST;
            else if(  read && ((address & 0x1F) == (DMA_LEN << 2)) ) 		r_target_fsm = TGT_READ_STATUS;
            else if(  read && ((address & 0x1F) == (DMA_IRQ << 2)) ) 		r_target_fsm = TGT_READ_NOIRQ;
            else                                                        	r_target_fsm = TGT_ERROR;
        }
        break;
    case TGT_WRITE_SOURCE:
        if(r_master_fsm == DMA_IDLE)
        {
            if( (p_d.read() & 0x3) != 0 )
            {
	        printf("ERROR in component PibusDma : %s\n",m_name);
	        printf("The source buffer base address must be word aligned\n");
                exit(1);
            }
            r_source     = p_d.read() & 0xFFFFFFFC;
        }
        r_target_fsm = TGT_IDLE;
        break;
    case TGT_WRITE_DEST:
        if(r_master_fsm == DMA_IDLE)
        {
            if( (p_d.read() & 0x3) != 0 )
            {
	        printf("ERROR in component PibusDma : %s\n",m_name);
	        printf("The destination buffer base address must be word aligned\n");
                exit(1);
            }
            r_dest       = p_d.read() & 0xFFFFFFFC;
        }
        r_target_fsm = TGT_IDLE;
        break;
    case TGT_WRITE_NWORDS:
        if(r_master_fsm == DMA_IDLE)
        {
            if( (p_d.read() & 0x3) != 0 )
            {
	        printf("ERROR in component PibusDma : %s\n",m_name);
	        printf("The transfer length must be multiple of 4 bytes\n");
                exit(1);
            }
            r_nwords     = p_d.read() >> 2;
            r_stop       = false;
        }
        r_target_fsm = TGT_IDLE;
        break;
    case TGT_WRITE_RESET:
        r_stop       = true;
        r_target_fsm = TGT_IDLE;
        break;
    case TGT_WRITE_NOIRQ:
        r_irq_disable = p_d.read();
        r_target_fsm = TGT_IDLE;
        break;

    case TGT_ERROR:
    case TGT_READ_STATUS:
    case TGT_READ_SOURCE:
    case TGT_READ_DEST:
    case TGT_READ_NOIRQ:
        r_target_fsm = TGT_IDLE;
        break;
    } // end switch target fsm
	
    // The master FSM controls the following registers :
    // r_master_fsm, r_read_ptr, r_write_ptr, r_index, r_count
    // Soft Reset : After each burst (read or write), the master FSM
    // test the r_stop flip-flop to stop the ongoing transfer if requested. 
    // It goes to the DMA_SUCCESS state when the tranfer is isuccessfully 
    // completed, to assert the IRQ signaling the completion.
    // In case of bus error, it goes to the DMA_WRITE_ERROR or DMA_READ_ERROR
    // state to assert the IRQ signaling the completion.

    switch(r_master_fsm) {
    case DMA_IDLE :
	if (r_stop == false) 
        {
            r_master_fsm = DMA_READ_REQ;
            r_read_ptr   = r_source;
            r_write_ptr  = r_dest;
            r_count      = r_nwords;
            r_index      = 0;
            if(r_nwords < m_burst)  	r_max  = r_nwords;
            else 			r_max  = m_burst;
	}
        break;
    case DMA_READ_REQ :
	if(p_gnt.read() == true) r_master_fsm = DMA_READ_AD;
        break;
    case DMA_READ_AD :
	r_index    	= r_index + 1;
	r_count    	= r_count - 1;
	r_read_ptr 	= r_read_ptr + 4;
	if(r_index == r_max-1)		r_master_fsm = DMA_READ_DT;
	else				r_master_fsm = DMA_READ_DTAD;
        break;
    case DMA_READ_DTAD :
	if(p_ack.read() == PIBUS_ACK_READY) 
        {
            m_buf[r_index] = (uint32_t)p_d.read();
            r_index 	= r_index + 1;
            r_count 	= r_count - 1;
            r_read_ptr 	= r_read_ptr + 4;
	    if(r_index == r_max-1)	r_master_fsm = DMA_READ_DT;
	}
        else if(p_ack.read() == PIBUS_ACK_ERROR) 
        {
            r_master_fsm = DMA_READ_ERROR;
        }
        break;
   case DMA_READ_DT :
	if(p_ack.read() == PIBUS_ACK_READY) 
        {
            m_buf[r_index] = (uint32_t)p_d.read();
            r_index      = 0;
	    if(r_stop == true) 	r_master_fsm = DMA_IDLE; 
            else  		r_master_fsm = DMA_WRITE_REQ;
	}
        else if(p_ack.read() == PIBUS_ACK_ERROR) 
        {
            r_master_fsm = DMA_READ_ERROR;
        }
        break;
    case DMA_WRITE_REQ :
	if(p_gnt.read() == true) r_master_fsm = DMA_WRITE_AD;
        break;
    case DMA_WRITE_AD :
	r_index 	= r_index + 1;
	r_write_ptr   	= r_write_ptr + 4;
	if(r_index == r_max - 1)	r_master_fsm = DMA_WRITE_DT;
	else				r_master_fsm = DMA_WRITE_DTAD;
        break;
    case DMA_WRITE_DTAD :
        if(p_ack.read() == PIBUS_ACK_READY) 
        {
            r_index = r_index + 1;
            r_write_ptr = r_write_ptr + 4;
            if(r_index == r_max - 1) 	r_master_fsm = DMA_WRITE_DT;
	}
        else if(p_ack.read() == PIBUS_ACK_ERROR) 
        {
            r_master_fsm = DMA_WRITE_ERROR;
        }
        break;
    case DMA_WRITE_DT :
        if(p_ack.read() == PIBUS_ACK_READY) 
        {
            r_index = 0;
            if(r_stop == true)  	r_master_fsm = DMA_IDLE; 
            else if(r_count == 0)	r_master_fsm = DMA_SUCCESS; 
            else 
            {
                if(r_count < m_burst)  	r_max = r_count;
                else			r_max = m_burst;
                r_master_fsm = DMA_READ_REQ;
            }
        }
        if(p_ack.read() == PIBUS_ACK_ERROR) 
        {
            r_master_fsm = DMA_WRITE_ERROR;
        }
        break;
    case DMA_SUCCESS :
    case DMA_READ_ERROR :
    case DMA_WRITE_ERROR :
        if(r_stop == true) r_master_fsm = DMA_IDLE;
        break;
    } // end switch r_master_fsm

}  // end transition

/////////////////////////
void PibusDma::genMoore()
{

    // p_ack & p_d signals 
    switch(r_target_fsm) {
    case TGT_IDLE:
        break;
    case TGT_READ_STATUS:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_master_fsm;
        break;
    case TGT_READ_SOURCE:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_source;
        break;
    case TGT_READ_DEST:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_dest;
        break;
    case TGT_READ_NOIRQ:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_irq_disable;
        break;
    case TGT_ERROR:
	p_ack = PIBUS_ACK_ERROR;
        break;
    default:
	p_ack = PIBUS_ACK_READY;
        break;
    } // end switch target fsm

    // p_req signal
    if((r_master_fsm == DMA_READ_REQ) || (r_master_fsm == DMA_WRITE_REQ)) 	p_req = true;
    else									p_req = false;

    // p_a, p_lock, p_read, p_opc signals
    if((r_master_fsm == DMA_READ_AD) || (r_master_fsm == DMA_READ_DTAD)) 
    {
	p_a   = (uint32_t)r_read_ptr;
	p_opc = PIBUS_OPC_WDU;
	p_read = true;
	if(r_index == r_max-1) 	p_lock = false;
	else			p_lock = true;
    }
    if((r_master_fsm == DMA_WRITE_AD) || (r_master_fsm == DMA_WRITE_DTAD)) 
    {
	p_a   = (uint32_t)r_write_ptr;
	p_opc = PIBUS_OPC_WDU;
	p_read = false;
	if(r_index == r_max-1) 	p_lock = false;
	else			p_lock = true;
    }

    // p_d signal
    if((r_master_fsm == DMA_WRITE_DTAD) || (r_master_fsm == DMA_WRITE_DT)) p_d = (uint32_t)m_buf[r_index];

    // IRQ signal
    if(((r_master_fsm == DMA_SUCCESS)     ||
        (r_master_fsm == DMA_READ_ERROR)  ||
        (r_master_fsm == DMA_WRITE_ERROR) ) && (r_irq_disable == 0)) 	p_irq = true;
    else							        p_irq = false;
} // end GenMoore()

///////////////////////////////////////////////////////
PibusDma::PibusDma(sc_module_name		name, 
	         	uint32_t		tgtid,
	  		PibusSegmentTable	&segtab,
	  		uint32_t		burst)
    : m_name(name),
      m_tgtid(tgtid),
      m_burst(burst),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_req("p_req"),
      p_gnt("p_gnt"),
      p_sel("p_sel"),
      p_a("p_a"),
      p_read("p_read"),
      p_opc("p_opc"),
      p_lock("p_lock"),
      p_ack("p_ack"),
      p_d("p_d"),
      p_tout("p_tout"),
      p_irq("p_irq") 
{
    SC_METHOD(transition);
    sensitive_pos << p_ck;

    SC_METHOD(genMoore);
    sensitive_neg << p_ck;

    strcpy (m_master_str[0], "SUCCESS");
    strcpy (m_master_str[1], "READ_ERROR");
    strcpy (m_master_str[2], "WRITE_ERROR");
    strcpy (m_master_str[3], "IDLE");
    strcpy (m_master_str[4], "READ_REQ");
    strcpy (m_master_str[5], "READ_AD");
    strcpy (m_master_str[6], "READ_DTAD");
    strcpy (m_master_str[7], "READ_DT");
    strcpy (m_master_str[8], "WRITE_REQ");
    strcpy (m_master_str[9], "WRITE_AD");
    strcpy (m_master_str[10], "WRITE_DTAD");
    strcpy (m_master_str[11], "WRITE_DT");

    strcpy (m_target_str[0], "IDLE");
    strcpy (m_target_str[1], "WRITE_SOURCE");
    strcpy (m_target_str[2], "WRITE_DEST");
    strcpy (m_target_str[3], "WRITE_NWORDS");
    strcpy (m_target_str[4], "WRITE_RESET");
    strcpy (m_target_str[5], "WRITE_NOIRQ");
    strcpy (m_target_str[6], "READ_SOURCE");
    strcpy (m_target_str[7], "READ_DEST");
    strcpy (m_target_str[8], "READ_STATUS");
    strcpy (m_target_str[9], "READ_NOIRQ");
    strcpy (m_target_str[10], "ERROR");

    if((m_burst > 1024) || (m_burst < 1)) 
    {
	printf("ERROR in component PibusDma : %s\n",m_name);
	printf("m_burst must be larger than 0 and no larger than 1024\n");
        exit(1);
    }

    m_buf = new uint32_t[m_burst];

    // get segment base address and segment size
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();

    if(m_segsize < 32) 
    {
	printf("ERROR in component PibusDma : %s\n", m_name);
	printf("The size of the segment cannot be smaller than 32 bytes\n");
        exit(1);
    }

    if((m_segbase & 0x0000001F) != 0) 
    {
	printf("ERROR in component PibusDma : %s\n", m_name);
	printf("The base adress of the segment must be multiple of 32 bytes\n");
        exit(1);
    }
    std::cout << std::endl << "Instanciation of PibusDma : " << m_name << std::endl;
    std::cout << "    burst length = " << m_burst << std::endl;
    std::cout << "    segment " << m_segname << std::hex
              << " | base = 0x" << m_segbase
              << " | size = 0x" << m_segsize << std::endl;

} // end constructor

////////////////////////////////
void PibusDma::printTrace()
{
    std::cout << m_name << "_target : " << m_target_str[r_target_fsm] << "   "
              << m_name << "_master : " << m_master_str[r_master_fsm] 
              << " / r_source = " << std::hex << r_source.read() 
              << " / r_dest = " << std::hex << r_dest.read()  
              << " / wcount = " << std::dec << r_count.read() << std::endl;
}


}} // end namespace
