/*
 * SPDX-FileCopyrightText: 2003 Andrew Sobala <aes@gnome.org>
 * SPDX-FileCopyrightText: 2005 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
