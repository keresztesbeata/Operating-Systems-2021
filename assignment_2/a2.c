#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include "a2_helper.h"
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>

#define ERROR_CREATING_PROCESS 1
#define ERROR_CREATING_THREAD 2
#define ERROR_JOINING_THREAD 3
#define ERROR_CREATING_SEMAPHORE 4

#define PRINT_ERROR_CREATING_PROCESS { perror("Cannot create new process."); }
#define PRINT_ERROR_CREATING_THREAD { perror("Error creating a new thread."); }
#define PRINT_ERROR_JOINING_THREAD { perror("Error joining a thread."); }
#define PRINT_ERROR_CREATING_SEMAPHORE {perror("Error creating the semaphore");}

#define SEM_START_TH3 "/sema3"
#define SEM_START_TH4 "/sema4"

#define SEM_ENTER "/sem_enter"
#define SEM_LEAVE "/sem_leave"
#define SEM_STAY "/sem_stay"
#define SEM_COUNT "/sem_count"

typedef struct thread_args{
    int th_id;
    int pr_id;
}thread_args_t;

int p_id = 1; // parent's id
int c_nr = 1; // child count

sem_t sem_end_after_th2,sem_start_after_th3;
sem_t *sem_enter,*sem_leave,*sem_stay,*sem_count;
sem_t *sem_start_th4, *sem_start_th3;

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

void P(sem_t *sem)
{
    sem_wait(sem);
}

void V(sem_t *sem)
{
    sem_post(sem);
}

void * task_of_threads_in_p3(void * arg) {
    thread_args_t th_arg = *(thread_args_t*)arg;
    if(th_arg.th_id == 2) { // thread 2 has to start after thread 3
        P(&sem_start_after_th3);
    }
    if(th_arg.th_id == 4) { // thread 4 has to start after thread 2 from P2
        P(sem_start_th4);
    }
    info(BEGIN, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 3) {
        V(&sem_start_after_th3);
    }
    if(th_arg.th_id == 3) { // thread 3 has to leave after thread 2
        P(&sem_end_after_th2);
    }
    info(END, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 2) {
        V(&sem_end_after_th2);
    }
    if(th_arg.th_id == 4) { // when thread 4 finishes it must signal thread 3 from P2 to start
        V(sem_start_th3);
    }
    return 0;
}

