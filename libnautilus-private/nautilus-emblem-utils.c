/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-emblem-utils.c: Utilities for handling emblems
 
   Copyright (C) 2002 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>

#include <utime.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "nautilus-icon-factory.h"
#include <eel/eel-string.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-icon-theme.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "nautilus-emblem-utils.h"

#define EMBLEM_NAME_TRASH   "emblem-trash"
#define EMBLEM_NAME_SYMLINK "emblem-symbolic-link"
#define EMBLEM_NAME_NOREAD  "emblem-noread"
#define EMBLEM_NAME_NOWRITE "emblem-nowrite"
#define EMBLEM_NAME_NOTE    "emblem-note"
#define EMBLEM_NAME_DESKTOP "emblem-desktop"

GList *
nautilus_emblem_list_availible (void)
{
	GnomeIconTheme *icon_theme;
	GList *list;
	
	icon_theme = nautilus_icon_factory_get_icon_theme ();
	list = gnome_icon_theme_list_icons (icon_theme, "Emblems");
	g_object_unref (icon_theme);
	return list;
}

void
nautilus_emblem_refresh_list (void)
{
	GnomeIconTheme *icon_theme;
	
	icon_theme = nautilus_icon_factory_get_icon_theme ();
	gnome_icon_theme_rescan_if_needed (icon_theme);
	g_object_unref (icon_theme);
}

char *
nautilus_emblem_get_icon_name_from_keyword (const char *keyword)
{
	return g_strconcat ("emblem-", keyword, NULL);
}


/* check for reserved keywords */
static gboolean
is_reserved_keyword (const char *keyword)
{
	GList *availible;
	char *icon_name;
	gboolean result;

	/* check intrinsic emblems */
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_TRASH) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_CANT_READ) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_SYMBOLIC_LINK) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_NOTE) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_DESKTOP) == 0) {
		return TRUE;
	}

	availible = nautilus_emblem_list_availible ();
	icon_name = nautilus_emblem_get_icon_name_from_keyword (keyword);
	/* see if the keyword already exists */
	result = g_list_find_custom (availible,
				     (char *) icon_name,
				     (GCompareFunc) eel_strcasecmp) != NULL;
	eel_g_list_free_deep (availible);	
	g_free (icon_name);
	return result;
}

gboolean
nautilus_emblem_should_show_in_list (const char *emblem)
{
	if (strcmp (emblem, EMBLEM_NAME_TRASH) == 0) {
		return FALSE;
	}
	if (strcmp (emblem, EMBLEM_NAME_SYMLINK) == 0) {
		return FALSE;
	}
	if (strcmp (emblem, EMBLEM_NAME_NOREAD) == 0) {
		return FALSE;
	}
	if (strcmp (emblem, EMBLEM_NAME_NOWRITE) == 0) {
		return FALSE;
	}
	if (strcmp (emblem, EMBLEM_NAME_NOTE) == 0) {
		return FALSE;
	}
	if (strcmp (emblem, EMBLEM_NAME_DESKTOP) == 0) {
		return FALSE;
	}

	return TRUE;
}

char *
nautilus_emblem_get_keyword_from_icon_name (const char *emblem)
{
	g_return_val_if_fail (emblem != NULL, NULL);

	if (eel_str_has_prefix (emblem, "emblem-")) {
		return g_strdup (&emblem[7]);
	} else {
		return g_strdup (emblem);
	}
}

GdkPixbuf *
nautilus_emblem_load_pixbuf_for_emblem (const char *uri)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled;

	pixbuf = eel_gdk_pixbuf_load (uri);

	g_return_val_if_fail (pixbuf != NULL, NULL);

	scaled = eel_gdk_pixbuf_scale_down_to_fit (pixbuf,
						   NAUTILUS_ICON_SIZE_STANDARD,
						   NAUTILUS_ICON_SIZE_STANDARD);
	g_object_unref (G_OBJECT (pixbuf));

	return scaled;
}

