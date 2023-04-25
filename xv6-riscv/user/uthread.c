#include "uthread.h"
#include "kernel/types.h"
#include "user.h"

static struct uthread uthreads[MAX_UTHREADS];
static struct uthread* current_uthread;  // TODO who is the current thread?!?!

int uthread_create(void (*start_func)(), enum sched_priority priority) {

    // find a free entry in the thread table
    int free_entry = -1;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == FREE) {
        free_entry = i;
        break;
        }
    }

    // no free entry in the thread table
    if (free_entry == -1) {
        return -1;
    }

    // initialize the thread table entries
    threads[free_entry].priority = priority;

    // allocate a new stack for the thread
    void *stack = malloc(STACK_SIZE);

    // failed to allocate memory for the stack
    if (stack == NULL) {
        threads[free_entry].state = FREE;
        return -1;
    }
    threads[free_entry].stack = stack;

    // set the 'ra' register to the start_func and 'sp' to the top of the relevant ustack
    threads[free_entry].context.sp = (uint64_t)stack + STACK_SIZE;
    threads[free_entry].context.ra = (uint64_t)start_func;

    // set as RUNNABLE
    threads[free_entry].state = RUNNABLE;
    return 0;

    // TODO -> meaning?
    // Hint: A uthread_exit(…) call should be carried out explicitly at the end of
    // each given start_func(…) .
}

void uthread_yield() {
    int i;
    struct uthread* next_thread = NULL;

    // find next thread
    for (i = 0; i < MAX_UTHREADS; i++) {
        if (uthreads[i].state == RUNNABLE && (next_thread == NULL || uthreads[i].priority > next_thread->priority)) {
            next_thread = &uthreads[i];
        }
    }

    if (next_thread != NULL) {
        struct uthread* prev_thread = current_uthread;

        // update the current thread pointer
        current_uthread = next_thread;

        // save the context of the current thread and switch to the next thread
        uswtch(&prev_thread->context, &current_uthread->context);
    }
}

void uthread_exit() {
    int num_free_threads = 0;
    int i;
    for (i = 0; i < MAX_UTHREADS; i++) {
        if (threads[i].state == FREE) {
            num_free_threads++;
        }
    }
    if (num_free_threads == MAX_UTHREADS)
        exit(0);
    
    else
        uthread_yield();

    // TODO -> maybe too loop over threads array and look for threads[i] == RUNNABLE and then call uswtch (might be nore efficient)
}

enum sched_priority uthread_set_priority(enum sched_priority priority){
    enum sched_priority prev_priority = current_thread.priority;
    current_thread.priority = priority;
    return prev_priority;
}

enum sched_priority uthread_get_priority(){
    return current_thread.priority;
}