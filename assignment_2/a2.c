#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include "a2_helper.h"
#include <semaphore.h>
#define ERROR_CREATING_PROCESS 1
#define ERROR_CREATING_THREAD 2
#define ERROR_JOINING_THREAD 3
#define ERROR_LOCKS 4
#define ERROR_COND_VAR 5
// #define ERROR_CREATING_SEMAPHORE 3

#define PRINT_ERROR_CREATING_PROCESS { perror("Cannot create new process."); }
#define PRINT_ERROR_CREATING_THREAD { perror("Error creating a new thread."); }
#define PRINT_ERROR_JOINING_THREAD { perror("Error joining a thread."); }
#define PRINT_ERROR_LOCKS { perror("Error trying to unlock or acquire a lock."); }
#define PRINT_ERROR_COND_VAR { perror("Error waiting for or signaling a condition."); }
// #define PRINT_ERROR_CREATING_SEMAPHORE {perror("Error creating the semaphore");exit(ERROR_CREATING_SEMAPHORE);}

typedef struct thread_args{
    int th_id;
    int pr_id;
}thread_args_t;

int p_id = 1; // parent's id
int c_nr = 1; // child count
pthread_t th[4];
thread_args_t th_args[4];

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_start = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_end = PTHREAD_COND_INITIALIZER;

bool allowed_to_finish = false;
bool allowed_to_start = false;

void create_process(int parent_id) {
    c_nr++;
    if(p_id == parent_id) {
        pid_t pid = fork();
        if(pid < 0) PRINT_ERROR_CREATING_PROCESS
        if(pid == 0) { // inside child process
            p_id = c_nr;
            info(BEGIN, p_id, 0);
        }
    }
}

void * do_work(void * arg) {
    int error_code = 0;
    int * return_value = &error_code;
    thread_args_t th_arg = *(thread_args_t*)arg;
    if(pthread_mutex_lock(&lock) != 0) {
        error_code = ERROR_LOCKS;
        goto finish;
    }
    while (th_arg.th_id == 2 && !allowed_to_start){
        if(pthread_cond_wait(&cond_start, &lock) != 0) {
            error_code = ERROR_COND_VAR;
            goto finish;
        }
    }
    info(BEGIN, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 3) {
        allowed_to_start = true;
        if(pthread_cond_broadcast(&cond_start) != 0) {
            error_code = ERROR_COND_VAR;
         goto finish;
        }
    }
    if(pthread_mutex_unlock(&lock) != 0) {
        error_code = ERROR_LOCKS;
        goto finish;
    }
    if(pthread_mutex_lock(&lock) != 0) {
        error_code = ERROR_LOCKS;
        goto finish;
    }
    while (th_arg.th_id == 3 && !allowed_to_finish){
        if(pthread_cond_wait(&cond_end, &lock) != 0) {
            error_code = ERROR_COND_VAR;
            goto finish;
        }
    }
    info(END, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 2) {
        allowed_to_finish = true;
        if(pthread_cond_broadcast(&cond_end) != 0) {
            error_code = ERROR_COND_VAR;
            goto finish;
        }
    }
    if(pthread_mutex_unlock(&lock) != 0) {
        error_code = ERROR_LOCKS;
        goto finish;
    }

    finish:
    return return_value;
}

int synchronizing_threads_in_same_process(){
    int error_code = 0;
    // initialize the thread arguments
    for(int i=0;i<4;i++) {
        th_args[i].pr_id = 3;
        th_args[i].th_id = i+1;
    }
    // create the threads
    for(int i=0;i<4;i++) {
        if (pthread_create(&th[i], NULL, do_work,&th_args[i]) != 0) {
            error_code = ERROR_CREATING_THREAD;
        }
    }
    // join the created threads
    for(int i=0;i<4;i++) {
        int status = 0;
        if(pthread_join(th[i], (void *)&status) != 0) {
            error_code = ERROR_JOINING_THREAD;
        }
        else if(status == ERROR_LOCKS) {
            PRINT_ERROR_LOCKS
        }else if(status == ERROR_COND_VAR) {
            PRINT_ERROR_COND_VAR
        }
    }
    return error_code;
}

void threads_barrier() {

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

    if(p_id == 3) {
        int return_code = synchronizing_threads_in_same_process();
        if(return_code == ERROR_CREATING_THREAD) {
            PRINT_ERROR_CREATING_THREAD
        }else if(return_code == ERROR_JOINING_THREAD) {
            PRINT_ERROR_JOINING_THREAD
        }
    }
    // wait for the child processes to terminate
    while(wait(&return_status) > 0) {
        c_nr--;
    }
    info(END, p_id, 0);
    return 0;
}
