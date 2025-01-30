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


static struct sockaddr_in cli_addr;
static const char *SOCK_FILE = "/var/tmp/aesdsocketdata";
static const int PORT_NO = 9000;

static FILE *file;
static int sockfd;
static int clientsockfd;

static void daemonize(void);
static void sig_handler(int signo);
static int handle_client(int newsockfd, FILE *file);

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

    while (1)
    {
        clientsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (clientsockfd < 0)
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

        if (0 != handle_client(clientsockfd, file))
        {
            syslog(LOG_ERR, "handle_client: %m");
        }

        // Shutdown the client socket
        if (shutdown(clientsockfd, SHUT_RDWR) < 0)
        {
            perror("ERROR shutting down client socket");
            syslog(LOG_ERR, "ERROR shutting down client socket: %m");}

        close(clientsockfd);
        clientsockfd = 0;

        printf("Closed connection from %s\r\n", inet_ntoa(cli_addr.sin_addr));
        syslog(LOG_INFO, "Closed connection from %s\r\n", inet_ntoa(cli_addr.sin_addr));
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

static int handle_client(int newsockfd, FILE *file)
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
        n = read(newsockfd, &buffer[xbuf], BUFSIZE - 1);
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

    if (write(newsockfd, fileBuffer, fileSize) < 0)
    {
        perror("ERROR writing to socket");
        ret = 1;
    }
    
    free(buffer);
    free(fileBuffer);    

    return ret;
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

            if (clientsockfd > 0)
            {
                // Shutdown the client socket
                if (shutdown(clientsockfd, SHUT_RDWR) < 0)
                {
                    perror("ERROR shutting down client socket");
                    syslog(LOG_ERR, "ERROR shutting down client socket: %m");
                }
                close(clientsockfd);
            }
            close(sockfd);
            closelog();
            exit(0);
            break;
        default:
            break;
    }
}
