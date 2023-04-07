#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char** argv){
    int pid = fork();

    if (pid != 0) {
        set_cfs_priority(0);
        for (;;) {
            //TODO: complete
        }
        
    } else {
        pid = fork();

        if (pid != 0) {
            set_cfs_priority(1);
            for (;;) {
                //TODO: complete
            }
        } else {
            set_cfs_priority(2);
            for (;;) {
                //TODO: complete
            }
        }
    }


    exit(0, "");
}