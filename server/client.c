#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "client.h"

int handle_client(int *newsockfd, FILE *file)
{
    int ret = 0;
    const int BUFSIZE = 256;
    char *buffer = (char *)malloc(BUFSIZE);
    bzero(buffer, BUFSIZE);
    int n = BUFSIZE;
    int size = BUFSIZE;
    int xbuf = 0;
    while (1)
    {
        n = read(*newsockfd, &buffer[xbuf], BUFSIZE - 1);
        if (n < 0)
        {
            perror("ERROR reading from socket");
            ret = 1;
            break;
        }
        else
        {
            if (n == 0)
            {
                break;
            }
            else
            {
                buffer[xbuf + n] = '\0';
                if (buffer[xbuf + n - 1] == '\n')
                {
                    break;
                }
                else
                {
                    //printf("n: %d\r\n", n);
                    //printf("Received: %s\r\n", &buffer[xbuf]);
                    xbuf += n;
                    size += n;
                    //printf("buffer: %s\r\n", buffer);
                    //printf("Size: %d\r\n", size);
                    char *temp = realloc(buffer, size);
                    if (temp == NULL)
                    {
                        perror("realloc");
                        ret = 1;
                        break;
                    }
                    buffer = temp;
                }
            }
        }
    }

    printf("Here is the message: %s\r\n", buffer);
    if (-1 == (int)fwrite(buffer, 1, strlen(buffer), file))
    {
        perror("write");
        ret = 1;
    }
    fflush(file);
    
    int fileSize = ftell(file);
    if (fileSize < 0)
    {
        perror("ftell");
        ret = 1;
    }
    printf("File size: %d\r\n", fileSize);

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        ret = 1;
    }

    char *fileBuffer = (char *)malloc(fileSize);
    if (fileBuffer == NULL)
    {
        perror("malloc");
        ret = 1;
    }

    if ((int)fread(fileBuffer, 1, fileSize, file) != fileSize)
    {
        perror("fread");
        ret = 1;
    }

    if (write(*newsockfd, fileBuffer, fileSize) < 0)
    {
        perror("ERROR writing to socket");
        ret = 1;
    }
    
    free(buffer);
    free(fileBuffer);    

    return ret;
}
