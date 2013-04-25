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

#include <stdbool.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xmalloc.h"
#include "system.h"
#include "macros.h"
#include "tsearch.h"
#include "namespace.h"

#include "init.h"


static struct ts *tree; // global

struct signal_arg {
	struct event *ev_sigchld;
};

struct signal_accept {
	struct event_base *ev_base;
	char *chroot;
};

static void sigchld_func(evutil_socket_t fd, short what, void *arg) {
	struct signal_arg *a = (struct signal_arg*)arg;
	struct event *ev_sigchld = a->ev_sigchld;

	if (fd == SIGCHLD) {
		bool run = true;
		while (run) {
			int status;
			pid_t pid = waitpid(-1, &status, WUNTRACED|WNOHANG);
			if (pid == 0) {
				run = false;
			} else if (pid < 0 && errno == EAGAIN) {
				run = false;
			} else if (pid < 0 && errno == ECHILD) {
				if (ev_sigchld) event_free(ev_sigchld);
				run = false;
			} else if (pid > 0) {
				if (WIFEXITED(status) || WIFSIGNALED(status)) {
					int server = ts_search(tree, pid);
					if (server >= 0) {
						w(shutdown(server, SHUT_WR));
						ts_remove(tree, pid);
					}
				}
			} else {
				perror("waitpid");
			}
		}
	}
}

static void tree_remove(int value) {
	int key = ts_find_value(tree, value);
	if (key >= 0) ts_remove(tree, key);
}

static void server_func(evutil_socket_t fd, short what, void *arg) {
	struct event *event = arg;
	bool run = true;
	while (run) {
		char buf[16];
		ssize_t s = read(fd, buf, sizeof(buf));
		if (s > 0) {
			fprintf(stderr, "discard server read.\n");
		} else if (s == 0) {
			event_free(event);
			w(close(fd));
			tree_remove(fd);
			run = false;
		} else if (s < 0 && errno == EAGAIN) {
			run = false;
		} else {
			perror("server read");
			event_free(event);
			w(close(fd));
			tree_remove(fd);
			run = false;
		}
	}
}

static void write_ns_fd(int server, char *chroot) {
	int fds[num_ns];

	for (int i = 0; i < num_ns; ++i) {
		char path[PATH_MAX];
		snprintf(path, PATH_MAX, "/proc/self/ns/%s", ns[i]);
		fds[i] = w(open(path, O_RDONLY|O_CLOEXEC));
	}

	struct iovec iov[2] = {
		{ .iov_base = "\0", .iov_len = 1 },
		{ .iov_base = chroot, .iov_len = chroot == NULL ? 0 : strlen(chroot)+1 }
	};
	char buf[CMSG_SPACE(sizeof(fds))];  /* ancillary data buffer */

	struct msghdr msg = {0};
	msg.msg_iov = iov;
	msg.msg_iovlen = chroot == NULL ? 1 : 2;
	msg.msg_control = buf;
	msg.msg_controllen = CMSG_LEN(sizeof(fds));

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fds));
	memmove(CMSG_DATA(cmsg), fds, sizeof(fds));

	x(sendmsg(server, &msg, 0) == iov[0].iov_len+iov[1].iov_len);

	for (int i = 0; i < num_ns; ++i) {
		w(close(fds[i]));
	}
}

struct attach_arg {
	struct event *event;
	int state;
};

static void attach_func(evutil_socket_t fd, short what, void *arg) {
	struct attach_arg *a = (struct attach_arg *)arg;
	struct event *event = a->event;
	bool run = true;
	while (run) {
		char buf[16];
		struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };

		struct ucred cred;
		char cms[CMSG_SPACE(sizeof(cred))];

		struct msghdr msg = {0};
		msg.msg_name = 0;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = cms;
		msg.msg_controllen = sizeof(cms);

		ssize_t s = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
		if (s > 0) {
			switch (a->state) {
				case 0:
					{
						struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
						if (buf[0] == 1 && cmsg != NULL
								&& cmsg->cmsg_level == SOL_SOCKET
								&& cmsg->cmsg_type == SCM_CREDENTIALS
								&& cmsg->cmsg_len == CMSG_LEN(sizeof(cred))) {
							memmove(&cred, CMSG_DATA(cmsg), sizeof(cred));

							ts_insert(tree, cred.pid, fd);

							a->state = 1;
						} else {
							fprintf(stderr, "server read unknown msg\n");
							event_free(event);
							free(arg);
							w(close(fd));
							tree_remove(fd);
							run = false;
						}
					}
					break;
				default:
					fprintf(stderr, "server unknown state\n");
					event_free(event);
					free(arg);
					w(close(fd));
					tree_remove(fd);
					run = false;
					break;
			}
		} else if (s == 0) {
			event_free(event);
			free(arg);
			w(close(fd));
			tree_remove(fd);
			run = false;
		} else if (s < 0 && errno == EAGAIN) {
			run = false;
		} else {
			perror("server read");
			event_free(event);
			free(arg);
			w(close(fd));
			tree_remove(fd);
			run = false;
		}
	}
}

