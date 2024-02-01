///////////////////////////////////////////////////////////////////////////
// File: pibus_mips32_xcache.cpp
// Author: Alain Greiner
// Date : 13/04/2010  
// Copyright UPMC/LIP6
// This program is released under the GNU public license
/////////////////////////////////////////////////////////////////////////////        

#include "pibus_mips32_xcache.h"
#include "alloc_elems.h"

using namespace soclib::caba;
using namespace soclib::common;

namespace soclib { namespace caba {

using namespace soclib::caba;
using namespace soclib::common;

////////////////////////////////////
inline uint32_t be2mask(uint32_t be)
{
    switch(be) {
    case 0xF : return 0xFFFFFFFF;
    case 0x1 : return 0x000000FF;
    case 0x2 : return 0x0000FF00;
    case 0x4 : return 0x00FF0000;
    case 0x8 : return 0xFF000000;
    case 0x3 : return 0x0000FFFF;
    case 0xC : return 0xFFFF0000;
    case 0x0 : return 0x00000000;
    default  :
        std::cout << "ERROR in PibuMis32Xcache " << std::endl;
        std::cout << "Invalid value for the BE field in a write request" << std::endl;
        exit(0);
    }
}

//////////////////////////////////////////////////////////////////////////
PibusMips32Xcache::PibusMips32Xcache (	sc_module_name 		name, 
					PibusSegmentTable 	&segtab,
					uint32_t		proc_id,
					uint32_t		icache_ways,
					uint32_t		icache_sets,
					uint32_t		icache_words,
					uint32_t		dcache_ways,
					uint32_t		dcache_sets,
					uint32_t		dcache_words,
					uint32_t		wbuf_depth,
					bool			snoop_active)
    : m_name(name),
      m_cached_table(segtab.getCachedTable()),
      m_icache_sets(icache_sets),
      m_icache_words(icache_words),
      m_icache_ways(icache_ways),
      m_dcache_sets(dcache_sets),
      m_dcache_words(dcache_words),
      m_dcache_ways(dcache_ways),
      m_msb_shift(32 - segtab.getMSBnumber()),
      m_msb_mask((0x1 << segtab.getMSBnumber()) - 1),
      m_snoop_active(snoop_active),

      r_proc( (std::string)name, proc_id),

      r_dcache_fsm("r_dcache_fsm"),
      r_dcache_save_addr("r_dcache_save_addr"),
      r_dcache_save_wdata("r_dcache_save_wdata"),
      r_dcache_save_type("r_dcache_save_type"),
      r_dcache_save_be("r_dcache_save_be"),
      r_dcache_save_cached("r_dcache_save_cached"),
      r_dcache_save_rdata("r_dcache_save_rdata"),

      r_llsc_pending("r_llsc_pending"),
      r_llsc_addr("r_llsc_addr"),

      r_icache_fsm("r_icache_fsm"),
      r_icache_save_addr("r_icache_save_addr"),

      r_pibus_fsm("r_pibus_fsm"),
      r_pibus_wcount("r_pibus_wcount"),
      r_pibus_addr("r_pibus_addr"),
      r_pibus_wdata("r_pibus_wdata"),
      r_pibus_opc("r_pibus_opc"),

      r_snoop_dcache_inval_req("r_snoop_dcache_inval_req"),
      r_snoop_dcache_inval_way("r_snoop_dcache_inval_way"),
      r_snoop_dcache_inval_set("r_snoop_dcache_inval_set"),
      r_snoop_llsc_inval_req("r_snoop_llsc_inval_req"),
      r_snoop_flush_req("r_snoop_flush_req"),
      r_snoop_address_save("r_snoop_address_save"),

      r_wbuf_data("r_wbuf_data", wbuf_depth),
      r_wbuf_addr("r_wbuf_addr", wbuf_depth),
      r_wbuf_type("r_wbuf_type", wbuf_depth),

