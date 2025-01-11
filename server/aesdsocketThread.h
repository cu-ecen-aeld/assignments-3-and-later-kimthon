#ifndef __AESDSOCKET_THREAD_H__
#define __AESDSOCKET_THREAD_H__

#include <pthread.h>
#include <stdio.h>

struct thread_data
{
    int socket;
    char client_ip[INET6_ADDRSTRLEN];
    int done;
};

struct pthread_mutex_with_initflag
{
    pthread_mutex_t mutex;
    int initialized;
};

// add mutex_initialized
void init_mutex(struct pthread_mutex_with_initflag *mutex)
{
    if(pthread_mutex_init(&(mutex->mutex), NULL) != 0) mutex->initialized = 0;
    else mutex->initialized = 1;
}

// del mutex_
void destroy_mutex(struct pthread_mutex_with_initflag *mutex)
{
    if(mutex->initialized) pthread_mutex_destroy(&(mutex->mutex));
}

#endif
