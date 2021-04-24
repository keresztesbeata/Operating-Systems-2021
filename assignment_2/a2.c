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
#define ERROR_CREATING_SEMAPHORE 3

#define PRINT_ERROR_CREATING_PROCESS {perror("Cannot create new process"); exit(ERROR_CREATING_PROCESS);}
#define PRINT_ERROR_CREATING_THREAD {perror("Error creating a new thread"); exit(ERROR_CREATING_THREAD);}
#define PRINT_ERROR_CREATING_SEMAPHORE {perror("Error creating the semaphore");exit(ERROR_CREATING_SEMAPHORE);}


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
    int status;
    thread_args_t th_arg = *(thread_args_t*)arg;
    status = pthread_mutex_lock(&lock);
    if (status != 0)
        return 0;
    while (th_arg.th_id == 2 && !allowed_to_start){
        status = pthread_cond_wait(&cond_start, &lock);
        if (status != 0)
            return 0;
    }
    info(BEGIN, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 3) {
        allowed_to_start = true;
        status = pthread_cond_broadcast(&cond_start);
        if (status != 0)
            return 0;
    }
    status = pthread_mutex_unlock(&lock);
    if (status != 0)
        return 0;
    /* ----------------------------------------*/
    status = pthread_mutex_lock(&lock);
    if (status != 0)
        return 0;
    while (th_arg.th_id == 3 && !allowed_to_finish){
        status = pthread_cond_wait(&cond_end, &lock);
        if (status != 0)
            return 0;
    }
    info(END, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 2) {
        allowed_to_finish = true;
        status = pthread_cond_broadcast(&cond_end);
        if (status != 0)
            return 0;
    }
    status = pthread_mutex_unlock(&lock);
    if (status != 0)
        return 0;
    return 0;
}

void create_thread(int nr) {
    if (pthread_create(&th[nr], NULL, do_work,&th_args[nr]) != 0) {
        PRINT_ERROR_CREATING_THREAD
    }
}

void synchronizing_threads_in_same_process(){
    // initialize the thread arguments
    for(int i=0;i<4;i++) {
        th_args[i].pr_id = 3;
        th_args[i].th_id = i+1;
    }
    // create the threads
    for(int i=0;i<4;i++) {
        create_thread(i);
    }
    // join the created threads
    for(int i=0;i<4;i++) {
        pthread_join(th[i], NULL);
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

    if(p_id == 3) {
        synchronizing_threads_in_same_process();
    }
    // wait for the child processes to terminate
    while(wait(&return_status) > 0) {
        c_nr--;
    }
    info(END, p_id, 0);
    return 0;
}
