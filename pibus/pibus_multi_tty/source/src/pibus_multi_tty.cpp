/////////////////////////////////////////////////////////////////////
// File: pibus_multi_tty.cpp
// Authors : Alain Greiner 
// Date : 23/03/2010 
// This program is released under the GNU Public License
// Copyright : UPMC-LIP6
/////////////////////////////////////////////////////////////////////

#include "pibus_multi_tty.h"
#include "alloc_elems.h"

using namespace soclib::caba;
using namespace soclib::common;

namespace soclib { namespace caba {

/////////////////////////////////////////////////////////////////
// 	Function used to open the UNIX m_pty channel
//	This function is defined for LINUX / BSD compatibility
/////////////////////////////////////////////////////////////////
static int getmpt()
{
        int pty;

        /* Linux style */
        pty = open("/dev/ptmx", O_RDWR|O_NOCTTY);
        if ( pty >= 0 )
                return pty;

        /* scan Berkeley-style */
        char name[] = "/dev/ptyp0";
        int tty;
        while (access(name, 0) == 0) {
                if ((pty = open(name, O_RDWR)) >= 0) {
                        name[5] = 't';
                        if ((tty = open(name, O_RDWR)) >= 0) {
                                close(tty);
                                return pty;
                        }
                        name[5] = 'p';
                        close(pty);
                }

                /* get next pty name */

                if (name[9] == 'f') {
                        name[8]++;
                        name[9] = '0';
                } else if (name[9] == '9')
                        name[9] = 'a';
                else
                        name[9]++;
        }
        errno = ENOENT;
        return -1;
}

////////////////////////////////////////////////////////////
PibusMultiTty::PibusMultiTty(sc_module_name 	 	name,
				uint32_t                tgtid,
				PibusSegmentTable	&segtab,
				uint32_t   		ntty)
    : m_name(name),
      m_tgtid(tgtid),
      m_ntty(ntty),
      p_ck("p_ck"),
      p_resetn("p_resetn"),
      p_sel("p_sel"),
      p_a("p_a"),
      p_read("p_read"),
      p_opc("p_opc"),
      p_ack("p_ack"),
      p_d("p_d"),
      p_tout("p_tout"),
      p_irq_get(soclib::common::alloc_elems<sc_out<bool> >("p_irq_get",ntty)),
      p_irq_put(soclib::common::alloc_elems<sc_out<bool> >("p_irq_put",ntty))
{
    SC_METHOD (transition);
    sensitive << p_ck.pos();

    SC_METHOD (genMoore);
    sensitive << p_ck.neg();

    strcpy (m_fsm_str[0], "IDLE");
    strcpy (m_fsm_str[1], "DISPLAY");
    strcpy (m_fsm_str[2], "STATUS");
    strcpy (m_fsm_str[3], "KEYBOARD");
    strcpy (m_fsm_str[4], "CONFIG");
    strcpy (m_fsm_str[5], "ERROR");

    // get the base address and segment size 
    std::list<SegmentTableEntry> seglist = segtab.getTargetSegmentList(tgtid);
    m_segbase = (*seglist.begin()).getBase(); 
    m_segsize = (*seglist.begin()).getSize(); 
    m_segname = (*seglist.begin()).getName(); 

    if ((m_ntty < 1) || (m_ntty > 16)) 
    {
	printf(" ERROR in PibusMultiTty component : %s\n",m_name);
	printf(" The number of terminals cannot be larger than 16 !\n");
	exit(1); 
    }
    if ((m_segbase & 0xF) != 0) 
    {
	printf(" ERROR in PibusMultiTty component : %s\n",m_name);
	printf(" The base adress must be multiple of 16 !\n");
	exit(1); 
    }
    if (m_segsize < 16*m_ntty) 
    {
	printf(" ERROR in PibusMultiTty component : %s\n",m_name);
	printf(" The segment size cannot be less than m_ntty * 16 bytes !\n");
	exit(1); 
    }

    // XTERMs & m_ptys initialisation 
    for(size_t index = 0 ; index < m_ntty ; index++) 
    {

    // define terminal nane
	char	xterm_name[40];
	snprintf(xterm_name, 40, "%s_%d", m_name, index);

    // create PTY
	if((m_pty[index] = getmpt()) < 0) 
        {
            printf(" ERROR in PibusMultiTty component : %s\n", m_name);
            printf(" The m_pty channel to XTERM %d cannot be open !\n", index);
            exit(1); 
	}
	grantpt(m_pty[index]);
	unlockpt(m_pty[index]);

    // create XTERM
	if((m_pid[index] = fork()) < 0) {
            printf(" ERROR in PibusMultiTty component : %s\n", m_name);
            printf(" Cannot fork the XTERM %d !\n", index);
            exit(1); 
	}
	if (m_pid[index] == 0)  // we are in the child process -> XTERM
        {
	    // open PTY on the XTERM side
            const char *name = ptsname(m_pty[index]);
            int	fd = open(name, O_RDWR);
            if ( fd < 0 ) 
            {
                printf(" ERROR in PibusMultiTty component : %s\n", m_name);
                printf(" The m_pty channel from XTERM %d cannot be open !\n", index);
                exit(1); 
            }
            char pty_string[20];
            snprintf(pty_string, 20, "-S0/%d", fd);  

            // set modes
            struct termios attrs;
            tcgetattr( fd, &attrs );
            cfmakeraw( &attrs );
            attrs.c_oflag |= ONLRET;
            attrs.c_oflag &= ~ONLCR;
            tcsetattr( fd, TCSANOW, &attrs );

	    execlp("xterm", "xterm", 
			pty_string, 		// m_pty ident for the XTERM
                       "-T", xterm_name, 	// XTERM name
                       "-bg", "black", 		// background colour
                       "-fg", "green", 		// foreground colour
                       (char*)NULL);
            printf(" ERROR in the PibusMultiTty component : \n");
            printf(" the XTERM terminal %d cannot be open !\n", index);
            exit(1);
        } 
        else 		// we are in the parent process -> SystemC simulator
        {
            // Xterm gives out a line with its window ID; we dont care
            // about it and we dont want it to get through to the
            // simulated platform... Let's strip it.
            char buf;
            do { read( m_pty[index], &buf, 1 ); } while( buf != '\n' && buf != '\r' );

            // Change XTERM CR/LF mode
            write( m_pty[index], "\x1b[20h", 5 );

            // And change m_pty modes
            struct termios attrs;
            tcgetattr( m_pty[index], &attrs );
            cfmakeraw( &attrs );
            attrs.c_iflag |= IGNCR;
            tcsetattr( m_pty[index], TCSANOW, &attrs );

            fcntl( m_pty[index], F_SETFL, O_NONBLOCK );
            // The last char we want to strip is not always present,
            // so dont block if not here
            read( m_pty[index], &buf, 1 ); 
        }
    } // end for m_ntty

    std::cout << std::endl << "Instanciation of PibusMultiTty : " << m_name << std::endl;
    std::cout << "    ntty = " << m_ntty << std::endl;
    std::cout << "    segment " << m_segname << std::hex 
                  << " | base = 0x" << m_segbase
                  << " | size = 0x" << m_segsize << std::endl;

} // end constructor

////////////////////////////////
PibusMultiTty::~PibusMultiTty()
{
    for(uint32_t i = 0 ; i < m_ntty ; i++) 
    {
        kill(m_pid[i],SIGTERM);
    }
} // end destructor

/////////////////////////////////
void PibusMultiTty::transition()
{
    uint32_t 	address;
    char       	data;

    if(p_resetn == false) 
    {
        r_fsm_state = FSM_IDLE;
        for(size_t i = 0 ; i < m_ntty ; i++) 
        {
            r_keyboard_sts[i] = false;	// buffer empty
            r_display_sts[i] = false;   // buffer empty
            r_keyboard_msk[i] = true;   // IRQ enable
            r_display_msk[i] = false;   // IRQ disable
        }
        return;
    } // end p_resetn

    // The p_sel signal is taken into account in all FSM state,
    // All states have all the same next state
    if (p_sel == true) 
    {
        address = (uint32_t)p_a.read();
        r_index   = (address >> 4 ) & 0xf;
        if ((address < m_segbase) || (address >= (m_segbase + m_segsize)))  	r_fsm_state = FSM_ERROR; 
        else if (((address & 0xC) == TTY_WRITE) && (p_read == false))  		r_fsm_state = FSM_DISPLAY;  
        else if (((address & 0xC) == TTY_STATUS) && (p_read == true))  		r_fsm_state = FSM_STATUS;  
        else if (((address & 0xC) == TTY_READ) && (p_read == true))  		r_fsm_state = FSM_KEYBOARD;  
        else if (((address & 0xC) == TTY_CONFIG) && (p_read == false))  	r_fsm_state = FSM_CONFIG;  
        else  									r_fsm_state = FSM_ERROR; 
    } 
    else 
    {
        r_fsm_state = FSM_IDLE;
    }

    //  write character on the output m_pty 
    if(r_fsm_state == FSM_DISPLAY) 
    {
        data   = (char)(p_d.read()  & 0x000000FF);
        write(m_pty[r_index], &data, 1);
    }

    // reset keyboard status
    if(r_fsm_state == FSM_KEYBOARD) r_keyboard_sts[r_index] = false;
    
    // scan all m_pty inputs
    for(size_t i = 0 ; i < m_ntty ; i++) 
    {
        if((r_keyboard_sts[i] == false) && (r_fsm_state != FSM_KEYBOARD)) 
        {
            // write into buffer if buffer empty and no read request
            if(read(m_pty[i], &data, 1) != -1) 
            {
                // typing ctrl C close the XTERM
                if(data == 0x03) kill(m_pid[i],SIGTERM);	
                // typing enter generates two characters : CR then NL
                // The CR character is discarded
		if(data == 0x0d) read(m_pty[i], &data, 1); 
                r_keyboard_sts[i] = true;
                r_keyboard_buf[i] = (uint32_t)data;
            }
	}
    } // end for

} // end transition()

//////////////////////////////
void PibusMultiTty::genMoore()
{
    switch(r_fsm_state) {
    case FSM_IDLE :
        break;
    case FSM_DISPLAY :
    case FSM_CONFIG : 
        p_ack = PIBUS_ACK_READY;
        break;
    case FSM_STATUS : 
        p_ack = PIBUS_ACK_READY;
        if     ( !r_display_sts[r_index] && !r_keyboard_sts[r_index] )	p_d = 0x0;
        else if( !r_display_sts[r_index] &&  r_keyboard_sts[r_index] )	p_d = 0x1;
        else if(  r_display_sts[r_index] && !r_keyboard_sts[r_index] )	p_d = 0x2;
        else if(  r_display_sts[r_index] &&  r_keyboard_sts[r_index] )	p_d = 0x3;
        break;
    case FSM_KEYBOARD : 
        p_ack = PIBUS_ACK_READY;
        p_d = (uint32_t)r_keyboard_buf[r_index];
        break;
    case FSM_ERROR :
        p_ack=PIBUS_ACK_ERROR;
        break;
    } // end switch r_fsm_state


    for(size_t i = 0 ; i < m_ntty ; i++) 
    {
        p_irq_get[i] = r_keyboard_sts[i] & r_keyboard_msk[i]; // active if keyboard buffer full
        p_irq_put[i] = !r_display_sts[i] & r_display_msk[i];  // active if display buffer empty
    }
} // end genMoore()

////////////////////////////////
void PibusMultiTty::printTrace()
{
    std::cout << m_name << " : " << m_fsm_str[r_fsm_state] 
                        << "   keyboard status[0] = " << r_keyboard_sts[0]
                        << "   display status[0] = "  << r_display_sts[0] << std::endl;
}

}} // end namespaces
