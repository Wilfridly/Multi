//////////////////////////////////////////////////////////////////
// File : pibus_mnemonics.h
// Author : Alain Greiner
// Date : 01/01/2010
// Copyright UPMC/LIP6
//
// This file defines the mnemonics for the PIBUS opcodes
// and acknowledges.
/////////////////////////////////////////////////////////////////

#ifndef PIBUS_MNEMONICS_H
#define PIBUS_MNEMONICS_H

#define sc_register sc_core::sc_signal

namespace soclib { namespace common {

// PIBUS ACK Codes
enum {
PIBUS_ACK_WAIT    = 0,  
PIBUS_ACK_ERROR   = 1, 
PIBUS_ACK_READY   = 2, 
PIBUS_ACK_RETRY   = 3, 
};

// PIBUS OPC codes
enum {
PIBUS_OPC_NOP   =0x0, 
PIBUS_OPC_WD32  =0x1, // 32 words burst
PIBUS_OPC_WDU   =0x2, // single word transaction
PIBUS_OPC_WDC   =0x3, // ??
PIBUS_OPC_WD2   =0x4, // 2  words burst
PIBUS_OPC_WD4   =0x5, // 4  words burst
PIBUS_OPC_WD8   =0x6, // 8  words burst
PIBUS_OPC_WD16  =0x7, // 16 words burst
PIBUS_OPC_HW0   =0x8, // lower half word
PIBUS_OPC_HW1   =0xA, // upper half word
PIBUS_OPC_TB0   =0x9, // ??
PIBUS_OPC_TB1   =0xB, // ??
PIBUS_OPC_BY0   =0xC, // byte 0
PIBUS_OPC_BY1   =0xD, // byte 1
PIBUS_OPC_BY2   =0xE, // byre 2
PIBUS_OPC_BY3   =0xF, // byte 3
};

}} // end namespace

#endif

