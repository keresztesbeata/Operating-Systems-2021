#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "a2_helper.h"
#include <semaphore.h>
#include <fcntl.h>

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

typedef struct thread_args{
    int th_id;
    int pr_id;
}thread_args_t;

int p_id = 1; // parent process's id
int c_nr = 1; // child process count

// semaphores used by process 2 and 3
sem_t sem_end_after_th2;
sem_t sem_start_after_th3;
sem_t *sem_start_th4;
sem_t *sem_start_th3;

// semaphores used by process 7
sem_t sem_limit;
sem_t sem_leave;
sem_t sem_barrier;
sem_t sem_enter;

/*
 * Lets the process with the given process id to create a new child process.
 */
void create_process(int parent_id) {
    // increment the child count whenever a new child process is created
    c_nr++;
    if(p_id == parent_id) {
        pid_t pid = fork();
        if(pid < 0) PRINT_ERROR_CREATING_PROCESS
        if(pid == 0) {
            // inside child process
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
    // thread 2 has to start after thread 3
    if(th_arg.th_id == 2) {
        P(&sem_start_after_th3);
    }
    // thread 4 has to start after thread 2 from P2
    if(th_arg.th_id == 4) {
        P(sem_start_th4);
    }
    info(BEGIN, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 3) {
        V(&sem_start_after_th3);
    }
    // thread 3 has to leave after thread 2
    if(th_arg.th_id == 3) {
        P(&sem_end_after_th2);
    }
    info(END, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 2) {
        V(&sem_end_after_th2);
    }
    // when thread 4 finishes it must signal thread 3 from P2 to start
    if(th_arg.th_id == 4) {
        V(sem_start_th3);
    }
    return 0;
}

int synchronizing_threads_in_same_process(){
    int error_code = 0;
    pthread_t th[4];
    thread_args_t th_args[4];
    // create unnamed semaphore for controlling when should thread 2 from process 3 start (only after thread 3)
    if (sem_init(&sem_start_after_th3, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // create unnamed semaphore for controlling when should thread 3 from process 3 end (after thread 2)
    if (sem_init(&sem_end_after_th2, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // open named semaphore for controlling when should thread 3 from process 2 start
    sem_start_th3 = sem_open(SEM_START_TH3,O_CREAT,0600,0);
    if(sem_start_th3 == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // open named semaphore for controlling when should thread 4 from process 3 start
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
    // destroy the semaphores
    sem_destroy(&sem_start_after_th3);
    sem_destroy(&sem_end_after_th2);
    return error_code;
}

void * task_of_threads_in_p7(void * arg) {
    thread_args_t th_arg = *(thread_args_t*)arg;
    // threads who attempt to enter before thread 15 are blocked
    if(th_arg.th_id != 15) {
        P(&sem_enter);
    }
    // unblock the next waiting thread
    V(&sem_enter);
    // at most 4 threads can enter at a time
    P(&sem_limit);
    info(BEGIN, th_arg.pr_id, th_arg.th_id);

    if(th_arg.th_id == 15) {
        // allow the next 3 threads to enter the room
        V(&sem_enter);
        // wait until the room is full
        P(&sem_leave);
    }

    if(th_arg.th_id != 15) {
        int value;
        sem_getvalue(&sem_limit, &value);
        // check if all permissions were taken <=> there are already 4 threads inside together with thread 15
        if(value <= 0) {
            // signal thread 15 to leave the room
            V(&sem_leave);
        }
        // remains blocked until thread 15 lifts the barrier
        P(&sem_barrier);
        // unblock the next waiting thread
        V(&sem_barrier);
    }
    info(END, th_arg.pr_id, th_arg.th_id);
    if(th_arg.th_id == 15) {
        // lift the barrier after leaving the room
        V(&sem_barrier);
    }
    V(&sem_limit);
    return 0;
}

int threads_barrier() {
    int error_code = 0;
    pthread_t th[38];
    thread_args_t th_args[38];
    // create semaphore for limiting the number of threads in a room
    if (sem_init(&sem_limit, 1, 4) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // create semaphore for blocking thread 15, not allowing it to leave until there have gathered other 3 threads inside the room
    if (sem_init(&sem_leave, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // create semaphore for blocking the threads entering after thread 15, so that they wouldn't leave before 15
    if (sem_init(&sem_barrier, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // create a semaphore for blocking the threads that try to enter before thread 15, so that when th 15 enters there
    // would always be at least another 3 threads to unblock it
    if (sem_init(&sem_enter, 1, 0) < 0) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // initialize the thread arguments
    for(int i=0;i<38;i++) {
        th_args[i].pr_id = 7;
        th_args[i].th_id = i+1;
    }
    // create the threads
    for(int i=0;i<38;i++) {
        if (pthread_create(&th[i], NULL, task_of_threads_in_p7, &th_args[i]) != 0) {
            error_code = ERROR_CREATING_THREAD;
        }
    }
    // join the created threads
    for(int i=0;i<38;i++) {
        int status = 0;
        if(pthread_join(th[i], (void *)&status) != 0) {
            error_code = ERROR_JOINING_THREAD;
            goto finish;
        }
    }
    finish:
    // destroy the semaphores
    sem_destroy(&sem_limit);
    sem_destroy(&sem_leave);
    sem_destroy(&sem_barrier);
    sem_destroy(&sem_enter);
    return error_code;
}

void * task_of_threads_in_p2(void * arg) {
    thread_args_t th_arg = *(thread_args_t*)arg;
    // thread 3 must start after thread 4 from P3 finishes
    if(th_arg.th_id == 3) {
        P(sem_start_th3);
    }
    info(BEGIN, th_arg.pr_id, th_arg.th_id);

    info(END, th_arg.pr_id, th_arg.th_id);
    // thread 2 has to signal thread 4 from P3 to start
    if(th_arg.th_id == 2) {
        V(sem_start_th4);
    }
    return 0;
}

int synchronizing_threads_in_diff_processes() {
    int error_code = 0;
    pthread_t th[5];
    thread_args_t th_args[5];
    // create a named semaphore which controls when should thread 3 from process 2 begin
    sem_start_th3 = sem_open(SEM_START_TH3,O_CREAT,0600,0);
    if(sem_start_th3 == SEM_FAILED) {
        error_code = ERROR_CREATING_SEMAPHORE;
        goto finish;
    }
    // create a named semaphore which controls when should thread 4 from process 3 begin
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
    // close and unlink the named semaphores
    sem_close(sem_start_th3);
    sem_unlink(SEM_START_TH3);
    sem_close(sem_start_th4);
    sem_unlink(SEM_START_TH4);
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

    if(p_id == 2) {
        int return_code = synchronizing_threads_in_diff_processes();
        if(return_code == ERROR_CREATING_THREAD) {
            PRINT_ERROR_CREATING_THREAD
        }else if(return_code == ERROR_JOINING_THREAD) {
            PRINT_ERROR_JOINING_THREAD
        }else if(return_code == ERROR_CREATING_SEMAPHORE) {
            PRINT_ERROR_CREATING_SEMAPHORE
        }
    }
    if(p_id == 3) {
        int return_code = synchronizing_threads_in_same_process();
        if(return_code == ERROR_CREATING_THREAD) {
            PRINT_ERROR_CREATING_THREAD
        }else if(return_code == ERROR_JOINING_THREAD) {
            PRINT_ERROR_JOINING_THREAD
        }else if(return_code == ERROR_CREATING_SEMAPHORE) {
            PRINT_ERROR_CREATING_SEMAPHORE
        }
    }
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