int synchronizing_threads_in_same_process(){
    int error_code = 0;
    pthread_t th[4];
    thread_args_t th_args[4];
    if (sem_init(&sem_start_after_th3, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    if (sem_init(&sem_end_after_th2, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    sem_start_th3 = sem_open(SEM_START_TH3,O_CREAT,0600,0);
    if(sem_start_th3 == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    sem_start_th4 = sem_open(SEM_START_TH4,O_CREAT,0600,0);
    if(sem_start_th4== SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // initialize the thread arguments
    for(int i=0;i<4;i++) {
        th_args[i].pr_id = 3;
        th_args[i].th_id = i+1;
    }
    // create the threads
    for(int i=0;i<4;i++) {
        if (pthread_create(&th[i], NULL, task_of_threads_in_p3, &th_args[i]) != 0) {
            error_code = ERROR_CREATING_THREAD;
        }
    }
    // join the created threads
    for(int i=0;i<4;i++) {
        int status = 0;
        if(pthread_join(th[i], (void *)&status) != 0) {
            error_code = ERROR_JOINING_THREAD;
            goto finish;
        }
    }
    finish:
    sem_destroy(&sem_start_after_th3);
    sem_destroy(&sem_end_after_th2);
    return error_code;
}

bool allow_to_leave = true;

void * task_of_threads_in_p7(void * arg) {
    thread_args_t th_arg = *(thread_args_t*)arg;
    int value;
    sem_getvalue(sem_count,&value);
    if(!allow_to_leave && value >= 3) {
        //printf("you can leave now\n");
        V(sem_leave);
    }
    P(sem_enter);
    // at most 4 threads can enter at a time
    info(BEGIN, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 15) {
        allow_to_leave = false;
        P(sem_leave);
    }

    if(th_arg.th_id != 15 && !allow_to_leave) {
        //printf("thread %d is blocked\n",th_arg.th_id);
        V(sem_count);
        P(sem_leave);
       // printf("t %d signal for the next to leave\n",th_arg.th_id);
        V(sem_leave);
    }
    info(END, th_arg.pr_id, th_arg.th_id);

    if(th_arg.th_id == 15) {
        allow_to_leave = true;
        V(sem_leave);
    }

    V(sem_enter);
    return 0;
}

int threads_barrier() {
    int error_code = 0;
    pthread_t th[38];
    thread_args_t th_args[38];
    // create and initialize the semaphores
    sem_enter = sem_open(SEM_ENTER, O_CREAT, 0600, 4);
    if (sem_enter == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    int value;
    sem_getvalue(sem_enter,&value);
    printf("value sem_enter = %d\n",value);

    sem_leave = sem_open(SEM_LEAVE, O_CREAT, 0600, 0);
    if (sem_leave == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    sem_getvalue(sem_leave,&value);
    printf("value sem_leave= %d\n",value);

    sem_count= sem_open(SEM_COUNT, O_CREAT, 0600, 0);
    if (sem_count == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    sem_getvalue(sem_count,&value);
    printf("value sem_count= %d\n",value);

   //  initialize the thread arguments
    for(int i=0;i<38;i++) {
        th_args[i].pr_id = 7;
        th_args[i].th_id = i+1;
    }

    // create the threads
    for(int i=0;i<38;i++) {
        if (pthread_create(&th[i], NULL, task_of_threads_in_p7, &th_args[i]) != 0) {
            error_code = ERROR_CREATING_THREAD;
            goto finish;
        }
    }
    // join the created threads
    for(int i=0;i<38;i++) {
        int status = 0;
        if(pthread_join(th[i], (void *)&status) != 0) {
            error_code = ERROR_JOINING_THREAD;
        }
    }
    finish:
    sem_unlink(SEM_ENTER);
    sem_unlink(SEM_LEAVE);
    sem_unlink(SEM_COUNT);
    return error_code;
}

void * task_of_threads_in_p2(void * arg) {
    thread_args_t th_arg = *(thread_args_t*)arg;
    if(th_arg.th_id == 3) { // thread 3 must start after thread 4 from P3 finishes
        P(sem_start_th3);
    }
    info(BEGIN, th_arg.pr_id, th_arg.th_id);

    info(END, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 2) { // thread 2 has to signal thread 4 from P3 to start
        V(sem_start_th4);
    }
    return 0;
}

int synchronizing_threads_in_diff_processes() {
    int error_code = 0;
    pthread_t th[5];
    thread_args_t th_args[5];
    sem_start_th3 = sem_open(SEM_START_TH3,O_CREAT,0600,0);
    if(sem_start_th3 == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    sem_start_th4 = sem_open(SEM_START_TH4,O_CREAT,0600,0);
    if(sem_start_th4== SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // initialize the thread arguments
    for(int i=0;i<5;i++) {
        th_args[i].pr_id = 2;
        th_args[i].th_id = i+1;
    }
    // create the threads
    for(int i=0;i<5;i++) {
        if (pthread_create(&th[i], NULL, task_of_threads_in_p2, &th_args[i]) != 0) {
            error_code = ERROR_CREATING_THREAD;
        }
    }
    // join the created threads
    for(int i=0;i<5;i++) {
        int status = 0;
        if(pthread_join(th[i], (void *)&status) != 0) {
            error_code = ERROR_JOINING_THREAD;
            goto finish;
        }
    }
    finish:
    sem_close(sem_start_th3);
    sem_close(sem_start_th4);
    return error_code;
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

//    if(p_id == 2) {
//        int return_code = synchronizing_threads_in_diff_processes();
//        if(return_code == ERROR_CREATING_THREAD) {
//            PRINT_ERROR_CREATING_THREAD
//        }else if(return_code == ERROR_JOINING_THREAD) {
//            PRINT_ERROR_JOINING_THREAD
//        }else if(return_code == ERROR_CREATING_SEMAPHORE) {
//            PRINT_ERROR_CREATING_SEMAPHORE
//        }
//    }
//    if(p_id == 3) {
//        int return_code = synchronizing_threads_in_same_process();
//        if(return_code == ERROR_CREATING_THREAD) {
//            PRINT_ERROR_CREATING_THREAD
//        }else if(return_code == ERROR_JOINING_THREAD) {
//            PRINT_ERROR_JOINING_THREAD
//        }else if(return_code == ERROR_CREATING_SEMAPHORE) {
//            PRINT_ERROR_CREATING_SEMAPHORE
//        }
//    }
    if(p_id == 7) {
        int return_code = threads_barrier();
        if(return_code == ERROR_CREATING_THREAD) {
            PRINT_ERROR_CREATING_THREAD
        }else if(return_code == ERROR_JOINING_THREAD) {
            PRINT_ERROR_JOINING_THREAD
        }else if(return_code == ERROR_CREATING_SEMAPHORE) {
            PRINT_ERROR_CREATING_SEMAPHORE
        }
    }
    // wait for the child processes to terminate
    while(wait(&return_status) > 0) {
        c_nr--;
    }
    info(END, p_id, 0);
    return 0;
}

