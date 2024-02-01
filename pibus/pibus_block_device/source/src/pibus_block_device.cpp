////////////////////////////////////////////////////////////////////////////
// File : pibus_block_device.cpp
// Date : 22/03/2010
// author : Alain Greiner 
// This program is released under the GNU public license
// Copyright UPMC - LIP6
////////////////////////////////////////////////////////////////////////////

#include "pibus_block_device.h"
#include <stdint.h>
#include <iostream>
#include <fcntl.h>

namespace soclib { namespace caba {

using namespace soclib::caba;
using namespace soclib::common;

///////////////////////////////////
void PibusBlockDevice::transition()
{
    if(p_resetn.read() == false) 
    {
        r_master_fsm = M_IDLE;
	    r_target_fsm = T_IDLE;
	    r_irq_enable = true;
        r_go         = false;
	return;
    } 

    // The Target FSM controls the following registers:
    // r_target_fsm, r_irq_enable, r_nblocks, r_buf adress, r_lba, r_go, r_read
    switch(r_target_fsm) {
    case T_IDLE:
        if(p_sel.read() == true) 
        { 
	        uint32_t address = (uint32_t)p_a.read();
            bool     read    = p_read.read();
	        if( (address < m_segbase) || (address >= m_segbase + m_segsize) ) 	    r_target_fsm = T_ERROR;
            else if( !read && ((address & 0x1F) == (BLOCK_DEVICE_BUFFER<<2)) ) 		r_target_fsm = T_WRITE_BUFFER;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_BUFFER<<2)) ) 		r_target_fsm = T_READ_BUFFER;
            else if( !read && ((address & 0x1F) == (BLOCK_DEVICE_COUNT<<2)) ) 		r_target_fsm = T_WRITE_COUNT;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_COUNT<<2)) ) 		r_target_fsm = T_READ_COUNT;
            else if( !read && ((address & 0x1F) == (BLOCK_DEVICE_LBA<<2)) ) 		r_target_fsm = T_WRITE_LBA;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_LBA<<2)) ) 		r_target_fsm = T_READ_LBA;
            else if( !read && ((address & 0x1F) == (BLOCK_DEVICE_OP<<2)) ) 		    r_target_fsm = T_WRITE_OP;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_STATUS<<2)) ) 		r_target_fsm = T_READ_STATUS;
            else if( !read && ((address & 0x1F) == (BLOCK_DEVICE_IRQEN<<2)) ) 		r_target_fsm = T_WRITE_IRQEN;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_IRQEN<<2)) ) 		r_target_fsm = T_READ_IRQEN;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_SIZE<<2)) ) 		r_target_fsm = T_READ_SIZE;
            else if(  read && ((address & 0x1F) == (BLOCK_DEVICE_BLOCK<<2)) ) 		r_target_fsm = T_READ_BLOCK;
            else                                                        	        r_target_fsm = T_ERROR;
        }
        break;
    case T_WRITE_BUFFER:
        if( r_master_fsm == M_IDLE ) r_buf_address = p_d.read();
        r_target_fsm    = T_IDLE;
        break;
    case T_WRITE_COUNT:
        if( r_master_fsm == M_IDLE ) r_nblocks = p_d.read();
        r_target_fsm    = T_IDLE;
        break;
    case T_WRITE_LBA:
        if( r_master_fsm == M_IDLE ) r_lba = p_d.read();
        r_target_fsm    = T_IDLE;
        break;
    case T_WRITE_OP:
        if( r_master_fsm == M_IDLE ) 
        {
            if(p_d.read() == BLOCK_DEVICE_READ)
            {
                r_read = true;
                r_go = true;
            }
            if(p_d.read() == BLOCK_DEVICE_WRITE)
            {
                r_read = false;
                r_go = true;
            }
        }
        r_target_fsm    = T_IDLE;
        break;
    case T_WRITE_IRQEN:
        r_irq_enable    = (p_d.read() != 0);
        r_target_fsm    = T_IDLE;
        break;

    case T_READ_BUFFER:
    case T_READ_COUNT:
    case T_READ_LBA:
    case T_READ_IRQEN:
    case T_READ_SIZE:
    case T_READ_BLOCK:
    case T_ERROR:
        r_target_fsm    = T_IDLE;
        break;

    case T_READ_STATUS:
        r_target_fsm    = T_IDLE;
        if( (r_master_fsm == M_READ_SUCCESS ) ||
            (r_master_fsm == M_READ_ERROR   ) ||
            (r_master_fsm == M_WRITE_SUCCESS) ||
            (r_master_fsm == M_WRITE_ERROR  ) )	  r_go = false;
          
        break;
    } // end switch target fsm
	
    // The master FSM controls the following registers :
    // r_master_fsm, r_word_count, r_block_count, m_local_buffer 
    switch(r_master_fsm) {
    case M_IDLE :
	if (r_read && r_go) 
        {
            r_block_count = 0;
            r_latency_count = m_latency;
            r_master_fsm = M_READ_BLOCK;
        }
	if (!r_read && r_go) 
        {
            r_block_count = 0;
            r_latency_count = m_latency;
            r_master_fsm = M_WRITE_REQ;
        }
        break;
    case M_READ_BLOCK:  // read one block after waiting m_latency cycles
        if(r_latency_count == 0)
        {
            r_latency_count = m_latency;
            ::lseek(m_fd, (r_lba + r_block_count)*m_block_size, SEEK_SET);
            if( ::read(m_fd, m_local_buffer, m_block_size) < 0 )  r_master_fsm = M_READ_ERROR;
            else                                                  r_master_fsm = M_READ_REQ;
        }
        else
        {
            r_latency_count = r_latency_count - 1;
        }
        break;
    case M_READ_REQ:
	    if(p_gnt.read() == true) 
        {
            r_master_fsm = M_READ_AD;
            r_word_count = 0;
        }
        break;
    case M_READ_AD:
	    r_word_count  = r_word_count + 1;
		r_master_fsm = M_READ_DTAD;
        break;
    case M_READ_DTAD:
        if ( p_tout.read() or (p_ack.read() == PIBUS_ACK_ERROR) )
        {
            r_master_fsm = M_READ_ERROR;
        }
	    else if ( p_ack.read() == PIBUS_ACK_READY ) 
        {
	        r_word_count  = r_word_count + 1;
	        if(r_word_count == (m_block_size>>2)-1) r_master_fsm = M_READ_DT;
	    }
        break;
    case M_READ_DT:
	    if ( (p_ack.read() == PIBUS_ACK_ERROR) or p_tout.read() )  
        {
            r_master_fsm = M_READ_ERROR;
        }
	    else if ( p_ack.read() == PIBUS_ACK_READY )  
        {
            r_word_count = 0;
            r_master_fsm = M_READ_TEST;
        }
        break;
    case M_READ_TEST:
        if( r_block_count == r_nblocks - 1 )
        {
            r_block_count = 0;
            r_master_fsm = M_READ_SUCCESS;
         }
        else
        {
            r_block_count = r_block_count + 1;
            r_master_fsm = M_READ_BLOCK;
        }
        break;
    case M_READ_SUCCESS:
        if( !r_go ) r_master_fsm = M_IDLE;
        break;
    case M_READ_ERROR:
        if( !r_go ) r_master_fsm = M_IDLE;
        break;

    case M_WRITE_REQ:
	    if(p_gnt.read() == true) 
        {
            r_master_fsm = M_WRITE_AD;
            r_word_count = 0;
        }
        break;
    case M_WRITE_AD:
	    r_word_count  = r_word_count + 1;
		r_master_fsm = M_WRITE_DTAD;
        break;
    case M_WRITE_DTAD:
        if ( p_tout.read() or (p_ack.read() == PIBUS_ACK_ERROR) ) 
        {
            r_master_fsm = M_WRITE_ERROR;
        }
        else if ( p_ack.read() == PIBUS_ACK_READY ) 
        {
            m_local_buffer[r_word_count - 1] = p_d.read();
	        r_word_count  = r_word_count + 1;
	        if(r_word_count == (m_block_size>>2)-1) r_master_fsm = M_WRITE_DT;
	    }
        break;
    case M_WRITE_DT:
        if ( p_tout.read() or (p_ack.read() == PIBUS_ACK_ERROR) ) 
        {
            r_master_fsm = M_WRITE_ERROR;
        }
        else if ( p_ack.read() == PIBUS_ACK_READY ) 
        {
            m_local_buffer[r_word_count - 1] = p_d.read();
            r_master_fsm = M_WRITE_BLOCK;
            r_word_count = 0;
        }
        break;
    case M_WRITE_BLOCK:
        if(r_latency_count == 0)
        {
            r_latency_count = m_latency;
            ::lseek(m_fd, (r_lba + r_block_count)*m_block_size, SEEK_SET);
            if( ::write(m_fd, m_local_buffer, m_block_size) < 0 )  r_master_fsm = M_WRITE_ERROR;
            else                                                   r_master_fsm = M_WRITE_TEST;
        }
        else
        {
            r_latency_count = r_latency_count - 1;
        }
        break;
    case M_WRITE_TEST:
        if( r_block_count == r_nblocks - 1 )
        {
            r_block_count = 0;
            r_master_fsm = M_WRITE_SUCCESS;
        }
        else
        {
            r_block_count = r_block_count + 1;
            r_master_fsm = M_WRITE_REQ;
        }
        break;
    case M_WRITE_SUCCESS:
        if( !r_go ) r_master_fsm = M_IDLE;
        break;
    case M_WRITE_ERROR:
        if( !r_go ) r_master_fsm = M_IDLE;
        break;
    } // end switch r_master_fsm

}  // end transition

