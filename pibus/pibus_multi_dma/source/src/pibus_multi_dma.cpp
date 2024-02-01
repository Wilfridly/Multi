////////////////////////////////////////////////////////////////////////////
// File : pibus_multi_dma.cpp
// Date : 01/01/2012
// author : Alain Greiner 
// This program is released under the GNU public license
// Copyright UPMC - LIP6
////////////////////////////////////////////////////////////////////////////

#include "pibus_multi_dma.h"
#include "alloc_elems.h"

namespace soclib { namespace caba {

using namespace soclib::caba;
using namespace soclib::common;

////////////////////////////////
void PibusMultiDma::transition()
{
    if(p_resetn.read() == false) 
    {
 	r_master_fsm  = MST_IDLE;
	r_target_fsm  = TGT_IDLE;
        for ( size_t k=0 ; k<m_channels ; k++ )
        {
            r_channel_fsm[k]      = CHANNEL_IDLE;
	    r_channel_active[k]   = false;
            r_channel_done[k]     = false;
            r_channel_error[k]    = false;
	    r_channel_noirq[k]    = false;
        }
	return;
    } 

    // The Target FSM controls the following registers:
    // r_target_fsm, r_target_index, and the r_channel_*[k] registers 
    // when the corresponding channel FSM is IDLE.

    switch( r_target_fsm.read() ) {
    case TGT_IDLE:
    {
        if(p_sel.read() == true) 
        { 
	    uint32_t address = (uint32_t)p_a.read();
            bool     read    = p_read.read();
	    if( (address < m_segbase) || (address >= m_segbase + m_segsize) ) 	r_target_fsm = TGT_ERROR;
            else if( !read && ((address & 0x1F) == (DMA_SRC << 2)) ) 		r_target_fsm = TGT_WRITE_SOURCE;
            else if( !read && ((address & 0x1F) == (DMA_DST << 2)) ) 		r_target_fsm = TGT_WRITE_DEST;
            else if( !read && ((address & 0x1F) == (DMA_LEN << 2)) ) 		r_target_fsm = TGT_WRITE_LENGTH;
            else if( !read && ((address & 0x1F) == (DMA_RST << 2)) ) 		r_target_fsm = TGT_WRITE_RESET;
            else if( !read && ((address & 0x1F) == (DMA_IRQ << 2)) ) 		r_target_fsm = TGT_WRITE_NOIRQ;
            else if(  read && ((address & 0x1F) == (DMA_SRC << 2)) ) 		r_target_fsm = TGT_READ_SOURCE;
            else if(  read && ((address & 0x1F) == (DMA_DST << 2)) ) 		r_target_fsm = TGT_READ_DEST;
            else if(  read && ((address & 0x1F) == (DMA_LEN << 2)) ) 		r_target_fsm = TGT_READ_STATUS;
            else if(  read && ((address & 0x1F) == (DMA_IRQ << 2)) ) 		r_target_fsm = TGT_READ_NOIRQ;
            else                                                        	r_target_fsm = TGT_ERROR;

            // get channel index
            r_target_index = (address >> 5) & 0xF;
        }
        break;
    }
    case TGT_WRITE_SOURCE:
    {
        uint32_t k = r_target_index.read();    
        if(r_channel_fsm[k] == CHANNEL_IDLE)
        {
            if( (p_d.read() & 0x3) != 0 )
            {
	        printf("ERROR in component PibusMultiDma : %s\n",m_name);
	        printf("The source buffer base address must be word aligned\n");
                exit(1);
            }
            r_channel_source[k] = p_d.read();
        }
        r_target_fsm = TGT_IDLE;
        break;
    }
    case TGT_WRITE_DEST:
    {
        uint32_t k = r_target_index.read();    
        if(r_channel_fsm[k] == CHANNEL_IDLE)
        {
            if( (p_d.read() & 0x3) != 0 )
            {
	        printf("ERROR in component PibusMultiDma : %s\n",m_name);
	        printf("The destination buffer base address must be word aligned\n");
                exit(1);
            }
            r_channel_dest[k] = p_d.read();
        }
        r_target_fsm = TGT_IDLE;
        break;
    }
    case TGT_WRITE_LENGTH:
    {
        uint32_t k = r_target_index.read();    
        if(r_channel_fsm[k] == CHANNEL_IDLE)
        {
            if( (p_d.read() & 0x3) != 0 )
            {
	        printf("ERROR in component PibusMultiDma : %s\n",m_name);
	        printf("The transfer length must be multiple of 4 bytes\n");
                exit(1);
            }
            r_channel_length[k]   = p_d.read();
            r_channel_active[k] = true;
        }
        r_target_fsm = TGT_IDLE;
        break;
    }
    case TGT_WRITE_RESET:
    {
        uint32_t k = r_target_index.read();    
        r_channel_active[k]  = false;
        r_target_fsm = TGT_IDLE;
        break;
    }
    case TGT_WRITE_NOIRQ:
    {
        uint32_t k = r_target_index.read();    
        r_channel_noirq[k] =  (bool)p_d.read();
        r_target_fsm = TGT_IDLE;
        break;
    }
    case TGT_ERROR:
    case TGT_READ_STATUS:
    case TGT_READ_SOURCE:
    case TGT_READ_DEST:
    case TGT_READ_NOIRQ:
    {
        r_target_fsm = TGT_IDLE;
        break;
    }
    } // end switch target fsm
	
    // The master FSM implements a round-robin policy between the clients channels
    // It controls the following registers :
    // r_master_fsm, r_master_index, r_master_count
    // r_channel_done[k] set and r_channel_error[k] to signal the pibus 
    // transaction completion.

    switch( r_master_fsm.read() ) {
    case MST_IDLE :
    {
        bool found = false;
	for( size_t n=0 ; (n < m_channels) and not found ; n++ )
        {
            size_t k = (r_master_index.read() + n) % m_channels;
            if ( (r_channel_fsm[k] == CHANNEL_READ_REQ) or
                 (r_channel_fsm[k] == CHANNEL_WRITE_REQ) )
            {
                uint32_t nwords = r_channel_length[k].read() >> 2;
                found           = true;
                r_master_index  = k;
                r_master_count  = 0;
                if( nwords < m_burst ) r_master_burst = nwords;
                else                   r_master_burst = m_burst;
                if( r_channel_fsm[k].read() == CHANNEL_READ_REQ ) r_master_fsm = MST_READ_REQ;
                else                                              r_master_fsm = MST_WRITE_REQ;
            }
        }
        break;
    }
    case MST_READ_REQ :
    {
	if(p_gnt.read() == true) r_master_fsm = MST_READ_AD;
        break;
    }
    case MST_READ_AD :
    {
        uint32_t k = r_master_index.read();
	if( r_master_burst.read() == 1 ) r_master_fsm = MST_READ_DT;
	else				     r_master_fsm = MST_READ_DTAD;
	r_master_count      = r_master_count.read() + 1;
        r_channel_source[k] = r_channel_source[k].read() + 4;
        break;
    }
    case MST_READ_DTAD :
    {
        uint32_t k = r_master_index.read();
	if( p_ack.read() != PIBUS_ACK_WAIT ) 
        {
            uint32_t word = r_master_count.read();
            r_channel_buf[k][word] = (uint32_t)p_d.read();
	    r_master_count         = r_master_count.read() + 1;
            r_channel_source[k]    = r_channel_source[k].read() + 4;
	    if( r_master_count == (r_master_burst.read() - 1) ) r_master_fsm = MST_READ_DT;
	    else				                r_master_fsm = MST_READ_DTAD;
	}
        break;
    }
    case MST_READ_DT :
    {
        uint32_t k = r_master_index.read();
	if( p_ack.read() == PIBUS_ACK_READY ) 
        {
            uint32_t word          = r_master_count.read();
            r_channel_buf[k][word] = (uint32_t)p_d.read();
            r_channel_done[k]      = true;
            r_channel_error[k]     = false;
            r_master_fsm           = MST_IDLE;
        }
        else if( p_ack.read() == PIBUS_ACK_ERROR ) 
        {
            r_channel_done[k]      = true;
            r_channel_error[k]     = true;
            r_master_fsm           = MST_IDLE;
        }
        break;
    }
    case MST_WRITE_REQ :
    {
	if(p_gnt.read() == true) r_master_fsm = MST_WRITE_AD;
        break;
    }
    case MST_WRITE_AD :
    {
        uint32_t k = r_master_index.read();
	if( r_master_burst.read() == 1 ) r_master_fsm = MST_WRITE_DT;
	else                             r_master_fsm = MST_WRITE_DTAD;
	r_master_count      = r_master_count.read() + 1;
        r_channel_dest[k]   = r_channel_dest[k].read() + 4;
        r_channel_length[k] = r_channel_length[k].read() - 4;
        break;
    }
    case MST_WRITE_DTAD :
    {
        uint32_t k = r_master_index.read();
        if( p_ack.read() != PIBUS_ACK_WAIT ) 
        {
	    r_master_count         = r_master_count.read() + 1;
            r_channel_dest[k]      = r_channel_dest[k].read() + 4;
            r_channel_length[k]    = r_channel_length[k].read() - 4;
	    if( r_master_count == (r_master_burst.read() - 1) ) r_master_fsm = MST_WRITE_DT;
	    else				                r_master_fsm = MST_WRITE_DTAD;
	}
        break;
    }
    case MST_WRITE_DT :
    {
        uint32_t k = r_master_index.read();
        if( p_ack.read() == PIBUS_ACK_READY ) 
        {
            r_channel_done[k]      = true;
            r_channel_error[k]     = false;
            r_master_fsm           = MST_IDLE;
        }
        else if( p_ack.read() == PIBUS_ACK_ERROR )
        {
            r_channel_done[k]      = true;
            r_channel_error[k]     = true;
            r_master_fsm           = MST_IDLE;
        }
        break;
    }
    } // end switch r_master_fsm

    // For each channel (k), the corresponding CHANNEL FSM  
    // define the channel state and control the following registers
    // - r_channel_fsm[k]
    // - r_channel_done[k] reset
    // Soft reset : After each burst (read or write), the master FSM
    // test the r_activate flip-flop to stop the ongoing transfer if requested. 
    // It goes to the MST_SUCCESS state when the tranfer is isuccessfully 
    // completed, to assert the IRQ signaling the completion.
    // In case of bus error, it goes to the MST_WRITE_ERROR or MST_READ_ERROR
    // state to assert the IRQ signaling the completion.
    for( size_t k=0 ; k<m_channels ; k++ )
    {
        switch( r_channel_fsm[k].read() )
        {
            case CHANNEL_IDLE:
            {
                if ( r_channel_active[k].read() ) r_channel_fsm[k] = CHANNEL_READ_REQ;
                break;
            }
            case CHANNEL_READ_REQ:      // requesting a VCI READ transaction
            {
                if ( (r_master_fsm.read() == MST_READ_REQ) and (r_master_index.read() == k) ) 
                    r_channel_fsm[k] = CHANNEL_READ_WAIT;
                break;
            }
            case CHANNEL_READ_WAIT:     // waiting  VCI READ response
            {
                if ( r_channel_done[k].read() ) 
                {
                    if      ( not r_channel_active[k].read() ) r_channel_fsm[k] = CHANNEL_IDLE;
                    else if ( r_channel_error[k].read() )      r_channel_fsm[k] = CHANNEL_READ_ERROR;
                    else                                       r_channel_fsm[k] = CHANNEL_WRITE_REQ;
                    r_channel_done[k] = false;
                }
                break;
            }
            case CHANNEL_WRITE_REQ:     // requesting a VCI WRITE transaction
            {
                if ( (r_master_fsm.read() == MST_WRITE_REQ) and (r_master_index.read() == k) ) 
                    r_channel_fsm[k] = CHANNEL_WRITE_WAIT;
                break;
            }
            case CHANNEL_WRITE_WAIT:    // waiting VCI WRITE response
            {
                if ( r_channel_done[k].read() ) 
                {
                    if      ( not r_channel_active[k].read() ) r_channel_fsm[k] = CHANNEL_IDLE;
                    else if ( r_channel_error[k].read() )      r_channel_fsm[k] = CHANNEL_WRITE_ERROR;
                    else if ( r_channel_length[k].read()== 0 ) r_channel_fsm[k] = CHANNEL_DONE;
                    else                                       r_channel_fsm[k] = CHANNEL_READ_REQ;
                    r_channel_done[k] = false;
                }
                break;
            }
            case CHANNEL_DONE:
            case CHANNEL_READ_ERROR:
            case CHANNEL_WRITE_ERROR:
            {
                if ( not r_channel_active[k] ) r_channel_fsm[k] = CHANNEL_IDLE;
                break; 
            }
        } // end switch r_channel_fsm[k]
    } //end for channels

}  // end transition

//////////////////////////////
void PibusMultiDma::genMoore()
{
    uint32_t	tk = r_target_index.read();

    // p_ack & p_d signals 
    switch(r_target_fsm) {
    case TGT_IDLE:
        break;
    case TGT_READ_STATUS:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_channel_fsm[tk].read();;
        break;
    case TGT_READ_SOURCE:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_channel_source[tk].read();
        break;
    case TGT_READ_DEST:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_channel_dest[tk].read();
        break;
    case TGT_READ_NOIRQ:
	p_ack = PIBUS_ACK_READY;
	p_d = (uint32_t)r_channel_noirq[tk].read();
        break;
    case TGT_ERROR:
	p_ack = PIBUS_ACK_ERROR;
        break;
    default:
	p_ack = PIBUS_ACK_READY;
        break;
    } // end switch target fsm

    // p_req signal
    if((r_master_fsm == MST_READ_REQ) || (r_master_fsm == MST_WRITE_REQ)) 	p_req = true;
    else									p_req = false;

    uint32_t	mk = r_master_index.read();

    // p_a, p_lock, p_read, p_opc signals
    if((r_master_fsm == MST_READ_AD) || (r_master_fsm == MST_READ_DTAD)) 
    {
	p_a   = (uint32_t)r_channel_source[mk].read();
	p_opc = PIBUS_OPC_WDU;
	p_read = true;
	if(r_master_count.read() == r_master_burst.read() - 1) p_lock = false;
	else			                                p_lock = true;
    }
    if((r_master_fsm == MST_WRITE_AD) || (r_master_fsm == MST_WRITE_DTAD)) 
    {
	p_a   = (uint32_t)r_channel_dest[mk].read();
	p_opc = PIBUS_OPC_WDU;
	p_read = false;
	if(r_master_count.read() == r_master_burst.read() - 1) p_lock = false;
	else			                                p_lock = true;
    }

    // p_d signal
    if((r_master_fsm == MST_WRITE_DTAD) || (r_master_fsm == MST_WRITE_DT)) 
    {
        uint32_t word = r_master_count.read();
        p_d = (uint32_t)r_channel_buf[mk][word];
    }

    // IRQ signal
    for( size_t k=0 ; k<m_channels ; k++ )
    {
        p_irq[k] = (( (r_channel_fsm[k].read() == CHANNEL_DONE) or
                      (r_channel_fsm[k].read() == CHANNEL_READ_ERROR) or
                      (r_channel_fsm[k].read() == CHANNEL_WRITE_ERROR) )
                      and not r_channel_noirq[k].read() );
    }
} // end GenMoore()

///////////////////////////////////////////////////////
PibusMultiDma::PibusMultiDma(sc_module_name	name, 
         	             uint32_t		tgtid,
	  		     PibusSegmentTable	&segtab,
	  		     size_t		burst,
	  		     size_t		channels)
    : 
      r_target_fsm("r_target_fsm"),
      r_target_index("r_target_indesx"),
      r_master_fsm("r_master_fsm"),
      r_master_index("r_master_count"),
      r_master_count("r_master_index"),
      r_master_burst("r_master_burst"),
      r_channel_fsm(alloc_elems<sc_signal<int> >("r_channel_fsm", channels)),
      r_channel_source(alloc_elems<sc_signal<uint32_t> >("r_channel_source", channels)),
      r_channel_dest(alloc_elems<sc_signal<uint32_t> >("r_channel_dest", channels)),
      r_channel_length(alloc_elems<sc_signal<uint32_t> >("r_channel_length", channels)),
      r_channel_noirq(alloc_elems<sc_signal<bool> >("r_channel_noirq", channels)),
      r_channel_active(alloc_elems<sc_signal<bool> >("r_channel_active", channels)),
      r_channel_done(alloc_elems<sc_signal<bool> >("r_channel_done", channels)),
      r_channel_error(alloc_elems<sc_signal<bool> >("r_channel_error", channels)),
      m_name(name),
      m_tgtid(tgtid),
      m_burst(burst),
      m_channels(channels),
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
      p_irq(alloc_elems<sc_core::sc_out<bool> >("p_irq", channels))
  
{
    SC_METHOD(transition);
    sensitive_pos << p_ck;

    SC_METHOD(genMoore);
    sensitive_neg << p_ck;

    strcpy (m_channel_str[0], "DONE");
    strcpy (m_channel_str[1], "READ_ERROR");
    strcpy (m_channel_str[2], "IDLE");
    strcpy (m_channel_str[3], "WRITE_ERROR");
    strcpy (m_channel_str[4], "READ_REQ");
    strcpy (m_channel_str[5], "READ_WAIT");
    strcpy (m_channel_str[6], "WRITE_REQ");
    strcpy (m_channel_str[7], "WRITE_WAIT");

    strcpy (m_master_str[0], "IDLE");
    strcpy (m_master_str[1], "READ_REQ");
    strcpy (m_master_str[2], "READ_AD");
    strcpy (m_master_str[3], "READ_DTAD");
    strcpy (m_master_str[4], "READ_DT");
    strcpy (m_master_str[5], "WRITE_REQ");
    strcpy (m_master_str[6], "WRITE_AD");
    strcpy (m_master_str[7], "WRITE_DTAD");
    strcpy (m_master_str[8], "WRITE_DT");

    strcpy (m_target_str[0], "IDLE");
    strcpy (m_target_str[1], "WRITE_SOURCE");
    strcpy (m_target_str[2], "WRITE_DEST");
    strcpy (m_target_str[3], "WRITE_LENGTH");
    strcpy (m_target_str[4], "WRITE_RESET");
    strcpy (m_target_str[5], "WRITE_NOIRQ");
    strcpy (m_target_str[6], "READ_SOURCE");
    strcpy (m_target_str[7], "READ_DEST");
    strcpy (m_target_str[8], "READ_STATUS");
    strcpy (m_target_str[9], "READ_NOIRQ");
    strcpy (m_target_str[10], "ERROR");

    if( (channels < 1) or (channels > 16) )
    {
	printf("ERROR in component PibusMultiDma : %s\n",m_name);
	printf("number of channels must be larger than 0 and no larger than 16\n");
        exit(1);
    }

    if((m_burst > 1024) || (m_burst < 1)) 
    {
	printf("ERROR in component PibusMultiDma : %s\n",m_name);
	printf("burst must be larger than 0 and no larger than 1024 words\n");
        exit(1);
    }

    r_channel_buf = new uint32_t*[channels];
    for( size_t k=0 ; k<channels ; k++) r_channel_buf[k] = new uint32_t[burst];

    // get segment base address and segment size
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase();
    m_segsize = (*seglist.begin()).getSize();
    m_segname = (*seglist.begin()).getName();

    if( m_segsize < channels*4096 ) 
    {
	printf("ERROR in component PibusMultiDma : %s\n", m_name);
	printf("The size of the segment cannot be smaller than 4K*channels\n");
        exit(1);
    }

    if((m_segbase & 0x000003FF) != 0) 
    {
	printf("ERROR in component PibusMultiDma : %s\n", m_name);
	printf("The base adress of the segment must be multiple of 4K\n");
        exit(1);
    }
    std::cout << std::endl << "Instanciation of PibusMultiDma : " << m_name << std::endl;
    std::cout << "    burst length = " << m_burst << std::endl;
    std::cout << "    channels     = " << m_burst << std::endl;
    std::cout << "    segment " << m_segname << std::hex
              << " | base = 0x" << m_segbase
              << " | size = 0x" << m_segsize << std::endl;

} // end constructor

////////////////////////////////
void PibusMultiDma::printTrace()
{
    std::cout << m_name << "_target : " << m_target_str[r_target_fsm.read()] << " / "
              << m_name << "_master : " << m_master_str[r_master_fsm]  
                        << " for channel " << r_master_index.read() << std::endl;
    for( size_t k=0 ; k<m_channels ; k++ )
    {
        if( r_channel_active[k].read() )
        {
            std::cout  << m_name << "_channel " << k << " : " << m_channel_str[r_channel_fsm[k].read()]
                       << " / source = " << std::hex << r_channel_source[k].read() 
                       << " / dest = " << std::hex << r_channel_dest[k].read() 
                       << " / nwords = " << std::dec << r_channel_length[k].read() << std::endl;
        }
    }
} // end printTrace


}} // end namespace
