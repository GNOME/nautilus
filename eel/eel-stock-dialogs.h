
/* eel-stock-dialogs.h: Various standard dialogs for Eel.

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
   see <http://www.gnu.org/licenses/>.

   Authors: Darin Adler <darin@eazel.com>
*/

#pragma once

#include <gtk/gtk.h>

typedef void (* EelCancelCallback) (gpointer callback_data);

/* Dialog for cancelling something that normally is fast enough not to need a dialog. */
void       eel_timed_wait_start               (EelCancelCallback  cancel_callback,
					       gpointer           callback_data,
					       const char        *wait_message,
					       GtkWindow         *parent_window);
void       eel_timed_wait_start_with_duration (int                duration,
					       EelCancelCallback  cancel_callback,
					       gpointer           callback_data,
					       const char        *wait_message,
					       GtkWindow         *parent_window);
void       eel_timed_wait_stop                (EelCancelCallback  cancel_callback,
					       gpointer           callback_data);
