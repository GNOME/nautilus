/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#ifndef EAZEL_INSTALLER_PUBLIC_H
#define EAZEL_INSTALLER_PUBLIC_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome.h>
#include <eazel-install-public.h>
#include <eazel-install-problem.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_INSTALLER           (eazel_installer_get_type ())
#define EAZEL_INSTALLER(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_INSTALLER, EazelInstaller))
#define EAZEL_INSTALLER_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass),  \
							      TYPE_EAZEL_INSTALLER, EazelInstallerClass))
#define IS_EAZEL_INSTALLER(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_INSTALLER))
#define IS_EAZEL_INSTALLER_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_INSTALLER))

typedef struct _EazelInstaller EazelInstaller;
typedef struct _EazelInstallerClass EazelInstallerClass;

struct _EazelInstallerClass 
{
	GtkObjectClass parent_class;
};

struct _EazelInstaller
{
	GtkObject parent;

	GnomeDruid *druid;
	GnomeDruidPage *back_page;
	GnomeDruidPage *finish_good;
	GnomeDruidPage *finish_evil;
	GtkWidget *window;

	EazelInstall *service;

	EazelInstallProblem *problem;
	GList *problems;

	GList *categories;

	GList *install_categories;
	GList *force_remove_categories;

	GList *must_have_categories;
	GList *implicit_must_have;
	GList *dont_show;

	GList *failure_info;		/* GList<char *> */
	char *tmpdir;

	gboolean debug, output;
	gboolean test;
	gboolean uninstalling;
	gboolean downloaded_anything;

	unsigned long total_bytes_downloaded;
	unsigned long last_KB;

	gboolean successful;

	/* once we've got this, we know mystery errors were caused by rpm (this is kind of a hack) */
	gboolean got_dep_check;

	GList *packages_possible_broken;

	int total_packages;
	int total_size;
	int total_mb;
};

GtkType            eazel_installer_get_type(void);
EazelInstaller    *eazel_installer_new   (void);
void               eazel_installer_unref (GtkObject *object);
void               eazel_installer_do_install (EazelInstaller *installer,
					       GList *categories, 
					       gboolean remove);
void               eazel_installer_post_install (EazelInstaller *installer);

/* gtk-hackery.c */
void log_debug (const gchar *format, ...);
void get_pixmap_width_height (char **xpmdata, int *width, int *height);
GdkPixbuf *create_pixmap (GtkWidget *widget, char **xpmdata);
GtkWidget *create_gtk_pixmap (GtkWidget *widget, char **xpmdata);
GtkWidget *gtk_label_new_with_font (const char *text, const char *fontname);
void gtk_label_set_color (GtkWidget *label, guint32 rgb);
void gtk_box_add_padding (GtkWidget *box, int pad_x, int pad_y);
GtkWidget *gtk_label_as_hbox (GtkWidget *label);
GtkWidget *gtk_box_nth (GtkWidget *box, int n);
void gnome_reply_callback (int reply, gboolean *answer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALLER_PUBLIC_H */


