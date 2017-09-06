#include "types.h"
#include "stat.h"
#include "user.h"

int 
main (int argc, char *argv[]) {
    int child_pid = fork();
    if(child_pid < 0) {
        printf(1,"Error : fork failed.\n");
        exit();
    }
    else if(child_pid == 0) {
        int i = 0;
        while (i < 10) {
            yield();
            printf(1,"Child\n");
            i++;
        }
        wait();
        exit();
    }
    else {
        int i = 0;
        while (i < 10) {
            yield();
            printf(1,"Parent\n");
        }
        wait();
        exit();
    }
}
