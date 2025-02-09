#ifndef SLIST_H
#define SLIST_H

#include <pthread.h>
#include <sys/queue.h>
#include "threading.h"

struct slist_data_s
{
    pthread_t *thread;
    thread_data_t *thread_data;
    SLIST_ENTRY(slist_data_s) entries;
};
typedef struct slist_data_s slist_data_t;

void initialize_list(void);
void insert_thread_data(pthread_t *thread, thread_data_t *thread_data);
void check_list(pthread_t **thread, thread_data_t **thread_data);
void free_list(pthread_t **thread, thread_data_t **thread_data);

#endif // SLIST_H
