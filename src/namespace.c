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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>

#include "macros.h"
#include "arguments.h"
#include "init.h"
#include "namespace.h"

struct arg {
	int listen;
	int server;
	int client;
	struct arguments arguments;
};

static int initialize (void *a) {
	struct arg *arg = (struct arg *)a;
	if (arg->client >= 0)
		w(close(arg->client));
	do_init(arg->listen, arg->server, arg->arguments);
	return 0;
}

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

pid_t new_namespace(int listen, int server, int client, struct arguments arguments) {
	struct arg arg = {
		.listen = listen,
		.server = server,
		.client = client,
		.arguments = arguments,
	};

	pid_t pid = w(clone(initialize, child_stack + STACK_SIZE, CLONE_NEWUSER|CLONE_NEWNS|CLONE_NEWUTS|CLONE_NEWIPC|CLONE_NEWPID|CLONE_NEWNET|SIGCHLD, &arg));

	char path[PATH_MAX] = {0};
	char mapping[25] = {0};

	snprintf(path, PATH_MAX, "/proc/%ld/%s_map", (long)pid, "uid");
	int fd = w(open(path, O_RDWR));
	snprintf(mapping, sizeof(mapping), "0 %ld 1", (long)getuid());
	int len = strlen(mapping);
	x(write(fd, mapping, len) == len);
	close(fd);

	snprintf(path, PATH_MAX, "/proc/%ld/%s_map", (long)pid, "gid");
	fd = w(open(path, O_RDWR));
	snprintf(mapping, sizeof(mapping), "0 %ld 1", (long)getgid());
	len = strlen(mapping);
	x(write(fd, mapping, len) == len);
	close(fd);

	return pid;
}
