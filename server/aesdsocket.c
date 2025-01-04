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
#include <netdb.h>

#define PORT "9000"
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

char* get_ip_str(struct sockaddr_storage* addr, char* ip_str, size_t max_len) {
    void* src_addr;
    const char* result;

    switch (addr->ss_family) {
        case AF_INET:
            src_addr = &(((struct sockaddr_in*)addr)->sin_addr);
            break;
        case AF_INET6:
            src_addr = &(((struct sockaddr_in6*)addr)->sin6_addr);
            break;
        default:
            return NULL;
    }

    result = inet_ntop(addr->ss_family, src_addr, ip_str, max_len);
    return result ? ip_str : NULL;
}

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE] = {0};

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);


    // SIGNAL Setting
    struct sigaction sa;
    memset(&sa, 0x00, sizeof(struct sigaction));
    sa.sa_handler=signal_handler;

    if(sigaction(SIGINT, &sa, NULL) != 0) {
        syslog(LOG_ERR, "SIGINIT failed: %s", strerror(errno));
        return -1;
    }

    if(sigaction(SIGTERM, &sa, NULL) != 0) {
        syslog(LOG_ERR, "SIGTERM failed: %s", strerror(errno));
        return -1;
    }


    // IP ADDRESS SETTING
    struct addrinfo hints, *res;

    memset(&hints, 0x00, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed: %s", strerror(errno));
        return -1;
    }

    // SOCKET SETTING
    if ((server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }


    if (bind(server_fd, res->ai_addr, res->ai_addrlen) < 0) {
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

    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_storage);
    char client_ip[INET6_ADDRSTRLEN];
    while (running) {
        int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_socket < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        char *ip_str = get_ip_str(&client_addr, client_ip, sizeof(client_ip));
        if(ip_str == NULL)
        {
            syslog(LOG_ERR, "IP Client ERROR : %s", strerror(errno));
            continue;
        }

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
