#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    usleep(thread_func_args->wait_to_obtain_ms*1000);


    int tl = pthread_mutex_lock(thread_func_args->mutex);
    if (tl != 0) {
        printf("Attempt to obtain mutex failed with %d\n", tl);
    } 
    
    usleep(thread_func_args->wait_to_release_ms*1000);

    int tu = pthread_mutex_unlock(thread_func_args->mutex);
    if (tu != 0) {
        printf("Attempt to unlock mutex failed with %d\n", tu);
        thread_func_args->thread_complete_success = false;
    } else {
        thread_func_args->thread_complete_success = true;
    }
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data* thread_func_args = (struct thread_data*) malloc(sizeof(struct thread_data));
    thread_func_args->mutex = mutex;
    thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->wait_to_release_ms = wait_to_release_ms;
    thread_func_args->thread_complete_success = false;
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    /*
    int pmi = pthread_mutex_init(mutex, NULL);
    if (pmi != 0) {
        printf("Failed to initialize account mutex, error was %d\n", pmi);
        return false;
    }
    */
    int pc = pthread_create(thread, NULL, threadfunc, thread_func_args);
    if (pc != 0) {
        printf("Thread can't be created, error was %d\n", pc);
        free(thread_func_args);
        return false;
    } 

    /*
    int pj = pthread_join(*thread, NULL);
    if (pj != 0) {
        printf("Attempt to pthread_join thread failed with %d\n", pj);
        return false;
    } 
    pthread_mutex_destroy(mutex);
    
    */
   return true;
}

