#ifndef GNOME_TYPES_H
#define GNOME_TYPES_H
/****
  Gnome-wide useful types.
  ****/


/* string is a g_malloc'd string which should be freed, or NULL if the
   user cancelled. */
typedef void (* GnomeStringCallback)(gchar * string, gpointer data); 

/* See gnome-uidefs for the Yes/No Ok/Cancel defines which can be
   "reply" */
typedef void (* GnomeReplyCallback)(gint reply, gpointer data);

/* Do something never, only when the user wants, or always. */
typedef enum {
  GNOME_PREFERENCES_NEVER,
  GNOME_PREFERENCES_USER,
  GNOME_PREFERENCES_ALWAYS
} GnomePreferencesType;


#endif
