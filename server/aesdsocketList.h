#ifndef __AESDSOCKET_LIST_H__
#define __AESDSOCKET_LIST_H__

#include<stdio.h>
#include<pthread.h>
#include"aesdsocketThread.h"

struct linked_list_node {
    pthread_t thread;
    struct linked_list_node *prev;
    struct linked_list_node *next;
    struct thread_data data;
};

struct linked_list_head {
    struct linked_list_node *start;
    struct linked_list_node *end;
};

void add_linked_list(struct linked_list_head *list_head, struct linked_list_node *node) {
    // initialize
    if(list_head->end == NULL) {
        list_head->start = node;
        list_head->end = node;

        node->prev = NULL;
        node->next = NULL;
    }
    else {
        node->prev = list_head->end;

        list_head->end->next = node;
        node->next = NULL;
    }
}

void delete_linked_list(struct linked_list_head *list_head, struct linked_list_node *node) {

    if(node->prev != NULL) {
        node->prev->next = node->next;
    }

    if(node->next != NULL) {
        node->next->prev = node->prev;
    }

    if(list_head->start == node) {
        list_head->start = node->next;
    }

    if(list_head->end == node) {
        list_head->end = node->prev;
    }
}

#endif
