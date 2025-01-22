#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    
    thread_data_t* thread_func_args = (thread_data_t*)thread_param;
    pthread_mutex_t *mutex = thread_func_args->mutex;
    int wait_to_obtain_ms = thread_func_args->wait_to_obtain_ms;
    int wait_to_release_ms = thread_func_args->wait_to_release_ms;

    usleep(wait_to_obtain_ms * 1000);
    pthread_mutex_lock(mutex);
    usleep(wait_to_release_ms * 1000);
    pthread_mutex_unlock(mutex);
    thread_func_args->thread_complete_success = true;
    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    bool ret = false;

    thread_data_t* td = (thread_data_t*)malloc(sizeof(thread_data_t));

    td->mutex = mutex;
    td->wait_to_obtain_ms = wait_to_obtain_ms;
    td->wait_to_release_ms = wait_to_release_ms;
    td->thread_complete_success = false;

    if (pthread_create(thread, NULL, (void*)threadfunc, (void*)td) != 0)
    {
        ERROR_LOG("pthread_create failed with error: %d", errno);
        ret = false;
    }
    else
    {
        ret = true;
    }

    return ret;
}

