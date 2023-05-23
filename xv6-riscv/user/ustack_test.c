#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/ustack.h"


#define BUFFER_SIZES { 50, 128, 300, 512}

void test_allocator() {

    char* state;
    // Attempt to allocate a buffer larger than the maximum allowed size
    void* large_buffer = ustack_malloc(MAX_ALLOC_SIZE * 2);
    if (large_buffer == (void*)-1)
        state = "TEST PASSED";
    else
        state = "TEST FAILED";
    printf("Allocation of a large buffer test - %s\n", state);


    // Test empty stack case
    int length = ustack_free();
    if (length == -1)
        state = "TEST PASSED";
    else
        state = "TEST FAILED";

    printf("Free of empty 'heap' - %s\n", state);


    int buffer_sizes[] = BUFFER_SIZES;
    int num_buffers = sizeof(buffer_sizes) / sizeof(buffer_sizes[0]);
    void* buffers[num_buffers];

    // Allocate buffers of different sizes
    state = "TEST PASSED";
    for (int i = 0; i < num_buffers; i++) {
        buffers[i] = ustack_malloc(buffer_sizes[i]);
        if (buffers[i] == (void*)-1){
            printf("Allocation of buffer %d failed\n", i);
            state = "TEST FAILED";
            break;
        }
        else
            printf("Buffer %d allocated successfully\n", i);
    }

    printf("Allocation of buffers of different sizes - %s\n", state);


    // Free buffers in reverse order
    state = "TEST PASSED";
    for (int i = num_buffers - 1; i >= 0; i--) {
        int length = ustack_free();
        if (length == -1){
            printf("Error occured\n");
            state = "TEST FAILED";
            break;

        }
        else if (length != buffer_sizes[i]){
            printf("Number of bytes freed memory does not match tonumber of bytes allocated - %d vs %d\n", length, buffer_sizes[i]);
            state = "TEST FAILED";
            break;

        }
        else
            printf("Buffer of length %d freed\n", length);
    }

    printf("Free buffers in reverse order - %s\n", state);

    // try combinations of malloc and free? m m f m f f
    // TODO check with len = 0
}

int main() {
    test_allocator();
    exit(0);
}
