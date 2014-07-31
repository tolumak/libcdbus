/*
 * D-Bus C Bindings library
 *
 * Copyright 2011 S.I.S.E. S.A.
 * Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "list.h"
#include "libcdbus.h"
#include "log.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define EXTSTR_BUFF_SIZE 16
#define EXTSTR_BUFFER(s) ((s)->buffer + (s)->size)
#define EXTSTR_REM_SIZE(s) ((s)->buf_size - (s)->size)

struct extensible_string_t {
	int size;
	char * buffer;
	int buf_size;
};

struct watch_t {
	struct list_item_t litem;
	DBusWatch *dbwatch;
	DBusConnection *cnx;
	struct pollfd *pollfd;
};

struct timeout_t;
typedef void (*timeout_cb)(struct timeout_t *timeout, void *data);

struct timeout_t {
	struct list_item_t ordered;
	DBusTimeout *dbtimeout;
	DBusConnection *cnx;
	int interval;
	int value;
	int oneshot;
	timeout_cb cb;
	void * cb_data;
};


static DECLARE_LIST_INIT(watch_list);
static DECLARE_LIST_INIT(timeout_ordered_list);


dbus_bool_t add_watch(DBusWatch *dbwatch, void *data)
{
	struct watch_t *watch;
	DBusConnection *cnx = (DBusConnection *)data;

	LOG(LOG_DEBUG, "add watch\n");

	watch = malloc(sizeof(*watch));
	if (!watch)
		return FALSE;
	memset(watch, 0, sizeof(*watch));

	watch->dbwatch = dbwatch;
	watch->cnx = cnx;
	if (list_add_tail(&watch_list, &watch->litem) < 0)
		goto err_free;

	return TRUE;

err_free:
	free(watch);
	return FALSE;
}

void rem_watch(DBusWatch *dbwatch, void *data)
{
	struct watch_t *watch;
	struct list_item_t *item;

	LOG(LOG_DEBUG, "rem watch\n");

	list_lock(&watch_list);
	item = __list_get_first(&watch_list);
	while(item) {
		watch = container_of(item, struct watch_t, litem);
		if (watch->dbwatch == dbwatch)
			break;

		item = __list_get_next(item);
	}
	list_unlock(&watch_list);

	if (!item)
		return;

	list_rem_item(item);
	free(watch);
}

int timeout_enable(struct timeout_t *timeout)
{
	struct list_item_t *item;
	struct timeout_t *ordered_timeout;

	timeout->value = timeout->interval;

	list_lock(&timeout_ordered_list);
	item = __list_get_first(&timeout_ordered_list);
	while (item) {
		ordered_timeout = container_of(item, struct timeout_t, ordered);
		if (ordered_timeout->value > timeout->value)
			break;
		item = __list_get_next(item);
	}
	if (item)
		__list_insert_before(&timeout->ordered, item);
	else
		__list_add_tail(&timeout_ordered_list, &timeout->ordered);

	list_unlock(&timeout_ordered_list);

	return 0;
}

int timeout_disable(struct timeout_t *timeout)
{
	list_rem_item(&timeout->ordered);
	return 0;
}

void free_timeout(void *data)
{
	struct timeout_t *timeout = data;

	LOG(LOG_DEBUG, "free timeout\n");
	timeout_disable(timeout);

	free(timeout);
}

void timeout_toggled(DBusTimeout *dbtimeout, void *data)
{
	struct timeout_t *timeout;

	LOG(LOG_DEBUG, "timeout toggled\n");

	if (!dbtimeout)
		return;

	timeout = dbus_timeout_get_data(dbtimeout);
	if (!timeout) {
		/* FIXME: error handling, warn dbus */
		return;
	}

	timeout->interval = dbus_timeout_get_interval(dbtimeout);

	/* We disable the timeout, because this function could be called
	   without really toggling the timer but simply to change the interval
	   value. In our implementation, a timeout could be added twice in the
	   list */
	timeout_disable(timeout);

	if (dbus_timeout_get_enabled(dbtimeout) == TRUE) {
		timeout_enable(timeout);
	}
}

