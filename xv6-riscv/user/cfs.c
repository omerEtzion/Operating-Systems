#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char** argv){
    int pid = fork();

    if (pid != 0) {
        set_cfs_priority(0);
        int i;
        for (i = 0; i < 1000000; i++) {
            if (i%100000 == 0)
                sleep(10); // 1 sec
        }
        int cfs_priority;
        int rtime;
        int stime;
        int retime; 
        int pid = getpid();
        get_cfs_stats(pid, &cfs_priority, &rtime, &stime, &retime);
        wait(0, 0);
        printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
    
        exit(0, "");

    } else {
        pid = fork();

        if (pid != 0) {
            set_cfs_priority(1);
            int j;
            for (j = 0; j < 1000000; j++) {
                if (j%100000 == 0)
                    sleep(10); // 1 sec
            }

            int pid = getpid();
            int cfs_priority;
            int rtime;
            int stime;
            int retime; 
            get_cfs_stats(pid, &cfs_priority, &rtime, &stime, &retime);
            wait(0, 0);
            printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
            
            exit(0, "");

        } else {
            set_cfs_priority(2);
            int k;
            for (k = 0; k < 3000000; k++) {
                if (k%300000 == 0)
                    sleep(10); // 1 sec
            }

            int cfs_priority;
            int rtime;
            int stime;
            int retime; 
            int pid = getpid();
            get_cfs_stats(pid, &cfs_priority, &rtime, &stime, &retime);
            printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
            
            exit(0, "");
        }
    }
}