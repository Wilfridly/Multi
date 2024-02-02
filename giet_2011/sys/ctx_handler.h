#ifndef _TASK_H
#define _TASK_H

/*
 * Current running task index 
 * and task context array are
 * used by the TTY driver.
 */

extern unsigned char _current_task_array[];
extern unsigned int  _task_context_array[];


/*
 * Prototype of the context switch function
 */
void _ctx_switch();

#endif