dbus_bool_t add_timeout(DBusTimeout *dbtimeout, void *data)
{
	struct timeout_t *timeout;
	DBusConnection *cnx = (DBusConnection *)data;

	LOG(LOG_DEBUG, "add timeout\n");
	timeout = malloc(sizeof(*timeout));
	if (!timeout)
		return FALSE;
	memset(timeout, 0, sizeof(*timeout));

	timeout->dbtimeout = dbtimeout;
	timeout->cnx = cnx;

	dbus_timeout_set_data(dbtimeout, timeout, free_timeout);

	timeout_toggled(timeout->dbtimeout, data);

	return TRUE;
}

void rem_timeout(DBusTimeout *dbtimeout, void *data)
{
	struct timeout_t *timeout;

	LOG(LOG_DEBUG, "rem timeout\n");
	if (!dbtimeout)
		return;

	timeout = dbus_timeout_get_data(dbtimeout);
	if (timeout) {
		timeout_disable(timeout);
		free_timeout(timeout);
	}
}

void dispatch(struct timeout_t *timeout, void* data)
{
	while (dbus_connection_dispatch(timeout->cnx) ==
		DBUS_DISPATCH_DATA_REMAINS) {
		LOG(LOG_DEBUG, "connection dispatch\n");
	}
}

struct timeout_t* new_dispatch_timeout(DBusConnection *cnx)
{
	struct timeout_t *timeout;

	LOG(LOG_DEBUG, "new dispatch timeout\n");
	timeout = malloc(sizeof(*timeout));
	if (!timeout)
		return NULL;

	memset(timeout, 0, sizeof(*timeout));

	timeout->cnx = cnx;
	timeout->value = timeout->interval = 0;
	timeout->cb = dispatch;
	timeout->cb_data = NULL;
	timeout->oneshot = 1;

	return timeout;
}

void dispatch_status(DBusConnection *cnx, DBusDispatchStatus new_status,
		void *data)
{
	struct timeout_t *timeout;

	if (!dbus_connection_get_is_connected(cnx))
		return;

	timeout = new_dispatch_timeout(cnx);
	if (timeout)
		timeout_enable(timeout);
}

DBusConnection* cdbus_get_connection(DBusBusType bus_type, char *name,
				int replace)
{
	DBusError error;
	DBusConnection *cnx;
	int flags = 0;
	int result;
	struct timeout_t *timeout;

	dbus_error_init(&error);

	/* we call timeout handle function as soon as possible, to
	   get the reference time */
	cdbus_timeout_handle();

	/* get a connection to the bus */
	cnx = dbus_bus_get(bus_type, &error);
	if (!cnx || (dbus_error_is_set(&error) == TRUE)) {
		goto err;
	}

	flags |= DBUS_NAME_FLAG_ALLOW_REPLACEMENT;
	flags |= DBUS_NAME_FLAG_DO_NOT_QUEUE;
	if (replace)
		flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;


	/* request the connection name */
	result = dbus_bus_request_name(cnx, name, flags, &error);
	if ((result < 0) || (dbus_error_is_set(&error) == TRUE))
		goto connection_unref;


	/* setup the connection by installing handlers */
	dbus_connection_set_watch_functions(cnx, add_watch, rem_watch, NULL,
					cnx,
					NULL);
	dbus_connection_set_timeout_functions(cnx, add_timeout, rem_timeout,
					timeout_toggled, cnx, NULL);
	dbus_connection_set_dispatch_status_function(cnx, dispatch_status, NULL,
						NULL);


	timeout = new_dispatch_timeout(cnx);
	if (!timeout)
		goto connection_unref;

	timeout_enable(timeout);

	return cnx;

connection_unref:
	dbus_connection_unref(cnx);
err:
	return NULL;
}

/*
   This function allocate an array of struct pollfd containing (nfds +
   reserve_slots) entries where nfds is the number of fds needed by libcdbus.
   The user can set reserve_slots to a value different from 0 to preallocate
   entries for its own file descriptors.
   If nfds and reserve_slots are null, the allocation is not done.
   Process pollfd must be called after the poll call to free the array.
 */
