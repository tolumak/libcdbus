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

DBusConnection* cdbus_get_connection(DBusBusType bus_type, char *name,
				int replace);

/* Main loop functions */
int cdbus_build_pollfds(struct pollfd ** fds, int *nfds, int reserve_slots);
int cdbus_process_pollfds(struct pollfd * fds, int nfds);
int cdbus_next_timeout_event();
int cdbus_timeout_handle();

#endif
