#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>

#include "client.h"
#include "threading.h"


// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    thread_data_t* thread_func_args = (thread_data_t*)thread_param;
    pthread_mutex_t *mutex = thread_func_args->mutex;

    pthread_mutex_lock(mutex);

#if USE_AESD_CHAR_DEVICE == 0
    if (0 != handle_client(thread_func_args->sockfd, thread_func_args->filefd))
#else //USE_AESD_CHAR_DEVICE
    if (0 != handle_client(thread_func_args->sockfd, &thread_func_args->devfd))
#endif //USE_AESD_CHAR_DEVICE
    {
        syslog(LOG_ERR, "handle_client: %m");
    }

    pthread_mutex_unlock(mutex);
    thread_func_args->thread_complete_success = true;
    
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, thread_data_t *td)
{
    bool ret = false;

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