      r_icache("r_icache", icache_ways, icache_sets, icache_words),
      r_dcache("r_dcache", dcache_ways, dcache_sets, dcache_words),

      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_irq("p_irq"),
      p_req("p_req"),
      p_gnt("p_gnt"),
      p_lock("p_lock"),
      p_read("p_read"),
      p_opc("p_opc"),
      p_a("p_a"),
      p_d("p_d"),
      p_ack("p_ack"),
      p_tout("p_tout"),
      p_avalid("p_avalid")
{
    SC_METHOD (transition);
    sensitive_pos << p_ck;
  
    SC_METHOD (genMoore);
    sensitive_neg << p_ck;

    if ( (m_icache_ways > 16) || (m_dcache_ways > 16) )
    {
        std::cout << "ERROR in PibusMips32Xcache : " << name << std::endl;
        std::cout << "The number of ways cannot be larger than 16" << std::endl;
        exit(0);
    }
    if      ( m_icache_words == 1  )  m_line_inst_mask = 0xFFFFFFFC;
    else if ( m_icache_words == 2  )  m_line_inst_mask = 0xFFFFFFF8;
    else if ( m_icache_words == 4  )  m_line_inst_mask = 0xFFFFFFF0;
    else if ( m_icache_words == 8  )  m_line_inst_mask = 0xFFFFFFE0;
    else if ( m_icache_words == 16 )  m_line_inst_mask = 0xFFFFFFC0;
    else if ( m_icache_words == 32 )  m_line_inst_mask = 0xFFFFFF80;
    else
    {
        std::cout << "ERROR in PibusMips32Xcache : " << name << std::endl;
        std::cout << "The instruction cache line width can be 1,2,4,8,16,32 words" << std::endl;
        exit(0);
    } 
    if      ( m_dcache_words == 1  )  m_line_data_mask = 0xFFFFFFFC;
    else if ( m_dcache_words == 2  )  m_line_data_mask = 0xFFFFFFF8;
    else if ( m_dcache_words == 4  )  m_line_data_mask = 0xFFFFFFF0;
    else if ( m_dcache_words == 8  )  m_line_data_mask = 0xFFFFFFE0;
    else if ( m_dcache_words == 16 )  m_line_data_mask = 0xFFFFFFC0;
    else if ( m_dcache_words == 32 )  m_line_data_mask = 0xFFFFFF80;
    else
    {
        std::cout << "ERROR in PibusMips32Xcache : " << name << std::endl;
        std::cout << "The data cache line width can be 1,2,4,8,16,32 words" << std::endl;
        exit(0);
    } 

    std::cout << std::endl << "Instanciation of PibusMips32Xcache : " << m_name << std::dec << std::endl;
    std::cout << "    proc_id      = " << proc_id      << std::endl;
    std::cout << "    icache_ways  = " << icache_ways  << std::endl;
    std::cout << "    icache_sets  = " << icache_sets  << std::endl;
    std::cout << "    icache_words = " << icache_words << std::endl;
    std::cout << "    dcache_ways  = " << dcache_ways  << std::endl;
    std::cout << "    dcache_sets  = " << dcache_sets  << std::endl;
    std::cout << "    dcache_words = " << dcache_words << std::endl;
    std::cout << "    wbuf_depth   = " << wbuf_depth   << std::endl;
    std::cout << "    snoop        = " << snoop_active << std::endl;
 
    strcpy(m_dcache_fsm_str[0],  "DCACHE_IDLE");
    strcpy(m_dcache_fsm_str[1],  "DCACHE_WRITE_UPDT");
    strcpy(m_dcache_fsm_str[2],  "DCACHE_WRITE_REQ");
    strcpy(m_dcache_fsm_str[3],  "DCACHE_MISS_SELECT");
    strcpy(m_dcache_fsm_str[4],  "DCACHE_MISS_INVAL");
    strcpy(m_dcache_fsm_str[5],  "DCACHE_MISS_WAIT");
    strcpy(m_dcache_fsm_str[6],  "DCACHE_MISS_UPDT");
    strcpy(m_dcache_fsm_str[7],  "DCACHE_UNC_WAIT");
    strcpy(m_dcache_fsm_str[8],  "DCACHE_UNC_GO");
    strcpy(m_dcache_fsm_str[9],  "DCACHE_ERROR");
    strcpy(m_dcache_fsm_str[10], "DCACHE_INVAL");
    strcpy(m_dcache_fsm_str[11], "DCACHE_SC_WAIT");

    strcpy(m_icache_fsm_str[0], "ICACHE_IDLE");
    strcpy(m_icache_fsm_str[1], "ICACHE_MISS_SELECT");
    strcpy(m_icache_fsm_str[2], "ICACHE_MISS_INVAL");
    strcpy(m_icache_fsm_str[3], "ICACHE_MISS_WAIT");
    strcpy(m_icache_fsm_str[4], "ICACHE_MISS_UPDT");
    strcpy(m_icache_fsm_str[5], "ICACHE_UNC_WAIT");
    strcpy(m_icache_fsm_str[6], "ICACHE_UNC_GO");
    strcpy(m_icache_fsm_str[7], "ICACHE_ERROR");

    strcpy(m_pibus_fsm_str[0], "PIBUS_IDLE");
    strcpy(m_pibus_fsm_str[1], "PIBUS_READ_REQ");
    strcpy(m_pibus_fsm_str[2], "PIBUS_READ_AD");
    strcpy(m_pibus_fsm_str[3], "PIBUS_READ_DTAD");
    strcpy(m_pibus_fsm_str[4], "PIBUS_READ_DT");
    strcpy(m_pibus_fsm_str[5], "PIBUS_WRITE_REQ");
    strcpy(m_pibus_fsm_str[6], "PIBUS_WRITE_AD");
    strcpy(m_pibus_fsm_str[7], "PIBUS_WRITE_DT");

} // end  constructor

PibusMips32Xcache::~PibusMips32Xcache () {} 


////////////////////////////////////
void PibusMips32Xcache::transition()
{
    // RESET
    if (p_resetn == false) 
    { 
        r_proc.reset();
        r_wbuf_data.init();
        r_wbuf_type.init();
        r_wbuf_addr.init();
        r_icache.reset();
        r_dcache.reset();

        r_dcache_fsm             = DCACHE_IDLE; 
        r_icache_fsm             = ICACHE_IDLE; 
        r_pibus_fsm              = PIBUS_IDLE;

        r_icache_miss_req        = false;
        r_dcache_miss_req        = false;
        r_icache_unc_req         = false;
        r_dcache_unc_req         = false;
        r_dcache_sc_req          = false;

        r_pibus_rsp_ok           = false;
        r_pibus_rsp_error        = false;

        r_llsc_pending	         = false;

        r_snoop_flush_req        = false;
        r_snoop_dcache_inval_req = false;
        r_snoop_llsc_inval_req   = false;

        c_total_cycles  = 0;
        c_total_inst    = 0;
        c_imiss_count   = 0;
        c_imiss_frz     = 0;
        c_iunc_count    = 0;
        c_iunc_frz      = 0;
        c_dread_count   = 0;
        c_dmiss_count   = 0;
        c_dmiss_frz     = 0;
        c_dunc_count    = 0;
        c_dunc_frz      = 0;
        c_write_count   = 0;
        c_sc_ok_count	= 0;
        c_sc_ko_count	= 0;
        c_write_frz     = 0;
        return;
    } 

    c_total_cycles++;

    r_proc.getRequests( m_ireq, m_dreq );

    m_irsp.valid = false;
    m_drsp.valid = false;

    /////////////////////////////////////////////////////////////////////
    // The ICACHE FSM has 6 states and controls :
    // - r_icache_fsm 
    // - r_icache
    // - r_icache_save_addr 
    // - r_icache_save_way 
    // - r_icache_save_set 
    // - r_icache_miss_req set
    // - r_icache_unc_req set
    // - r_pibus_rsp_ok reset
    // - r_pibus_rsp_error reset
    // - m_irsp 
    //////////////////////////////////////////////////////////////////////

    switch( r_icache_fsm.read() ) {
    case ICACHE_IDLE :
    {
        if (m_ireq.valid)
        {
            uint32_t  	icache_ins;
            bool    	icache_hit;
            size_t      icache_way;
            size_t      icache_set;
            size_t      icache_word;
            bool    	icache_cacheable = m_cached_table[((m_ireq.addr >> m_msb_shift) & m_msb_mask)];

            // store address
            r_icache_save_addr = m_ireq.addr & 0xFFFFFFFC;


            if ( icache_cacheable ) 
            {
                icache_hit = r_icache.read( m_ireq.addr,
                                            &icache_ins,
                                            &icache_way,
                                            &icache_set,
                                            &icache_word );
                if ( icache_hit ) 
                {
                    m_irsp.valid          = true;
                    m_irsp.error          = false;
                    m_irsp.instruction    = icache_ins;
                }   
                else 
                { 
                    c_imiss_count++;
                    c_imiss_frz++;
                    r_icache_save_way  = icache_way;
                    r_icache_save_set  = icache_set;
                    r_icache_miss_req  = true;
                    r_icache_fsm       = ICACHE_MISS_SELECT;
                }
            }
            else 			
            {
                c_iunc_count++;
                c_iunc_frz++;
                r_icache_unc_req   = true;
                r_icache_fsm       = ICACHE_UNC_WAIT;
            } 
        }
        break;
    }
    case ICACHE_MISS_SELECT :
    {
        c_imiss_frz++;
        uint32_t victim;	// unused
        bool	 valid;
        size_t   way;
        size_t   set;
        valid = r_icache.victim_select( r_icache_save_addr.read() & m_line_inst_mask, 
                                        &victim,
                                        &way,
                                        &set );
        r_icache_save_way = way;
        r_icache_save_set = set;
        if ( valid ) r_icache_fsm = ICACHE_MISS_INVAL;
        else	     r_icache_fsm = ICACHE_MISS_WAIT;
        break;
    }
    case ICACHE_MISS_INVAL :
    {
        c_imiss_frz++;
        uint32_t nline;		// unused
        r_icache.inval( r_icache_save_way.read(),
                        r_icache_save_set.read(),
                        &nline );
        r_icache_fsm = ICACHE_MISS_WAIT;
        break;
    }
    case ICACHE_MISS_WAIT :
    {
        c_imiss_frz++;
        if( r_pibus_ins.read() && r_pibus_rsp_ok.read() )
        {
            if( r_pibus_rsp_error.read() ) 
            {
                r_icache_fsm      = ICACHE_ERROR;
                r_pibus_rsp_error = false;
                r_pibus_rsp_ok    = false;
            }
            else
            {
                r_icache_fsm      = ICACHE_MISS_UPDT;
                r_pibus_rsp_ok    = false;
            }
        }
        break;
    }
    case ICACHE_MISS_UPDT :
    {
        c_imiss_frz++;
        r_icache.update( r_icache_save_addr.read() & m_line_inst_mask, 
                         r_icache_save_way.read(),
                         r_icache_save_set.read(),
                         r_pibus_buf );
        r_icache_fsm = ICACHE_IDLE;
        break;
    }
    case ICACHE_UNC_WAIT :
    {
        c_iunc_frz++;
        if( r_pibus_ins.read() && r_pibus_rsp_ok.read() )
        {
            if( r_pibus_rsp_error.read() )
            { 
                r_icache_fsm      = ICACHE_ERROR;
                r_pibus_rsp_error = false;
                r_pibus_rsp_ok    = false;
            }
            else
            {
                r_icache_fsm      = ICACHE_UNC_GO;
                r_pibus_rsp_ok    = false;
            }
        }
        break;
    }
    case ICACHE_UNC_GO :
    {
        if( m_ireq.addr & 0xFFFFFFFC == r_icache_save_addr.read() )
        {
            m_irsp.valid          = true; 
            m_irsp.error          = false;
            m_irsp.instruction    = r_pibus_buf[0];
        }
        r_icache_fsm = ICACHE_IDLE;
        break;
    }
    case ICACHE_ERROR :
    {
        m_irsp.valid       = true;
        m_irsp.error       = true;
        m_irsp.instruction = 0;
        r_icache_fsm       = ICACHE_IDLE;     
        break;
    }
    } // end switch r_icache_fsm

    //////////////////////////////////////////////////////////////////////////////////////
    // The DCACHE controler has 12 states and controls :
    // - r_dcache_fsm 
    // - r_dcache
    // - r_dcache_save_addr
    // - r_dcache_save_wdata 
    // - r_dcache_save_type 
    // - r_dcache_save_be 
    // - r_dcache_save_rdata 
    // - r_dcache_save_way 
    // - r_dcache_save_set 
    // - r_dcache_save_word
    // - r_dcache_miss_req set
    // - r_dcache_unc_req set
    // - r_pibus_rsp_ok reset
    // - r_pibus_rsp_error reset
    // - r_llsc_pending
    // - r_llsc_addr
    // - m_drsp 
    // There is seven mutually exclusive conditions to exit the IDLE state :
    // - SNOOP  => to SNOOP_INVAL_* or SNOOP_FLUSH for one cycle, then IDLE.
    // - CACHED READ MISS => to MISS_SELECT, then MISS_WAIT, then MISS_UPDT
    //   (to update the cache), and finally to IDLE.
    // - UNCACHED READ => to UNC_WAIT, then to UNC_GO
    //   (to return the uncached data to processor), and finally to IDLE.
    // - WRITE HIT => to WRITE_UPDT (to update the cache), then to WRITEREQ
    //   (to post the request in the write buffer).
    // - WRITE MISS => directly to  WRITE_REQ. 
    // - SC (if llsc pending) => to SC_WAIT to send a write transaction on the bus,
    //   then to IDLE. 
    // - XTN INVAL => to the INVAL state for one cycle, then to IDLE.
    // Implementation note : to support write bursts, the processor requests are
    // taken into account in the WRITEREQ state as well as in the IDLE state.
    //////////////////////////////////////////////////////////////////////////////////////

    switch ( r_dcache_fsm.read() ) {
    case DCACHE_WRITE_REQ :
    {
        if ( !r_wbuf_data.wok() )
        {
            // stay in this state if the write buffer is full
            c_write_frz++;
            break;
        }
        // if write request is accepted, the next state and the response
        // are computed as in the DCACHE_IDLE state below ...
    }
    case DCACHE_IDLE :
    {
        // llsc inval request
        if ( r_snoop_llsc_inval_req.read() ) 
        {
            r_llsc_pending         = false;
            r_snoop_llsc_inval_req = false;
        }

        // flush request
        if ( r_snoop_flush_req.read() )	    
        {
            r_dcache.reset();
            r_snoop_flush_req        = false;
            r_snoop_dcache_inval_req = false;
            r_dcache_fsm             = DCACHE_IDLE;     
        }

        // dcache inval request
        else if ( r_snoop_dcache_inval_req.read() )
        {
            uint32_t dummy;
            r_dcache.inval( r_snoop_dcache_inval_way.read(), 
                            r_snoop_dcache_inval_set.read(),
                            &dummy );
            r_snoop_dcache_inval_req = false;
            r_dcache_fsm = DCACHE_IDLE;     
        }

        // Processor request
        else if ( m_dreq.valid )
        {
            bool        dcache_hit;;
            uint32_t    dcache_rdata;
            bool        dcache_cacheable = m_cached_table[((m_dreq.addr >> m_msb_shift) & m_msb_mask)];
            size_t	dcache_way;
            size_t	dcache_set;
            size_t	dcache_word;
            
            if ( dcache_cacheable ) 
            {
                dcache_hit = r_dcache.read( m_dreq.addr, 
                                            &dcache_rdata, 
                                            &dcache_way,
                                            &dcache_set,
                                            &dcache_word );
                
                r_dcache_save_way   = dcache_way;
                r_dcache_save_set   = dcache_set;
                r_dcache_save_word  = dcache_word;
                r_dcache_save_rdata = dcache_rdata;
            }
            else
            {
                dcache_hit = false;
            }

            // READ or LL request
            if ( (m_dreq.type == soclib::common::Iss2::DATA_READ) || 
                 (m_dreq.type == soclib::common::Iss2::DATA_LL  ) ) 
            {
                c_dread_count++;
                if ( dcache_hit )
                {
                    m_drsp.valid	= true;
                    m_drsp.error    	= false;
                    m_drsp.rdata    	= dcache_rdata;
                    r_dcache_fsm  	= DCACHE_IDLE;
                }
                else if ( dcache_cacheable )
                {
                    c_dmiss_count++;
                    c_dmiss_frz++;
                    r_dcache_miss_req  = true;
                    r_dcache_fsm       = DCACHE_MISS_SELECT;
                    r_dcache_save_addr = m_dreq.addr & m_line_data_mask;
                    r_dcache_save_type = m_dreq.type;
                }
                else
                {
                    c_dunc_count++;
                    c_dunc_frz++;
                    r_dcache_unc_req   = true;
                    r_dcache_fsm       = DCACHE_UNC_WAIT;
                    r_dcache_save_addr = m_dreq.addr & 0xFFFFFFFC;
                    r_dcache_save_type = m_dreq.type;
                }

                if ( m_dreq.type == soclib::common::Iss2::DATA_LL ) 
                {
                    r_llsc_pending = true;
                    r_llsc_addr = m_dreq.addr;
                } 
            }   

            // WRITE request
            else if ( m_dreq.type == soclib::common::Iss2::DATA_WRITE ) 
            {
                c_write_count++;
                r_dcache_save_addr 	= m_dreq.addr;
                r_dcache_save_type 	= m_dreq.type;
                r_dcache_save_wdata	= m_dreq.wdata;
                r_dcache_save_be        = m_dreq.be;
                if ( dcache_hit && dcache_cacheable ) 	r_dcache_fsm = DCACHE_WRITE_UPDT;
                else 					r_dcache_fsm = DCACHE_WRITE_REQ;
                m_drsp.valid = true;
                m_drsp.error = false;
                m_drsp.rdata = 0;
            } 

            // SC request
            else if ( m_dreq.type == soclib::common::Iss2::DATA_SC)  
            {
                if ( r_llsc_pending && 
                     (r_llsc_addr.read() == m_dreq.addr) &&
                     !r_snoop_llsc_inval_req.read() )
                {
                    r_dcache_save_addr   = m_dreq.addr;
                    r_dcache_save_wdata  = m_dreq.wdata;
                    r_dcache_save_cached = dcache_hit;
                    r_dcache_sc_req      = true;
                    r_dcache_fsm         = DCACHE_SC_WAIT;
                }
                else
                {
                    c_sc_ko_count++;
                    r_dcache_fsm = DCACHE_IDLE;
                    m_drsp.valid = true;
                    m_drsp.error = false;
		    m_drsp.rdata = Iss2::SC_NOT_ATOMIC;
                }
            }

            // XTN_WRITE requests
            else if ( m_dreq.type == soclib::common::Iss2::XTN_WRITE )	
            {
                if( m_dreq.addr/4 == soclib::common::Iss2::XTN_DCACHE_INVAL)
                {
                    // test if the address contained in wdata is in the cache
                    dcache_hit = r_dcache.hit( m_dreq.wdata,
                                               &dcache_way,
                                               &dcache_set,
                                               &dcache_word );
                    if( dcache_hit )
                    {
                        r_dcache_save_way   = dcache_way;
                        r_dcache_save_set   = dcache_set;
                        r_dcache_fsm 	    = DCACHE_INVAL;
                    }
                    else
                    {
                        m_drsp.valid = true;
                        m_drsp.error = false;
                        m_drsp.rdata = 0;
                        r_dcache_fsm = DCACHE_IDLE;
                    }
                }
                else if( m_dreq.addr/4 == soclib::common::Iss2::XTN_SYNC)
                {
                    // do nothing, as this cache implements a strict sequencial behaviour 
                    // for load/store instructions
                    m_drsp.valid	= true;
                    m_drsp.error    	= false;
                    m_drsp.rdata    	= 0;
                    r_dcache_fsm 	= DCACHE_IDLE;
                }
                else
                {
                    std::cout << "ERROR in PibuMis32Xcache " << m_name << std::endl;
                    std::cout << "only DCACHE_INVAL & SYNC external requests are supported" << std::endl;
                    exit(0);
                }  
            }

            // XTN_READ requests are not supported
            if ( m_dreq.type == soclib::common::Iss2::XTN_READ )
            {
                std::cout << "ERROR in PibuMis32Xcache " << m_name << std::endl;
                std::cout << "XTN_READ are not supported" << std::endl;
                exit(0);
            }
        }
        else   // no valid m_dreq and no snoop request
        {
            r_dcache_fsm = DCACHE_IDLE;
        }
        break;
    }
    case DCACHE_INVAL:
    {
        uint32_t dummy;
        r_dcache.inval( r_dcache_save_way.read(),
                        r_dcache_save_set.read(),
                        &dummy );
        m_drsp.valid	= true;
        m_drsp.error    = false;
        m_drsp.rdata    = 0;
        r_dcache_fsm = DCACHE_IDLE;
        break;
    }
    case DCACHE_WRITE_UPDT:
    {
        r_dcache.write( r_dcache_save_way.read(),
                        r_dcache_save_set.read(),
                        r_dcache_save_word.read(),
                        r_dcache_save_wdata.read(),
                        r_dcache_save_be.read() );
        r_dcache_fsm = DCACHE_WRITE_REQ;
        break;
    }
    case DCACHE_SC_WAIT:
    {
        // abort the SC request and reset llsc registration in case of snoop request
        if (  r_snoop_llsc_inval_req.read() )
        {
            c_sc_ko_count++;
            r_llsc_pending  = false;
            r_dcache_sc_req = false;
            r_dcache_fsm    = DCACHE_IDLE;
            m_drsp.valid    = true;
            m_drsp.error    = false;
            m_drsp.rdata    = Iss2::SC_NOT_ATOMIC;
        }
        // return to IDLE and complete SC request as soon as transaction accepted on Pibus
        else if ( (r_pibus_fsm == PIBUS_WRITE_AD) and not r_dcache_sc_req.read() ) 
        {
            c_sc_ok_count++;
            r_llsc_pending = false;
            if ( r_dcache_save_cached )
            {
                r_dcache.write( r_dcache_save_way.read(), 
                                r_dcache_save_set.read(),
                                r_dcache_save_word.read(),
                                r_dcache_save_wdata.read() );
            }
            m_drsp.valid   = true;
            m_drsp.error   = false;
            m_drsp.rdata   = Iss2::SC_ATOMIC;
            r_dcache_fsm   = DCACHE_IDLE;
        }
        break;
    }
    case DCACHE_MISS_SELECT :
    {
        c_dmiss_frz++;
        uint32_t victim;	// unused
        bool	 valid;
        size_t   way;
        size_t   set;
        valid = r_dcache.victim_select( r_dcache_save_addr.read(),
                                        &victim,
                                        &way,
                                        &set );
        r_dcache_save_way = way;
        r_dcache_save_set = set;
        if ( valid ) r_dcache_fsm = DCACHE_MISS_INVAL;
        else	     r_dcache_fsm = DCACHE_MISS_WAIT;
        break;
    }
    case DCACHE_MISS_INVAL :
    {
        c_dmiss_frz++;
        uint32_t nline;		// unused
        r_dcache.inval( r_dcache_save_way.read(),
                        r_dcache_save_set.read(),
                        &nline );
        r_dcache_fsm = DCACHE_MISS_WAIT;
        break;
    }
    case DCACHE_MISS_WAIT:
    {
        c_dmiss_frz++;
        if( !r_pibus_ins.read() && r_pibus_rsp_ok.read() )
        {
            if( r_pibus_rsp_error.read() ) 
            {
                r_dcache_fsm      = DCACHE_ERROR;
                r_pibus_rsp_error = false;
                r_pibus_rsp_ok    = false;
            }
            else
            {
                r_dcache_fsm      = DCACHE_MISS_UPDT;
                r_pibus_rsp_ok    = false;
            }
        }
        break;
    }
    case DCACHE_MISS_UPDT:
    {
        c_dmiss_frz++;
        r_dcache.update( r_dcache_save_addr.read(),
                         r_dcache_save_way.read(),
                         r_dcache_save_set.read(),
                         r_pibus_buf );
        r_dcache_fsm = DCACHE_IDLE;
        break;
    }
    case DCACHE_UNC_WAIT:
    {
        c_dunc_frz++;
        if( !r_pibus_ins.read() && r_pibus_rsp_ok.read() )
        {
            if( r_pibus_rsp_error.read() ) 
            {
                r_dcache_fsm      = DCACHE_ERROR;
                r_pibus_rsp_error = false;
                r_pibus_rsp_ok    = false;
            }
            else
            {
                r_dcache_fsm      = DCACHE_UNC_GO;
                r_pibus_rsp_ok    = false;
            }
        }
        break;
    }
    case DCACHE_UNC_GO :
    {
        if( (m_dreq.addr & 0xFFFFFFFC) == r_dcache_save_addr.read() )
        {
            m_drsp.valid    = true; 
            m_drsp.error    = false;
            m_drsp.rdata    = r_pibus_buf[0];
        }
        r_dcache_fsm      = DCACHE_IDLE;
        break;
    }
    case DCACHE_ERROR :
    {
        m_drsp.valid    = true;
        m_drsp.error    = true;
        m_drsp.rdata    = 0;
        r_dcache_fsm  = DCACHE_IDLE;
        break;
    }
    } // end switch r_dcache_fsm 

    ///////////////////////////////////////////////////////////////////////////////////
    //  Transmit m_drsp, m_irsp and the irq to the processor to execute one ISS cycle 
    //  Compute the number of instructions actually executed with three conditions:
    //  - valid request
    //  - valid response
    //  - new address
    ///////////////////////////////////////////////////////////////////////////////////

    uint32_t it = 0;
    if ( p_irq.read() ) it = 1;
    r_proc.executeNCycles(1, m_irsp, m_drsp, it);

    // if ( (m_ireq.valid && !m_irsp.valid) || (m_dreq.valid && !m_drsp.valid) || !m_ireq.valid ) c_frz_cycles++;

    if ( m_ireq.valid && m_irsp.valid && (m_ireq.addr != r_icache_save_addr.read()) )  c_total_inst++;

    //////////////////////////////////////////////////////////////////////////////
    // The SNOOP FSM implements a snoop_invalidate policy.
    // It controls the following registers:
    // - r_snoop_dcache_inval_req  : slot invalidation request to DCACHE FSM
    // - r_snoop_dcache_inval_way  : way to be invalidated
    // - r_snoop_dcache_inval_set  : set to be invalidated
    // - r_snoop_llsc_inval_req    : LLSC reservation invalidation request
    // - r_snoop_flush_req         : flush request (both DCACHE & LLSC)
    // - r_snoop_address_save      : previous external hit address
    //
    // There is three types of external hit: 
    // a - the external write matches a locally cached line.
    // b - the external write matches a requested cache line.
    // c - the external write matches a pending llsc address.
    // These conditions are checked at all cycles. 
    //
    // The output are the three invalidation request flip-flops used
    // by the DCACHE FSM, and the snoop_llsc_inval combinational
    // signal used by the PIBUS FSM.
    //
    // - In cases (a) or (b), the SNOOP FSM request the DCACHE FSM to invalidate 
    // the proper cache line, using the r_snoop_dcache_inval_req flip-flop
    // and the r_snoop_dcache_inval_set and r_snoop_dcache_inval_way registers.
    // If another external hit is detected, before completion of the first one, 
    // the SNOOP FSM enters the panic mode, and request a DCACHE global flush, 
    // using the r_snoop_dcache_flush_req flip-flop.
    // - In case (c), the SNOOP FSM request the DCACHE to invalidate the LLSC
    // reservation using the r_snoop_llsc_inval_req flip-flop, and the
    // snoop_llsc_inval signal is used by the PIBUS FSM to cancel a
    // possibly started SC transaction request.
    // 
    // The 3 request flip-flops are handled by the DCACHE FSM in the IDLE state,
    // and must be reset by the DCACHE FSM.
    /////////////////////////////////////////////////////////////////////////////

    bool		snoop_llsc_inval   = false;

    if ( m_snoop_active )
    {
        uint32_t	snoop_addr = p_a.read();
        size_t  	snoop_way  = 0; 
        size_t  	snoop_set  = 0; 
        size_t  	snoop_word = 0;
        bool		cache_hit  = false;
        bool		wait_hit   = false;
        bool		external_write;
  
        external_write = p_avalid.read() and 
                         not p_read.read() and 
                         (r_pibus_fsm.read() != PIBUS_WRITE_AD); 

        if ( external_write )
        {
            cache_hit = r_dcache.hit( snoop_addr, 
                                      &snoop_way, 
                                      &snoop_set, 
                                      &snoop_word);

            wait_hit  = ((snoop_addr & m_line_data_mask) == (r_dcache_save_addr.read() & m_line_data_mask))
                        and ((r_dcache_fsm == DCACHE_MISS_WAIT) or (r_dcache_fsm == DCACHE_MISS_UPDT));

            // previous pending dcache inval request and new external hit on another line
            if( r_snoop_dcache_inval_req.read() and (cache_hit  or wait_hit) and         
                ((r_snoop_address_save.read() & m_line_data_mask) != (snoop_addr & m_line_data_mask)) ) 
            {  
                if ( r_dcache_fsm != DCACHE_IDLE ) // we cannot handle the new external hit
                {
                    r_snoop_flush_req = true;
                }
            }

            if ( cache_hit ) 
            {
                r_snoop_dcache_inval_req = true;
                r_snoop_dcache_inval_way = snoop_way;
                r_snoop_dcache_inval_set = snoop_set;
                r_snoop_address_save     = snoop_addr;
            }
            else if ( wait_hit )  // cache_hit and wait_hit cannot be simultaneously true
            {
                r_snoop_dcache_inval_req = true;
                r_snoop_dcache_inval_way = r_dcache_save_way.read();
                r_snoop_dcache_inval_set = r_dcache_save_set.read();
                r_snoop_address_save     = snoop_addr;
            }

            snoop_llsc_inval  = (snoop_addr == r_llsc_addr.read()) && r_llsc_pending.read();
            if ( snoop_llsc_inval  ) r_snoop_llsc_inval_req   = true;
        }
    } // end if snoop_active
        
    //////////////////////////////////////////////////////////////////////////
    // The PIBUS controler has 10 states and controls :
    // - r_pibus_fsm 
    // - r_pibus_wcount
    // - r_pibus_addr 
    // - r_pibus_ins
    // - r_pibus_write_data
    // - r_pibus_write_type
    // - r_pibus_read_type
    // - r_pibus_buf 
    // - r_icache_miss_req reset
    // - r_icache_unc_req reset
    // - r_dcache_miss_req reset
    // - r_dcache_unc_req reset
    // - r_dcache_sc_req reset
    // 
    // There is 7 write request types :  WDU, WH0, WH1, WB0, WB1, WB2, WB3, 
    // and 6 read request types : WDU, WD2, WD4, WD8, WD16, WD32.
    // Read requests can be for data or instructions.
    // The cache controller implement the following priorities :
    // 1/ DATA WRITE       : write buffer not empty
    // 2/ DATA SC          : r_dcache_sc_req
    // 3/ DATA READ        : r_dcache_miss_req or r_dcache_unc_req
    // 4/ INSTRUCTION READ : r_icache_miss_req or r_icache_unc_req
    //////////////////////////////////////////////////////////////////////////

    switch (r_pibus_fsm) {
    case PIBUS_IDLE : 
    {
        r_pibus_wcount = 0;

	if ( r_wbuf_data.rok() )		// WRITE request
        {
            r_pibus_ins   = false;
            r_pibus_addr  = r_wbuf_addr.read();
            r_pibus_wdata = r_wbuf_data.read();
            r_pibus_opc   = r_wbuf_type.read();
            r_pibus_fsm   = PIBUS_WRITE_REQ; 
        }
        else if ( r_dcache_sc_req.read() )	// SC request
        {
            // Cancel the bus transaction request in case of external hit on a LL/SC address
            if (  snoop_llsc_inval )
            {
                r_dcache_sc_req = false;
            }
            else
            {
                r_pibus_ins     = false;
                r_pibus_addr    = r_dcache_save_addr.read();
                r_pibus_wdata   = r_dcache_save_wdata.read();
                r_pibus_opc     = PIBUS_OPC_WDU;
                r_pibus_fsm     = PIBUS_WRITE_REQ; 
                r_dcache_sc_req = false;
            }
        }
        else if ( r_dcache_miss_req.read() )	// DMISS request
        {
            r_pibus_ins   = false;
            r_pibus_addr  = r_dcache_save_addr.read();
            if      ( m_dcache_words == 1  ) r_pibus_opc = PIBUS_OPC_WDU;
            else if ( m_dcache_words == 2  ) r_pibus_opc = PIBUS_OPC_WD2;
            else if ( m_dcache_words == 4  ) r_pibus_opc = PIBUS_OPC_WD4;
            else if ( m_dcache_words == 8  ) r_pibus_opc = PIBUS_OPC_WD8;
            else if ( m_dcache_words == 16 ) r_pibus_opc = PIBUS_OPC_WD16;
            else if ( m_dcache_words == 32 ) r_pibus_opc = PIBUS_OPC_WD32;
            r_pibus_fsm       = PIBUS_READ_REQ;
            r_dcache_miss_req = false;
        }
        else if ( r_dcache_unc_req.read() )	// DUNC request
        {
            r_pibus_ins      = false;
            r_pibus_addr     = r_dcache_save_addr.read();
            r_pibus_opc      = PIBUS_OPC_WDU;
            r_pibus_fsm      = PIBUS_READ_REQ;
            r_dcache_unc_req = false;
        }
        else if ( r_icache_miss_req.read() )	// IMISS request
        {
            r_pibus_ins   = true;
            r_pibus_addr  = r_icache_save_addr.read() & m_line_inst_mask;
            if      ( m_icache_words == 1  ) r_pibus_opc = PIBUS_OPC_WDU;
            else if ( m_icache_words == 2  ) r_pibus_opc = PIBUS_OPC_WD2;
            else if ( m_icache_words == 4  ) r_pibus_opc = PIBUS_OPC_WD4;
            else if ( m_icache_words == 8  ) r_pibus_opc = PIBUS_OPC_WD8;
            else if ( m_icache_words == 16 ) r_pibus_opc = PIBUS_OPC_WD16;
            else if ( m_icache_words == 32 ) r_pibus_opc = PIBUS_OPC_WD32;
            r_pibus_fsm  = PIBUS_READ_REQ;
            r_icache_miss_req = false;
        }
        else if ( r_icache_unc_req.read() )	// IUNC request	
        {
            r_pibus_ins      = true;
            r_pibus_addr     = r_icache_save_addr;
            r_pibus_opc      = PIBUS_OPC_WDU;
            r_pibus_fsm      = PIBUS_READ_REQ;
            r_icache_unc_req = false;
        }
        break;
    }
    // READ transaction
    case PIBUS_READ_REQ :
    {
	if (p_gnt == true)  r_pibus_fsm = PIBUS_READ_AD; 
        break;
    }
    case PIBUS_READ_AD :
    {
	r_pibus_wcount = r_pibus_wcount + 1;
	if ( r_pibus_opc == PIBUS_OPC_WDU ) 	r_pibus_fsm = PIBUS_READ_DT; 
	else		 			r_pibus_fsm = PIBUS_READ_DTAD; 
        break;
    }
    case PIBUS_READ_DTAD :
    {
        if ( p_tout.read()  or (p_ack.read() == PIBUS_ACK_ERROR) )
        {
            r_pibus_rsp_error             = true;
        }

	if ( (p_ack.read() == PIBUS_ACK_READY) || (p_ack.read() == PIBUS_ACK_ERROR) )
        { 
            r_pibus_wcount = r_pibus_wcount.read() + 1;
            r_pibus_buf[r_pibus_wcount.read() - 1] = p_d.read();
            if (  r_pibus_ins.read() and 
                 (r_pibus_wcount.read() == m_icache_words-1) ) r_pibus_fsm = PIBUS_READ_DT; 
            if ( !r_pibus_ins.read() and 
                 (r_pibus_wcount.read() == m_dcache_words-1) ) r_pibus_fsm = PIBUS_READ_DT; 
	} 
        break;
    }
    case PIBUS_READ_DT :
    {
	if ( (p_ack.read() == PIBUS_ACK_ERROR) or p_tout.read() ) 
        { 
            r_pibus_rsp_error             = true;
            r_pibus_rsp_ok                = true;
            r_pibus_fsm                   = PIBUS_IDLE;
        } 

	if ( (p_ack.read() == PIBUS_ACK_READY) || (p_ack.read() == PIBUS_ACK_ERROR) )
        { 
            r_pibus_buf[r_pibus_wcount-1] = p_d.read();
            r_pibus_rsp_ok                = true;
            r_pibus_fsm                   = PIBUS_IDLE;
	}
        break;
    }
    // WRITE Transaction
    case PIBUS_WRITE_REQ : 
    {
        // start Pibus transaction if bus is allocated
	if (p_gnt == true) 
        {
            r_pibus_fsm = PIBUS_WRITE_AD; 
        }
        // Abort the bus transaction in case of external hit on a LL/SC address
        else if ( snoop_llsc_inval and (r_pibus_addr.read() == r_llsc_addr.read()) )
        {
            r_pibus_fsm = PIBUS_IDLE;
        }
        break;
    }
    case PIBUS_WRITE_AD :
    {
	r_pibus_fsm = PIBUS_WRITE_DT; 
        break;
    }
    case PIBUS_WRITE_DT :
    {
        if ( p_tout.read() or (p_ack.read() == PIBUS_ACK_ERROR) )
        {
            r_pibus_fsm = PIBUS_IDLE; 
            r_proc.setWriteBerr();
        }
	else if (p_ack.read() == PIBUS_ACK_READY) 
        { 
            r_pibus_fsm = PIBUS_IDLE; 
	} 
    }
    break;

    }; // end  switch r_pibus_fsm

    ///////////////////////////////////////////
    //  Write Buffer handling
    //  These FIFOs contain the write requests 
    //  from the DCACHE FSM to the PIBUS FSM.
    ///////////////////////////////////////////

    bool 	fifo_get  = (r_pibus_fsm == PIBUS_IDLE) && r_wbuf_data.rok();
    bool 	fifo_put  = (r_dcache_fsm == DCACHE_WRITE_REQ) && r_wbuf_data.wok();
    uint32_t	fifo_wdata = r_dcache_save_wdata;
    uint32_t	fifo_waddr = r_dcache_save_addr;
    uint32_t	fifo_wtype = PIBUS_OPC_WDU;
    if      ( r_dcache_save_be == 0x3)		fifo_wtype = PIBUS_OPC_HW0; 
    else if ( r_dcache_save_be == 0xC)		fifo_wtype = PIBUS_OPC_HW1; 
    else if ( r_dcache_save_be == 0x1) 		fifo_wtype = PIBUS_OPC_BY0;
    else if ( r_dcache_save_be == 0x2) 		fifo_wtype = PIBUS_OPC_BY1;
    else if ( r_dcache_save_be == 0x4) 		fifo_wtype = PIBUS_OPC_BY2;
    else if ( r_dcache_save_be == 0x8) 		fifo_wtype = PIBUS_OPC_BY3;

    if ((fifo_put == true) && (fifo_get == true)) 
    { 
	r_wbuf_data.put_and_get(fifo_wdata); 
	r_wbuf_addr.put_and_get(fifo_waddr); 
	r_wbuf_type.put_and_get(fifo_wtype); 
    }
    if ((fifo_put == true) && (fifo_get == false)) 
    { 
	r_wbuf_data.simple_put(fifo_wdata); 
	r_wbuf_addr.simple_put(fifo_waddr); 
	r_wbuf_type.simple_put(fifo_wtype); 
    }
    if ((fifo_put == false) && (fifo_get == true)) 
    { 
	r_wbuf_data.simple_get(); 
	r_wbuf_addr.simple_get(); 
	r_wbuf_type.simple_get(); 
    }

} // end transition()

//////////////////////////////////
void PibusMips32Xcache::genMoore()
{
    switch (r_pibus_fsm) {
    case PIBUS_IDLE       :
    {
	p_req = false; 
        break;
    }
    case PIBUS_READ_REQ :
    case PIBUS_WRITE_REQ :
    {
	p_req = true; 
        break;
    }
    case PIBUS_READ_AD   :
    case PIBUS_READ_DTAD :
    {
	p_req  = false; 
	p_a    = r_pibus_addr.read() + ( r_pibus_wcount.read() << 2);
	p_read = true;
        p_opc  = r_pibus_opc.read();
        if      ( r_pibus_opc == PIBUS_OPC_WD2 ) 	p_lock = (r_pibus_wcount < 1);
        else if ( r_pibus_opc == PIBUS_OPC_WD4 )	p_lock = (r_pibus_wcount < 3);
        else if ( r_pibus_opc == PIBUS_OPC_WD8 )	p_lock = (r_pibus_wcount < 7);
        else if ( r_pibus_opc == PIBUS_OPC_WD16)	p_lock = (r_pibus_wcount < 15);
        else if ( r_pibus_opc == PIBUS_OPC_WD32)	p_lock = (r_pibus_wcount < 31);
	else					 	p_lock = false; 
        break;
    }
    case PIBUS_READ_DT : 
    {
	p_req    = false;  
        break; 
    }
    case PIBUS_WRITE_AD :
    {
	p_req  = false; 
	p_a    = r_pibus_addr.read();
        p_read = false;
	p_lock = false;
	p_opc  = r_pibus_opc.read();
        break;
    }
    case PIBUS_WRITE_DT : 
    {
	p_req = false;  
	p_d   = r_pibus_wdata.read(); 
        break; 
    }
    } // end switch r_pibus_fsm 

} // end genMoore()
 
////////////////////////////////////
void PibusMips32Xcache::printTrace()
{
    std::cout << m_name << " : " << m_ireq << std::endl;
    std::cout << m_name << " : " << m_irsp << std::endl;
    std::cout << m_name << " : " << m_dreq << std::endl;
    std::cout << m_name << " : " << m_drsp << std::endl;
    std::cout << m_name << " : " << m_icache_fsm_str[r_icache_fsm] << "  "
                                 << m_dcache_fsm_str[r_dcache_fsm] << "  "
                                 << m_pibus_fsm_str[r_pibus_fsm] << std::endl;

    if ( r_wbuf_data.rok() ) std::cout << "  WBUF = " << r_wbuf_data.filled_status() << " ";
    if ( r_dcache_sc_req.read() ) std::cout << "  SC_REQ";
    if ( r_snoop_dcache_inval_req.read() ) std::cout << "  SNOOP_DCACHE_REQ";
    if ( r_snoop_llsc_inval_req.read() ) std::cout << "  SNOOP_LLSC_REQ";
    if ( r_snoop_flush_req.read() ) std::cout << "  SNOOP_FLUSH_REQ";
    if ( r_llsc_pending.read() ) std::cout << "  LLSC_ADDR : " << std::hex << r_llsc_addr;
    if ( r_wbuf_data.rok() or
         r_dcache_sc_req.read() or
         r_snoop_dcache_inval_req.read() or
         r_snoop_llsc_inval_req.read() or
         r_snoop_flush_req.read() or 
         r_llsc_pending.read() ) std::cout << std::endl;
}

/////////////////////////////////////////
void PibusMips32Xcache::printStatistics()
{
    std::cout << "*** " << name() << " at cycle " << std::dec << c_total_cycles << std::endl;
    std::cout << "- INSTRUCTIONS       = " << c_total_inst << std::endl ;
    std::cout << "- CPI                = " << (float)c_total_cycles/c_total_inst << std::endl ;
    std::cout << "- CACHED READ RATE   = " << (float)(c_dread_count-c_dunc_count)/c_total_inst << std::endl ;
    std::cout << "- UNCACHED READ RATE = " << (float)c_dunc_count/c_total_inst << std::endl ;
    std::cout << "- WRITE RATE         = " << (float)c_write_count/c_total_inst << std::endl;
    std::cout << "- IMISS RATE         = " << (float)c_imiss_count/c_total_inst << std::endl;
    std::cout << "- DMISS RATE         = " << (float)c_dmiss_count/(c_dread_count-c_dunc_count) << std::endl ;
    std::cout << "- IMISS COST         = " << (float)c_imiss_frz/c_imiss_count << std::endl;
    std::cout << "- DMISS COST         = " << (float)c_dmiss_frz/c_dmiss_count << std::endl;
    std::cout << "- UNC COST           = " << (float)c_dunc_frz/c_dunc_count << std::endl;
    std::cout << "- WRITE COST         = " << (float)c_write_frz/c_write_count << std::endl;
}

}} // end namespaces
