/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.c - implementation for window that lets user choose 
                                a program from a list

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
#include "nautilus-program-chooser.h"

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkclist.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvbox.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>

#define PROGRAM_LIST_NAME_COLUMN	 0
#define PROGRAM_LIST_COLUMN_COUNT	 1

/* gtk_window_set_default_width (and some other functions) use a
 * magic undocumented number of -2 to mean "ignore this parameter".
 */
#define NO_DEFAULT_MAGIC_NUMBER		-2

/* Scrolling list has no idea how tall to make itself. Its
 * "natural height" is just enough to draw the scroll bar controls.
 * Hardwire an initial window size here, but let user resize
 * bigger or smaller.
 */
#define PROGRAM_CHOOSER_DEFAULT_HEIGHT	 272

static void
populate_program_list (NautilusProgramChooserType type,
		       NautilusFile *file,
		       GtkCList *clist)
{
	int i;
	char **text;


	if (type == NAUTILUS_PROGRAM_CHOOSER_COMPONENTS) {
		for (i = 0; i < 10; ++i) {
			/* One extra slot so it's NULL-terminated */
			text = g_new0 (char *, PROGRAM_LIST_COLUMN_COUNT+1);
			text[PROGRAM_LIST_NAME_COLUMN] = g_strdup_printf ("Viewer %d", i+1); 
			gtk_clist_append (clist, text);
			
			g_strfreev (text);
		}
	} else {
		g_assert (type == NAUTILUS_PROGRAM_CHOOSER_APPLICATIONS);
		for (i = 0; i < 10; ++i) {
			/* One extra slot so it's NULL-terminated */
			text = g_new0 (char *, PROGRAM_LIST_COLUMN_COUNT+1);
			text[PROGRAM_LIST_NAME_COLUMN] = g_strdup_printf ("Application %d", i+1); 
			gtk_clist_append (clist, text);
			
			g_strfreev (text);
		}
	}

}		       

GnomeDialog *
nautilus_program_chooser_new (NautilusProgramChooserType type,
			      NautilusFile *file)
{
	GtkWidget *window;
	GtkWidget *prompt_label;
	GtkWidget *list_scroller, *clist;
	GtkWidget *remember_for_type, *remember_for_file;
	char *file_name, *prompt;
	const char *title;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	file_name = nautilus_file_get_name (file);

	switch (type) {
	case NAUTILUS_PROGRAM_CHOOSER_APPLICATIONS:
		title = _("Nautilus: Choose an application");
		prompt = g_strdup_printf (_("Choose an application with which to open \"%s\"."), file_name);
		break;
	case NAUTILUS_PROGRAM_CHOOSER_COMPONENTS:
	default:
		title = _("Nautilus: Choose a viewer");
		prompt = g_strdup_printf (_("Choose a viewer with which to display \"%s\"."), file_name);
		break;
	}
	g_free (file_name);
	
	window = gnome_dialog_new (title, 
				   GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL, 
				   NULL);
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     NO_DEFAULT_MAGIC_NUMBER,
				     PROGRAM_CHOOSER_DEFAULT_HEIGHT);

	/* Prompt at top of dialog. */
	prompt_label = gtk_label_new (prompt);
	gtk_widget_show (prompt_label);
  	g_free (prompt);

  	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), 
  			    prompt_label,
  			    FALSE, FALSE, 0);

	/* Scrolling list to hold choices. */
	list_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (list_scroller);
	gtk_box_pack_start_defaults (GTK_BOX (GNOME_DIALOG (window)->vbox), list_scroller);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroller), 
					GTK_POLICY_NEVER, 
					GTK_POLICY_AUTOMATIC);	  
	  
	clist = gtk_clist_new (PROGRAM_LIST_COLUMN_COUNT);
	gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
	populate_program_list (type, file, GTK_CLIST (clist));
	gtk_widget_show (clist);
	gtk_container_add (GTK_CONTAINER (list_scroller), clist);
	gtk_clist_column_titles_hide (GTK_CLIST (clist));

	/* Checkboxes to remember these for later. */
	remember_for_type = gtk_check_button_new_with_label (_("Always offer this program with this document type."));
	gtk_widget_show (remember_for_type);
  	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), 
  			    remember_for_type,
  			    FALSE, FALSE, 0);

	remember_for_file = gtk_check_button_new_with_label (_("Always offer this program with this document."));
	gtk_widget_show (remember_for_file);
  	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (window)->vbox), 
  			    remember_for_file,
  			    FALSE, FALSE, 0);

	/* Buttons close this dialog. */
  	gnome_dialog_set_close (GNOME_DIALOG (window), TRUE);

  	/* Make OK button the default. */
  	gnome_dialog_set_default (GNOME_DIALOG (window), GNOME_OK);

  	return GNOME_DIALOG (window);
}
