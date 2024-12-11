#include<stdio.h>
#include<syslog.h>
#include<string.h>

int main(int argc, char **argv)
{
    openlog(NULL, LOG_USER, LOG_DEBUG);
    if(argc < 3)
    {
        printf("PARAMETER ERROR");
        syslog(LOG_ERR, "PARAMETER ERROR");
        return 1;
    }

    char *path = argv[1];
    char *content = argv[2];

    FILE *file = fopen(path, "w+");
    if(file == NULL)
    {
        printf("FILE OPEN ERROR");
        syslog(LOG_ERR, "FILE OPEN ERROR");
        return 1;
    }

    int len = strlen(content);
    int rc = fwrite(content, sizeof(char), len, file);
    if(rc < len)
    {
        syslog(LOG_ERR, "FILE WRITE ERROR");
        return 1;
    }

    syslog(LOG_DEBUG, content);

    closelog();

    return 0;
}
