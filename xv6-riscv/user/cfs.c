#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// int get_string_length(int n) {
//     int length = 0;
    
//     // Special case for 0
//     if (n == 0) {
//         return 1;
//     }
    
//     // Calculate the number of digits in n
//     while (n != 0) {
//         length++;
//         n /= 10;
//     }
    
//     // Add 1 for the null terminator
//     length++;
    
//     return length;
// }

// void int_to_string(int n, char* str) {
//     if (n == 0) {
//         str[0] = '0';
//         str[1] = '\0';
//         return;
//     }

//     int is_negative = n < 0;
//     if (is_negative) {
//         n = -n;
//     }

//     int i = 0;
//     while (n > 0) {
//         str[i++] = '0' + (n % 10);
//         n /= 10;
//     }

//     if (is_negative) {
//         str[i++] = '-';
//     }

//     str[i] = '\0';

//     // Reverse the string
//     int j = i - 1;
//     i = 0;
//     while (i < j) {
//         char temp = str[i];
//         str[i++] = str[j];
//         str[j--] = temp;
//     }
// }

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
        printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
    
        wait(0, 0);
        exit(0, "proc1 finished\n");

    } else {
        pid = fork();

        if (pid != 0) {
            set_cfs_priority(1);
            int j;
            for (j = 0; j < 1000000; j++) {
                if (j%100000 == 0)
                    sleep(20); // 1 sec
            }

            int pid = getpid();
            int cfs_priority;
            int rtime;
            int stime;
            int retime; 
            get_cfs_stats(pid, &cfs_priority, &rtime, &stime, &retime);
            sleep(10);
            printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
            
            wait(0, 0);
            exit(0, "proc2 finished\n");

        } else {
            set_cfs_priority(2);
            int k;
            for (k = 0; k < 1000000; k++) {
                if (k%100000 == 0)
                    sleep(10); // 1 sec
            }

           int cfs_priority;
            int rtime;
            int stime;
            int retime; 
            int pid = getpid();
            get_cfs_stats(pid, &cfs_priority, &rtime, &stime, &retime);
            sleep(20);
            printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n\n", pid, cfs_priority, rtime, stime, retime);
            
            exit(0, "proc3 finished\n");
        }
    }
}