#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

int handle_client(int *newsockfd, FILE *file);

#endif // CLIENT_H
