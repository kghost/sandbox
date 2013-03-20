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

#include <stdlib.h>
#include <stdio.h>

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)

#define x(expr) if (!(expr)) { \
	perror(__FILE__ " " S__LINE__ " " #expr); \
	exit(EXIT_FAILURE); \
} \

#define w(expr) ({ \
	int ret; \
	if ((ret = (expr)) < 0) { \
		perror(__FILE__ " " S__LINE__ " " #expr); \
		exit(EXIT_FAILURE); \
	} \
	ret; \
})

#define p(expr) ({ \
	typeof(expr) ret; \
	if ((ret = (expr)) == NULL) { \
		perror(__FILE__ " " S__LINE__ " " #expr); \
		exit(EXIT_FAILURE); \
	} \
	ret; \
})