/////////////////////////////////
void PibusBlockDevice::genMoore()
{

    // p_ack & p_d signals 
    switch(r_target_fsm) {
    case T_IDLE:
        break;
    case T_READ_STATUS:
	    p_ack = PIBUS_ACK_READY;
	    if     (r_master_fsm == M_IDLE)       	    p_d = BLOCK_DEVICE_IDLE;
        else if(r_master_fsm == M_READ_SUCCESS)    	p_d = BLOCK_DEVICE_READ_SUCCESS;
        else if(r_master_fsm == M_WRITE_SUCCESS)   	p_d = BLOCK_DEVICE_WRITE_SUCCESS;
        else if(r_master_fsm == M_READ_ERROR)   	p_d = BLOCK_DEVICE_READ_ERROR;
        else if(r_master_fsm == M_WRITE_ERROR)  	p_d = BLOCK_DEVICE_WRITE_ERROR;
        else                                    	p_d = BLOCK_DEVICE_BUSY;
        break;
    case T_READ_BUFFER:
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)r_buf_address;
        break;
    case T_READ_COUNT:
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)r_nblocks;
        break;
    case T_READ_LBA:
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)r_lba;
        break;
    case T_READ_IRQEN:
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)r_irq_enable;
        break;
    case T_READ_SIZE:
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)m_device_size;
        break;
    case T_READ_BLOCK:
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)m_block_size;
        break;
    case T_ERROR:
        p_ack = PIBUS_ACK_ERROR;
        break;
    default:
        p_ack = PIBUS_ACK_READY;
        break;
    } // end switch target fsm

    // p_req signal
    if((r_master_fsm == M_READ_REQ) || (r_master_fsm == M_WRITE_REQ)) 	p_req = true;
    else								                                p_req = false;

    // p_a, p_lock, p_read, p_opc signals
    if((r_master_fsm == M_READ_AD) || (r_master_fsm == M_READ_DTAD)) 
    {
        p_a   = (uint32_t)r_buf_address + 
                (uint32_t)(r_block_count*m_block_size) + 
                (uint32_t)(r_word_count*4);
        p_opc = PIBUS_OPC_WDU;
        p_read = false;
        if(r_word_count == (m_block_size>>2)-1) 	p_lock = false;
        else		                            	p_lock = true;
    }
    if((r_master_fsm == M_WRITE_AD) || (r_master_fsm == M_WRITE_DTAD)) 
    {
        p_a   = (uint32_t)r_buf_address + 
                (uint32_t)(r_block_count*m_block_size) + 
                (uint32_t)(r_word_count*4);
        p_opc = PIBUS_OPC_WDU;
        p_read = true;
        if(r_word_count == (m_block_size>>2)-1) 	p_lock = false;
        else			                            p_lock = true;
    }

    // p_d signal
    if((r_master_fsm == M_READ_DTAD) || (r_master_fsm == M_READ_DT)) 
        p_d = m_local_buffer[r_word_count - 1];

    // IRQ signal
    if(((r_master_fsm == M_READ_SUCCESS)    ||
        (r_master_fsm == M_WRITE_SUCCESS)   ||
        (r_master_fsm == M_READ_ERROR)      ||
        (r_master_fsm == M_WRITE_ERROR) ) &&  r_irq_enable) p_irq = true;
    else			                            p_irq = false;
} // end GenMoore()

