/* GNOME GUI Library
 * Copyright (C) 1995-1998 Jay Painter
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */
#ifndef __GNOME_DIALOG_H__
#define __GNOME_DIALOG_H__


#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkaccelgroup.h>
#include <libgnome/gnome-defs.h>

#include <stdarg.h>

BEGIN_GNOME_DECLS

#define GNOME_TYPE_DIALOG            (gnome_dialog_get_type ())
#define GNOME_DIALOG(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_DIALOG, GnomeDialog))
#define GNOME_DIALOG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_DIALOG, GnomeDialogClass))
#define GNOME_IS_DIALOG(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_DIALOG))
#define GNOME_IS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_DIALOG))

typedef struct _GnomeDialog        GnomeDialog;
typedef struct _GnomeDialogClass   GnomeDialogClass;

/* The vbox can be accessed directly; if you fool with anything else,
   you're on your own. */
struct _GnomeDialog
{
  GtkWindow window;
  /*< public >*/
  GtkWidget * vbox;
  /*< private >*/
  GtkWidget * action_area; /* A button box, not an hbox */

  GList *buttons;
  GtkAccelGroup * accelerators;

  unsigned int click_closes : 1;
  unsigned int just_hide : 1;

  gpointer padding;
};

struct _GnomeDialogClass
{
  GtkWindowClass parent_class;

  void (* clicked)  (GnomeDialog *dialog, gint button_number);
  gboolean (* close) (GnomeDialog * dialog);
};

/* GnomeDialog creates an action area with the buttons of your choice.
   You should pass the button names (possibly GNOME_STOCK_BUTTON_*) as 
   arguments to gnome_dialog_new(). The buttons are numbered in the 
   order you passed them in, starting at 0. These numbers are used
   in other functions, and passed to the "clicked" callback. */

guint      gnome_dialog_get_type       (void);

/* Arguments: Title and button names, then NULL */
GtkWidget* gnome_dialog_new            (const gchar * title,
					...);
/* Arguments: Title and NULL terminated array of button names. */
GtkWidget* gnome_dialog_newv           (const gchar * title,
					const gchar **buttons);

/* For now this just means the dialog can be centered over
   its parent. */
void       gnome_dialog_set_parent     (GnomeDialog * dialog,
					GtkWindow   * parent);

/* Note: it's better to use GnomeDialog::clicked rather than 
   connecting to a button. These are really here in case
   you're lazy. */
/* Connect to the "clicked" signal of a single button */
void       gnome_dialog_button_connect (GnomeDialog *dialog,
					gint button,
					GtkSignalFunc callback,
					gpointer data);
/* Connect the object to the "clicked" signal of a single button */
void       gnome_dialog_button_connect_object (GnomeDialog *dialog,
					       gint button,
					       GtkSignalFunc callback,
					       GtkObject * obj);

/* Run the dialog, return the button # that was pressed or -1 if none.
   (this sets the dialog modal while it blocks)
 */
gint       gnome_dialog_run	       (GnomeDialog *dialog);
gint       gnome_dialog_run_and_close  (GnomeDialog *dialog);


/* Set the default button. - it will have a little highlight, 
   and pressing return will activate it. */
void       gnome_dialog_set_default    (GnomeDialog *dialog,
					gint         button);

/* Makes the nth button the focused widget in the dialog */
void       gnome_dialog_grab_focus     (GnomeDialog *dialog,
					gint button);
/* Set sensitivity of a button */
void       gnome_dialog_set_sensitive  (GnomeDialog *dialog,
					gint         button,
					gboolean     setting);

/* Set the accelerator for a button. Note that there are two
   default accelerators: "Return" will be the same as clicking
   the default button, and "Escape" will emit delete_event. 
   (Note: neither of these is in the accelerator table,
   Return is a Gtk default and Escape comes from a key press event
   handler.) */
void       gnome_dialog_set_accelerator(GnomeDialog * dialog,
					gint button,
					const guchar accelerator_key,
					guint8       accelerator_mods);

/* Hide and optionally destroy. Destroys by default, use close_hides() 
   to change this. */
void       gnome_dialog_close (GnomeDialog * dialog);

/* Make _close just hide, not destroy. */
void       gnome_dialog_close_hides (GnomeDialog * dialog, 
				     gboolean just_hide);

/* Whether to close after emitting clicked signal - default is
   FALSE. If clicking *any* button should close the dialog, set it to
   TRUE.  */
void       gnome_dialog_set_close      (GnomeDialog * dialog,
					gboolean click_closes);

/* Normally an editable widget will grab "Return" and keep it from 
   activating the dialog's default button. This connects the activate
   signal of the editable to the default button. */
void       gnome_dialog_editable_enters   (GnomeDialog * dialog,
					   GtkEditable * editable);

/* Use of append_buttons is discouraged, it's really
   meant for subclasses. */
void       gnome_dialog_append_buttons  (GnomeDialog * dialog,
					 const gchar * first,
					 ...);
void       gnome_dialog_append_button   (GnomeDialog * dialog,
				         const gchar * name);
void       gnome_dialog_append_buttonsv (GnomeDialog * dialog,
					 const gchar **buttons);

/* Add button with arbitrary text and pixmap. */
void       gnome_dialog_append_button_with_pixmap (GnomeDialog * dialog,
						   const gchar * name,
						   const gchar * pixmap);
void       gnome_dialog_append_buttons_with_pixmaps (GnomeDialog * dialog,
						     const gchar **names,
						     const gchar **pixmaps);

/* Don't use this either; it's for bindings to languages other 
   than C (which makes the varargs kind of lame... feel free to fix)
   You want _new, see above. */
void       gnome_dialog_construct  (GnomeDialog * dialog,
				    const gchar * title,
				    va_list ap);
void       gnome_dialog_constructv (GnomeDialog * dialog,
				    const gchar * title,
				    const gchar **buttons);

END_GNOME_DECLS

#endif /* __GNOME_DIALOG_H__ */