int cdbus_build_pollfds(struct pollfd ** fds, int *nfds, int reserve_slots)
{
	struct list_item_t *item;
	struct watch_t *watch;
	struct pollfd *curr;
	int fd;
	int flags;

	if (!fds || !nfds) {
		return -1;
	}

	/* To avoid the parsing of the list to find enabled watch,
	   we set nfds to the number of element in the watch list.
	   So, maybe there will be some free pollfd at the end of the array */
	*nfds = list_get_nb(&watch_list);
	if (*nfds < 0)
		return -1;
	if ((reserve_slots + *nfds) == 0)
		return 0;

	*fds = malloc(sizeof(struct pollfd) * (*nfds + reserve_slots));
	if (!*fds)
		return -1;
	memset(*fds, 0, sizeof(struct pollfd) * (*nfds + reserve_slots));

	*nfds = 0;
	curr = *fds;

	list_lock(&watch_list);

	item = __list_get_first(&watch_list);
	while (item) {
		watch = container_of(item, struct watch_t, litem);

		if (dbus_watch_get_enabled(watch->dbwatch) == TRUE) {
			fd = dbus_watch_get_unix_fd(watch->dbwatch);
			flags = dbus_watch_get_flags(watch->dbwatch);
			if (fd >= 0) {
				curr->fd = fd;
				if (flags & DBUS_WATCH_READABLE)
					curr->events |= POLLIN | POLLPRI;
				if (flags & DBUS_WATCH_WRITABLE)
					curr->events |= POLLOUT | POLLWRBAND;
				/* The pointer to the pollfd struct is stored
				   in the watch structure to speedup the process
				   function */
				watch->pollfd = curr;
				(*nfds)++;
				curr++;
			}
		}
		item = __list_get_next(item);
	}

	list_unlock(&watch_list);

	return 0;
}

/* Check events in the pollfd array and call dbus_watch_handle accordingly */
int cdbus_process_pollfds(struct pollfd * fds, int nfds)
{
	struct list_item_t *item;
	struct watch_t *watch;
	int fd;
	int flags = 0;

	if (!fds || (nfds < 0))
		return -1;

	if (!nfds)
		goto free;

	list_lock(&watch_list);

	item = __list_get_first(&watch_list);
	while (item) {
		watch = container_of(item, struct watch_t, litem);
		fd = dbus_watch_get_unix_fd(watch->dbwatch);
		if (watch->pollfd && (fd >=0)) {
			if (watch->pollfd->revents) {
				if (watch->pollfd->revents & POLLERR)
					flags |= DBUS_WATCH_ERROR;
				if (watch->pollfd->revents & POLLHUP)
					flags |= DBUS_WATCH_HANGUP;
				if (watch->pollfd->revents & (POLLIN | POLLPRI))
					flags |= DBUS_WATCH_READABLE;
				if (watch->pollfd->revents &
					(POLLOUT | POLLWRBAND))
					flags |= DBUS_WATCH_WRITABLE;
				LOG(LOG_DEBUG, "watch handle\n");
				dbus_watch_handle(watch->dbwatch, flags);
			}
		}

		item = __list_get_next(item);
	}

	list_unlock(&watch_list);

free:
	free(fds);

	return 0;
}

/* Return the time to the next timeout (in ms) */
int cdbus_next_timeout_event()
{
	struct list_item_t *item;
	struct timeout_t *timeout;
	item = list_get_first(&timeout_ordered_list);

	if (!item)
		return -1;

	timeout = container_of(item, struct timeout_t, ordered);
	return (timeout->value < 0) ? 0 : timeout->value;
}