/////////////////////////////////////////////////////////////
PibusBlockDevice::PibusBlockDevice( sc_module_name	   	name, 
	         	                    uint32_t	    	tgtid,
	  		                        PibusSegmentTable	&segtab,
	  		                        char*       	    filename,
	  		                        uint32_t	        block_size,
                                    uint32_t          	latency)

    : m_name(name),
      m_tgtid(tgtid),
      m_latency(latency),
      m_block_size(block_size),
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

    strcpy (m_master_str[0], "IDLE");
    strcpy (m_master_str[1], "READ_BLOCK");
    strcpy (m_master_str[2], "READ_REQ");
    strcpy (m_master_str[3], "READ_AD");
    strcpy (m_master_str[4], "READ_DTAD");
    strcpy (m_master_str[5], "READ_DT");
    strcpy (m_master_str[6], "READ_SUCCESS");
    strcpy (m_master_str[7], "READ_ERROR");
    strcpy (m_master_str[8], "WRITE_REQ");
    strcpy (m_master_str[9], "WRITE_AD");
    strcpy (m_master_str[10], "WRITE_DTAD");
    strcpy (m_master_str[11], "WRITE_DT");
    strcpy (m_master_str[12], "WRITE_BLOCK");
    strcpy (m_master_str[13], "WRITE_SUCCESS");
    strcpy (m_master_str[14], "WRITE_ERROR");
    strcpy (m_master_str[15], "READ_TEST");
    strcpy (m_master_str[16], "WRITE_TEST");

    strcpy (m_target_str[0], "IDLE");
    strcpy (m_target_str[1], "WRITE_BUFFER");
    strcpy (m_target_str[2], "READ_BUFFER");
    strcpy (m_target_str[3], "WRITE_COUNT");
    strcpy (m_target_str[4], "READ_COUNT");
    strcpy (m_target_str[5], "WRITE_LBA");
    strcpy (m_target_str[6], "READ_LBA");
    strcpy (m_target_str[7], "WRITE_OP");
    strcpy (m_target_str[8], "READ_STATUS");
    strcpy (m_target_str[9], "WRITE_IRQEN");
    strcpy (m_target_str[10], "READ_IRQEN");
    strcpy (m_target_str[11], "READ_SIZE");
    strcpy (m_target_str[12], "READ_BLOCK");
    strcpy (m_target_str[13], "ERROR");

    if( (m_block_size != 128) && 
        (m_block_size != 256) && 
        (m_block_size != 512) && 
        (m_block_size != 1024) )
    {
	    printf("ERROR in component PibusBlockDevice : %s\n", m_name);
	    printf("The block size must be 128, 256, 512 or 1024 bytes\n");
        exit(1);
    }

    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();

    if(m_segsize < 32) 
    {
	    printf("ERROR in component PibusBlockDevice : %s\n", m_name);
	    printf("The size of the segment cannot be smaller than 32 bytes\n");
        exit(1);
    }

    if((m_segbase & 0x0000001F) != 0) 
    {
	    printf("ERROR in component PibusBlockDevice : %s\n", m_name);
	    printf("The base adress of the segment must be multiple of 32 bytes\n");
        exit(1);
    }

    m_fd = ::open(filename, O_RDWR);
    if ( m_fd < 0 ) 
    {
        std::cout << "Error : block device " << m_name << std::endl;
        std::cout << "Unable to open file " << filename << std::endl;
        exit(1);
    }

    m_device_size = lseek(m_fd, 0, SEEK_END) / m_block_size;
    if ( m_device_size > ((uint64_t)1<<32) ) 
    {
        std::cout << "Warning: block device " << m_name << std::endl;
        std::cout << "The file " << filename << std::endl;
        std::cout << "has more blocks than addressable with the 32 bits PIBUS address" << std::endl;
        m_device_size = ((uint64_t)1<<32);
    }

    m_local_buffer = new uint32_t[m_block_size>>2];

    std::cout << std::endl << "Instanciation of PibusBlockDevice : " << m_name << std::endl;
    std::cout << "    file_name  = " << filename << std::endl;
    std::cout << "    block_size = " << std::dec << m_block_size << std::endl;
    std::cout << "    latency    = " << std::dec << m_latency << std::endl;
    std::cout << "    segment " << m_segname << std::hex
              << " | base = 0x" << m_segbase
              << " | size = 0x" << m_segsize << std::endl;

} // end constructor

///////////////////////////////////
void PibusBlockDevice::printTrace()
{
    std::cout << m_name << "_target : " << m_target_str[r_target_fsm] << "   "
              << m_name << "_master : " << m_master_str[r_master_fsm] 
              << "    irq_enable = " << r_irq_enable.read() 
              << "    block_count = " << r_block_count.read() << std::endl; 
}


}} // end namespace

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

