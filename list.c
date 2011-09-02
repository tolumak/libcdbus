/*
 * D-Bus C Bindings library
 *
 * Copyright 2011 S.I.S.E. S.A.
 * Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
 *
 */

#include "list.h"

int __list_get_nb(struct list_t *list)
{
	return list->nb;
}
int __list_add_tail(struct list_t *list, struct list_item_t *item)
{
	if (!list || !item)
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
	if (!next)
		return -1;

	item->next = next;
	item->prev = next->prev;
	next->prev = item;
	item->list = next->list;
	if (item->prev == NULL)
		next->list->head = item;
	next->list->nb++;

	return 0;
}

int __list_rem_item(struct list_item_t *item)
{
	if (!item)
		return -1;

	if (!item->list)
		return -1;

	if (item->list->head == item)
		item->list->head = NULL;
	if (item->list->tail == item)
		item->list->tail = NULL;
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

	pthread_mutex_lock(&list->lock);
	return 0;
}

int list_unlock(struct list_t *list)
{
	if (!list)
		return -1;

	pthread_mutex_unlock(&list->lock);
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
	if (list_lock(item->list) < 0)
		return -1;

	ret = __list_rem_item(item);
	list_unlock(item->list);
	return ret;
}
