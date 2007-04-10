/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2006 Christian Persch
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
 *
 * This is the throbber (for busy feedback) for the location bar
 *
 */

#include <config.h>

#include "nautilus-throbber.h"

#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-accessibility.h>
#include <glib/gi18n.h>

static AtkObject *nautilus_throbber_get_accessible   (GtkWidget *widget);

G_DEFINE_TYPE (NautilusThrobber, nautilus_throbber, EPHY_TYPE_SPINNER)

static void
nautilus_throbber_init (NautilusThrobber *throbber)
{
}	

void
nautilus_throbber_start (NautilusThrobber *throbber)
{
	ephy_spinner_start (EPHY_SPINNER (throbber));
}

void
nautilus_throbber_stop (NautilusThrobber *throbber)
{
	ephy_spinner_stop (EPHY_SPINNER (throbber));
}

void
nautilus_throbber_set_size (NautilusThrobber *throbber, GtkIconSize size)
{
	ephy_spinner_set_size (EPHY_SPINNER (throbber), size);
}

static void
nautilus_throbber_class_init (NautilusThrobberClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);

	widget_class->get_accessible = nautilus_throbber_get_accessible;
}

static AtkObjectClass *a11y_parent_class = NULL;

static void
nautilus_throbber_accessible_initialize (AtkObject *accessible,
					 gpointer   widget)
{
	atk_object_set_name (accessible, _("throbber"));
	atk_object_set_description (accessible, _("provides visual status"));

	a11y_parent_class->initialize (accessible, widget);
}

static void
nautilus_throbber_accessible_class_init (AtkObjectClass *klass)
{
	a11y_parent_class = g_type_class_peek_parent (klass);

	klass->initialize = nautilus_throbber_accessible_initialize;
}

static void
nautilus_throbber_accessible_image_get_size (AtkImage *image,
					     gint     *width,
					     gint     *height)
{
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (image)->widget;
	if (!widget) {
		*width = *height = 0;
	} else {
		*width = widget->allocation.width;
		*height = widget->allocation.height;
	}
}

static void
nautilus_throbber_accessible_image_interface_init (AtkImageIface *iface)
{
	iface->get_image_size = nautilus_throbber_accessible_image_get_size;
}

static GType
nautilus_throbber_accessible_get_type (void)
{
        static GType type = 0;

	/* Action interface
	   Name etc. ... */
	if (G_UNLIKELY (type == 0)) {
		const GInterfaceInfo atk_image_info = {
			(GInterfaceInitFunc) nautilus_throbber_accessible_image_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = eel_accessibility_create_derived_type 
			("NautilusThrobberAccessible",
			 GTK_TYPE_IMAGE,
			 nautilus_throbber_accessible_class_init);
		
                g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                             &atk_image_info);
        }

        return type;
}

static AtkObject *
nautilus_throbber_get_accessible (GtkWidget *widget)
{
	AtkObject *accessible;
	
	if ((accessible = eel_accessibility_get_atk_object (widget))) {
		return accessible;
	}
	
	accessible = g_object_new 
		(nautilus_throbber_accessible_get_type (), NULL);
	
	return eel_accessibility_set_atk_object_return (widget, accessible);
}

GtkWidget    *
nautilus_throbber_new (void)
{
	return g_object_new (NAUTILUS_TYPE_THROBBER, NULL);
}
