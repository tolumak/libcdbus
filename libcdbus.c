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
#include "libcdbus-version.h"
#include "macro.h"

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

struct signal_t {
	struct list_item_t item;
	DBusConnection * cnx;
	char * sender;
	char * object;
	char * match_rule;
	struct cdbus_user_data_t data;
};

static DECLARE_LIST_INIT(watch_list);
static DECLARE_LIST_INIT(timeout_ordered_list);
static DECLARE_LIST_INIT(signal_list);


static dbus_bool_t add_watch(DBusWatch *dbwatch, void *data)
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

static void rem_watch(DBusWatch *dbwatch, void *data)
{
	struct watch_t *watch;
	struct list_item_t *item;

	LOG(LOG_DEBUG, "rem watch\n");

	list_lock(&watch_list);
	__for_each_list_item(&watch_list, item, litem, watch) {
		if (watch->dbwatch == dbwatch)
			break;
	}
	list_unlock(&watch_list);

	if (!item)
		return;

	list_rem_item(item);
	free(watch);
}

static int timeout_enable(struct timeout_t *timeout)
{
	struct list_item_t *item;
	struct timeout_t *ordered_timeout;

	timeout->value = timeout->interval;

	list_lock(&timeout_ordered_list);
	__for_each_list_item(&timeout_ordered_list, item, ordered, ordered_timeout) {
		if (ordered_timeout->value > timeout->value)
			break;
	}
	if (item)
		__list_insert_before(&timeout->ordered, item);
	else
		__list_add_tail(&timeout_ordered_list, &timeout->ordered);

	list_unlock(&timeout_ordered_list);

	return 0;
}

static int timeout_disable(struct timeout_t *timeout)
{
	list_rem_item(&timeout->ordered);
	return 0;
}

static void free_timeout(void *data)
{
	struct timeout_t *timeout = data;

	LOG(LOG_DEBUG, "free timeout\n");
	free(timeout);
}

static void timeout_toggled(DBusTimeout *dbtimeout, void *data)
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

static dbus_bool_t add_timeout(DBusTimeout *dbtimeout, void *data)
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

static void rem_timeout(DBusTimeout *dbtimeout, void *data)
{
	struct timeout_t *timeout;

	LOG(LOG_DEBUG, "rem timeout\n");
	if (!dbtimeout)
		return;

	timeout = dbus_timeout_get_data(dbtimeout);
	if (timeout) {
		timeout_disable(timeout);
	}
}

static void dispatch(struct timeout_t *timeout, void* data)
{
	while (dbus_connection_dispatch(timeout->cnx) ==
		DBUS_DISPATCH_DATA_REMAINS) {
		LOG(LOG_DEBUG, "connection dispatch\n");
	}
}

static struct timeout_t* new_dispatch_timeout(DBusConnection *cnx)
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

static void dispatch_status(DBusConnection *cnx, DBusDispatchStatus new_status,
		void *data)
{
	struct timeout_t *timeout;

	if (!dbus_connection_get_is_connected(cnx))
		return;

	timeout = new_dispatch_timeout(cnx);
	if (timeout)
		timeout_enable(timeout);
}