/* utility to make sure the passed-in keyword only contains alphanumeric characters */
static gboolean
emblem_keyword_valid (const char *keyword)
{
	const char *p;
	gunichar c;
	
	for (p = keyword; *p; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);

		if (!g_unichar_isalnum (c) &&
		    !g_unichar_isspace (c)) {
			return FALSE;
		}
	}
	
	return TRUE;
}

gboolean
nautilus_emblem_verify_keyword (GtkWindow *parent_window,
				const char *keyword,
				const char *display_name)
{
	if (keyword == NULL || strlen (keyword) == 0) {
		eel_show_error_dialog (_("Sorry, but you must specify a non-blank keyword for the new emblem."), 
				       _("Couldn't install emblem"), GTK_WINDOW (parent_window));
		return FALSE;
	} else if (!emblem_keyword_valid (keyword)) {
		eel_show_error_dialog (_("Sorry, but emblem keywords can only contain letters, spaces and numbers."), 
				       _("Couldn't install emblem"), GTK_WINDOW (parent_window));
		return FALSE;
	} else if (is_reserved_keyword (keyword)) {
		char *error_string;

		/* this really should never happen, as a user has no idea
		 * what a keyword is, and people should be passing a unique
		 * keyword to us anyway
		 */
		error_string = g_strdup_printf (_("Sorry, but there is already an emblem named \"%s\".  Please choose a different name for it."), display_name);
		eel_show_error_dialog (error_string, 
				       _("Couldn't install emblem"), GTK_WINDOW (parent_window));
		g_free (error_string);
		return FALSE;
	} 

	return TRUE;
}

void
nautilus_emblem_install_custom_emblem (GdkPixbuf *pixbuf,
				       const char *keyword,
				       const char *display_name,
				       GtkWindow *parent_window)
{
	GnomeVFSURI *vfs_uri;
	char *path, *dir, *stat_dir;
	FILE *file;
	struct stat stat_buf;
	struct utimbuf ubuf;
	
	g_return_if_fail (pixbuf != NULL);

	if (!nautilus_emblem_verify_keyword (parent_window, keyword, display_name)) {
		return;
	}

	dir = g_strdup_printf ("%s/.icons/gnome/48x48/emblems",
			       g_get_home_dir ());
	stat_dir = g_strdup_printf ("%s/.icons/gnome",
				    g_get_home_dir ());

	vfs_uri = gnome_vfs_uri_new (dir);

	g_return_if_fail (vfs_uri != NULL);
	
	eel_make_directory_and_parents (vfs_uri, 0755);
	gnome_vfs_uri_unref (vfs_uri);
	
	path = g_strdup_printf ("%s/emblem-%s.png", dir, keyword);

	/* save the image */
	if (eel_gdk_pixbuf_save_to_file (pixbuf, path) != TRUE) {
		eel_show_error_dialog (_("Sorry, unable to save custom emblem."), 
				       _("Couldn't install emblem"), GTK_WINDOW (parent_window));
		g_free (dir);
		g_free (stat_dir);
		g_free (path);
		return;
	}

	g_free (path);

	if (display_name != NULL) {
		path = g_strdup_printf ("%s/emblem-%s.icon", dir, keyword);
		file = fopen (path, "w+");
		
		if (file == NULL) {
			eel_show_error_dialog (_("Sorry, unable to save custom emblem name."), 
					       _("Couldn't install emblem"), GTK_WINDOW (parent_window));
			g_free (stat_dir);
			g_free (dir);
			return;
		}
		
		/* write the icon description */
		fprintf (file, "\n[Icon Data]\n\nDisplayName=%s\n", display_name);
		fflush (file);
		fclose (file);
	}

	/* Touch the toplevel dir */
	if (stat (stat_dir, &stat_buf) == 0) {
		ubuf.actime = stat_buf.st_atime;
		ubuf.modtime = time (NULL);
		utime (stat_dir, &ubuf);
	}
	
	g_free (dir);
	g_free (stat_dir);

	return;
}

