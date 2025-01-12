#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[])
{
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <filename> %s <string>\r\n", argv[0], argv[1]);
        syslog(LOG_ERR, "Usage: %s <filename> %s <string>", argv[0], argv[1]);
        closelog();
        return EPERM;
    }

    FILE *file = fopen(argv[1], "w");
    if (file == NULL)
    {
        perror("fopen");
        syslog(LOG_ERR, "fopen: %m");
        closelog();
        return EPERM;
    }

//    if (-1 == fwrite(file, argv[2], strlen(argv[2])))
    if (-1 == (int)fwrite(argv[2], 1, strlen(argv[2]), file))
    {
        perror("write");
        syslog(LOG_ERR, "write: %m");
        fclose(file);
        closelog();
        return EPERM;
    }
    else
    {
        syslog(LOG_INFO, "Writing %s to %s", argv[2], argv[1]);
    }

    fclose(file);
    closelog();
    
    return 0;
}