static void listen_func(evutil_socket_t fd, short what, void *arg) {
	struct signal_accept *signal_accept = (struct signal_accept*)arg;
	struct event_base *ev_base = signal_accept->ev_base;
	bool run = true;
	while (run) {
		struct sockaddr_un addr;
		socklen_t size = sizeof(addr);
		int server = accept4(fd, &addr, &size, SOCK_NONBLOCK|SOCK_CLOEXEC);
		if (server > 0) {
			int on = 1;
			setsockopt(server, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
			write_ns_fd(server, signal_accept->chroot);
			struct event *ev_server = p(event_new(ev_base, server, EV_READ|EV_PERSIST, attach_func, NULL));
			struct attach_arg *attach_arg = xmalloc(sizeof(attach_arg));
			attach_arg->event = ev_server;
			attach_arg->state = 0;
			w(event_assign(ev_server, ev_base, server, EV_READ|EV_PERSIST, attach_func, attach_arg));
			event_add(ev_server, NULL);
		} else if (server < 0 && errno == EAGAIN) {
			run = false;
		} else {
			perror("accept4");
		}
	}
}

void do_init(int listenfd, int server, struct arguments arguments) {
	if (listenfd >= 0) {
		w(evutil_make_socket_closeonexec(listenfd));
		w(evutil_make_socket_nonblocking(listenfd));
	}
	if (server >= 0) {
		w(evutil_make_socket_closeonexec(server));
		w(evutil_make_socket_nonblocking(server));
	}

	if (arguments.daemon) w(setsid());
	//if (arguments.fakeroot) setenv("FAKEROOTDONTTRYCHOWN", "1", true);

	if (arguments.chroot != NULL) {
		w(chroot(arguments.chroot));
		w(chdir("/"));
	}
	umount("/proc"); // ignore umount fail
	w(mount("proc", "/proc", "proc", 0, NULL));

	pid_t child;
	if (arguments.command == NULL || w(child = fork()) > 0) {
		tree = ts_new();
		if (arguments.command != NULL)
			ts_insert(tree, child, server);

		struct event_config *ev_config = p(event_config_new());
		w(event_config_require_features(ev_config, EV_FEATURE_FDS));
		struct event_base *ev_base = p(event_base_new_with_config(ev_config));
		event_config_free(ev_config);

		struct signal_arg signal_arg = { .ev_sigchld = NULL };
		struct event *ev_sigchld = p(evsignal_new(ev_base, SIGCHLD, sigchld_func, &signal_arg));
		// dont detach sigchld if keep running
		if (!arguments.keep) signal_arg.ev_sigchld = ev_sigchld;
		event_add(ev_sigchld, NULL);

		struct event *ev_listen = NULL;
		struct signal_accept signal_accept = { .ev_base = ev_base, .chroot = arguments.chroot };
		if (listenfd >= 0) {
			ev_listen = p(event_new(ev_base, listenfd, EV_READ|EV_PERSIST, listen_func, &signal_accept));
			event_add(ev_listen, NULL);
			w(listen(listenfd, 5));
		}
		if (server >= 0) {
			struct event *ev_server = p(event_new(ev_base, server, EV_READ|EV_PERSIST, server_func, NULL));
			w(event_assign(ev_server, ev_base, server, EV_READ|EV_PERSIST, server_func, ev_server));
			event_add(ev_server, NULL);
		}

		w(event_base_dispatch(ev_base));

		event_base_free(ev_base);

		ts_free(tree);
	} else {
		w(execvp(arguments.command[0], arguments.command));
	}
}
