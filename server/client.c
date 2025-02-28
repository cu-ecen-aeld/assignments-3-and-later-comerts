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

#include "aesd_ioctl.h"
#include "client.h"

#define DEVICE_SIZE (4096)
#define SEEK_COMMAND ("AESDCHAR_IOCSEEKTO:")
#define FSIZE (4096)


#if USE_AESD_CHAR_DEVICE == 0
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
#else //USE_AESD_CHAR_DEVICE

int echo_buffer(int *devfd, int *newsockfd)
{
    int ret = 0;
    unsigned int rpos = 0;
    int rsize;
    char *fileBuffer = (char*)malloc(FSIZE);

    if (fileBuffer == NULL)
    {
        perror("malloc");
        ret = 1;
    }

    do
    {
        if ((rsize = read(*devfd, &fileBuffer[rpos], FSIZE)) < 0)
        {
            perror("read");
            ret = 1;
        }
        else if (rsize == 0)
        {
            fileBuffer[rpos] = '\0';
            break;
        }
        else
        {
            rpos += rsize;
        }
    } while (rpos < FSIZE);

    if (write(*newsockfd, fileBuffer, strlen(fileBuffer)) < 0)
    {
        perror("ERROR writing to socket");
        ret = 1;
    }

    free(fileBuffer);

    return ret;
}

int handle_client(int *newsockfd, int *devfd)
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
                        goto out;
                    }
                    buffer = temp;
                }
            }
        }
    }

    printf("Here is the message: %s\r\n", buffer);
    syslog(LOG_INFO, "Received: %s", buffer);

    /* check if AESDCHAR_IOCSEEKTO: */
    char *seek = strstr(buffer, SEEK_COMMAND);
    if (seek != NULL)
    {
        syslog(LOG_INFO, "Seeking to offset");
        struct aesd_seekto seekto;
        seek += strlen(SEEK_COMMAND);
        char tmp_str[256];
        strncpy(tmp_str, seek, strstr(seek, ",") - seek);
        seekto.write_cmd = atoi(tmp_str);
        seek = strstr(seek, ",") + 1;
        strncpy(tmp_str, seek, strstr(seek, "\n") - seek);
        seekto.write_cmd_offset = atoi(tmp_str);
        syslog(LOG_INFO, "Seeking to write_cmd: %d, write_cmd_offset: %d", seekto.write_cmd, seekto.write_cmd_offset);
        if (ioctl(*devfd, AESDCHAR_IOCSEEKTO, &seekto) < 0)
        {
            perror("ioctl");
            ret = 1;
            goto out;
        }

        ret = echo_buffer(devfd, newsockfd);
    }
    else
    {
        syslog(LOG_INFO, "Writing to device");
        if (write(*devfd, buffer, strlen(buffer)) < 0)
        {
            perror("write");
            ret = 1;
            goto out;
        }

        ret = echo_buffer(devfd, newsockfd);
    }

out:
    free(buffer);

    return ret;
}
#endif //USE_AESD_CHAR_DEVICE
