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

#include "list.h"
#include "config.h"

int __list_get_nb(struct list_t *list)
{
	return list->nb;
}
int __list_add_tail(struct list_t *list, struct list_item_t *item)
{
	if (!list || !item)
		return -1;
	if (item->list)
		return -1;

	item->list = list;
	item->prev = list->tail;
	item->next = NULL;
	if (list->tail) {
		list->tail->next = item;
	}
	list->tail = item;
	if (!list->head)
		list->head = item;
	list->nb++;
	return 0;
}

int __list_add_head(struct list_t *list, struct list_item_t *item)
{
	if (!list || !item)
		return -1;
	if (item->list)
		return -1;

	item->list = list;
	item->prev = NULL;
	item->next = list->head;
	if (list->head) {
		list->head->prev = item;
	}
	list->head = item;
	if (!list->tail)
		list->tail = item;
	list->nb++;
	return 0;
}

struct list_item_t* __list_get_first(struct list_t *list)
{
	if (!list)
		return NULL;

	return list->head;
}

struct list_item_t* __list_get_last(struct list_t *list)
{
	if (!list)
		return NULL;

	return list->tail;
}

struct list_item_t* __list_get_next(struct list_item_t *item)
{
	if (!item)
		return NULL;

	if (!item->list)
		return NULL;

	return item->next;
}

struct list_item_t* __list_get_prev(struct list_item_t *item)
{
	if (!item)
		return NULL;

	if (!item->list)
		return NULL;

	return item->prev;
}

int __list_insert_before(struct list_item_t *item, struct list_item_t *next)
{
	if (!item || !next)
		return -1;
	if (!next->list)
		return -1;
	if (item->list)
		return -1;

	item->next = next;
	item->prev = next->prev;
	next->prev = item;
	item->list = next->list;
	if (item->prev == NULL)
		next->list->head = item;
	else
		item->prev->next = item;
	next->list->nb++;

	return 0;
}

int __list_insert_after(struct list_item_t *item, struct list_item_t *prev)
{
	if (!item || !prev)
		return -1;
	if (!prev->list)
		return -1;
	if (item->list)
		return -1;

	item->prev = prev;
	item->next = prev->next;
	prev->next = item;
	item->list = prev->list;
	if (item->next == NULL)
		prev->list->tail = item;
	else
		item->next->prev = item;
	prev->list->nb++;

	return 0;
}

int __list_rem_item(struct list_item_t *item)
{
	if (!item)
		return -1;

	if (!item->list)
		return -1;

	if (item->list->head == item)
		item->list->head = item->next;
	if (item->list->tail == item)
		item->list->tail = item->prev;
	if (item->prev)
		item->prev->next = item->next;
	if (item->next)
		item->next->prev = item->prev;
	item->list->nb--;
	item->list = NULL;

	return 0;
}

int list_lock(struct list_t *list)
{
	if (!list)
		return -1;

#ifdef LIBUTILS_PTHREAD_LOCK
	pthread_mutex_lock(&list->lock);
#endif
#ifdef LIBUTILS_IRQ_LOCK
	irq_disable();
#endif
	return 0;
}

int list_unlock(struct list_t *list)
{
	if (!list)
		return -1;

#ifdef LIBUTILS_PTHREAD_LOCK
	pthread_mutex_unlock(&list->lock);
#endif
#ifdef LIBUTILS_IRQ_LOCK
	irq_enable();
#endif
	return 0;
}


int list_get_nb(struct list_t *list)
{
	int ret;
	if (list_lock(list) < 0)
		return -1;

	ret = __list_get_nb(list);
	list_unlock(list);
	return ret;
}

int list_add_tail(struct list_t *list, struct list_item_t *item)
{
	int ret;
	if (list_lock(list) < 0)
		return -1;

	ret = __list_add_tail(list, item);
	list_unlock(list);
	return ret;
}

int list_add_head(struct list_t *list, struct list_item_t *item)
{
	int ret;
	if (list_lock(list) < 0)
		return -1;

	ret = __list_add_head(list, item);
	list_unlock(list);
	return ret;
}

int list_insert_before(struct list_item_t *item, struct list_item_t *next)
{
	int ret;
	if (!next || !next->list)
		return -1;

	if (list_lock(next->list) < 0)
		return -1;

	ret = __list_insert_before(item, next);
	list_unlock(next->list);
	return ret;
}

int list_insert_after(struct list_item_t *item, struct list_item_t *prev)
{
	int ret;
	if (!prev || !prev->list)
		return -1;

	if (list_lock(prev->list) < 0)
		return -1;

	ret = __list_insert_before(item, prev);
	list_unlock(prev->list);
	return ret;
}

struct list_item_t* list_get_first(struct list_t *list)
{
	struct list_item_t *ret;
	if (list_lock(list) < 0)
		return NULL;

	ret = __list_get_first(list);
	list_unlock(list);
	return ret;
}

struct list_item_t* list_get_last(struct list_t *list)
{
	struct list_item_t *ret;
	if (list_lock(list) < 0)
		return NULL;

	ret = __list_get_last(list);
	list_unlock(list);
	return ret;
}

struct list_item_t* list_get_next(struct list_item_t *item)
{
	struct list_item_t *ret;
	if (list_lock(item->list) < 0)
		return NULL;

	ret = __list_get_next(item);
	list_unlock(item->list);
	return ret;
}

struct list_item_t* list_get_prev(struct list_item_t *item)
{
	struct list_item_t *ret;
	if (list_lock(item->list) < 0)
		return NULL;

	ret = __list_get_prev(item);
	list_unlock(item->list);
	return ret;
}

int list_rem_item(struct list_item_t *item)
{
	int ret;
	struct list_t * list = item->list;
	if (list_lock(list) < 0)
		return -1;

	ret = __list_rem_item(item);
	list_unlock(list);
	return ret;
}
