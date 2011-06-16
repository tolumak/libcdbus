#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libcdbus.h"


static int done = 0;

int hello_object_world(DBusConnection *cnx, DBusMessage *msg)
{
	DBusMessage * reply;
	const char * string = "Hello World!";

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return -1;
	dbus_message_append_args(reply, DBUS_TYPE_STRING, &string,
				DBUS_TYPE_INVALID);

	dbus_connection_send(cnx, reply, NULL);
	dbus_message_unref(reply);
}

DBusHandlerResult hello_object(DBusConnection *cnx, DBusMessage *msg, void *data)
{
	const char * member;
	DBusMessage * error;

	member = dbus_message_get_member(msg);
	if (!strcmp(member, "World"))
		hello_object_world(cnx, msg);
	else {
		error = dbus_message_new_error(msg, DBUS_ERROR_FAILED, "invalid member");
		dbus_connection_send(cnx, error, NULL);
		dbus_message_unref(error);
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}

void sighandler(int signal)
{
	printf("signal %d catched\n", signal);
	done = 1;
}

DBusObjectPathVTable vtable = {
	.message_function = hello_object,
};

#define FIFO_PATH "/tmp/toto"

int main(int argc, char **argv)
{
	struct sigaction action;
	DBusConnection *cnx;
	int timeout;
	struct pollfd * fds;
	int nfds;
	int ret;
	int fifofd;
	char c;

	memset(&action, 0, sizeof(action));
	action.sa_handler = sighandler;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);

	if (mkfifo(FIFO_PATH, S_IRUSR | S_IWUSR) < 0)
		return -1;

	fifofd = open(FIFO_PATH, O_RDWR);
	if (fifofd < 0)
		goto unlink_fifo;

	cnx = cdbus_get_connection(DBUS_BUS_SESSION, "fr.sise.test", 0);
	if (!cnx)
		return -1;

	dbus_connection_register_object_path(cnx, "/hello", &vtable, NULL);

	while(!done) {
		timeout = cdbus_next_timeout_event();
		if (cdbus_build_pollfds(&fds, &nfds, 1) < 0)
			break;
		fds[nfds].fd = fifofd;
		fds[nfds].events = POLLIN;
		ret = poll(fds, nfds + 1, timeout);
		if (ret > 0) {
			cdbus_process_pollfds(fds, nfds);
			if (fds[nfds].revents) {
				while (read(fifofd, &c, 1) > 0)
					printf("%c", c);
			}
		}
		cdbus_timeout_handle();
	}

	dbus_connection_unref(cnx);

unlink_fifo:
	unlink(FIFO_PATH);

	return 0;
}
