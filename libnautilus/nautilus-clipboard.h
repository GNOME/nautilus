/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

#ifndef NAUTILUS_CLIPBOARD_H
#define NAUTILUS_CLIPBOARD_H

#include <gtk/gtkeditable.h>
#include <bonobo/bonobo-control.h>

/* Components should use this, which includes menu merging. */
void nautilus_clipboard_set_up_editable_from_bonobo_control         (GtkEditable     *target,
								     BonoboControl   *control);

/* Local editable widgets should set up clipboard capabilities using this function.
   They can obtain their local ui container using the function
   nautilus_window_get_bonobo_ui_container */
void nautilus_clipboard_set_up_editable_from_bonobo_ui_container   (GtkEditable     *target,
								    Bonobo_UIContainer container);

#endif /* NAUTILUS_CLIPBOARD_H */
