#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char** argv){
    // int pid = fork();

    // if (pid != 0) {
    //     set_cfs_priority(0);
    //     int i;
    //     for (i = 0; i < 1000000; i++) {
    //         if (i%100000 == 0)
    //             sleep(10); // 1 sec
    //     }
    //     uint64* cfs_priority = 0;
    //     uint64* rtime = 0;
    //     uint64* stime = 0;
    //     uint64* retime = 0; 
    //     pid = getpid();
    //     get_cfs_stats(pid, cfs_priority, rtime, stime, retime);
    //     printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, *cfs_priority, *rtime, *stime, *retime);
        
    // } else {
    //     pid = fork();

    //     if (pid != 0) {
    //         set_cfs_priority(1);
    //         int j;
    //         for (j = 0; j < 1000000; j++) {
    //             if (j%100000 == 0)
    //                 sleep(10); // 1 sec
    //         }

    //         uint64* cfs_priority = 0;
    //         uint64* rtime = 0;
    //         uint64* stime = 0;
    //         uint64* retime = 0; 
    //         pid = getpid();
    //         get_cfs_stats(pid, cfs_priority, rtime, stime, retime);
    //         printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, *cfs_priority, *rtime, *stime, *retime);
        
    //     } else {
    //         set_cfs_priority(2);
    //         int k;
    //         for (k = 0; k < 1000000; k++) {
    //             if (k%100000 == 0)
    //                 sleep(10); // 1 sec
    //         }

    //         uint64* cfs_priority = 0;
    //         uint64* rtime = 0;
    //         uint64* stime = 0;
    //         uint64* retime = 0; 
    //         pid = getpid();
    //         get_cfs_stats(pid, cfs_priority, rtime, stime, retime);
    //         printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, *cfs_priority, *rtime, *stime, *retime);
    //     }
    // }

    int cfs_priority;
    int rtime;
    int stime;
    int retime; 
    int pid = getpid();
    printf("%d, %d, %d, %d, %d\n", pid, &cfs_priority, &rtime, &stime, &retime);
    get_cfs_stats(pid, &cfs_priority, &rtime, &stime, &retime);
    printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
    

    exit(0, "");
}