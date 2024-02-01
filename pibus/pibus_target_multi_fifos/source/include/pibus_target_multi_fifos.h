/**************************************************************************
 * File : pibus_target_multi_fifos.h
 * Date : 20/08/2006
 * author : Daniela Genius & Alain Greiner
 * Copyright : UPMC - LIP6
 *
 * This component is a generic PIBUs controler, acting as a slave
 * on the PIBUS. It can be used to interface any hardware coprocessor.
 * It provides six different communication services:
 * - Up to 4 Read Fifos from which the coprocessor can read data.
 * - Up to 4 Write Fifos to which the coprocessor can write data.
 * _ Up to 4 coprocessor Configuration Registers (write)
 * - Up to 4 coprocessor Status Registers (read)
 * - One Soft Reset pseudo register (write)
 * - One Strobe pseudo register (write)
 * A threshold interrupt is issued when a Write Fifo is "nearly" full,
 * or a Read Fifo is "nearly" empty. 
 * The threshold is a parameter for input/output fifos.
 *
 * This component decodes bits A6 to A2 of the ADDRESS :
 *  - bits A6/A5/A4 define the access type    
 *         0  0  0 : Soft Reset
 *         0  0  1 : Strobe    
 *         0  1  0 : Configuraytion Registers
 *         0  1  1 : Status Registers
 *         1  0  0 : Read Fifo Data
 *         1  0  1 : Read Fifo Status
 *         1  1  0 : Write Fido Data
 *         1  1  1 : Write Fifo Status
 *  - bits A3/A2 define the FIFO or register index 
 ****************************************************************************
 * This component has 8 "template" parameters
 * - int	N_FIFO_READ	: number of Read Fifos
 * - int	N_FIFO_WRITE	: number of Write Fifos
 * - int	FIFO_READ_SIZE	: depth of Read Fifos
 * - int	FIFO_WRITE_SIZE	: depth of Write Fifos
 * - int  	THRESHOLD_READ	: threshold value for all Read Fifos
 * _ int  	THRESHOLD_WRITE	: threshold value for all Write Fifos
 * - int  	N_CONFIG_REG	: number of Configuration Registers
 * - int  	N_STATUS_REG	: number of Status Registers
 ****************************************************************************
 * This component has 3 "constructor" parameters
 * - sc_module_name 	name	: instance name
 * - int		index	: PIBUS target index
 * - pibusSegmentTable	segtab	: segment table
 ****************************************************************************/

#ifndef PIBUS_TARGET_MULTI_FIFOS_H
#define PIBUS_TARGET_MULTI_FIFOS_H

#include "systemc.h"
#include "shared/soclib_generic_fifo.h"
#include "pibus_mnemonics.h"
			

template <int 	 N_FIFO_READ, 
            int  N_FIFO_WRITE,
            int  FIFO_READ_SIZE,
            int  FIFO_WRITE_SIZE,
            int  THRESHOLD_READ,
            int  THRESHOLD_WRITE,
            int  N_CONFIG_REG,
            int  N_STATUS_REG>

