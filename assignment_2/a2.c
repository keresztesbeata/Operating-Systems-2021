#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "a2_helper.h"

#define ERROR_CREATING_PROCESS 1
#define ERROR_TERMINATING_PROCESS 2

#define PRINT_ERROR_CREATING_P {perror("Cannot create new process"); exit(ERROR_CREATING_PROCESS);}
#define PRINT_ERROR_TERMINATING_P {perror("Error terminating process"); exit(ERROR_TERMINATING_PROCESS);}

pid_t pid;
pid_t pid1;
pid_t pid2;
pid_t pid3;
pid_t pid4;
pid_t pid5;
pid_t pid6;
pid_t pid7;
pid_t pid8;
pid_t pid9;

int p_nr = 1; // parent's id
int c_nr = 1; // child's id

int main(){
    init();
    info(BEGIN, 1, 0);
    int return_status;
    c_nr++;
    pid = fork();   // create P2
    if(pid < 0) PRINT_ERROR_CREATING_P
    if(pid == 0) { // inside child processes
        p_nr = c_nr;
        info(BEGIN, p_nr, 0);
        c_nr++;
        pid1 = fork();   // create P3
        if(pid1 < 0) PRINT_ERROR_CREATING_P
        if(pid1 == 0) { // inside child process
            p_nr = c_nr;
            info(BEGIN,p_nr,0);
        }
        c_nr++;
        if(pid1 > 0) {
            pid2 = fork();   // create P4
            if(pid2 < 0) PRINT_ERROR_CREATING_P
            if(pid2 == 0) { // inside child process
                p_nr = c_nr;
                info(BEGIN,p_nr,0);
            }
        }
        if(pid1 == 0)
            c_nr++;
        if(pid1 == 0 || pid2 == 0) {
            c_nr++;
            pid = fork();   // create P5,P6 by P3 and P4
            if(pid < 0) PRINT_ERROR_CREATING_P
            if(pid == 0) { // inside child process
                p_nr = c_nr;
                info(BEGIN,p_nr,0);
            }
        }
    }
    // wait for the child processes to terminate
    while(wait(&return_status) > 0) {
        c_nr--;
    }
    info(END, p_nr, 0);

    return 0;
}
