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
#include "nautilus-profiler.h"

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gdk-font-extensions.h>
#include <glib.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* These are defined in eazel-tools/profiler/profiler.C */
extern void profile_on (void);
extern void profile_off (void);
extern void profile_reset (void);
extern void profile_dump (const char *file_name, gboolean);

void
nautilus_profiler_bonobo_ui_reset_callback (BonoboUIComponent *component, 
					    gpointer user_data, 
					    const char *verb)
{
	profile_reset ();
}

void
nautilus_profiler_bonobo_ui_start_callback (BonoboUIComponent *component, 
					    gpointer user_data, 
					    const char *verb)
{
	profile_on ();
}

void
nautilus_profiler_bonobo_ui_stop_callback (BonoboUIComponent *component, 
					   gpointer user_data, 
					   const char *verb)
{
	profile_off ();
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

typedef struct
{
	GtkWidget *main_box;
	GtkWidget *text;
	GtkWidget *ver_scroll_bar;
} ScrolledText;

static ScrolledText *
scrolled_text_new (void)
{
	ScrolledText *scrolled_text;

	scrolled_text = g_new (ScrolledText, 1);

	scrolled_text->main_box = gtk_hbox_new (FALSE, 0);

	scrolled_text->text = gtk_text_new (NULL, NULL);

	scrolled_text->ver_scroll_bar = gtk_vscrollbar_new (GTK_TEXT (scrolled_text->text)->vadj);

	gtk_box_pack_start (GTK_BOX (scrolled_text->main_box), scrolled_text->text, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (scrolled_text->main_box), scrolled_text->ver_scroll_bar, FALSE, FALSE, 0);

	gtk_widget_show (scrolled_text->ver_scroll_bar);
	gtk_widget_show (scrolled_text->text);

	return scrolled_text;
}

typedef struct
{
	GtkWidget *window;
	ScrolledText *scrolled_text;
} DumpDialog;

static void
window_delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	g_return_if_fail (GTK_IS_WINDOW (widget));

	gtk_widget_hide (widget);
}

static void
window_print_button_callback (GtkWidget *widget, gpointer callback_data)
{
	DumpDialog *dump_dialog = (DumpDialog *) callback_data;

	g_return_if_fail (dump_dialog != NULL);

	/* Implement me */
	g_assert_not_reached ();
}

static void
window_save_button_callback (GtkWidget *widget, gpointer callback_data)
{
	DumpDialog *dump_dialog = (DumpDialog *) callback_data;

	g_return_if_fail (dump_dialog != NULL);

	/* Implement me */
	g_assert_not_reached ();
}

static DumpDialog *
dump_dialog_new (const char *title)
{
	DumpDialog *dump_dialog;
	GtkWidget *print_button;
	GtkWidget *save_button;
	GtkWidget *main_box;
	GtkWidget *button_box;
	
	dump_dialog = g_new (DumpDialog, 1);
	
	dump_dialog->window = gtk_window_new (GTK_WINDOW_DIALOG);
	eel_gtk_window_set_up_close_accelerator 
		(GTK_WINDOW (dump_dialog->window));
	gtk_signal_connect (GTK_OBJECT (dump_dialog->window),
			    "delete_event", 
			    GTK_SIGNAL_FUNC (window_delete_event),
			    dump_dialog->window);

	gtk_widget_set_usize (dump_dialog->window, 700, 700);

	main_box = gtk_vbox_new (FALSE, 0);
	dump_dialog->scrolled_text = scrolled_text_new ();
	gtk_text_set_editable (GTK_TEXT (dump_dialog->scrolled_text->text), FALSE);
	gtk_text_set_word_wrap (GTK_TEXT (dump_dialog->scrolled_text->text), FALSE);
	gtk_text_set_line_wrap (GTK_TEXT (dump_dialog->scrolled_text->text), FALSE);

	print_button = gtk_button_new_with_label (_("Print"));
	save_button = gtk_button_new_with_label (_("Save"));

	gtk_widget_set_sensitive (print_button, FALSE);
	gtk_widget_set_sensitive (save_button, FALSE);

	gtk_signal_connect (GTK_OBJECT (print_button),
			    "clicked", 
			    GTK_SIGNAL_FUNC (window_print_button_callback),
			    dump_dialog);

	gtk_signal_connect (GTK_OBJECT (save_button),
			    "clicked", 
			    GTK_SIGNAL_FUNC (window_save_button_callback),
			    dump_dialog);

	gtk_container_add (GTK_CONTAINER (dump_dialog->window), main_box);

	button_box = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (button_box), 4);

	gtk_box_pack_start (GTK_BOX (main_box), dump_dialog->scrolled_text->main_box, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (main_box), button_box, FALSE, FALSE, 0);

	gtk_box_pack_end (GTK_BOX (button_box), print_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (button_box), save_button, FALSE, FALSE, 0);

	gtk_widget_show_all (dump_dialog->window);

	return dump_dialog;
}

static void
dump_dialog_show (const char *dump_data, const char *title)
{
	static DumpDialog *dump_dialog = NULL;
	GdkFont *font;

	g_return_if_fail (dump_data != NULL);

	if (dump_dialog == NULL) {
		dump_dialog = dump_dialog_new (_("Profile Dump"));
	}

	gtk_text_forward_delete (GTK_TEXT (dump_dialog->scrolled_text->text),
				 gtk_text_get_length (GTK_TEXT (dump_dialog->scrolled_text->text)));
	
	font = eel_gdk_font_get_fixed ();

	gtk_text_freeze (GTK_TEXT (dump_dialog->scrolled_text->text));

	/* delete existing text in buffer */
	gtk_text_set_point (GTK_TEXT (dump_dialog->scrolled_text->text), 0);
	gtk_text_forward_delete (GTK_TEXT (dump_dialog->scrolled_text->text), 
		gtk_text_get_length(GTK_TEXT (dump_dialog->scrolled_text->text)));
	
	gtk_text_insert (GTK_TEXT (dump_dialog->scrolled_text->text),
			 font,
			 NULL,
			 NULL,
			 dump_data,
			 strlen (dump_data));

	gtk_text_thaw (GTK_TEXT (dump_dialog->scrolled_text->text));

	gdk_font_unref (font);

	if (title != NULL) {
		gtk_window_set_title (GTK_WINDOW (dump_dialog->window), title);
	}

	gtk_widget_show (dump_dialog->window);
}

void
nautilus_profiler_bonobo_ui_report_callback (BonoboUIComponent *component, 
					     gpointer user_data,
					     const char *path)
{
	char *dump_file_name;
	char *uri;

	int dump_size = 0;
	char *dump_contents = NULL;

	GtkWidget *window = NULL;

	g_return_if_fail (component != NULL);
	g_return_if_fail (GTK_IS_WINDOW (user_data));

	dump_file_name = g_strdup ("/tmp/nautilus-profile-log-XXXXXX");

	if (mktemp (dump_file_name) != dump_file_name) {
		g_free (dump_file_name);
		dump_file_name = g_strdup_printf ("/tmp/nautilus-profile-log.%d", getpid ());
	}
	
	window = GTK_WIDGET (user_data);

	widget_set_busy_cursor (window);

	profile_dump (dump_file_name, TRUE);

	uri = g_strdup_printf ("file://%s", dump_file_name);
	
	if (eel_read_entire_file (uri, &dump_size, &dump_contents) == GNOME_VFS_OK) {
		dump_dialog_show (dump_contents, uri);
	}

	widget_clear_busy_cursor (window);

	remove (dump_file_name);

	g_free (dump_file_name);
}
