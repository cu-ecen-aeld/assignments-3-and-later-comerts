 #include <stdio.h>
 #include <stdlib.h>
 #include <sys/queue.h>
 #include "threading.h"
 #include "slist.h"
  
 SLIST_HEAD(slisthead, slist_data_s) head;

 // SLIST.
 void initialize_list(void)
 {
     SLIST_INIT(&head);
 }
 
 void insert_thread_data(pthread_t *thread, thread_data_t *thread_data)
 {
    // Allocate memory for a new list entry
    slist_data_t *new_entry = malloc(sizeof(slist_data_t));
    if (new_entry == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Initialize the new entry
    new_entry->thread = thread;
    new_entry->thread_data = thread_data;

    // Insert the new entry at the head of the list
    SLIST_INSERT_HEAD(&head, new_entry, entries);
}

void check_list(pthread_t **thread, thread_data_t **thread_data)
{
    slist_data_t *entry;
    *thread = NULL;
    *thread_data = NULL;

    SLIST_FOREACH(entry, &head, entries)
    {
        if (entry->thread_data->thread_complete_success == true)
        {
            *thread = entry->thread;
            *thread_data = entry->thread_data;
            SLIST_REMOVE(&head, entry, slist_data_s, entries);
            free(entry);
            return;
        }
    }
}

void free_list(pthread_t **thread, thread_data_t **thread_data)
{
    slist_data_t *entry;
    *thread = NULL;
    *thread_data = NULL;

    if (!SLIST_EMPTY(&head))
    {
        SLIST_FOREACH(entry, &head, entries)
        {
            *thread = entry->thread;
            *thread_data = entry->thread_data;
            SLIST_REMOVE(&head, entry, slist_data_s, entries);
            free(entry);
            return;
        }
    }
}
