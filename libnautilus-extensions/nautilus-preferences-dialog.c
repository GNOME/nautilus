/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-dialog.c - Implementation for preferences dialog.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-preferences-dialog.h"
#include "nautilus-gtk-macros.h"

/* #include "caption-table.h" */

#include <libgnomeui/gnome-stock.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include <gnome.h>

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

struct _NautilusPreferencesDialogDetails
{
	GtkWidget	*prefs_box;
};

static const gchar * stock_buttons[] =
{
	"OK",
	NULL
};

static const gint UNKNOWN_BUTTON = -1;
static const gint OK_BUTTON = 0;
static const gint DEFAULT_BUTTON = 0;
static const guint DEFAULT_BORDER_WIDTH = 0;

static const guint PREFS_DIALOG_DEFAULT_WIDTH = 500;
static const guint PREFS_DIALOG_DEFAULT_HEIGHT = 300;

enum 
{
	COMMAND_ROW = 0,
	USERNAME_ROW,
	PASSWORD_ROW
};

/* NautilusPreferencesDialogClass methods */
static void nautilus_preferences_dialog_initialize_class (NautilusPreferencesDialogClass *klass);
static void nautilus_preferences_dialog_initialize       (NautilusPreferencesDialog      *prefs_dialog);

/* GtkObjectClass methods */
static void nautilus_preferences_dialog_destroy          (GtkObject                *object);
static void dialog_clicked                         (GtkWidget                *widget,
						    gint                      n,
						    gpointer                  data);
static void dialog_show                            (GtkWidget                *widget,
						    gpointer                  data);
static void dialog_destroy                         (GtkWidget                *widget,
						    gpointer                  data);

/* Misc private stuff */
static void nautilus_preferences_dialog_construct        (NautilusPreferencesDialog      *prefs_dialog,
						    const gchar              *dialog_title);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesDialog, 
				   nautilus_preferences_dialog, 
				   gnome_dialog_get_type ())

/*
 * NautilusPreferencesDialogClass methods
 */
static void
nautilus_preferences_dialog_initialize_class (NautilusPreferencesDialogClass * klass)
{
	GtkObjectClass * object_class;
	GtkWidgetClass * widget_class;
	
	object_class = GTK_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

 	parent_class = gtk_type_class(gnome_dialog_get_type());

	/* GtkObjectClass */
	object_class->destroy = nautilus_preferences_dialog_destroy;
}

static void
nautilus_preferences_dialog_initialize (NautilusPreferencesDialog * prefs_dialog)
{
	prefs_dialog->details = g_new (NautilusPreferencesDialogDetails, 1);

	prefs_dialog->details->prefs_box = NULL;
}

static void
dialog_clicked(GtkWidget * widget, gint n, gpointer data)
{
	NautilusPreferencesDialog * prefs_dialog = (NautilusPreferencesDialog *) data;

	g_assert(prefs_dialog);

	gtk_widget_hide(GTK_WIDGET(prefs_dialog));
}

static void
dialog_show(GtkWidget * widget, gpointer data)
{
	NautilusPreferencesDialog * prefs_dialog = (NautilusPreferencesDialog *) data;

	g_assert(prefs_dialog);
}

static void
dialog_destroy(GtkWidget * widget, gpointer data)
{
	NautilusPreferencesDialog * prefs_dialog = (NautilusPreferencesDialog *) data;

	g_assert(prefs_dialog);
}

static void
nautilus_preferences_dialog_construct (NautilusPreferencesDialog *prefs_dialog,
				 const gchar	     *dialog_title)
{
	GnomeDialog *gnome_dialog;

	g_assert (prefs_dialog != NULL);
	g_assert (prefs_dialog->details != NULL);

	g_assert (prefs_dialog->details->prefs_box == NULL);

	gnome_dialog = GNOME_DIALOG (prefs_dialog);

	gnome_dialog_constructv (gnome_dialog, dialog_title, stock_buttons);

	/* Setup the dialog */
	gtk_window_set_policy (GTK_WINDOW (prefs_dialog), 
			       FALSE,			 /* allow_shrink */
			       TRUE,			 /* allow_grow */
			       FALSE);			 /* auto_shrink */

 	gtk_widget_set_usize (GTK_WIDGET (prefs_dialog), 
			      PREFS_DIALOG_DEFAULT_WIDTH, 
			      PREFS_DIALOG_DEFAULT_HEIGHT);

	/* Doesnt work in enlightenment or sawmill */
#if 0
	/* This is supposed to setup the window manager functions */
	gdk_window_set_functions (GTK_WIDGET (prefs_dialog)->window, GDK_FUNC_MOVE | GDK_FUNC_RESIZE);
#endif

 	gtk_window_set_position (GTK_WINDOW (prefs_dialog), GTK_WIN_POS_CENTER);

 	gtk_container_set_border_width (GTK_CONTAINER(prefs_dialog), 
					DEFAULT_BORDER_WIDTH);
	
	gnome_dialog_set_default (GNOME_DIALOG(prefs_dialog), 
				  DEFAULT_BUTTON);

	gtk_signal_connect (GTK_OBJECT (prefs_dialog),
			    "clicked",
			    GTK_SIGNAL_FUNC (dialog_clicked),
			    (gpointer) prefs_dialog);

	gtk_signal_connect (GTK_OBJECT (prefs_dialog),
			    "show",
			    GTK_SIGNAL_FUNC(dialog_show),
			    (gpointer) prefs_dialog);
	
	gtk_signal_connect (GTK_OBJECT (prefs_dialog),
			    "destroy",
			    GTK_SIGNAL_FUNC (dialog_destroy),
			    (gpointer) prefs_dialog);

	/* Configure the GNOME_DIALOG's vbox */
 	g_assert (gnome_dialog->vbox);

	prefs_dialog->details->prefs_box = nautilus_preferences_box_new (_("Prefs Box"));
	
	gtk_box_set_spacing (GTK_BOX (gnome_dialog->vbox), 10);
	
	gtk_box_pack_start (GTK_BOX (gnome_dialog->vbox),
			    prefs_dialog->details->prefs_box,
			    TRUE,	/* expand */
			    TRUE,	/* fill */
			    0);		/* padding */

	gtk_widget_show (prefs_dialog->details->prefs_box);
}

GtkWidget*
nautilus_preferences_dialog_new (const gchar *dialog_title)
{
	NautilusPreferencesDialog *prefs_dialog;

	prefs_dialog = gtk_type_new (nautilus_preferences_dialog_get_type ());

	nautilus_preferences_dialog_construct (prefs_dialog, dialog_title);

	return GTK_WIDGET (prefs_dialog);
}

/* GtkObjectClass methods */
static void
nautilus_preferences_dialog_destroy(GtkObject* object)
{
	NautilusPreferencesDialog * prefs_dialog;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFS_DIALOG (object));
	
	prefs_dialog = NAUTILUS_PREFERENCES_DIALOG(object);

	g_free (prefs_dialog->details);

	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkWidget*
nautilus_preferences_dialog_get_prefs_box (NautilusPreferencesDialog *prefs_dialog)
{
	g_return_val_if_fail (prefs_dialog != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_PREFS_DIALOG (prefs_dialog), NULL);

	return prefs_dialog->details->prefs_box;
}
