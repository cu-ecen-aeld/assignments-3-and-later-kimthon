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
#include <sys/time.h>
#include <time.h>
#include "aesdsocketThread.h"
#include "aesdsocketList.h"

#define PORT "9000"
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_fd;
int running = 1;
FILE *file = NULL;
struct pthread_mutex_with_initflag thread_recv_send_mutex = {0};

// thread manage list
struct linked_list_head list_head = {0};

void cleanup();
void cleanup_completed_threads();
void signal_handler(int signo);
void timer_handler(int signum);
char* get_ip_str(struct sockaddr_storage* addr, char* ip_str, size_t max_len);

// thread function
void *pthread_recv_send(void* param);

int main(int argc, char *argv[]) {

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

    // Timer Setting
    struct sigaction timer_sa;
    memset(&timer_sa, 0x00, sizeof(struct sigaction));
    timer_sa.sa_handler=timer_handler;

    if(sigaction(SIGALRM, &timer_sa, NULL) != 0) {
        syslog(LOG_ERR, "SIGALRM failed: %s", strerror(errno));
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
        if(chdir("/") != 0)
        {
            syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
            close(server_fd);
            return -1;
        }

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

    init_mutex(&thread_recv_send_mutex);
    if(!(thread_recv_send_mutex.initialized))
    {
        syslog(LOG_ERR, "mutex init error: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    // Timer Setting
    struct itimerval timer;
    timer.it_interval.tv_sec = 10;
    timer.it_interval.tv_usec = 0;
    timer.it_value = timer.it_interval;

    if(setitimer(ITIMER_REAL, &timer, NULL) != 0)
    {
        syslog(LOG_ERR, "setitimer failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_storage);
    while (running) {
        int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_socket < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }


        struct linked_list_node *node = (struct linked_list_node*)malloc(sizeof(struct linked_list_node));
        if(node == NULL) {
            syslog(LOG_ERR, "Node allocation failed: %s", strerror(errno));
            close(new_socket);
            continue;
        }

        struct thread_data* data = &(node->data);

        data->socket = new_socket;

        if(get_ip_str(&client_addr, data->client_ip, sizeof(data->client_ip)) == NULL) {
            syslog(LOG_ERR, "IP Client ERROR : %s", strerror(errno));
            continue;
        }

        data->done = 0;

        int create_result = pthread_create(&(node->thread), NULL, pthread_recv_send, (void*)data);
        if(create_result != 0) {
            syslog(LOG_ERR, "Thread creation failed: %s", strerror(create_result));
            free(node);
            close(new_socket);
            continue;
        }

        add_linked_list(&list_head, node);

        // done thread clear
        cleanup_completed_threads();
    }

    cleanup();
    return 0;
}

// memory clear
void cleanup() {
    if (file) fclose(file);
    if (server_fd != -1) close(server_fd);
    remove(FILE_PATH);
    closelog();

    struct linked_list_node *node = list_head.start;
    while(node != NULL) {
        pthread_join(node->thread, NULL);
        node = node->next;
    }
    cleanup_completed_threads();

    destroy_mutex(&thread_recv_send_mutex);
}

void cleanup_completed_threads() {
    struct linked_list_node *node = list_head.start;
    struct linked_list_node *next;
    while (node != NULL) {
        next = node->next;
        if (node->data.done) {
            pthread_join(node->thread, NULL);
            delete_linked_list(&list_head, node);
            free(node);
        }
        node = next;
    }
}

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        running = 0;
        cleanup();
        exit(0);
    }
}

void timer_handler(int signum) {
    time_t now;
    struct tm *tm_info;
    char timestamp[100];

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", tm_info);

    pthread_mutex_lock(&(thread_recv_send_mutex.mutex));
    fwrite(timestamp, sizeof(char), strlen(timestamp), file);
    fflush(file);
    pthread_mutex_unlock(&(thread_recv_send_mutex.mutex));
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

void *pthread_recv_send(void* param) {
    struct thread_data *data = (struct thread_data*) param;

    int socket = data->socket;;
    char* client_ip = data->client_ip;

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char buffer[BUFFER_SIZE] = {0};


    int bytes_received;
    while ((bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';

        pthread_mutex_lock(&(thread_recv_send_mutex.mutex));
        fwrite(buffer, sizeof(char), bytes_received, file);
        pthread_mutex_unlock(&(thread_recv_send_mutex.mutex));

        if (strchr(buffer, '\n') != NULL) {
            fflush(file);
            break;
        }
    }

    if (bytes_received < 0) {
        syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
    }

    pthread_mutex_lock(&(thread_recv_send_mutex.mutex));
    fseek(file, 0, SEEK_SET);
    size_t bytes_read;
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE - 1, file)) > 0) {
        pthread_mutex_unlock(&(thread_recv_send_mutex.mutex));
        buffer[bytes_read] = '\0';
        ssize_t bytes_sent = send(socket, buffer, bytes_read, 0);

        if (bytes_sent < 0) {
            syslog(LOG_ERR, "Send failed: %s", strerror(errno));
            break;
        }
        pthread_mutex_lock(&(thread_recv_send_mutex.mutex));
    }
    pthread_mutex_unlock(&(thread_recv_send_mutex.mutex));

    data->done = 1;

    close(socket);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);

    return NULL;
}
