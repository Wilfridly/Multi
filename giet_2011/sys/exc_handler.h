#ifndef _EXCP_HANDLER_H
#define _EXCP_HANDLER_H

/*
 * Exception Vector Table (indexed by cause register)
 *
 * 16 entries corresponding to 16 causes functions addresses
 */

typedef void (*_exc_func_t)(void);
extern const _exc_func_t _cause_vector[16];

#endif

