#ifndef GNOME_DIALOG_UTIL_H
#define GNOME_DIALOG_UTIL_H

#include <gnome-types.h>

/* Ask a yes or no question, and call the callback when it's answered. */
GtkWidget * gnome_question_dialog                 (const gchar * question,
						   GnomeReplyCallback callback,
						   gpointer data);

GtkWidget * gnome_question_dialog_parented        (const gchar * question,
						   GnomeReplyCallback callback,
						   gpointer data,
						   GtkWindow * parent);

#endif
