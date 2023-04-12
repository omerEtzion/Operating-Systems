#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char** argv){
    
    if (argc != 2 || argv[1][1] != '\0' || argv[1][0] > '2' || argv[1][0] < '0' ) {
        exit(1, "invalid argument\n");
    } else {
        int return_val = set_policy(argv[1][0] - '0');
        if (return_val != 0) {
            exit(1, "change of policy failed\n");
        } else {
            exit(0, "change of policy succeeded\n");
        }
    }

    // uint64 return_val = set_policy(argv[1][0] - '0');
    // if (return_val != 0) {
    //     exit(1, "failed to change scheduling policy\n");
    // } else {
    //     exit(0, "changed scheduling policy successfully\n");
    // }
}