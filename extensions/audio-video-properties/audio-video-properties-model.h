/*
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 * Copyright (C) 2005  Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef TOTEM_PROPERTIES_VIEW_H
#define TOTEM_PROPERTIES_VIEW_H

#include <nautilus-extension.h>

#define TOTEM_TYPE_PROPERTIES_VIEW	    (totem_properties_view_get_type ())
#define TOTEM_PROPERTIES_VIEW(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_PROPERTIES_VIEW, TotemPropertiesView))
#define TOTEM_PROPERTIES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_PROPERTIES_VIEW, TotemPropertiesViewClass))
#define TOTEM_IS_PROPERTIES_VIEW(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_PROPERTIES_VIEW))
#define TOTEM_IS_PROPERTIES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_PROPERTIES_VIEW))

typedef struct TotemPropertiesViewPriv TotemPropertiesViewPriv;

typedef struct {
	GObject parent;
	TotemPropertiesViewPriv *priv;
} TotemPropertiesView;

typedef struct {
	GObjectClass parent;
} TotemPropertiesViewClass;

GType                    totem_properties_view_get_type      (void);
void                     totem_properties_view_register_type (GTypeModule *module);

NautilusPropertiesModel *totem_properties_view_new           (const char *location);

#endif /* TOTEM_PROPERTIES_VIEW_H */
