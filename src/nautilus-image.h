/*
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_IMAGE (nautilus_image_get_type())

G_DECLARE_FINAL_TYPE (NautilusImage, nautilus_image, NAUTILUS, IMAGE, GtkWidget)

NautilusImage *
nautilus_image_new (void);

void
nautilus_image_set_source                               (NautilusImage *image,
                                                         GFile         *source);
void
nautilus_image_set_size                                 (NautilusImage *self,
                                                         gint           size);
void
nautilus_image_set_fallback                             (NautilusImage *self,
                                                         GdkPaintable  *paintable);

G_END_DECLS