static cdbus_proxy_fcn_t find_member(const char * member,
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

static cdbus_proxy_fcn_t find_member_with_interface(const char * interface,
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
	return find_member(member, itf_entry->itf_table);
}

static cdbus_proxy_fcn_t find_member_all_interfaces(const char * member,
					struct cdbus_interface_entry_t * table)
{
	struct cdbus_interface_entry_t * itf_entry = table;
	cdbus_proxy_fcn_t fcn = NULL;


 	while (itf_entry->itf_name) {
		fcn = find_member(member, itf_entry->itf_table);
		if (fcn)
			return fcn;
		itf_entry++;
	}

	return NULL;
}

static DBusHandlerResult message_handler(DBusConnection * cnx,
					DBusMessage * msg,
					void * user_data)
{
	struct list_item_t * item;
	struct signal_t * signal;
	cdbus_proxy_fcn_t proxy;

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		goto not_handled;

	for_each_list_item(&signal_list, item, item, signal) {
		if (!signal->data.object_table)
			continue;
		if ((signal->cnx = cnx)
			&& (!signal->object || dbus_message_has_path(msg, signal->object))
			&& (!signal->sender || dbus_message_has_sender(msg, signal->sender)))
			break;
	}

	if (!signal)
		goto signal_not_handled;

	if (dbus_message_get_interface(msg))
		proxy = find_member_with_interface(dbus_message_get_interface(msg),
						dbus_message_get_member(msg),
						signal->data.object_table);
	else
		proxy = find_member_all_interfaces(dbus_message_get_member(msg),
						signal->data.object_table);

	if (!proxy)
		goto signal_not_handled;

	if (proxy(cnx, msg, signal->data.user_data) < 0)
		goto signal_not_handled;


	LOG(LOG_DEBUG, "signal %s.%s from %s (%s) catched\n",
		dbus_message_get_interface(msg),
		dbus_message_get_member(msg),
		dbus_message_get_path(msg),
		dbus_message_get_sender(msg));

	return DBUS_HANDLER_RESULT_HANDLED;

signal_not_handled:
	LOG(LOG_DEBUG, "signal %s.%s from %s (%s) ignored\n",
		dbus_message_get_interface(msg),
		dbus_message_get_member(msg),
		dbus_message_get_path(msg),
		dbus_message_get_sender(msg));

not_handled:
	return  DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


DBusConnection* cdbus_get_connection(DBusBusType bus_type)
{
	DBusError error;
	DBusConnection *cnx;

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

	dbus_connection_add_filter(cnx, message_handler, NULL, NULL);

	dbus_error_free(&error);

	return cnx;

connection_unref:
	dbus_connection_unref(cnx);
err:
	dbus_error_free(&error);
	return NULL;
}

int cdbus_request_name(DBusConnection* cnx, char * name, int replace)
{
	int flags = 0;
	DBusError error;
	int ret = 0;

	flags |= DBUS_NAME_FLAG_ALLOW_REPLACEMENT;
	flags |= DBUS_NAME_FLAG_DO_NOT_QUEUE;
	if (replace)
		flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;

	dbus_error_init(&error);

	/* request the connection name */
	ret = dbus_bus_request_name(cnx, name, flags, &error);
	if ((ret < 0) || (dbus_error_is_set(&error) == TRUE))
		ret = -1;


	dbus_error_free(&error);
	return ret;
}

const char * cdbus_version_string()
{
	return libcdbus_version_string;
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

	__for_each_list_item(&watch_list, item, litem, watch) {
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
	list_unlock(&watch_list);
	while (item) {
		list_lock(&watch_list);
		watch = container_of(item, struct watch_t, litem);
		item = __list_get_next(item);
		list_unlock(&watch_list);
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

	}


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
	__for_each_list_item(&timeout_ordered_list, item, ordered, timeout) {

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
	list_unlock(&expired);
	while (item) {
		list_lock(&expired);
		timeout = container_of(item, struct timeout_t, ordered);
		item = __list_get_next(item);
		list_unlock(&expired);
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

post_update:
	previous.tv_sec = now.tv_sec;
	previous.tv_nsec = now.tv_nsec;

	return 0;
}

static int extstr_init(struct extensible_string_t * str)
{
	str->size = 0;
	str->buf_size = 0;
	str->buffer = malloc(EXTSTR_BUFF_SIZE);
	if (!str->buffer)
		return -1;
	str->buf_size = EXTSTR_BUFF_SIZE;
	return 0;
}

static void extstr_free(struct extensible_string_t * str)
{
	if (str->buffer) {
		free(str->buffer);
		str->buffer = NULL;
	}
	str->size = 0;
	str->buf_size = 0;
}

static int extstr_extend(struct extensible_string_t * str)
{
	char * buf;
	buf = realloc(str->buffer, str->buf_size + EXTSTR_BUFF_SIZE);

	if (!buf)
		return -1;

	str->buffer = buf;
	str->buf_size += EXTSTR_BUFF_SIZE;
	return 0;
}

static int extstr_append_sprintf(struct extensible_string_t * str,
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

static int generate_argument_xml(struct extensible_string_t * str,
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


static int generate_message_xml(struct extensible_string_t * str,
			struct cdbus_message_entry_t * msg)
{
	int ret;
	struct cdbus_arg_entry_t *arg = msg->msg_table;

	if (!msg->is_signal) {
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

	if (!msg->is_signal) {
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

static int generate_interface_xml(struct extensible_string_t * str,
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


static int generate_object_xml(DBusConnection * cnx,
			DBusMessage * msg,
			struct extensible_string_t * str,
			struct cdbus_interface_entry_t * table)
{
	int ret;

	struct cdbus_interface_entry_t *itf = table;
	char ** strarr, **curr;
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

	if (dbus_connection_list_registered(cnx, dbus_message_get_path(msg), &strarr)) {
		for (curr = strarr ; *curr ; curr++) {
			ret = extstr_append_sprintf(str, "<node name=\"%s\"/>", *curr);
			if (ret < 0)
				return -1;
		}
		dbus_free_string_array(strarr);
	}


	ret = extstr_append_sprintf(str, "</node>");

	if (ret < 0)
		return -1;

	return 0;
}


static int object_introspect(DBusConnection *cnx,
		DBusMessage *msg,
		struct cdbus_interface_entry_t * table)
{
	struct extensible_string_t str;
	int ret = 0;
	DBusMessage *reply;

	if (extstr_init(&str) < 0)
		return -1;

	ret = generate_object_xml(cnx, msg, &str, table);
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

static DBusHandlerResult object_dispatch(DBusConnection *cnx,
			DBusMessage *msg,
			void *data)
{
	struct cdbus_user_data_t * user_data = data;
	struct cdbus_interface_entry_t * table;
	const char * interface;
	const char * member;
	cdbus_proxy_fcn_t fcn;
	int ret;

	if (!data)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (user_data)
		table = user_data->object_table;

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
		fcn = find_member_with_interface(interface, member, table);
	} else {
		fcn = find_member_all_interfaces(member, table);
	}

	if (!fcn) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	ret = fcn(cnx, msg, user_data->user_data);
	if (ret < 0)
		LOG(LOG_WARNING, "Failed to execute handler for member %s "
			"of object %s\n", member, dbus_message_get_path(msg));

	return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusObjectPathVTable vtable = {
	.message_function = object_dispatch,
};

int cdbus_register_object(DBusConnection * cnx, const char * path,
			struct cdbus_user_data_t * user_data)
{
	int ret;
	ret = dbus_connection_register_object_path(cnx, path, &vtable,
						user_data);
	if (ret == FALSE)
		return -1;

	return 0;
}

int cdbus_unregister_object(DBusConnection * cnx, const char * path)
{
	int ret;
	ret = dbus_connection_unregister_object_path(cnx, path);
	if (ret == FALSE)
		return -1;
	return 0;
}

int cdbus_register_signals(DBusConnection * cnx, const char * sender, const char * path,
	struct cdbus_user_data_t * user_data)
{
	struct signal_t * signal;
	struct cdbus_interface_entry_t * itf_entry;
	int len;


	if (!cnx)
		return -1;
	if (!user_data || !user_data->object_table)
		return -1;

	signal = malloc(sizeof(*signal));
	if (!signal)
		return -1;
	memset(signal, 0, sizeof(*signal));

	LIST_ITEM_INIT(signal->item);
	signal->cnx = cnx;
	if (sender)
		signal->sender = strdup(sender);
	if (path)
		signal->object = strdup(path);
	signal->data.object_table = user_data->object_table;
	signal->data.user_data = user_data->user_data;
	signal->match_rule = malloc(sizeof(char) * DBUS_MAXIMUM_MATCH_RULE_LENGTH);

	if (!signal->match_rule)
		goto free;
	if (sender && !signal->sender)
		goto free;
	if (path && !signal->object)
		goto free;

	len = snprintf(signal->match_rule, DBUS_MAXIMUM_MATCH_RULE_LENGTH,
		"type='signal'");
	itf_entry = signal->data.object_table;
	while (itf_entry->itf_name) {
		len += snprintf(signal->match_rule + len,
				DBUS_MAXIMUM_MATCH_RULE_LENGTH - len,
				",interface='%s'",
				itf_entry->itf_name);
		itf_entry++;
	}
	if (signal->sender)
		len += snprintf(signal->match_rule + len,
				DBUS_MAXIMUM_MATCH_RULE_LENGTH - len,
				",sender='%s'",
				signal->sender);
	if (signal->object)
		len += snprintf(signal->match_rule + len,
				DBUS_MAXIMUM_MATCH_RULE_LENGTH - len,
				",path='%s'",
				signal->object);
	signal->match_rule[DBUS_MAXIMUM_MATCH_RULE_LENGTH - 1] = 0;
	dbus_bus_add_match(cnx, signal->match_rule, NULL);

	list_add_tail(&signal_list, &signal->item);

	return 0;

free:
	if (signal->match_rule)
		free(signal->match_rule);
 	if (signal->sender)
		free(signal->sender);
 	if (signal->object)
		free(signal->object);
	free(signal);
	return -1;

}

int cdbus_unregister_signals(DBusConnection * cnx, const char * sender, const char * path)
{
	struct list_item_t * item;
	struct signal_t * signal;

	for_each_list_item(&signal_list, item, item, signal) {
		if ((signal->cnx == cnx)
			&& (signal->sender == sender)
			&& (signal->object == path))
			break;
	}
	if (signal) {
		list_rem_item(&signal->item);
		dbus_bus_remove_match(cnx, signal->match_rule, NULL);

		if (signal->sender)
			free(signal->sender);
		if (signal->object)
			free(signal->object);
		free(signal);
		return 0;
	} else {
		return -1;
	}
}
