#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

#define USE_AESD_CHAR_DEVICE    (1) // Set to 1 to use AESD char device, 0 to use file

#if USE_AESD_CHAR_DEVICE == 0
int handle_client(int *newsockfd, FILE *file);
#else //USE_AESD_CHAR_DEVICE
int handle_client(int *newsockfd, int *devfd, int *fpos);
#endif //USE_AESD_CHAR_DEVICE

#endif // CLIENT_H
