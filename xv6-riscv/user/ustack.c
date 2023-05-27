#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user.h"
#include "ustack.h"





typedef struct Node {
    void* buffer;
    uint32 length;
    struct Node* prev;
} Node;

static char* stack_base = 0;
static char* stack_top = 0;
static Node* stack_list = 0;

void* ustack_malloc(uint32 len) {
    if(len > MAX_ALLOC_SIZE) {
        return (void*)-1;  // len exceeds the maximum allowed size
    }

    if(stack_base == 0) {
        // first call to ustack_malloc, allocate a new page
        stack_base = (char*)sbrk(PGSIZE);
        stack_top = stack_base;
    }

    if((uint64)(stack_top + len) > (uint64)(stack_base + PGSIZE)) {
        // not enough space in the current page, allocate a new page
        printf("not enough space in the current page, allocate a new page\n");
        stack_base = (char*)sbrk(PGSIZE);
        stack_top = stack_base;
    }

    Node* node = (Node*)stack_top;
    stack_top += sizeof(Node);

    node->buffer = stack_top;
    node->length = len;
    node->prev = stack_list;
    stack_list = node;

    stack_top += len; 
    
    // printf("node - %d\nbuff - %d\nlen - %d\nprev - %d\n\n", stack_list, stack_list->buffer, stack_list->length, stack_list->prev);

    return node->buffer;
}

int ustack_free() {

    if(stack_list == 0) {
        return -1;  // stack is empty
    }

    Node* node = stack_list;
    stack_list = stack_list->prev;
    
    stack_top = node->buffer;
    stack_top -= sizeof(Node);
    
    // stack_base = (char*)node;

    // save length before releasing page (edge case)
    int length = node->length;

    // TODO if allocated more then 1 page
    if(stack_top <= stack_base) {
        // All buffers have been freed, release the page
        printf("releasing page\n");
        sbrk(-PGSIZE);
        stack_base -= PGSIZE;
    }

    return length;
}

