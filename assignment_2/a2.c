#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "a2_helper.h"

#define ERROR_CREATING_PROCESS 1

#define PRINT_ERROR_CREATING_P {perror("Cannot create new process"); exit(ERROR_CREATING_PROCESS);}

int p_nr = 1; // parent's id
int c_nr = 1; // child count

void create_process(int nr) {
    c_nr++;
    if(p_nr == nr) {
        pid_t pid = fork();
        if(pid < 0) PRINT_ERROR_CREATING_P
        if(pid == 0) { // inside child process
            p_nr = c_nr;
            info(BEGIN,p_nr,0);
        }
    }
}

int main(){
    init();
    info(BEGIN, 1, 0);
    int return_status;
    create_process(1);
    create_process(2);
    create_process(2);
    create_process(4);
    create_process(3);
    create_process(5);
    create_process(4);
    create_process(6);
    // wait for the child processes to terminate
    while(wait(&return_status) > 0) {
        c_nr--;
    }
    info(END, p_nr, 0);
    return 0;
}
