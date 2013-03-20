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
#include <argp.h>
#include <sys/types.h>
#include <pwd.h>

#include "system.h"
#include "macros.h"
#include "arguments.h"
#include "namespace.h"
#include "control.h"

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static void show_version (FILE *stream, struct argp_state *state);

/* Option flags and variables.  These are initialized in parse_opt.  */
static struct argp_option options[] =
{
	{ "daemon", 'd', NULL, 0, _("run target program as a daemon"), 0 },
	{ "keep", 'k', "file", 0, _("keep the sandbox running, so it can be attached later"), 1 },
	{ "attach", 'a', "file", 0, _("attach to a running sandbox"), 1 },
	{ "fakeroot", 'f', NULL, 0, _("call fakeroot instead of shell, so it will be more like root"), 2 },
	{ NULL, 0, NULL, 0, NULL, 0 }
};

/* The argp functions examine these global variables.  */
const char *argp_program_bug_address = "<zealot0630@gmail.com>";
void (*argp_program_version_hook) (FILE *, struct argp_state *) = show_version;

static struct argp argp =
{
	options, parse_opt, _("-- [command]"),
	_("create a sandbox environment for separating running programs"),
	NULL, NULL, NULL
};

int main (int argc, char **argv)
{
	textdomain(PACKAGE);

	int arg_index = 0;

	struct arguments arguments;
	argp_parse(&argp, argc, argv, 0, &arg_index, &arguments);
	arguments.command = argv + arg_index;

	if (arguments.command && arguments.command[0]) {
		if (arguments.fakeroot) {
			int args = argc - arg_index;
			char *argv[args+3];
			argv[0] = "fakeroot";
			argv[1] = "--";
			memcpy(argv + 2, arguments.command, (args+1) * sizeof(char*));
			arguments.command = argv;
			control (arguments);
		} else {
			control (arguments);
		}
	} else if (!arguments.daemon) {
		if (arguments.fakeroot) {
			char *argv[2] = { "fakeroot", NULL };
			arguments.command = argv;
			control (arguments);
		} else {
			/* get & run user sh */
			size_t length = 0x100;
			char buf[length];
			struct passwd pwd;
			struct passwd *result;
			w(getpwuid_r(getuid(), &pwd, buf, length, &result));
			char *argv[2] = { result->pw_shell, NULL };
			arguments.command = argv;
			control (arguments);
		}
	}

	exit (0);
}

/* Parse a single option.  */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments= state->input;

	switch (key)
	{
		case ARGP_KEY_INIT:
			arguments->daemon = false;
			arguments->keep = NULL;
			arguments->attach = NULL;
			arguments->fakeroot = false;
			arguments->command = NULL;
			break;
		case 'k':
			if (arguments->attach)
				return EINVAL;
			if (!arg)
				arguments->keep = "/tmp/sandbox";
			else
				arguments->keep = arg;
			break;
		case 'a':
			if (arguments->keep)
				return EINVAL;
			if (!arg)
				arguments->attach = "/tmp/sandbox";
			else
				arguments->attach = arg;
			break;
		case 'd':
			arguments->daemon = true;
			break;
		case 'f':
			arguments->fakeroot = true;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/* Show the version number and copyright information.  */
static void show_version (FILE *stream, struct argp_state *state)
{
	(void) state;
	/* Print in small parts whose localizations can hopefully be copied
	   from other programs.  */
	fputs(PACKAGE" "VERSION"\n", stream);
	fprintf(stream, _("Written by %s.\n\n"), "Zang MingJie");
	fprintf(stream, _("Copyright (C) %s %s\n"), "2013", "Zang MingJie");
	fputs(_("" \
				"This program is free software; you may redistribute it under the terms of\n" \
				"the GNU General Public License.  This program has absolutely no warranty.\n"),
			stream);
}
