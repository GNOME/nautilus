/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bonobo-extensions.c - implementation of new functions that conceptually
                                  belong in bonobo. Perhaps some of these will be
                                  actually rolled into bonobo someday.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-bonobo-extensions.h"

/**
 * nautilus_bonobo_ui_handler_menu_toggle_appearance
 * 
 * Changes a toggleable bonobo menu item's apparent state
 * without invoking its callback.
 * 
 * @uih: The BonoboUIHandler for this menu item.
 * @path: The standard bonobo-style path specifier for this menu item.
 * @new_value: TRUE if item should appear checked (on), FALSE otherwise.
 */
void 
nautilus_bonobo_ui_handler_menu_set_toggle_appearance (BonoboUIHandler *uih,
				      	   		    const char *path,
				      	   		    gboolean new_value)
{
	BonoboUIHandlerCallback saved_callback;
	gpointer saved_callback_data;
	GDestroyNotify saved_destroy_notify;

	/* Temporarily clear out callback and data so when we
	 * set the toggle state the callback isn't called. 
	 */
	bonobo_ui_handler_menu_get_callback (uih, path, &saved_callback,
					     &saved_callback_data, &saved_destroy_notify);
	bonobo_ui_handler_menu_remove_callback_no_notify (uih, path);
        bonobo_ui_handler_menu_set_toggle_state (uih, path, new_value);
	bonobo_ui_handler_menu_set_callback (uih, path, saved_callback,
					     saved_callback_data, saved_destroy_notify);		
}