/* This function must be called when a timeout occurs */
int cdbus_timeout_handle()
{
	static struct timespec previous = { 0, 0 };
	struct timespec now;
	struct timeout_t *timeout;
	struct list_item_t *item;
	struct list_t expired;
	int ms;

	LIST_INIT(expired);

	clock_gettime(CLOCK_MONOTONIC, &now);

	if (previous.tv_sec == 0 && previous.tv_nsec == 0) {
		/* The first time the function is called,
		   the timeouts are not updated since we haven't time
		   reference */
		goto post_update;
	}

	ms = (now.tv_sec - previous.tv_sec) * 1000;
	ms += (now.tv_nsec - previous.tv_nsec) / 1000000;

	/* sanity check */
	if (ms < 0)
		ms = 0;

	list_lock(&timeout_ordered_list);
	item = __list_get_first(&timeout_ordered_list);
	while (item) {
		timeout = container_of(item, struct timeout_t, ordered);
		item = __list_get_next(item);

		timeout->value -= ms;
		if (timeout->value <= 0) {
			__list_rem_item(&timeout->ordered);
			list_add_tail(&expired, &timeout->ordered);
		}
	}
	list_unlock(&timeout_ordered_list);


	/* Handle the timers that had expired */
	list_lock(&expired);
	item = __list_get_first(&expired);
	while (item) {
		timeout = container_of(item, struct timeout_t, ordered);
		item = __list_get_next(item);
		__list_rem_item(&timeout->ordered);

		if (timeout->dbtimeout) {
			LOG(LOG_DEBUG, "timeout handle\n");
			dbus_timeout_handle(timeout->dbtimeout);
			timeout_enable(timeout);
		}
		if (timeout->cb) {
			timeout->cb(timeout, timeout->cb_data);
			if (timeout->oneshot) {
				free(timeout);
			} else {
				timeout_enable(timeout);
			}
		}
	}
	list_unlock(&timeout_ordered_list);

post_update:
	previous.tv_sec = now.tv_sec;
	previous.tv_nsec = now.tv_nsec;

	return 0;
}

cdbus_proxy_fcn_t find_method(const char * member,
			struct cdbus_message_entry_t * itf_table)
{
	struct cdbus_message_entry_t * msg_entry = itf_table;
 	while (msg_entry->msg_name) {
		if (!strcmp(msg_entry->msg_name, member))
			break;
		msg_entry++;
	}

	if (!msg_entry->msg_name)
		return NULL;
	return msg_entry->msg_fcn;
}

cdbus_proxy_fcn_t find_method_with_interface(const char * interface,
					const char * member,
					struct cdbus_interface_entry_t * table)
{
	struct cdbus_interface_entry_t * itf_entry = table;


 	while (itf_entry->itf_name) {
		if (!strcmp(itf_entry->itf_name, interface))
			break;
		itf_entry++;
	}

	if (!itf_entry->itf_name)
		return NULL;
	return find_method(member, itf_entry->itf_table);
}

cdbus_proxy_fcn_t find_method_all_interfaces(const char * member,
					struct cdbus_interface_entry_t * table)
{
	struct cdbus_interface_entry_t * itf_entry = table;
	cdbus_proxy_fcn_t fcn = NULL;


 	while (itf_entry->itf_name) {
		fcn = find_method(member, itf_entry->itf_table);
		if (fcn)
			return fcn;
		itf_entry++;
	}

	return NULL;
}

int extstr_init(struct extensible_string_t * str)
{
	str->size = 0;
	str->buf_size = 0;
	str->buffer = malloc(EXTSTR_BUFF_SIZE);
	if (!str->buffer)
		return -1;
	str->buf_size = EXTSTR_BUFF_SIZE;
	return 0;
}

void extstr_free(struct extensible_string_t * str)
{
	if (str->buffer) {
		free(str->buffer);
		str->buffer = NULL;
	}
	str->size = 0;
	str->buf_size = 0;
}

int extstr_extend(struct extensible_string_t * str)
{
	char * buf;
	buf = realloc(str->buffer, str->buf_size + EXTSTR_BUFF_SIZE);

	if (!buf)
		return -1;

	str->buffer = buf;
	str->buf_size += EXTSTR_BUFF_SIZE;
	return 0;
}

int extstr_append_sprintf(struct extensible_string_t * str,
			const char * format, ...)
{
	va_list ap;
	int n;
	int extended = 0;

	do {
		extended = 0;

		va_start(ap, format);
		n = vsnprintf(EXTSTR_BUFFER(str),
			EXTSTR_REM_SIZE(str),
			format,
			ap);
		va_end(ap);

		if (n >= EXTSTR_REM_SIZE(str)) {
			extended = 1;
			if (extstr_extend(str) < 0)
				return -1;
		} else {
			str->size += n;
		}

	} while (extended);

	return 0;
}

int generate_argument_xml(struct extensible_string_t * str,
			struct cdbus_arg_entry_t * arg)
{
	int ret;
	ret = extstr_append_sprintf(str,
				"<arg name=\"%s\" "
				"type=\"%s\" "
				"direction=\"%s\" />",
				arg->arg_name,
				arg->signature,
				(arg->direction == CDBUS_DIRECTION_IN) ? "in" : "out");
	return ret;
}


