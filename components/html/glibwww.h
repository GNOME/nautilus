/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <glib.h>

#ifndef _GLIBWWW_H_
#define _GLIBWWW_H_

/* Defined here so we don't have to include any libwww headers */
typedef struct _HTRequest GWWWRequest;

/* If status < 0, an error occured */
typedef void (*GWWWLoadToFileFunc) (const gchar *url, const gchar *file,
				    int status, gpointer user_data);
typedef void (*GWWWLoadToMemFunc) (const gchar *url, const gchar *buffer,
				   int size, int status, gpointer user_data);

/* Initialise enough of libwww for doing http/ftp downloads with
 * authentication, redirection and proxy support.
 */
void glibwww_init    (const gchar *appName, const gchar *appVersion);
void glibwww_cleanup (void); /* not necessary -- registered with g_atexit() */

typedef enum {
	DLG_PROGRESS = 1<<0,
	DLG_CONFIRM = 1<<1,
	DLG_PROMPT = 1<<2,
	DLG_AUTH = 1<<3
} GLibWWWDialogType;
/* register the GUI dialogs for glibwww.  This will take care of all the
 * authentication and progress bar stuff for the application. */
void glibwww_register_gnome_dialogs (GLibWWWDialogType type);

/* Setup proxies as needed -- use the http://proxyhost:port/ notation */
void glibwww_add_proxy   (const gchar *protocol, const gchar *proxy);
void glibwww_add_noproxy (const gchar *host);

/* Load a url to a file or to memory.  The callback will be invoked
 * exactly once. */
GWWWRequest *glibwww_load_to_file (const gchar *url, const gchar *file,
				   GWWWLoadToFileFunc callback,
				   gpointer user_data);

GWWWRequest *glibwww_load_to_mem (const gchar *url,
				  GWWWLoadToMemFunc callback,
				  gpointer user_data);

/* Abort a currently running download */
gboolean glibwww_abort_request(GWWWRequest *request);

/* Get the progress of the currently running request.  nread or total may
 * return a negative result if it can't determine how far along things are. */
void glibwww_request_progress (GWWWRequest *request,
			       glong *nread, glong *total);


/* This is called by glibwww_init, but may be useful if you only want to
 * use the callbacks provided by glibwww for embedding libwww into the
 * glib main loop */
void glibwww_register_callbacks (void);

#endif
