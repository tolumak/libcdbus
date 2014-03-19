/*
 * D-Bus C Bindings library
 *
 * Copyright 2011 S.I.S.E. S.A
 * Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
 *
 */

#ifndef __LIBCDBUS_H
#define __LIBCDBUS_H

#include <poll.h>
#include <dbus/dbus.h>

DBusConnection* cdbus_get_connection(DBusBusType bus_type);

int cdbus_request_name(DBusConnection* cnx, char * name, int replace);

const char * cdbus_version_string();

/* Main loop functions */
int cdbus_build_pollfds(struct pollfd ** fds, int *nfds, int reserve_slots);
int cdbus_process_pollfds(struct pollfd * fds, int nfds);
int cdbus_next_timeout_event();
int cdbus_timeout_handle();

struct cdbus_user_data_t;

/* Objects management */
int cdbus_register_object(DBusConnection * cnx, const char * path,
			struct cdbus_user_data_t * user_data);
int cdbus_unregister_object(DBusConnection * cnx, const char * path);

/* Signals */
int cdbus_register_signals(DBusConnection * cnx, const char * sender, const char * path,
	struct cdbus_user_data_t * user_data);
int cdbus_unregister_signals(DBusConnection * cnx, const char * sender, const char * path);


/* Private declarations */

typedef int (*cdbus_proxy_fcn_t)(DBusConnection *, DBusMessage*, void *);

#define CDBUS_DIRECTION_IN  0
#define CDBUS_DIRECTION_OUT 1

struct cdbus_arg_entry_t
{
	char * arg_name;
	int direction;
	char * signature;
};

struct cdbus_message_entry_t
{
	int is_signal;
	char *msg_name;
	cdbus_proxy_fcn_t msg_fcn;
	struct cdbus_arg_entry_t *msg_table;
};

struct cdbus_interface_entry_t
{
	char *itf_name;
	struct cdbus_message_entry_t *itf_table;
};

struct cdbus_user_data_t
{
	struct cdbus_interface_entry_t * object_table;
	void * user_data;
};

#endif
