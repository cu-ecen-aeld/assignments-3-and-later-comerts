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
#include <sys/queue.h>

//#include "queue.h"
#include "threading.h"
#include "slist.h"
#include "client.h"

#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\r\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\r\n" , ##__VA_ARGS__)

static struct sockaddr_in cli_addr;
static const char *SOCK_FILE = "/var/tmp/aesdsocketdata";
static const int PORT_NO = 9000;

static FILE *file;
static int sockfd;

static void daemonize(void);
static void sig_handler(int signo);

int main(int argc, char *argv[])
{
    bool isDaemon = false;

    if (argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0)
        {
            isDaemon = true;
        }
        else
        {
            fprintf(stderr, "Usage: %s [-d]\r\n", argv[0]);
            syslog(LOG_ERR, "Usage: %s [-d]", argv[0]);
            closelog();
            return EPERM;
        }
    }

    if (isDaemon)
    {
        daemonize();
    }

    syslog(LOG_INFO, "aesdsocket started");

    socklen_t clilen;
    struct sockaddr_in serv_addr;

	struct sigaction new_action = {0};
	struct sigaction old_action = {0};

	/* Set up the structure to specify the new action. */
	new_action.sa_handler = sig_handler;
	(void)sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	sigaction(SIGINT, NULL, &old_action);
	if (old_action.sa_handler != SIG_IGN)
	{
        sigaction(SIGINT, &new_action, NULL);
	}
	sigaction(SIGTERM, NULL, &old_action);
	if (old_action.sa_handler != SIG_IGN)
	{
        sigaction(SIGTERM, &new_action, NULL);
	}

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    file = fopen(SOCK_FILE, "a+");
    if (file == NULL)
    {
        perror("fopen");
        syslog(LOG_ERR, "fopen: %m");
        closelog();
        return EPERM;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    // Set socket options to reuse the address
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("ERROR setting socket options");
        syslog(LOG_ERR, "ERROR setting socket options: %m");
        close(sockfd);
        fclose(file);
        closelog();
        exit(1);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_NO);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        syslog(LOG_ERR, "ERROR on binding: %m");
        close(sockfd);
        fclose(file);
        closelog();
        exit(1);
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    initialize_list();

    while (1)
    {   
        int *clientsockfd = malloc(sizeof(int));
        *clientsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        
        if (*clientsockfd < 0)
        {
            perror("ERROR on accept");
            syslog(LOG_ERR, "ERROR on accept: %m");
            close(sockfd);
            fclose(file);
            closelog();
            exit(1);
        }

        printf("Accepted connection from %s\r\n", inet_ntoa(cli_addr.sin_addr));
        syslog(LOG_INFO, "Accepted connection from %s\r\n", inet_ntoa(cli_addr.sin_addr));

        printf("Creating thread for %s\r\n", inet_ntoa(cli_addr.sin_addr));
        syslog(LOG_INFO, "Creating thread for %s\r\n", inet_ntoa(cli_addr.sin_addr));
        
        pthread_t *sockThread = malloc(sizeof(pthread_t));
        if (sockThread == NULL)
        {
            perror("malloc");
            syslog(LOG_ERR, "malloc: %m");
            close(*clientsockfd);
            free(clientsockfd);
            continue;
        }
        pthread_mutex_t *sockMutex = malloc(sizeof(pthread_mutex_t));
        if (sockMutex == NULL)
        {
            perror("malloc");
            syslog(LOG_ERR, "malloc: %m");
            close(*clientsockfd);
            free(sockThread);
            continue;
        }

        thread_data_t *threadData = malloc(sizeof(thread_data_t));
        if (threadData == NULL)
        {
            perror("malloc");
            syslog(LOG_ERR, "malloc: %m");
            close(*clientsockfd);
            continue;
        }

        threadData->sockfd = clientsockfd;
        threadData->mutex = sockMutex;
        threadData->filefd = file;
        threadData->thread_complete_success = false;

        if (pthread_mutex_init(sockMutex, NULL) != 0)
        {
            perror("pthread_mutex_init");
            syslog(LOG_ERR, "pthread_mutex_init: %m");
            close(*clientsockfd);
            continue;
        }

        insert_thread_data(sockThread, threadData);

        if (start_thread_obtaining_mutex(sockThread, threadData) == false)
        {
            perror("start_thread_obtaining_mutex");
            syslog(LOG_ERR, "start_thread_obtaining_mutex: %m");
        }

        printf("Thread created for %s\r\n", inet_ntoa(cli_addr.sin_addr));
        syslog(LOG_INFO, "Thread created for %s\r\n", inet_ntoa(cli_addr.sin_addr));
        
        // Wait for the thread to complete
        printf("Waiting for thread to complete\r\n");

        while (1)
        {
            pthread_t *thread = NULL;
            thread_data_t *thread_data = NULL;
            check_list(&thread, &thread_data);
            if (thread != NULL)
            {
                // Shutdown the client socket
                if (shutdown(*thread_data->sockfd, SHUT_RDWR) < 0)
                {
                    perror("ERROR shutting down client socket");
                    syslog(LOG_ERR, "ERROR shutting down client socket: %m");
                }

                close(*thread_data->sockfd);

                if (pthread_join(*thread, NULL) != 0)
                {
                    perror("pthread_join");
                    syslog(LOG_ERR, "pthread_join: %m");
                }
                else
                {
                    if (thread_data->thread_complete_success == true)
                    {
                        printf("Thread completed successfully\r\n");
                        syslog(LOG_INFO, "Thread completed successfully\r\n");
                    }
                    else
                    {
                        printf("Thread failed\r\n");
                        syslog(LOG_ERR, "Thread failed\r\n");
                    }

                    free(thread_data->sockfd);
                    free(thread_data->mutex);
                    free(thread_data);
                    free(thread);
                }
            }
            else
            {
                break;
            }
        }
    }

    fclose(file);
    close(sockfd);
    closelog();
    return 0;
}

