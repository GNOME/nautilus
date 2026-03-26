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

typedef enum
{
    NAUTILUS_IMAGE_STATUS_THUMBNAIL,
    NAUTILUS_IMAGE_STATUS_LOADING_ATTRIBUTES,
    NAUTILUS_IMAGE_STATUS_LOADING_THUMBNAIL,
    NAUTILUS_IMAGE_STATUS_FALLBACK,
} NautilusImageStatus;

NautilusImage *
nautilus_image_new (void);

GFile *
nautilus_image_get_source                               (NautilusImage *self);
void
nautilus_image_set_source                               (NautilusImage *image,
                                                         GFile         *source);

int
nautilus_image_get_size                                 (NautilusImage *self);
void
nautilus_image_set_size                                 (NautilusImage *self,
                                                         gint           size);

GdkPaintable  *
nautilus_image_get_fallback                             (NautilusImage *self);
void
nautilus_image_set_fallback                             (NautilusImage *self,
                                                         GdkPaintable  *paintable);

NautilusImageStatus
nautilus_image_get_status                               (NautilusImage *self);

G_END_DECLS

