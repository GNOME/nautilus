/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-desktop-window.c

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

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-desktop-window.h"

#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomeui/gnome-winhints.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-link.h>


struct NautilusDesktopWindowDetails {
	GList *unref_list;
};

static void nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass);
static void nautilus_desktop_window_initialize       (NautilusDesktopWindow      *window);
static void destroy                                  (GtkObject                  *object);
static void realize                                  (GtkWidget                  *widget);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDesktopWindow, nautilus_desktop_window, NAUTILUS_TYPE_WINDOW)

static void
nautilus_desktop_window_initialize_class (NautilusDesktopWindowClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = destroy;
	GTK_WIDGET_CLASS (klass)->realize = realize;
}

static void
nautilus_desktop_window_initialize (NautilusDesktopWindow *window)
{
	window->details = g_new0 (NautilusDesktopWindowDetails, 1);

	/* FIXME bugzilla.eazel.com 1251: 
	 * Although Havoc had this call to set_default_size in
	 * his code, it seems to have no effect for me. But the
	 * set_usize below does seem to work.
	 */
	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_screen_width (),
				     gdk_screen_height ());

	/* These calls seem to have some effect, but it's not clear if
	 * they are the right thing to do.
	 */
	gtk_widget_set_uposition (GTK_WIDGET (window), 0, 0);
	gtk_widget_set_usize (GTK_WIDGET (window),
			      gdk_screen_width (),
			      gdk_screen_height ());

	/* Tell the window manager to never resize this. This is not
	 * known to have any specific beneficial effect with any
	 * particular window manager for the case of the desktop
	 * window, but it doesn't seem to do any harm.
	 */
	gtk_window_set_policy (GTK_WINDOW (window),
			       FALSE, FALSE, FALSE);
}

NautilusDesktopWindow *
nautilus_desktop_window_new (NautilusApplication *application)
{
	NautilusDesktopWindow *window;
	char *desktop_directory_path;
	char *desktop_directory_uri;
	GnomeVFSURI *trash_dir_uri;
	char *trash_dir_uri_text;
	GnomeVFSResult result;

	window = NAUTILUS_DESKTOP_WINDOW
		(gtk_object_new (nautilus_desktop_window_get_type(),
				 "app", application,
				 "app_id", "nautilus",
				 NULL));

	desktop_directory_path = nautilus_get_desktop_directory ();

	/* Create the trash.
	 * We can do this at some other time if we want, but this
	 * might be a good time.
	 */
	result = gnome_vfs_find_directory (NULL, GNOME_VFS_DIRECTORY_KIND_TRASH, 
					   &trash_dir_uri, TRUE, 0777);
	if (result == GNOME_VFS_OK) {
		trash_dir_uri_text = gnome_vfs_uri_to_string (trash_dir_uri,
							      GNOME_VFS_URI_HIDE_NONE);
		gnome_vfs_uri_unref (trash_dir_uri);
		nautilus_link_create (desktop_directory_path,
				      "Trash", "trash-empty.png", 
				      trash_dir_uri_text);
		g_free (trash_dir_uri_text);
	}

	/* Point window at the desktop folder.
	 * Note that nautilus_desktop_window_initialize is too early to do this.
	 */
	desktop_directory_uri = nautilus_get_uri_from_local_path
		(desktop_directory_path);
	g_free (desktop_directory_path);
	nautilus_window_goto_uri (NAUTILUS_WINDOW (window), desktop_directory_uri);
	g_free (desktop_directory_uri);
	
	gtk_widget_show (GTK_WIDGET (window));

	return window;
}

static void
destroy (GtkObject *object)
{
	NautilusDesktopWindow *window;

	window = NAUTILUS_DESKTOP_WINDOW (object);

	nautilus_gtk_object_list_free (window->details->unref_list);
	g_free (window->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
realize (GtkWidget *widget)
{
	NautilusDesktopWindow *window;
	GtkContainer *dock_as_container;
	GList *children, *p;
	GtkWidget *child;

	window = NAUTILUS_DESKTOP_WINDOW (widget);

	/* Hide unused pieces of the GnomeApp.
	 * We don't want a menu bar, toolbars, or status bar on the desktop.
	 * But we don't want to hide the client area!
	 */
	gtk_widget_hide (GNOME_APP (window)->menubar);
	gtk_widget_hide (GNOME_APP (window)->statusbar);
	dock_as_container = GTK_CONTAINER (GNOME_APP (window)->dock);
	children = gtk_container_children (dock_as_container);
	for (p = children; p != NULL; p = p->next) {
		child = p->data;
		
		if (child != gnome_dock_get_client_area (GNOME_DOCK (dock_as_container))) {
			gtk_widget_ref (child);
			window->details->unref_list = g_list_prepend
				(window->details->unref_list, child);
			gtk_container_remove (dock_as_container, child);
		}
	}
	g_list_free (children);

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_PRESS_MASK);
			      
	/* Do the work of realizing. */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	/* FIXME bugzilla.eazel.com 1253: 
	 * Looking at the gnome_win_hints implementation,
	 * it looks like you can call these with an unmapped window,
	 * but when I tried doing it in initialize it didn't work.
	 * We'd like to set these earlier so the window doesn't show
	 * up in front of everything before going to the back.
	 */

	/* Put this window behind all the others. */
	gnome_win_hints_set_layer (widget, WIN_LAYER_DESKTOP);

	/* Make things like the task list ignore this window and make
	 * it clear that it it's at its full size.
	 */
	gnome_win_hints_set_state (widget,
				   WIN_STATE_STICKY
				   | WIN_STATE_MAXIMIZED_VERT
				   | WIN_STATE_MAXIMIZED_HORIZ
				   | WIN_STATE_FIXED_POSITION
				   | WIN_STATE_HIDDEN
				   | WIN_STATE_ARRANGE_IGNORE);

	/* Make sure that focus, and any window lists or task bars also
	 * skip the window.
	 */
	gnome_win_hints_set_hints (widget,
				   WIN_HINTS_SKIP_FOCUS
				   | WIN_HINTS_SKIP_WINLIST
				   | WIN_HINTS_SKIP_TASKBAR);

	/* FIXME bugzilla.eazel.com 1255: 
	 * Should we do a gdk_window_move_resize here, in addition to
	 * the calls in initialize above that set the size?
	 */
	gdk_window_move_resize (widget->window,
				0, 0,
				gdk_screen_width (),
				gdk_screen_height ());

	/* Get rid of the things that window managers add to resize
	 * and otherwise manipulate the window.
	 */
        gdk_window_set_decorations (widget->window, 0);
        gdk_window_set_functions (widget->window, 0);
}
