#include <config.h>
#include <ctx_handler.h>
#include <drivers.h>

/* Size (in words) of a task context */
#define TASK_CTXT_SIZE 64

/*
 * Table of (NB_PROCS * NB_MAXTASKS) task context.
 */
unsigned int _task_context_array[NB_PROCS * NB_MAXTASKS * TASK_CTXT_SIZE];

/*
 * Current running task index on each processor.
 */
unsigned char _current_task_array[NB_PROCS] = { [0 ... NB_PROCS-1] = 0 };

/*
 * Number of tasks on each processor.
 */
unsigned char _task_number_array[NB_PROCS] = { [0 ... NB_PROCS-1] = 1 };

/*
 * _ctx_switch()
 *
 * This function performs a context switch between the current running task and
 * another task.
 * It can be used in a multi-processor architecture, with the assumption that
 * the tasks are statically allocated to processors.
 * The max number of processors is (NB_PROCS), and the max number of tasks is
 * (NB_MAXTASKS).
 * The scheduling policy is round-robin : for each processor, the task index is
 * incremented, modulo the number of tasks allocated to the processor.
 *
 * The function has no argument, and no return value.
 *
 * It uses three global variables:
 * - _current_task_array :  an array of (NB_PROCS) task index:
 *    index of the task actually running on each processor
 * - _task_number_array : an array of (NB_PROCS) numbers:
 *    the number of tasks allocated to each processor
 * - _task_context_array : an array of (NB_PROCS * NB_MAXTASKS) task contexts:
 *    at most 8 processors / each processor can run up to 4 tasks
 *
 * Caution : This function is intended to be used with periodic interrupts.  It
 * can be directly called by the OS, but interrupts must be disabled before
 * calling.
 */

extern void _task_switch(unsigned int *, unsigned int *);

void _ctx_switch()
{
    unsigned char curr_task_index;
    unsigned char next_task_index;

    unsigned int *curr_task_context;
    unsigned int *next_task_context;

    unsigned int proc_id;

    proc_id = _procid();

    /* first, test if there is more than one task to schedule on the processor.
     * otherwise, let's just return. */
    if (_task_number_array[proc_id] <= 1)
        return;

    /* find the task context of the currently running task */
    curr_task_index = _current_task_array[proc_id];
    curr_task_context = &_task_context_array[(proc_id * NB_MAXTASKS + curr_task_index)
        * TASK_CTXT_SIZE];

    /* find the task context of the next running task (using a round-robin
     * policy) */
    next_task_index = (curr_task_index + 1) % _task_number_array[proc_id];
    next_task_context = &_task_context_array[(proc_id * NB_MAXTASKS + next_task_index)
        * TASK_CTXT_SIZE];

    /* before doing the task switch, update the _current_task_array with the
     * new task index */
    _current_task_array[proc_id] = next_task_index;

    /* now, let's do the task switch */
    _task_switch(curr_task_context, next_task_context);
}