gboolean
nautilus_emblem_can_remove_emblem (const char *keyword)
{
	char *path;
	gboolean ret = TRUE;
	
	path = g_strdup_printf ("%s/.icons/gnome/48x48/emblems/emblem-%s.png",
				g_get_home_dir (), keyword);

	if (access (path, F_OK|W_OK) != 0) {
		ret = FALSE;
	}

	g_free (path);

	return ret;
}

gboolean
nautilus_emblem_can_rename_emblem (const char *keyword)
{
	char *path;
	gboolean ret = TRUE;
	
	path = g_strdup_printf ("%s/.icons/gnome/48x48/emblems/emblem-%s.png",
				g_get_home_dir (), keyword);

	if (access (path, F_OK|R_OK) != 0) {
		ret = FALSE;
	}

	g_free (path);

	return ret;
}

/* of course, this only works for custom installed emblems */
gboolean
nautilus_emblem_remove_emblem (const char *keyword)
{
	char *path, *dir, *stat_dir;
	struct stat stat_buf;
	struct utimbuf ubuf;
	
	 
	dir = g_strdup_printf ("%s/.icons/gnome/48x48/emblems",
			       g_get_home_dir ());
	stat_dir = g_strdup_printf ("%s/.icons/gnome",
				    g_get_home_dir ());

	path = g_strdup_printf ("%s/emblem-%s.png", dir, keyword);

	/* delete the image */
	if (unlink (path) != 0) {
		/* couldn't delete it */
		g_free (dir);
		g_free (stat_dir);
		g_free (path);
		return FALSE;
	}

	g_free (path);

	path = g_strdup_printf ("%s/emblem-%s.icon", dir, keyword);

	if (unlink (path) != 0) {
		g_free (dir);
		g_free (stat_dir);
		g_free (path);
		return FALSE;
	}

	/* Touch the toplevel dir */
	if (stat (stat_dir, &stat_buf) == 0) {
		ubuf.actime = stat_buf.st_atime;
		ubuf.modtime = time (NULL);
		utime (stat_dir, &ubuf);
	}
	
	g_free (dir);
	g_free (stat_dir);

	return TRUE;
}

/* this only works for custom emblems as well */
gboolean
nautilus_emblem_rename_emblem (const char *keyword, const char *name)
{
	char *path, *dir, *stat_dir, *icon_name;
	struct stat stat_buf;
	struct utimbuf ubuf;
	FILE *file;
	
	dir = g_strdup_printf ("%s/.icons/gnome/48x48/emblems",
			       g_get_home_dir ());
	stat_dir = g_strdup_printf ("%s/.icons/gnome",
				    g_get_home_dir ());

	path = g_strdup_printf ("%s/emblem-%s.icon", dir, keyword);

	file = fopen (path, "w+");
	g_free (path);
		
	if (file == NULL) {
		g_free (dir);
		g_free (stat_dir);
		return FALSE;
	}

		
	/* write the new icon description */
	fprintf (file, "\n[Icon Data]\n\nDisplayName=%s\n", name);
	fflush (file);
	fclose (file);

	icon_name = nautilus_emblem_get_icon_name_from_keyword (keyword);
	nautilus_icon_factory_remove_from_cache (icon_name, NULL, 
						 NAUTILUS_ICON_SIZE_STANDARD);
	
	g_free (icon_name);

	/* Touch the toplevel dir */
	if (stat (stat_dir, &stat_buf) == 0) {
		ubuf.actime = stat_buf.st_atime;
		ubuf.modtime = time (NULL);
		utime (stat_dir, &ubuf);
	}
	
	g_free (dir);
	g_free (stat_dir);

	return TRUE;
}

char *
nautilus_emblem_create_unique_keyword (const char *base)
{
	char *keyword;
	time_t t;
	int i;

	time (&t);
	i=0;

	keyword = NULL;
	do {
		g_free (keyword);
		keyword = g_strdup_printf ("user%s%d%d", base, (int)t, i++);
	} while (is_reserved_keyword (keyword));

	return keyword;
}
