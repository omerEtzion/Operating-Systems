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

    // // allocate a new stack for the thread
    // void *ustack = malloc(STACK_SIZE);

    // // failed to allocate memory for the stack
    // if (ustack == 0) {
    //     uthreads[free_entry].state = FREE;
    //     return -1;
    // }
    // uthreads[free_entry].ustack = (char*)ustack;

    // ***** no need to allocate a new stack, the stack is statically allocated to the struct uthread
    // ***** instead, just set the sp to the top (last address) of the stack


    // set the 'ra' register to the start_func and 'sp' to the top of the relevant ustack
    uthreads[free_entry].context.sp = uthreads[free_entry].ustack + STACK_SIZE - 1;
    uthreads[free_entry].context.ra = (uint64)start_func;

    // set as RUNNABLE
    uthreads[free_entry].state = RUNNABLE;
    return 0;

    // TODO -> meaning?
    // Hint: A uthread_exit(…) call should be carried out explicitly at the end of
    // each given start_func(…) .
}

// void uthread_yield() {
//     int i;
//     struct uthread* next_uthread = 0;

//     // find next thread
//     for (i = 0; i < MAX_UTHREADS; i++) {
//         if (uthreads[i].state == RUNNABLE && (next_uthread == 0 || uthreads[i].priority > next_uthread->priority)) {
//             next_uthread = &uthreads[i];
//         }
//     }

//     if (next_uthread != 0) {
//         struct uthread* prev_thread = current_uthread;

//         // update the current thread pointer
//         current_uthread = next_uthread;

//         // save the context of the current thread and switch to the next thread
//         uswtch(&prev_thread->context, &current_uthread->context);
//     }
// }

void uthread_yield() {
    current_uthread->state = RUNNABLE;
    struct uthread* next_uthread = find_next_uthread();

    if (next_uthread != 0) {
        next_uthread->state = RUNNING;
        context_swtch(next_uthread);
    } else {
        current_uthread->state = RUNNING;
    }

    // this is the code if a uthread of higher priority does not yield to a uthread of lower priority
    // if (next_uthread != current_uthread) {
    //     next_uthread->state = RUNNING;
    //     context_swtch(next_uthread);
    // } else {
    //     current_uthread->state = RUNNING;
    // }
}

// void uthread_exit() {
//     current_uthread->state = FREE;
//     int num_free_threads = 0;
//     int i;
//     for (i = 0; i < MAX_UTHREADS; i++) {
//         if (uthreads[i].state == FREE) {
//             num_free_threads++;
//         }
//     }
//     if (num_free_threads == MAX_UTHREADS)
//         exit(0);
    
//     else
//         uthread_yield();

//     // TODO -> maybe too loop over threads array and look for threads[i] == RUNNABLE and then call uswtch (might be nore efficient)
// }

void uthread_exit() {
    current_uthread->state = FREE;
    
    struct uthread* next_uthread = find_next_uthread();

    if (next_uthread != 0) {
        context_swtch(next_uthread);
    } else {
        exit(0);
    }

    // this is the code if a uthread of higher priority does not yield to a uthread of lower priority
    // if (next_uthread != current_uthread) {
    //     context_swtch(next_uthread);
    // } else {
    //     exit(0);
    // }
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

        // this is the code if a uthread of higher priority does not yield to a uthread of lower priority
        // if (next_uthread != current_uthread) {
        //     context_swtch(next_uthread);
        // }
    }

    return -1;
}

struct uthread* uthread_self(){
    return current_uthread;
}

// struct uthread* find_next_uthread(){
//     int i;
//     struct uthread* next_uthread = 0;

//     // find next thread
//     for (i = 0; i < MAX_UTHREADS; i++) {
//         if (uthreads[i].state == RUNNABLE && (next_uthread == 0 || uthreads[i].priority > next_uthread->priority)) {
//             next_uthread = &uthreads[i];
//         }
//     }

//     return next_uthread;
// }

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

    // ***** right now, even if the current_uthread is the highest priority, it will yield its time to a lower priority uthread when calling uthread_yield
    // should it be like this? if not, this is the correct code:

    // struct uthread* next_uthread = current_uthread;

    // while (uthread_iter != current_uthread) {
    //     if (uthread_iter == &uthreads[MAX_UTHREADS]) {
    //         uthread_iter = &uthreads[0];
    //     }

    //     if (uthread_iter->state == RUNNABLE && (uthread_iter->priority > next_uthread->priority || (uthread_iter->priority == next_uthread->priority && next_uthread == current_uthread))) {
    //         // if uthread_iter is of higher priority, or if it's the first uthread we found of equal priority (for the round robin)
    //         next_uthread = uthread_iter;
    //     }
        
    //     uthread_iter += sizeof(struct uthread);
    // }

    // return next_uthread;

    // and then, if this function returns current_uthread instead of 0, we know that there are no higher priority uthreads

}

void context_swtch(struct uthread* next_uthread){
    struct uthread* prev_thread = current_uthread;

    // update the current thread pointer
    current_uthread = next_uthread;

    // save the context of the current thread and switch to the next thread
    uswtch(&prev_thread->context, &current_uthread->context);
}