#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char** argv){
    int pid = fork();

    if (pid != 0) {
        // set_ps_priority(1);
        for (;;) {
            set_ps_priority(1);
            fprintf(2, "1");
        }
        
    } else {
        pid = fork();

        if (pid != 0) {
            // set_ps_priority(5);
            for (;;) {
                set_ps_priority(5);
                fprintf(2, "2");
            }
        } else {
            // set_ps_priority(10);
            for (;;) {
                set_ps_priority(10);
                fprintf(2, "3");
            }
        }
    }


    exit(0, "");
}