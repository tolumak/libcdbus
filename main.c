#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "libcdbus.h"
#include "fr_sise_test.h"


static int done = 0;

int fr_sise_test_Hello(char * who, char ** out)
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

void fr_sise_test_Hello_free(char * who, char ** out)
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
		goto close_fifo;

	if (cdbus_register_object(cnx, "/fr/sise/test",
					fr_sise_test_object_table) < 0)
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
			while (read(fifofd, &c, 1) > 0)
				printf("%c", c);
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
