/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* nautilus-link-set-window.h: window for configuring link sets
 */

#ifndef NAUTILUS_LINK_SET_WINDOW_H
#define NAUTILUS_LINK_SET_WINDOW_H

#include <glib.h>
#include <libnautilus-private/nautilus-link-set.h>
#include <gtk/gtkwindow.h>

GtkWindow	*nautilus_link_set_configure_window (const char *directory_path, 
							GtkWindow *window);
GtkWindow 	*nautilus_link_set_toggle_configure_window (const char *directory_path, 
								GtkWindow *window_to_update);

#endif /* NAUTILUS_LINK_SET_H */
