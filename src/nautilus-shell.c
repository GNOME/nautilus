/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* nautilus-shell.h: Server side of the Nautilus:Shell CORBA object
 * that represents the shell across processes.
 */

#include <config.h>
#include "nautilus-shell.h"

/* FIXME: This is a workaround for ORBit bug where including idl files
 * in other idl files causes trouble.
 */
#include "nautilus-shell-interface.h"
#define nautilus_view_component_H

#include "nautilus-desktop-window.h"
#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>

struct NautilusShellDetails {
	NautilusApplication *application;
};

static void nautilus_shell_initialize       (NautilusShell          *shell);
static void nautilus_shell_initialize_class (NautilusShellClass     *klass);
static void destroy                         (GtkObject              *shell);
static void corba_open_windows              (PortableServer_Servant  servant,
					     const Nautilus_URIList *list,
					     CORBA_Environment      *ev);
static void corba_open_default_window       (PortableServer_Servant  servant,
					     CORBA_Environment      *ev);
static void corba_start_desktop             (PortableServer_Servant  servant,
					     CORBA_Environment      *ev);
static void corba_stop_desktop              (PortableServer_Servant  servant,
					     CORBA_Environment      *ev);
static void corba_quit	                    (PortableServer_Servant  servant,
					     CORBA_Environment      *ev);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusShell, nautilus_shell, BONOBO_OBJECT_TYPE)

static void
nautilus_shell_initialize_class (NautilusShellClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
}

static POA_Nautilus_Shell__epv *
nautilus_shell_get_epv (void)
{
	static POA_Nautilus_Shell__epv epv;
	epv.open_windows = corba_open_windows;
	epv.open_default_window = corba_open_default_window;
	epv.start_desktop = corba_start_desktop;
	epv.stop_desktop = corba_stop_desktop;
	epv.quit = corba_quit;
	return &epv;
}

static POA_Nautilus_Shell__vepv *
nautilus_shell_get_vepv (void)
{
	static POA_Nautilus_Shell__vepv vepv;
	vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv.Nautilus_Shell_epv = nautilus_shell_get_epv ();
	return &vepv;
}

static POA_Nautilus_Shell *
nautilus_shell_create_servant (void)
{
	POA_Nautilus_Shell *servant;
	CORBA_Environment ev;

	servant = (POA_Nautilus_Shell *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = nautilus_shell_get_vepv ();
	CORBA_exception_init (&ev);
	POA_Nautilus_Shell__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_error ("can't initialize Nautilus shell");
	}
	CORBA_exception_free (&ev);

	return servant;
}

static void
nautilus_shell_initialize (NautilusShell *shell)
{
	Nautilus_Shell corba_shell;

	shell->details = g_new0 (NautilusShellDetails, 1);

	corba_shell = bonobo_object_activate_servant
		(BONOBO_OBJECT (shell), nautilus_shell_create_servant ());
	bonobo_object_construct (BONOBO_OBJECT (shell), corba_shell);
}

static void
destroy (GtkObject *object)
{
	NautilusShell *shell;

	shell = NAUTILUS_SHELL (object);
	g_free (shell->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusShell *
nautilus_shell_new (NautilusApplication *application)
{
	NautilusShell *shell;

	shell = gtk_type_new (NAUTILUS_TYPE_SHELL);
	shell->details->application = application;
	return shell;
}

static void
display_caveat (GtkWindow *parent_window)
{
	GtkWidget *dialog;
	GtkWidget *frame;
	GtkWidget *pixmap;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *text;
	char *file_name;

	dialog = gnome_dialog_new (_("Nautilus: caveat"),
				   GNOME_STOCK_BUTTON_OK,
				   NULL);
  	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

  	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
  	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
  	gtk_widget_show (hbox);
  	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), 
  			    hbox,
  			    FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
	
	file_name = nautilus_pixmap_file ("About_Image.png");
	pixmap = gnome_pixmap_new_from_file (file_name);
	g_free (file_name);
	gtk_widget_show (pixmap);
	gtk_container_add (GTK_CONTAINER (frame), pixmap);

  	text = gtk_label_new
		(_("Welcome to Nautilus Preview Release 1."
		   "\n\n"
		   "Thanks for your interest in using Nautilus.  As "
		   "a user of the Preview Release, you try out "
		   "and provide feedback on an early version of Eazel's powerful new desktop "
		   "manager for GNOME."
		   "\n\n"
		   "As with any software under development, you should exercise caution "
		   "when running this program.  Some features are not yet complete or still "
		   "unstable.  You will encounter many more bugs than in the completed version.  "
		   "Eazel can provide no guarantee that it will work properly, or assume liability "
		   "for your use of it.  Use it at your own risk."
		   "\n\n"
		   "We hope that you will enjoy using Nautilus.  "
		   "For more information, please go to http://www.eazel.com."));
    	gtk_label_set_line_wrap (GTK_LABEL (text), TRUE);
	gtk_widget_show (text);
  	gtk_box_pack_start (GTK_BOX (hbox), text, FALSE, FALSE, 0);

  	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), parent_window);

	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
display_caveat_first_time (NautilusShell *shell, NautilusWindow *window)
{
	static gboolean showed_caveat;

	/* Show the "not ready for prime time" dialog after the first
	 * window appears, so it's on top.
	 */
	/* FIXME bugzilla.eazel.com 1256: It's not on top of the
         * windows other than the first one.
	 */
	if (!showed_caveat
	    && g_getenv ("NAUTILUS_NO_CAVEAT_DIALOG") == NULL) {
		gtk_signal_connect (GTK_OBJECT (window), "show",
				    display_caveat, window);
	}
	showed_caveat = TRUE;
}

static void
open_window (NautilusShell *shell, const char *uri)
{
	NautilusWindow *window;

	window = nautilus_application_create_window (shell->details->application);
	if (uri == NULL) {
		nautilus_window_go_home (window);
	} else {
		nautilus_window_goto_uri (window, uri);
	}
	display_caveat_first_time (shell, window);
}

static void
corba_open_windows (PortableServer_Servant servant,
		    const Nautilus_URIList *list,
		    CORBA_Environment *ev)
{
	NautilusShell *shell;
	int i;

	shell = NAUTILUS_SHELL (((BonoboObjectServant *) servant)->bonobo_object);

	/* Open windows at each requested location. */
	for (i = 0; i < list->_length; i++) {
		g_assert (list->_buffer[i] != NULL);
		open_window (shell, list->_buffer[i]);
	}
}

static void
corba_open_default_window (PortableServer_Servant servant,
			   CORBA_Environment *ev)
{
	NautilusShell *shell;

	shell = NAUTILUS_SHELL (((BonoboObjectServant *) servant)->bonobo_object);

	/* Open a window pointing at the default location. */
	open_window (shell, NULL);
}

static void
corba_start_desktop (PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	NautilusShell	      *shell;
	NautilusApplication   *application;

	shell	    = NAUTILUS_SHELL (((BonoboObjectServant *) servant)->bonobo_object);
	application = NAUTILUS_APPLICATION (shell->details->application);
	
	nautilus_application_open_desktop (application);
}

static void
corba_stop_desktop (PortableServer_Servant servant,
		      CORBA_Environment *ev)
{	
	nautilus_application_close_desktop ();
}

static void
corba_quit (PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	if (gtk_main_level () > 0) {
		gtk_main_quit ();
	}
}
