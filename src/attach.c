/* * Sandbox - Create a sandbox environment for separating running programs
 * Copyright (C) 2013 Zang MingJie <zealot0630@gmail.com>
 *
 * This file is part of Sandbox.
 *
 * Sandbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Sandbox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Sandbox.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <event2/event.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "macros.h"
#include "arguments.h"
#include "namespace.h"

struct arg {
	struct event *event;
	char **command;
};

static void client_func(evutil_socket_t fd, short what, void *arg) {
	struct arg *a = arg;
	struct event *event = a->event;
	char **command = a->command;
	bool run = true;
	while (run) {
		char buf[1024];
		struct iovec iov;
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);

		int fds[num_ns];
		char cms[CMSG_SPACE(sizeof(fds))];

		struct msghdr msg = {0};
		msg.msg_name = 0;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = cms;
		msg.msg_controllen = sizeof cms;

		ssize_t s = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
		if (s > 0) {
			struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
			if (buf[0] == 0 && cmsg != NULL
					&& cmsg->cmsg_level == SOL_SOCKET
					&& cmsg->cmsg_type == SCM_RIGHTS
					&& cmsg->cmsg_len == CMSG_LEN(sizeof(fds))) {
				memmove(fds, CMSG_DATA(cmsg), sizeof(fds));

				for (int i = 0; i < num_ns; ++i) {
					w(setns(fds[i], ns_type[i]));
					w(close(fds[i]));
				}

				if (w(fork()) > 0) {
					wait(NULL);
				} else {
					if (w(fork()) > 0) {
						w(close(fd));
						exit(EXIT_SUCCESS);
					} else {
						// notify init our pid
						w(send(fd, "\1", 1, 0));
						w(close(fd));
						if (s > 1) {
							w(chroot(buf+1));
							w(chdir("/"));
						}
						w(execvp(command[0], command));
					}
				}
			} else {
				fprintf(stderr, "client read unknown msg\n");
			}
		} else if (s == 0) {
			event_free(event);
			w(close(fd));
			run = false;
		} else if (s < 0 && errno == EAGAIN) {
			run = false;
		} else {
			perror("client read");
			event_free(event);
			w(close(fd));
			run = false;
		}
	}
}

void attach_namespace (int client, struct arguments arguments) {
	w(evutil_make_socket_closeonexec(client));
	w(evutil_make_socket_nonblocking(client));

	struct event_config *ev_config = p(event_config_new());
	w(event_config_require_features(ev_config, EV_FEATURE_FDS));
	struct event_base *ev_base = p(event_base_new_with_config(ev_config));
	event_config_free(ev_config);

	struct event *ev_client = p(event_new(ev_base, client, EV_READ|EV_PERSIST, client_func, NULL));

	struct arg arg = { .event = ev_client, .command = arguments.command };

	w(event_assign(ev_client, ev_base, client, EV_READ|EV_PERSIST, client_func, &arg));
	event_add(ev_client, NULL);

	w(event_base_dispatch(ev_base));

	event_base_free(ev_base);
}