struct PIBUS_TARGET_MULTI_FIFOS : sc_module {

//PIBUS ports

sc_in<bool>             	CLK;
sc_in<bool>             	RESETN;

sc_in<bool>			PI_READ;
sc_in<bool>			PI_SEL;
sc_in<bool>			PI_TOUT;
sc_in< sc_uint<4> >		PI_OPC;
sc_in< sc_uint<32> >		PI_A;
sc_inout<sc_uint<32> >		PI_D;
sc_inout< sc_uint<3> >		PI_ACK;	

sc_out<sc_uint<32> >		DOUT[N_FIFO_READ];                       
sc_out<bool> 			ROK[N_FIFO_READ];
sc_in<bool> 			R[N_FIFO_READ];

sc_in<sc_uint<32> >		DIN[N_FIFO_WRITE];                       
sc_out<bool> 			WOK[N_FIFO_WRITE];
sc_in<bool> 			W[N_FIFO_WRITE];

sc_out<int>  			CONFIG[N_CONFIG_REG];
sc_in<sc_uint<32> > 		STATUS[N_STATUS_REG];

sc_out<bool>            	SOFTRESET;
sc_out<bool>            	STROBE;

sc_out<bool>			FIFO_READ_IRQ[N_FIFO_READ];
sc_out<bool>			FIFO_WRITE_IRQ[N_FIFO_WRITE];

// STRUCTURAL PARAMETERS
const char 			*NAME;
int				IDENT;
int				BASE;
int				SIZE;

// REGISTERS

sc_register<int>		STATE;
sc_register<int>		INDEX;
sc_register<int>	  	COPROC_CONFIG[N_CONFIG_REG];
sc_register<int>		COPROC_STATUS[N_STATUS_REG];

soclib_generic_fifo<FIFO_READ_SIZE,32>	FIFO_R[N_FIFO_READ];
soclib_generic_fifo<FIFO_WRITE_SIZE,32>	FIFO_W[N_FIFO_WRITE];

// States of  FSM

enum { 
	FSM_IDLE   		= 0x0,
	FSM_ERROR  		= 0x1,
	FSM_STS_READ		= 0x2,
	FSM_DATA_READ		= 0x3,
	FSM_STS_WRITE		= 0x4,
	FSM_DATA_WRITE		= 0x5,
	FSM_COPROC_STATUS	= 0x6,
	FSM_COPROC_CONFIG	= 0x7,	
        FSM_SOFTRESET		= 0x8,
	FSM_STROBE		= 0x9,

};

/////////////////////////////////////////////
// 	Transition()   
/////////////////////////////////////////////
 
void Transition()
{

bool		fifo_w[N_FIFO_WRITE];
bool		fifo_r[N_FIFO_READ];

if(RESETN == false) {  
 	for(int n=0; n < N_FIFO_READ; n++){	FIFO_R[n].init(); }
 	for(int n=0; n < N_FIFO_WRITE; n++){	FIFO_W[n].init(); }
	for(int n = 0 ; n < N_CONFIG_REG ; n++) { COPROC_CONFIG[n]= 0; }
	STATE = FSM_IDLE;
	INDEX = 0;
	return;
}

//read coprocessor status registers
for(int n=0 ; n<N_STATUS_REG; n++) { COPROC_STATUS[n]= STATUS[n].read(); }

// fifo read
for(int n=0; n<N_FIFO_READ; n++){ fifo_r[n] = false; }

// fifo write
for(int n=0; n<N_FIFO_WRITE; n++){ fifo_w[n] = false; }

switch (STATE) { 
case FSM_IDLE :
	if (PI_SEL == true) {
		int  addr = PI_A.read(); 
		INDEX = (addr >> 2) & 0x3; 	// bits 3 and 2 define the index
		int cmd = (addr >>4) & 0x7; 	// bits 4, 5 and 6 define the commnand
		if ((addr < BASE) || (addr > BASE + SIZE)) {
		    	STATE = FSM_ERROR;
		} else {
			switch (cmd) {
			case 0x0: //Soft Reset
				STATE = FSM_SOFTRESET;
			break;
			case 0x1: //Strobe
				STATE = FSM_STROBE;
			break;
			case 0x2: //Config register
				STATE = FSM_COPROC_CONFIG;
			break;
			case 0x3: //Status register
				STATE = FSM_COPROC_STATUS;
			break;
			case 0x4: //FIFO Read Data
				STATE = FSM_DATA_READ;
			break;
			case 0x5: //FIFO Read Status
				STATE = FSM_STS_READ;
			break;
			case 0x6: //FIFO Write Data
				STATE = FSM_DATA_WRITE;
			break;
			case 0x7: //FIFO Write Status
				STATE = FSM_STS_WRITE;
			break;
			} // end switch cmd
		}
	} // endif PI_SEL
break; 
case FSM_ERROR :  
	STATE=FSM_IDLE;
break;
case FSM_STS_READ :	
	STATE=FSM_IDLE;	
break;
case FSM_STS_WRITE : 
	STATE=FSM_IDLE;
break;
case FSM_DATA_READ : 
	fifo_r[INDEX] = true; 
	if(FIFO_R[INDEX].wok() == true) {STATE=FSM_IDLE;}
break;
case FSM_DATA_WRITE : 
	fifo_w[INDEX] = true;
	if(FIFO_W[INDEX].rok() == true) {STATE=FSM_IDLE;}
break;
case FSM_COPROC_CONFIG : 
        COPROC_CONFIG[INDEX]=(sc_uint<32>)PI_D.read();
	STATE=FSM_IDLE;	
break;
case FSM_COPROC_STATUS :
	STATE=FSM_IDLE;
break; 
case FSM_SOFTRESET : 
        for(int n=0; n<N_FIFO_READ; n++){ FIFO_R[n].init(); }
 	for(int n=0; n<N_FIFO_WRITE; n++){ FIFO_W[n].init(); }
	STATE = FSM_IDLE;
break;
case FSM_STROBE : 
	STATE=FSM_IDLE;
break;
} // end switch STATE

for (int n = 0 ; n < N_FIFO_READ ; n++) {
	if((fifo_r[n] == true ) && (R[n] == false)) { FIFO_R[n].simple_put(PI_D.read()); }  
	if((fifo_r[n] == true ) && (R[n] == true )) { FIFO_R[n].put_and_get(PI_D.read()); } 
	if((fifo_r[n] == false) && (R[n] == true )) { FIFO_R[n].simple_get();  }
	}

for (int n = 0 ; n < N_FIFO_WRITE ; n++) {
	if((fifo_w[n] == true ) && (W[n] == false)) { FIFO_W[n].simple_get(); }
	if((fifo_w[n] == true ) && (W[n] == true )) { FIFO_W[n].put_and_get(DIN[n].read());  } 
	if((fifo_w[n] == false) && (W[n] == true )) { FIFO_W[n].simple_put(DIN[n].read());  }
	}

};  // end Transition()

/////////////////////////////////////////////
// 	GenMoore()    
/////////////////////////////////////////////

void GenMoore()
{

// interface coprocessor FIFO Read
for(int n=0;n<N_FIFO_READ;n++){
  	DOUT[n] = FIFO_R[n].read();
  	ROK[n]  = FIFO_R[n].rok();
	}

// interface coprocessor FIFO Write
for(int n=0;n<N_FIFO_WRITE;n++){
  	WOK[n]  = FIFO_W[n].wok(); 
	}

// interface coprocessor configuration registers
for(int n=0;n<N_CONFIG_REG;n++){
  	CONFIG[n] = (sc_uint<32>)COPROC_CONFIG[n];
	}

//  signals SOFTRESET, STROBE, PI_D and PI_ACK
switch (STATE) { 
  	case FSM_IDLE : 
	SOFTRESET = false;
	STROBE = false;
	break; 
	
	case FSM_ERROR :
	SOFTRESET = false;
	STROBE = false;
	PI_ACK=ACK_ERR;
	break;
		
	case FSM_STS_READ :   
	SOFTRESET = false;
	STROBE = false;
	PI_D=(sc_uint<32>)FIFO_R[INDEX].filled_status();
	PI_ACK=ACK_RDY;
	break;
	
	case FSM_STS_WRITE :
	SOFTRESET = false;
	STROBE = false;	
	PI_D=(sc_uint<32>)FIFO_W[INDEX].filled_status();
	PI_ACK=ACK_RDY;
	break;

	case FSM_DATA_READ : 
	SOFTRESET = false;
	STROBE = false;					
	if(FIFO_R[INDEX].wok() == true) PI_ACK=ACK_RDY;
	else				PI_ACK=ACK_WAT;
	break;

	case FSM_DATA_WRITE :
	SOFTRESET = false;
	STROBE = false;	
	if(FIFO_W[INDEX].rok() == true) PI_ACK=ACK_RDY;
	else				PI_ACK=ACK_WAT;
	PI_D.write(FIFO_W[INDEX].read());	
	break;
    
	case FSM_COPROC_CONFIG : 
	SOFTRESET = false;
	STROBE = false;	
	PI_ACK=ACK_RDY;
	break;

        case FSM_COPROC_STATUS :  
	SOFTRESET = false;
	STROBE = false;	
	PI_ACK=ACK_RDY;	
	PI_D=(sc_uint<32>)COPROC_STATUS[INDEX].read();
	break;
		
 	case FSM_SOFTRESET :
	SOFTRESET = true;
	STROBE = false;
	PI_ACK=ACK_RDY;
	break;
 
	case FSM_STROBE :
	SOFTRESET = false;
	STROBE = true;
	PI_ACK=ACK_RDY;
	break;
} // end switch

// IRQs: one vector each for read and write
  
for(int n=0; n<N_FIFO_READ; n++){
    if(FIFO_R[n].filled_status()<THRESHOLD_READ){
        FIFO_READ_IRQ[n] = true;
    } else {
        FIFO_READ_IRQ[n] = false;
    }	    
}

for(int n=0; n<N_FIFO_WRITE; n++){
    if(FIFO_W[n].filled_status()>=THRESHOLD_WRITE){
        FIFO_WRITE_IRQ[n] = true;
    } else {
        FIFO_WRITE_IRQ[n] = false;
    }
} 
    
}; // end genMoore()

////////////////////////////////////////
//       Constructor   
////////////////////////////////////////

SC_HAS_PROCESS(PIBUS_TARGET_MULTI_FIFOS);

PIBUS_TARGET_MULTI_FIFOS(sc_module_name 	insname,	// instance name
			int			index,		// PIBUS target index
			SOCLIB_SEGMENT_TABLE	&segtab)	// segment table
{
	
SC_METHOD(Transition);
sensitive_pos << CLK;

SC_METHOD(GenMoore);
sensitive_neg << CLK;

NAME = (const char *)name;
IDENT = index;

// segment definition
std::list<SEGMENT_TABLE_ENTRY> seglist = segtab.getSegmentList(index);cout << "index " << index << endl;
BASE = (*seglist.begin()).getBase();cout << "BASE" << BASE << endl;
SIZE = (*seglist.begin()).getSize();cout << "SIZE" << SIZE << endl;
segNAME = (*seglist.begin()).getName();

if(( FIFO_READ_SIZE > 256) || ( FIFO_READ_SIZE < 1)) {
	perror("ERROR in PIBUS_TARGET_MULTI_FIFOS\n");
	perror("The FIFO_READ_SIZE parameter must be larger than 0 and no larger than 256\n");
	exit(1);
}
if(( FIFO_WRITE_SIZE > 256) || ( FIFO_WRITE_SIZE < 1)) {
	perror("ERROR in PIBUS_TARGET_MULTI_FIFOS\n");
	perror("The FIFO_WRITE_SIZE parameter must be larger than 0 and no larger than 256\n");
	exit(1);
}
if((N_FIFO_READ > 4) || (N_FIFO_READ < 1)) {
	perror("ERROR in PIBUS_TARGET_MULTI_FIFOS\n");
	perror("The number of Read Fifos must be larger than 0 and no larger than 4\n");
	exit(1);
}
if((N_FIFO_WRITE > 4) || (N_FIFO_WRITE < 1)) {
	perror("ERROR in PIBUS_TARGET_MULTI_FIFOS\n");
	perror("The number of Write Fifos must be larger than 0 and no larger than 4\n");
	exit(1);
}
if((N_CONFIG_REG > 4) || (N_CONFIG_REG < 1)) {
	perror("ERROR in PIBUS_TARGET_MULTI_FIFOS\n");
	perror("The number of Configuration Registers must be larger than 0 and no larger than 4\n");
	exit(1);
}
if((N_STATUS_REG > 4) || (N_STATUS_REG < 1)) {
	perror("ERROR in PIBUS_TARGET_MULTI_FIFOS\n");
	perror("The number of Status Registers must be larger than 0 and no larger than 4\n");
	exit(1);
}

printf("Successful Instanciation of PIBUS_TARGET_MULTI_FIFOS : %s\n",NAME);
}; // end constructor

}; // end struct PIBUS_TARGET_MULTI_FIFOS

#endif

