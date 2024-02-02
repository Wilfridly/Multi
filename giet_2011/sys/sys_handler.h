#ifndef _SYS_HANDLER_H
#define _SYS_HANDLER_H

/*
 * Syscall Vector Table (indexed by syscall index)
 *
 * 32 entries corresponding to 32 syscall handler addresses.
 *
 * No declaration of a special type for function pointer here, because syscall
 * handlers have each different prototypes.
 */

extern const void *_syscall_vector[32];

#endif
