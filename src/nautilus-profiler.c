/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Author: Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-profiler.c: Nautilus profiler hooks and reporting.
 */

#include <config.h>
#include <gtk/gtkdialog.h>

#include <glib.h>

#include "nautilus-profiler.h"

#include <stdlib.h>
#include <unistd.h>

/* These are defined in eazel-tools/profiler/profiler.C */
extern void profile_on (void);
extern void profile_off (void);
extern void profile_reset (void);
extern void profile_dump (const char *file_name);

void
nautilus_profiler_bonobo_ui_reset_callback (BonoboUIHandler *ui_handler, 
					    gpointer user_data,
					    const char *path)
{
	profile_reset ();
}

void
nautilus_profiler_bonobo_ui_start_callback (BonoboUIHandler *ui_handler, 
					    gpointer user_data,
					    const char *path)
{
	profile_on ();
}
void
nautilus_profiler_bonobo_ui_stop_callback (BonoboUIHandler *ui_handler, 
					   gpointer user_data,
					   const char *path)
{
	profile_off ();
}

static GtkWidget *
widget_find_ancestor_window (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	
	while (widget && !GTK_IS_WINDOW (widget)) {
		widget = widget->parent;
	}

	return widget;
}

static GtkWidget *
ui_handler_find_ancestor_window (BonoboUIHandler *ui_handler)
{
	GtkWidget *something;

	g_return_val_if_fail (ui_handler != NULL, NULL);
	
	something = bonobo_ui_handler_get_statusbar (ui_handler);
	
	if (!something) {
		something = bonobo_ui_handler_get_menubar (ui_handler);
	}

	if (!something) {
		return NULL;
	}
	    
	return widget_find_ancestor_window (something);
}

static void
widget_set_busy_cursor (GtkWidget *widget)
{
	GdkCursor *cursor;

        g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (widget)));
	
	cursor = gdk_cursor_new (GDK_WATCH);

	gdk_window_set_cursor (GTK_WIDGET (widget)->window, cursor);

	gdk_flush ();

	gdk_cursor_destroy (cursor);
}

static void
widget_clear_busy_cursor (GtkWidget *widget)
{
        g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (widget)));

	gdk_window_set_cursor (GTK_WIDGET (widget)->window, NULL);

	gdk_flush ();
}

void
nautilus_profiler_bonobo_ui_report_callback (BonoboUIHandler *ui_handler, 
					     gpointer user_data,
					     const char *path)
{
	char *dump_file_name;

	GtkWidget *window = NULL;

	dump_file_name = g_strdup ("nautilus-profile-log-XXXXXX");

	if (mktemp (dump_file_name) != dump_file_name) {
		g_free (dump_file_name);
		dump_file_name = g_strdup_printf ("/tmp/nautilus-profile-log.%d", getpid ());
	}

	window = ui_handler_find_ancestor_window (ui_handler);

	widget_set_busy_cursor (window);

	profile_dump (dump_file_name);

	/* Do something interesting with the data */

	widget_clear_busy_cursor (window);

	g_free (dump_file_name);
}
