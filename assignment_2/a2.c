#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "a2_helper.h"

#define ERROR_CREATING_PROCESS 1
#define ERROR_CREATING_THREAD 1

#define PRINT_ERROR_CREATING_PROCESS {perror("Cannot create new process"); exit(ERROR_CREATING_PROCESS);}
#define PRINT_ERROR_CREATING_THREAD {perror("Error creating a new thread"); exit(ERROR_CREATING_THREAD);};

typedef struct thread_args{
    int th_id;
    int p_id;
}thread_args_t;

int p_nr = 1; // parent's id
int c_nr = 1; // child count
pthread_t th[4];
thread_args_t th_args[4];

void create_process(int nr) {
    c_nr++;
    if(p_nr == nr) {
        pid_t pid = fork();
        if(pid < 0) PRINT_ERROR_CREATING_PROCESS
        if(pid == 0) { // inside child process
            p_nr = c_nr;
            info(BEGIN,p_nr,0);
        }
    }
}

void * do_work(void * arg) {
    thread_args_t th_arg = *(thread_args_t*)arg;
    info(BEGIN, th_arg.p_id, th_arg.th_id);
    // wait for the previous thread to finish
    int prev_th_id = th_arg.th_id-2;
    if(prev_th_id >= 0) {
        pthread_join(th[prev_th_id], NULL);
    }
    info(END, th_arg.p_id, th_arg.th_id);
    return 0;
}

void create_thread(int nr) {
    nr--;
    if (pthread_create(&th[nr], NULL, do_work,&th_args[nr]) != 0) {
        PRINT_ERROR_CREATING_THREAD
    }
}

void create_and_joining_threads(){
    // initialize the thread arguments
    for(int i=0;i<4;i++) {
        th_args[i].p_id = 3;
        th_args[i].th_id = i+1;
    }
    // create the threads
    create_thread(1);
    create_thread(3);
    create_thread(2);
    create_thread(4);
    // join the last thread
    pthread_join(th[3],NULL);
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

    if(p_nr == 3) {
        create_and_joining_threads();
    }
    // wait for the child processes to terminate
    while(wait(&return_status) > 0) {
        c_nr--;
    }
    info(END, p_nr, 0);
    return 0;
}