int generate_message_xml(struct extensible_string_t * str,
			struct cdbus_message_entry_t * msg)
{
	int ret;
	struct cdbus_arg_entry_t *arg = msg->msg_table;

	if (msg->msg_fcn) {
		ret = extstr_append_sprintf(str,
					"<method name=\"%s\">",
					msg->msg_name);
	} else {
		ret = extstr_append_sprintf(str,
					"<signal name=\"%s\">",
					msg->msg_name);
	}
	if (ret < 0)
		return -1;

	while(arg->arg_name) {
		generate_argument_xml(str, arg);
		arg++;
	}

	if (msg->msg_fcn) {
		ret = extstr_append_sprintf(str,
					"</method>");
	} else {
		ret = extstr_append_sprintf(str,
					"</signal>");
	}
	if (ret < 0)
		return -1;

	return 0;
}

int generate_interface_xml(struct extensible_string_t * str,
			struct cdbus_interface_entry_t * itf)
{
	int ret;
	struct cdbus_message_entry_t *msg = itf->itf_table;

	ret = extstr_append_sprintf(str,
				"<interface name=\"%s\">",
				itf->itf_name);
	if (ret < 0)
		return -1;

	while (msg->msg_name) {
		ret = generate_message_xml(str, msg);
		if (ret < 0)
			return -1;
		msg++;
	}

	ret = extstr_append_sprintf(str,
				"</interface>");
	if (ret < 0)
		return -1;

	return 0;
}


int generate_object_xml(struct extensible_string_t * str,
			struct cdbus_interface_entry_t * table)
{
	int ret;
	struct cdbus_interface_entry_t *itf = table;
	ret = extstr_append_sprintf(str, "%s\n",
				DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE);
	if (ret < 0)
		return -1;

	ret = extstr_append_sprintf(str, "<node>");
	if (ret < 0)
		return -1;

	while(itf->itf_name) {
		ret = generate_interface_xml(str, itf);
		if (ret < 0)
			return -1;
		itf++;
	}

	ret = extstr_append_sprintf(str, "</node>");
	if (ret < 0)
		return -1;

	return 0;
}


int object_introspect(DBusConnection *cnx,
		DBusMessage *msg,
		struct cdbus_interface_entry_t * table)
{
	struct extensible_string_t str;
	int ret = 0;
	DBusMessage *reply;

	if (extstr_init(&str) < 0)
		return -1;

	ret = generate_object_xml(&str, table);
	if (ret < 0)
		goto free;

	reply = dbus_message_new_method_return(msg);
	if (!reply) {
		ret = -1;
		goto free;
	}

	if (dbus_message_append_args(reply,
					DBUS_TYPE_STRING,
					&str.buffer,
					DBUS_TYPE_INVALID) == FALSE) {
		ret = -1;
		goto msg_unref;
	}

	dbus_connection_send(cnx, reply, NULL);

msg_unref:
	dbus_message_unref(reply);
free:
	extstr_free(&str);
	return ret;
}

DBusHandlerResult object_dispatch(DBusConnection *cnx,
			DBusMessage *msg,
			void *data)
{
	struct cdbus_interface_entry_t * table = data;
	const char * interface;
	const char * member;
	cdbus_proxy_fcn_t fcn;
	int ret;

	if (!data)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	member = dbus_message_get_member(msg);
	if (!member)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!strncmp(member, "Introspect", strlen(member))) {
		if (object_introspect(cnx, msg, table) < 0)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		else
			return DBUS_HANDLER_RESULT_HANDLED;
	}

	interface = dbus_message_get_interface(msg);
	if (interface) {
		fcn = find_method_with_interface(interface, member, table);
	} else {
		find_method_all_interfaces(member, table);
	}

	if (!fcn) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	ret = fcn(cnx, msg);
	if (ret < 0)
		LOG(LOG_WARNING, "Failed to execute handler for member %s"
			"of object %s", member, dbus_message_get_path(msg));

	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusObjectPathVTable vtable = {
	.message_function = object_dispatch,
};

int cdbus_register_object(DBusConnection * cnx, const char * path,
			struct cdbus_interface_entry_t * object_table)
{
	int ret;
	ret = dbus_connection_register_object_path(cnx, path, &vtable,
						object_table);
	if (ret == FALSE)
		return -1;

	return 0;
}