static void daemonize(void)
{
#if 0
    pid_t pid;

    // Fork off the parent process
    pid = fork();

    // If we got a good PID, then we can exit the parent process.
    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // On success: The child process becomes session leader
    if (setsid() < 0)
    {
        exit(EXIT_FAILURE);
    }

    // Fork off for the second time
    pid = fork();

    // An error occurred
    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }

    // Success: Let the parent terminate
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Set new file permissions
    umask(0);

    // Change the working directory to the root directory
    // or another appropriated directory
    chdir("/");

    // Close all open file descriptors
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
    {
        close(x);
    }
#else
    if (daemon(0, 0) != 0)
    {
        perror("daemon");
        syslog(LOG_ERR, "daemon: %m");
        exit(1);
    }
#endif
}

static void sig_handler(int signo)
{
    switch (signo)
    {
        case SIGINT:
        case SIGTERM:
            printf("Caught signal %d, exiting\r\n", signo);
            syslog(LOG_INFO, "Caught signal %d, exiting\r\n", signo);
            fclose(file);
            if (remove(SOCK_FILE) != 0)
            {
                perror("remove");
                syslog(LOG_ERR, "remove: %m");
            }

            while (1)
            {
                pthread_t *thread = NULL;
                thread_data_t *thread_data = NULL;
                free_list(&thread, &thread_data);
                if (thread != NULL)
                {
                    if (*thread_data->sockfd > 0)
                    {
                        // Shutdown the client socket
                        if (shutdown(*thread_data->sockfd, SHUT_RDWR) < 0)
                        {
                            perror("ERROR shutting down client socket");
                            syslog(LOG_ERR, "ERROR shutting down client socket: %m");
                        }

                        close(*thread_data->sockfd);
                    }

                    if (pthread_join(*thread, NULL) != 0)
                    {
                        perror("pthread_join");
                        syslog(LOG_ERR, "pthread_join: %m");
                    }
                    else
                    {
                        free(thread_data->sockfd);
                        free(thread_data->mutex);
                        free(thread_data);
                        free(thread);
                    }
                }
                else
                {
                    break;
                }
            }

            close(sockfd);
            closelog();
            exit(0);
            break;
        default:
            break;
    }
}
