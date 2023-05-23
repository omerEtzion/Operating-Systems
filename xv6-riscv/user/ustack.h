#define MAX_ALLOC_SIZE 512

/* 
    Allocates a buffer of length len onto the stack and returns a pointer to
    the beginning of the buffer.
    If len is larger than the maximum allowed size of an allocation operation, the function should return -1. If a call to sbrk() fails, the function
    should return -1.
*/
void* ustack_malloc(uint len); 


/*
    Frees the last allocated buffer and pops it from the stack. After this call
    completes, any pointers to the popped buffer are invalid and must not
    be used.
    Should return -1 on error, and the length of the freed buffer on success.
    If the stack is empty, the function should return -1. 
*/
 int ustack_free(void);