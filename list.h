/*
 * D-Bus C Bindings library
 *
 * Copyright 2011 S.I.S.E. S.A.
 * Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
 *
 */

#ifndef __LIBCDBUS_LIST_H
#define __LIBCDBUS_LIST_H

#include <pthread.h>

struct list_t;

struct list_item_t {
	struct list_item_t *prev;
	struct list_item_t *next;
	struct list_t *list;
};

struct list_t {
	pthread_mutex_t lock;
	struct list_item_t *head;
	struct list_item_t *tail;
	int nb;
};

#define DECLARE_LIST_INIT(list)					       \
	struct list_t (list) = { .head = NULL,			       \
				 .tail = NULL,			       \
				 .nb = 0,			       \
				 .lock = PTHREAD_MUTEX_INITIALIZER }

#define LIST_INIT(list) do {				\
		(list).head = (list).tail = NULL;	\
		(list).nb = 0;				\
		pthread_mutex_init(&(list).lock, NULL); \
	} while(0)

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

int list_get_nb(struct list_t *list);
int list_add_tail(struct list_t *list, struct list_item_t *item);
int list_add_head(struct list_t *list, struct list_item_t *item);

struct list_item_t* list_get_first(struct list_t *list);
struct list_item_t* list_get_last(struct list_t *list);
struct list_item_t* list_get_next(struct list_item_t *item);
struct list_item_t* list_get_prev(struct list_item_t *item);

int list_rem_item(struct list_item_t *item);

int list_lock(struct list_t *list);
int list_unlock(struct list_t *list);

/* Following functions must only be called if the list is locked */

int __list_get_nb(struct list_t *list);
int __list_add_tail(struct list_t *list, struct list_item_t *item);
int __list_add_head(struct list_t *list, struct list_item_t *item);

struct list_item_t* __list_get_first(struct list_t *list);
struct list_item_t* __list_get_last(struct list_t *list);
struct list_item_t* __list_get_next(struct list_item_t *item);
struct list_item_t* __list_get_prev(struct list_item_t *item);

int __list_insert_before(struct list_item_t *item, struct list_item_t *next);

int __list_rem_item(struct list_item_t *item);


#endif
