/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* This is the header file for the property browser window, which
 * gives the user access to an extensible palette of properties which
 * can be dropped on various elements of the user interface to
 * customize them 
 */

#ifndef NAUTILUS_PROPERTY_BROWSER_H
#define NAUTILUS_PROPERTY_BROWSER_H

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>

typedef struct NautilusPropertyBrowser NautilusPropertyBrowser;
typedef struct NautilusPropertyBrowserClass  NautilusPropertyBrowserClass;

#define NAUTILUS_TYPE_PROPERTY_BROWSER \
	(nautilus_property_browser_get_type ())
#define NAUTILUS_PROPERTY_BROWSER(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_PROPERTY_BROWSER, NautilusPropertyBrowser))
#define NAUTILUS_PROPERTY_BROWSER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PROPERTY_BROWSER, NautilusPropertyBrowserClass))
#define NAUTILUS_IS_PROPERTY_BROWSER(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PROPERTY_BROWSER))
#define NAUTILUS_IS_PROPERTY_BROWSER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PROPERTY_BROWSER))

typedef struct NautilusPropertyBrowserDetails NautilusPropertyBrowserDetails;

struct NautilusPropertyBrowser
{
	GtkWindow window;
	NautilusPropertyBrowserDetails *details;
};

struct NautilusPropertyBrowserClass
{
	GtkWindowClass parent_class;
};

GType                    nautilus_property_browser_get_type (void);
NautilusPropertyBrowser *nautilus_property_browser_new      (GdkScreen               *screen);
void                     nautilus_property_browser_show     (GdkScreen               *screen);
void                     nautilus_property_browser_set_path (NautilusPropertyBrowser *panel,
							     const char              *new_path);

#endif /* NAUTILUS_PROPERTY_BROWSER_H */
