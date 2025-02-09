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
#include <time.h>

//#include "queue.h"
#include "threading.h"
#include "slist.h"
#include "client.h"

#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\r\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\r\n" , ##__VA_ARGS__)

#define TIMER_INTERVAL_SEC (10U)

static struct sockaddr_in cli_addr;
static const char *SOCK_FILE = "/var/tmp/aesdsocketdata";
static const int PORT_NO = 9000;

static FILE *file;
static pthread_mutex_t fileMutex;
static int sockfd;
static timer_t timerid;

static void daemonize(void);
static void sig_handler(int signo);
static void timer_handler(FILE *file);

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
    struct sigevent sev = {0};
    struct itimerspec its = {0};

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

    sigaction(SIGUSR1, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN)
    {
        sigaction(SIGUSR1, &new_action, NULL);
    }

    if (pthread_mutex_init(&fileMutex, NULL) != 0)
    {
        perror("pthread_mutex_init");
        syslog(LOG_ERR, "pthread_mutex_init: %m");
        exit(EXIT_FAILURE);
    }

    // Set up timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
    {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    // Start timer
    its.it_value.tv_sec = TIMER_INTERVAL_SEC;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = TIMER_INTERVAL_SEC;
    its.it_interval.tv_nsec = 0;
    if (timer_settime(timerid, 0, &its, NULL) == -1)
    {
        perror("timer_settime");
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    initialize_list();

    while (1)
    {   
        int *clientsockfd = malloc(sizeof(int));
        if (clientsockfd == NULL)
        {
            perror("malloc");
            syslog(LOG_ERR, "malloc: %m");
            continue;
        }

        while ((*clientsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
        {
            if (errno == EINTR)
            {
                continue; // Retry if interrupted by signal
            }
            else
            {
                perror("ERROR on accept");
                syslog(LOG_ERR, "ERROR on accept: %m");
                free(clientsockfd);
                close(sockfd);
                fclose(file);
                closelog();
                exit(EXIT_FAILURE);
            }
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
        
        thread_data_t *threadData = malloc(sizeof(thread_data_t));
        if (threadData == NULL)
        {
            perror("malloc");
            syslog(LOG_ERR, "malloc: %m");
            close(*clientsockfd);
            continue;
        }

        threadData->sockfd = clientsockfd;
        threadData->mutex = &fileMutex;
        threadData->filefd = file;
        threadData->thread_complete_success = false;

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
        exit(EXIT_FAILURE);
    }
#endif
}

static void sig_handler(int signo)
{
    printf("Caught signal %d\r\n", signo);
    syslog(LOG_INFO, "Caught signal %d\r\n", signo);
    switch (signo)
    {
        case SIGINT:
        case SIGTERM:
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
            exit(EXIT_SUCCESS);
            break;

        case SIGUSR1:
            timer_handler(file); 
            break;

        default:
            break;
    }
}

static void timer_handler(FILE *file)
{
    char buf[256];
    time_t t;
    struct tm *tmp;

    printf("Timer expired\r\n");
    syslog(LOG_INFO, "Timer expired\r\n");

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL)
    {
        perror("localtime");
        syslog(LOG_ERR, "localtime: %m");
        return;
    }

    if (strftime(buf, sizeof(buf), "%F %R", tmp) == 0)
    {
        perror("strftime");
        syslog(LOG_ERR, "strftime: %m");
        return;
    }

    pthread_mutex_lock(&fileMutex);

    fprintf(file, "timestamp:%s\r\n", buf);

    pthread_mutex_unlock(&fileMutex);

    fflush(file);
}