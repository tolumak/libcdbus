/*
 * A double-linked list implementation with multi-thread support
 *
 * Copyright 2011-2014 S.I.S.E. S.A.
 * Author: Michel Lafon-Puyo <michel.lafonpuyo@gmail.com>
 *
 * This file is part of libcdbus
 *
 * libcdbus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef LIST_H
#define LIST_H

#include <pthread.h>
#include <stddef.h>
#include "macro.h"

struct list_t;

struct list_item_t {
	struct list_item_t *prev;
	struct list_item_t *next;
	struct list_t *list;
};

struct list_t {
#ifdef LIBUTILS_PTHREAD_LOCK
	pthread_mutex_t lock;
#endif
	struct list_item_t *head;
	struct list_item_t *tail;
	int nb;
};

#ifdef LIBUTILS_PTHREAD_LOCK
#define DECLARE_LIST_INIT(list)					       \
	struct list_t (list) = { .head = NULL,			       \
				 .tail = NULL,			       \
				 .nb = 0,			       \
				 .lock = PTHREAD_MUTEX_INITIALIZER     \
	}
#define LIST_INIT(list) do {				\
		(list).head = (list).tail = NULL;	\
		(list).nb = 0;				\
		pthread_mutex_init(&(list).lock, NULL); \
	} while(0)

#else
#define DECLARE_LIST_INIT(list)					       \
	struct list_t (list) = { .head = NULL,			       \
				 .tail = NULL,			       \
				 .nb = 0			       \
	}
#define LIST_INIT(list) do {				\
		(list).head = (list).tail = NULL;	\
		(list).nb = 0;				\
	} while(0)
#endif

#define LIST_ITEM_INIT(item) do {				\
		(item).prev = (item).next = NULL;		\
		(item).list = NULL;				\
	} while(0)

#define for_each_list_item(list, list_item, member, ptr)		\
  for ((list_item) = list_get_first(list), \
	 (ptr) = (list_item) ? container_of((list_item), typeof(*(ptr)), member) : NULL, \
	 (list_item) = (list_item) ? list_get_next(list_item) : NULL ;	\
       (ptr) ;								\
       (ptr) = (list_item) ? container_of((list_item), typeof(*(ptr)), member) : NULL, \
	 list_item = (list_item) ? list_get_next(list_item) : NULL)

#define list_item_get_list(item) ((item)->list)

int list_get_nb(struct list_t *list);
int list_add_tail(struct list_t *list, struct list_item_t *item);
int list_add_head(struct list_t *list, struct list_item_t *item);
int list_insert_before(struct list_item_t *item, struct list_item_t *next);
int list_insert_after(struct list_item_t *item, struct list_item_t *prev);

struct list_item_t* list_get_first(struct list_t *list);
struct list_item_t* list_get_last(struct list_t *list);
struct list_item_t* list_get_next(struct list_item_t *item);
struct list_item_t* list_get_prev(struct list_item_t *item);

int list_rem_item(struct list_item_t *item);

int list_lock(struct list_t *list);
int list_unlock(struct list_t *list);


/* Following functions must only be called if the list is locked */

#define __for_each_list_item(list, list_item, member, ptr)		\
  for ((list_item) = __list_get_first(list), \
	 (ptr) = (list_item) ? container_of((list_item), typeof(*(ptr)), member) : NULL, \
	 (list_item) = (list_item) ? __list_get_next(list_item) : NULL ;	\
       (ptr) ;								\
       (ptr) = (list_item) ? container_of((list_item), typeof(*(ptr)), member) : NULL, \
	 list_item = (list_item) ? __list_get_next(list_item) : NULL)


int __list_get_nb(struct list_t *list);
int __list_add_tail(struct list_t *list, struct list_item_t *item);
int __list_add_head(struct list_t *list, struct list_item_t *item);

struct list_item_t* __list_get_first(struct list_t *list);
struct list_item_t* __list_get_last(struct list_t *list);
struct list_item_t* __list_get_next(struct list_item_t *item);
struct list_item_t* __list_get_prev(struct list_item_t *item);

int __list_insert_before(struct list_item_t *item, struct list_item_t *next);
int __list_insert_after(struct list_item_t *item, struct list_item_t *prev);

int __list_rem_item(struct list_item_t *item);


#endif
