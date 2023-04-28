#include "uthread.h"
#include "kernel/types.h"
#include "user.h"

static struct uthread uthreads[MAX_UTHREADS];
static struct uthread* current_uthread = 0;  

int uthread_create(void (*start_func)(), enum sched_priority priority) {

    // find a free entry in the thread table
    int free_entry = -1;
    for (int i = 0; i < MAX_UTHREADS; i++) {
        if (uthreads[i].state == FREE) {
        free_entry = i;
        break;
        }
    }

    // no free entry in the thread table
    if (free_entry == -1) {
        return -1;
    }

    // initialize the thread table entries
    uthreads[free_entry].priority = priority;

    // set the 'sp' register to the top of the relevant ustack
    uthreads[free_entry].context.sp = (uint64)uthreads[free_entry].ustack + STACK_SIZE - 1;

    // set the start_func field of the thread's struct and set the 'ra' register to start_func_wrapper
    uthreads[free_entry].start_func = start_func;
    uthreads[free_entry].context.ra = (uint64)start_func_wrapper;

    // set as RUNNABLE
    uthreads[free_entry].state = RUNNABLE;
    return 0;

    // TODO -> meaning?
    // Hint: A uthread_exit(…) call should be carried out explicitly at the end of
    // each given start_func(…) .
}

void uthread_yield() {
    current_uthread->state = RUNNABLE;
    struct uthread* next_uthread = find_next_uthread();

    if (next_uthread != 0) {
        next_uthread->state = RUNNING;
        context_swtch(next_uthread);
    } else {
        current_uthread->state = RUNNING;
    }
}

void uthread_exit() {
    current_uthread->state = FREE;
    
    struct uthread* next_uthread = find_next_uthread();

    if (next_uthread != 0) {
        context_swtch(next_uthread);
    } else {
        exit(0);
    }
}

enum sched_priority uthread_set_priority(enum sched_priority priority){
    enum sched_priority prev_priority = current_uthread->priority;
    current_uthread->priority = priority;
    return prev_priority;
}

enum sched_priority uthread_get_priority(){
    return current_uthread->priority;
}

int uthread_start_all(){
    static int first = 1;

    if (first) {
        first = 0;

        struct uthread* next_uthread = find_next_uthread();

        if (next_uthread != 0) {
            context_swtch(next_uthread);
        }
    }

    return -1;
}

struct uthread* uthread_self(){
    return current_uthread;
}

struct uthread* find_next_uthread(){
    struct uthread* uthread_iter;
    if (current_uthread == 0) {
        // if this is the first round, we want to start the while loop 
        // at the begginning of uthreads and end it at the end of uthreads
        uthread_iter = &uthreads[0];
        current_uthread = &uthreads[MAX_UTHREADS]; 
    } else {
        // otherwise, we start the search from the uthread after current_uthread
        uthread_iter = current_uthread + sizeof(struct uthread);
    }
    
    struct uthread* next_uthread = 0;

    while (uthread_iter != current_uthread) {
        if (uthread_iter == &uthreads[MAX_UTHREADS]) {
            uthread_iter = &uthreads[0];
        }

        if (uthread_iter->state == RUNNABLE && (next_uthread == 0 || uthread_iter->priority > next_uthread->priority)) {
            next_uthread = uthread_iter;
        }
        
        uthread_iter += sizeof(struct uthread);
    }

    return next_uthread;
}

void context_swtch(struct uthread* next_uthread){
    struct uthread* prev_thread = current_uthread;

    // update the current thread pointer
    current_uthread = next_uthread;

    // save the context of the current thread and switch to the next thread
    uswtch(&prev_thread->context, &current_uthread->context);
}

void start_func_wrapper() {
    // a wrapper function for start_func that calls uthread_exit after start_func finishes
    uthread_self()->start_func();
    uthread_exit();
}