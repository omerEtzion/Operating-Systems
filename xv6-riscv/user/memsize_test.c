#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char** argv){
    fprintf(2, "------------- memsize test -------------\n");
    fprintf(2, "before malloc -> memory in bytes: %d\n", memsize());

    int *ptr;
    ptr = (int*) malloc(0);

    fprintf(2, "after malloc -> memory in bytes: %d\n", memsize());

    free(ptr);
    fprintf(2, "after freeing tha allocated memory -> memory in bytes: %d\n", memsize());


    exit(0, "");
}