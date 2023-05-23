#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user.h"
#include "ustack.h"


// define a struct to represent a stack node
typedef struct StackNode {
    void* buffer;
    uint32 length;
    struct StackNode* prev;
} StackNode;

StackNode* stack_top = 0;
void* heap_end = 0;

void* ustack_malloc(uint32 len) {
    
    // check if len exceeds the maximum allowed size
    if (len > MAX_ALLOC_SIZE)
        return (void*)-1;  
    
    // No allocation has been made yet, initialize the stack
    if (stack_top == 0) {
        heap_end = sbrk(0);
    }

    void* prev_buffer_end = 0;
    void* new_node_ptr;
    if (stack_top != 0){
        prev_buffer_end = stack_top->buffer + stack_top->length;
    }
    void* new_end = (uint8*)prev_buffer_end + len;
    printf("new end = %d, heap_end = %d, stack_top = %d\n", new_end, heap_end, stack_top);

    // Calculate the required alignment for StackNode struct
    uint32 alignment = sizeof(StackNode);

    // Calculate the aligned size
    uint32 aligned_size = (len + alignment - 1) & ~(alignment - 1);

    // sbrk is needed
    if (new_end > heap_end){
        printf("calling sbrk\n");
        // new_node_ptr = sbrk(len);  // allocate memory using sbrk()
        new_node_ptr = sbrk(aligned_size); // allocate memory using sbrk()
        // heap_end = heap_end + PGSIZE;
        heap_end = heap_end + aligned_size;

        if (new_node_ptr == (void*)-1)
            return (void*)-1;  // sbrk() failed
    } else {
        new_node_ptr = (uint8*)prev_buffer_end + ((alignment - 1) & ~ (alignment - 1));
    }
    // else
        // new_node_ptr = new_end;

    printf("new_node_ptr = %d\n", new_node_ptr);
    // create a new stack node and update the stack top
    StackNode* node = (StackNode*)new_node_ptr;
    node->buffer = new_node_ptr;
    node->length = len;
    node->prev = stack_top;
    stack_top = node;
    printf("node - %d\nbuff - %d\nlen - %d\nprev - %d\n\n", stack_top, stack_top->buffer, stack_top->length, stack_top->prev);
    
    return new_node_ptr;
}


int ustack_free() {
    printf("\n\tstart free\n");

    StackNode* node = stack_top;
    while(node != 0){
        printf("\n\tcurr size - %d\n", node->length);
        node = node->prev;
    }

    if (stack_top == 0)
        return -1;  // Stack is empty
    
    printf("\tafter first if\n");
    // get the buffer and length of the top node
    void* buffer = stack_top->buffer;
    uint32 length = stack_top->length;

    // free the buffer using sbrk() if it crosses a page boundary
    void* new_end = (uint8*)buffer - length;
    void* page_boundary = (void*)((uint64)new_end & ~(PGSIZE - 1));
    printf("\tbefore if\n");

    // update the stack top and release the memory of the top node
    stack_top = stack_top->prev;

    // void* page_boundary = (void*)PGROUNDDOWN((uint32)new_end);
    if (page_boundary < heap_end) {
        printf("\tinside if\n");
        uint32 diff = (uint8*)heap_end - (uint8*)page_boundary;
        printf("\tdiff = %d\n", diff);
        sbrk(-diff);
        heap_end = page_boundary;
    }

    printf("\tafter all\n\n");

    // return the length of the freed buffer
    return length;
}
