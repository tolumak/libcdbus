/*
 * D-Bus C Bindings library
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

#ifndef LIBCDBUS_H
#define LIBCDBUS_H

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

struct cdbus_user_data_t
{
	struct cdbus_interface_entry_t * object_table;
	void * user_data;
};

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

#endif
