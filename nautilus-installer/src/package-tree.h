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
 * Authors: Robey Pointer <robey@eazel.com>
 */


#ifndef PACKAGE_TREE_H
#define PACKAGE_TREE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* i'm ditching the "eazel" or "trilobite" prefix for this object, because if you link with this object,
 * you won't have any other package customizers.  it's a pretty darn specific widget to have.
 */
#define TYPE_PACKAGE_CUSTOMIZER		(package_customizer_get_type ())
#define PACKAGE_CUSTOMIZER(obj)		(GTK_CHECK_CAST ((obj), TYPE_PACKAGE_CUSTOMIZER, PackageCustomizer))
#define PACKAGE_CUSTOMIZER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), TYPE_PACKAGE_CUSTOMIZER, PackageCustomizerClass))
#define IS_PACKAGE_CUSTOMIZER(obj)	(GTK_CHECK_TYPE ((obj), TYPE_PACKAGE_CUSTOMIZER))
#define IS_PACKAGE_CUSTOMIZER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), TYPE_PACKAGE_CUSTOMIZER))

typedef struct _PackageCustomizer PackageCustomizer;
typedef struct _PackageCustomizerClass PackageCustomizerClass;
typedef struct _PackageCustomizerPrivate PackageCustomizerPrivate;

struct _PackageCustomizerClass
{
        GtkObjectClass parent_class;
};

struct _PackageCustomizer
{
        GtkObject parent;
        PackageCustomizerPrivate *private;
};


GtkType package_customizer_get_type (void);
PackageCustomizer *package_customizer_new (void);
void package_customizer_unref (GtkObject *object);
void package_customizer_set_package_list (PackageCustomizer *table, GList *package_tree);
GtkWidget *package_customizer_get_widget (PackageCustomizer *table);

void jump_to_package_tree_page (EazelInstaller *installer, GList *packages);

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* PACKAGE_TREE_H */
