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

#include <search.h>
#include <assert.h>

#include "xmalloc.h"
#include "macros.h"

struct item {
	int key;
	int value;
};

struct ts {
	void *tree;
};

struct ts *ts_new(void) {
	struct ts *ts = p(xmalloc(sizeof(ts)));
	ts->tree = NULL;
	return ts;
}

void ts_free(struct ts *ts) {
	tdestroy(ts->tree, free);
	free(ts);
}

int compare_item(const void *a, const void *b) {
	struct item *ia = (struct item *)a;
	struct item *ib = (struct item *)b;
	return ia->key - ib->key;
}

void ts_insert(struct ts *ts, int key, int value) {
	struct item *item = p(xmalloc(sizeof(item)));
	item->key = key;
	item->value = value;
	struct item *new_item = *(struct item **)p(tsearch(item, &ts->tree, compare_item));
	assert(new_item == item);
}

void ts_remove(struct ts *ts, int key) {
	struct item item = { .key = key };
	struct item *found = *(struct item **)p(tfind(&item, &ts->tree, compare_item));
	x(tdelete(found, &ts->tree, compare_item) != NULL);
	free(found);
}

static __thread int search_value;
static __thread const struct item *search_by_value;
static void visit(const void *nodep, VISIT value, int level) {
	if (value == preorder || value == leaf) {
		const struct item *item = *(const struct item **)nodep;
		if (item->value == search_value)
			search_by_value = item;
	}
}

int ts_find_value(struct ts *ts, int value) {
	search_value = value;
	search_by_value = NULL;
	twalk(ts->tree, visit);
	if (search_by_value != NULL)
		return search_by_value->key;
	else
		return -1;
}

int ts_search(struct ts *ts, int key) {
	struct item item = { .key = key };
	struct item **found = (struct item **)tfind(&item, &ts->tree, compare_item);
	if (found != NULL)
		return (*found)->value;
	else
		return -1;
}

