/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
   
   nautilus-self-checks.h: The self-check framework.
 
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

#ifndef NAUTILUS_SELF_CHECKS_H
#define NAUTILUS_SELF_CHECKS_H

#include <glib.h>

#define NAUTILUS_CHECK_RESULT(type, expression, expected_value) \
G_STMT_START { \
	nautilus_before_check (#expression, __FILE__, __LINE__); \
	nautilus_check_##type##_result (expression, expected_value); \
} G_STMT_END

#define NAUTILUS_CHECK_BOOLEAN_RESULT(expression, expected_value) \
	NAUTILUS_CHECK_RESULT(boolean, expression, expected_value)
#define NAUTILUS_CHECK_INTEGER_RESULT(expression, expected_value) \
	NAUTILUS_CHECK_RESULT(integer, expression, expected_value)
#define NAUTILUS_CHECK_STRING_RESULT(expression, expected_value) \
	NAUTILUS_CHECK_RESULT(string, expression, expected_value)

void nautilus_exit_if_self_checks_failed (void);

void nautilus_before_check               (const char *expression,
					  const char *file_name,
					  int         line_number);

void nautilus_check_boolean_result       (gboolean    result,
					  gboolean    expected_value);
void nautilus_check_integer_result       (long        result,
					  long        expected_value);
void nautilus_check_string_result        (char       *result,
					  const char *expected_value);

#define NAUTILUS_SELF_CHECK_FUNCTION_PROTOTYPE(function) \
	void function (void);

#define NAUTILUS_CALL_SELF_CHECK_FUNCTION(function) \
	function ();

#endif /* NAUTILUS_SELF_CHECKS_H */
