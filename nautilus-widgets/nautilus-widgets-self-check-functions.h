/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-widgets-self-check-functions.h: Wrapper and prototypes for all
   self-check functions in nautilus-widgets.
 
   Copyright (C) 2000 Eazel, Inc.
  
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
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <libnautilus-extensions/nautilus-self-checks.h>

/* See libnautilus-extensions for information on self tests */
void nautilus_widgets_run_self_checks (void);

#define NAUTILUS_WIDGETS_FOR_EACH_SELF_CHECK_FUNCTION(macro) \
	macro (nautilus_widgets_self_check_preference)
/* Add new self-check functions to the list above this line. */

/* Generate prototypes for all the functions. */
NAUTILUS_WIDGETS_FOR_EACH_SELF_CHECK_FUNCTION (NAUTILUS_SELF_CHECK_FUNCTION_PROTOTYPE)
