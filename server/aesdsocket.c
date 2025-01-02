#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_fd;
int running = 1;
FILE *file = NULL;

void cleanup() {
    if (file) fclose(file);
    if (server_fd != -1) close(server_fd);
    remove(FILE_PATH);
    closelog();
}

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        running = 0;
        cleanup();
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 3) < 0) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        if (fork() > 0) exit(0);

        setsid();

        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);

        umask(0);
        chdir("/");

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    file = fopen(FILE_PATH, "w+");
    if (file == NULL) {
        syslog(LOG_ERR, "File open error: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    while (running) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        char *client_ip = inet_ntoa(address.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        int bytes_received;
        while ((bytes_received = recv(new_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            fwrite(buffer, sizeof(char), bytes_received, file);
            if (strchr(buffer, '\n') != NULL) {
                fflush(file);
                break;
            }
        }

        if (bytes_received < 0) {
            syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
        }

        fseek(file, 0, SEEK_SET);
        size_t bytes_read;
        while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE - 1, file)) > 0) {
            buffer[bytes_read] = '\0';
            ssize_t bytes_sent = send(new_socket, buffer, bytes_read, 0);
            if (bytes_sent < 0) {
                syslog(LOG_ERR, "Send failed: %s", strerror(errno));
                break;
            }
        }

        close(new_socket);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    cleanup();
    return 0;
}
