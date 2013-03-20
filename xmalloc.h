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

/* Prototypes for functions defined in xmalloc.c  */

void *xmalloc (size_t n);
void *xcalloc (size_t n, size_t s);
void *xrealloc (void *p, size_t n);
char *xstrdup (char *p);
