/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
   
   nautilus-self-checks.c: The self-check framework.
 
   Copyright (C) 1999 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

#include "nautilus-self-checks.h"

#include <stdio.h>
#include <stdlib.h>

static gboolean failed;

static const char *current_expression;
static const char *current_file_name;
static int current_line_number;

void nautilus_exit_if_self_checks_failed (void)
{
	if (!failed) {
		return;
	}

	printf ("\n");

	exit (EXIT_FAILURE);
}

static void
nautilus_report_check_failure (char *result, char *expected)
{
	if (!failed) {
		printf ("\n");
	}

	printf ("FAIL: check failed in %s, line %d\n", current_file_name, current_line_number);
	printf ("      evaluated: %s\n", current_expression);
	printf ("       expected: %s\n", expected == NULL ? "NULL" : expected);
	printf ("            got: %s\n", result == NULL ? "NULL" : result);
	
	failed = TRUE;

	g_free (result);
	g_free (expected);
}

static char *
nautilus_strdup_boolean (gboolean boolean)
{
	if (boolean == FALSE) {
		return g_strdup ("FALSE");
	}
	if (boolean == TRUE) {
		return g_strdup ("TRUE");
	}
	return g_strdup_printf ("gboolean(%d)", boolean);
}

void
nautilus_before_check (const char *expression,
		       const char *file_name,
		       int line_number)
{
	current_expression = expression;
	current_file_name = file_name;
	current_line_number = line_number;
}

static void
nautilus_after_check (void)
{
	/* It would be good to check here if there was a memory leak. */
}

void
nautilus_check_boolean_result (gboolean result, gboolean expected)
{
	if (result != expected) {
		nautilus_report_check_failure (nautilus_strdup_boolean (result),
					       nautilus_strdup_boolean (expected));
	}
	nautilus_after_check ();
}

void
nautilus_check_integer_result (long result, long expected)
{
	if (result != expected) {
		nautilus_report_check_failure (g_strdup_printf ("%ld", result),
					       g_strdup_printf ("%ld", expected));
	}
	nautilus_after_check ();
}

void
nautilus_check_string_result (char *result, const char *expected)
{
	gboolean match;
	
	/* Stricter than nautilus_strcmp.
	 * NULL does not match "" in this test.
	 */
	if (expected == NULL) {
		match = result == NULL;
	} else {
		match = result != NULL && strcmp (result, expected) == 0;
	}

	if (!match) {
		nautilus_report_check_failure (result, g_strdup (expected));
	} else {
		g_free (result);
	}
	nautilus_after_check ();
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
