/*
 * D-Bus C Bindings library: test program
 *
 * Copyright 2011 S.I.S.E. S.A
 * Author: Michel Lafon-Puyo <mlafon-puyo@sise.fr>
 *
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "libcdbus.h"
#include "fr_sise_test.h"


static int done = 0;

int fr_sise_test_Hello(DBusConnection *cnx, DBusMessage *msg, void *data, char * who, char ** out)
{
	*out = malloc(sizeof(char) * 128);
	if (!*out)
		return -1;

	if (!who || strlen(who) == 0) {
		sprintf(*out, "Hello World!");
	} else {
		snprintf(*out, 128, "Hello %s!", who);
	}

	return 0;
}

void fr_sise_test_Hello_free(DBusConnection *cnx, DBusMessage *msg, void * data, char * who, char ** out)
{
	if (*out)
		free(*out);
}

void sighandler(int signal)
{
	printf("signal %d catched\n", signal);
	done = 1;
}


#define FIFO_PATH "/tmp/toto"

int main(int argc, char **argv)
{
	struct sigaction action;
	DBusConnection *cnx;
	int timeout;
	struct pollfd * fds;
	int nfds;
	int fifofd;
	char *msg;
	int tmp;
	int nbread;
	struct cdbus_user_data_t user_data;

	memset(&action, 0, sizeof(action));
	action.sa_handler = sighandler;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);

	if (mkfifo(FIFO_PATH, S_IRUSR | S_IWUSR) < 0)
		return -1;

	fifofd = open(FIFO_PATH, O_RDWR);
	if (fifofd < 0)
		goto unlink_fifo;

	cnx = cdbus_get_connection(DBUS_BUS_SESSION);
	if (!cnx)
		goto close_fifo;

	if (cdbus_request_name(cnx, "fr.sise.test", 0) < 0)
		goto unref_cnx;

	user_data.object_table = fr_sise_test_object_table;
	if (cdbus_register_object(cnx, "/fr/sise/test",
					&user_data) < 0)
		goto unref_cnx;

	while(!done) {
		timeout = cdbus_next_timeout_event();
		if (cdbus_build_pollfds(&fds, &nfds, 1) < 0)
			break;
		fds[nfds].fd = fifofd;
		fds[nfds].events = POLLIN;
		poll(fds, nfds + 1, timeout);
		cdbus_process_pollfds(fds, nfds);
		if (fds[nfds].revents) {
			msg = malloc(128 * sizeof(char));
			memset(msg, 0, 128 * sizeof(char));
			tmp = nbread = 0;
			do {
				tmp = read(fifofd, msg + nbread,
					sizeof(msg) - nbread);
				if (tmp < 0) {
					if (errno == EINTR)
						continue;
					else
						break;
				}
				nbread += tmp;
			}while(tmp > 0);

			if (tmp == 0) {
				if (fr_sise_test_Hi(cnx, NULL, NULL, msg) < 0) {
					printf("Failed to send signal!\n");
				}
			}

			free(msg);
		}
		cdbus_timeout_handle();
	}

unref_cnx:
	dbus_connection_unref(cnx);

close_fifo:
	close(fifofd);

unlink_fifo:
	unlink(FIFO_PATH);

	return 0;
}

struct fr_sise_test_ops fr_sise_test_ops =
{
	.Hello = fr_sise_test_Hello,
	.Hello_free = fr_sise_test_Hello_free,
};
