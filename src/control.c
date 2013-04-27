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
#include <stddef.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "macros.h"
#include "namespace.h"
#include "tty.h"
#include "control.h"

void control (struct arguments arguments) {
	if (arguments.daemon) {
		int nul = w(open("/dev/null",O_RDWR));
		w(dup2(nul, 0));
		w(dup2(nul, 1));
		w(dup2(nul, 2));
		w(close(nul));
	}

	// server: control current tty
	// client: current tty
	// listen: wait for future connection (attach)
	if (arguments.attach != NULL) {
		int client = w(socket(PF_UNIX, SOCK_SEQPACKET, 0));
		struct sockaddr_un local = { .sun_family = AF_UNIX, .sun_path = {0} };
		strncpy(local.sun_path+1, arguments.attach, sizeof(local.sun_path)-1);
		w(connect(client, (struct sockaddr *)&local,
					offsetof(struct sockaddr_un, sun_path)+1+strlen(local.sun_path+1)));

		attach_namespace(client, arguments);
	} else {
		int listen = -1;
		int server = -1;
		int client = -1;

		if (!arguments.daemon) {
			int sv[2];
			w(socketpair(PF_UNIX, SOCK_SEQPACKET, 0, sv));
			server = sv[0];
			client = sv[1];
		}

		if (arguments.keep != NULL) {
			listen = w(socket(PF_UNIX, SOCK_SEQPACKET, 0));
			struct sockaddr_un local = { .sun_family = AF_UNIX, .sun_path = {0} };
			strncpy(local.sun_path+1, arguments.keep, sizeof(local.sun_path)-1);
			w(bind(listen, (struct sockaddr *)&local,
						offsetof(struct sockaddr_un, sun_path)+1+strlen(local.sun_path+1)));
		}

		new_namespace(listen, server, client, arguments);

		if (listen >= 0) close(listen);
		if (server >= 0) close(server);
		if (!arguments.daemon) do_tty(client);
		if (client >= 0) close(client);
	}
}
