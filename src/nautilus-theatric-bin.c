/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-theatric-bin.h"

#include "animation/egg-animation.h"

struct _NautilusTheatricBin
{
    GtkBin parent_instance;

    unsigned int duration;
    gboolean flash;
    double opacity;
    double scale;

    EggAnimation *animation;
};

G_DEFINE_TYPE (NautilusTheatricBin, nautilus_theatric_bin, GTK_TYPE_BIN)

enum
{
    PROP_0,
    PROP_DURATION,
    PROP_FLASH,
    PROP_OPACITY,
    PROP_SCALE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static void
nautilus_theatric_bin_init (NautilusTheatricBin *self)
{
}

static void
nautilus_theatric_bin_on_flash_done (gpointer data)
{
    g_object_set (data,
                  "flash", FALSE,
                  "opacity", 1.0,
                  "scale", 1.0,
                  NULL);
}

static void
nautilus_theatric_bin_set_property (GObject      *object,
                                    unsigned int  property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NautilusTheatricBin *self;
    GtkWidget *widget;

    self = NAUTILUS_THEATRIC_BIN (object);
    widget = GTK_WIDGET (object);

    switch (property_id)
    {
        case PROP_DURATION:
        {
            self->duration = g_value_get_uint (value);
        }
        break;

        case PROP_FLASH:
        {
            GtkStyleContext *context;

            context = gtk_widget_get_style_context (widget);

            self->flash = g_value_get_boolean (value);

            g_clear_pointer (&self->animation, egg_animation_stop);

            if (self->flash)
            {
                self->opacity = 1.0;
                self->scale = 1.0;

                self->animation = egg_object_animate_full (self,
                                                           EGG_ANIMATION_EASE_IN_CUBIC,
                                                           self->duration,
                                                           gtk_widget_get_frame_clock (widget),
                                                           nautilus_theatric_bin_on_flash_done,
                                                           self,
                                                           "opacity", 0.0,
                                                           "scale", 2.0,
                                                           NULL);

                gtk_style_context_add_class (context, "flash");
            }
            else
            {
                gtk_style_context_remove_class (context, "flash");
            }

            gtk_widget_queue_draw (widget);
        }
        break;

        case PROP_OPACITY:
        {
            self->opacity = g_value_get_double (value);

            gtk_widget_queue_draw (widget);
        }
        break;

        case PROP_SCALE:
        {
            self->scale = g_value_get_double (value);

            gtk_widget_queue_draw (widget);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_theatric_bin_get_property (GObject      *object,
                                    unsigned int  property_id,
                                    GValue       *value,
                                    GParamSpec   *pspec)
{
    NautilusTheatricBin *self;

    self = NAUTILUS_THEATRIC_BIN (object);

    switch (property_id)
    {
        case PROP_DURATION:
        {
            g_value_set_uint (value, self->duration);
        }
        break;

        case PROP_FLASH:
        {
            g_value_set_boolean (value, self->flash);
        }
        break;

        case PROP_OPACITY:
        {
            g_value_set_double (value, self->opacity);
        }
        break;

        case PROP_SCALE:
        {
            g_value_set_double (value, self->scale);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_theatric_bin_snapshot_flash (GtkWidget   *widget,
                                      GtkSnapshot *snapshot)
{
    NautilusTheatricBin *self;
    GtkBin *bin;
    GtkWidget *child;
    GtkAllocation allocation;
    int offset_x;
    int offset_y;
    graphene_matrix_t transform;

    self = NAUTILUS_THEATRIC_BIN (widget);
    bin = GTK_BIN (widget);
    child = gtk_bin_get_child (bin);

    gtk_widget_get_allocation (widget, &allocation);

    offset_x = (allocation.width - allocation.width * self->scale) / 2;
    offset_y = (allocation.height - allocation.height * self->scale) / 2;

    graphene_matrix_init_scale (&transform, self->scale, self->scale, 1.0);

    gtk_snapshot_offset (snapshot, offset_x, offset_y);

    gtk_snapshot_push_opacity (snapshot, self->opacity);
    gtk_snapshot_push_transform (snapshot, &transform);
    gtk_widget_snapshot_child (widget, child, snapshot);
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_pop (snapshot);

    gtk_snapshot_offset (snapshot, -offset_x, -offset_y);
}

static void
nautilus_theatric_bin_snapshot (GtkWidget   *widget,
                                GtkSnapshot *snapshot)
{
    NautilusTheatricBin *self;
    GtkBin *bin;
    GtkWidget *child;

    self = NAUTILUS_THEATRIC_BIN (widget);
    bin = GTK_BIN (widget);
    child = gtk_bin_get_child (bin);
    if (child == NULL)
    {
        return;
    }

    if (self->flash)
    {
        nautilus_theatric_bin_snapshot_flash (widget, snapshot);
    }
    else
    {
        gtk_widget_snapshot_child (widget, child, snapshot);
    }
}

static void
nautilus_theatric_bin_dispose (GObject *object)
{
    NautilusTheatricBin *self;

    self = NAUTILUS_THEATRIC_BIN (object);

    g_clear_pointer (&self->animation, egg_animation_stop);

    G_OBJECT_CLASS (nautilus_theatric_bin_parent_class)->dispose (object);
}

static void
nautilus_theatric_bin_class_init (NautilusTheatricBinClass *klass)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    object_class->set_property = nautilus_theatric_bin_set_property;
    object_class->get_property = nautilus_theatric_bin_get_property;
    object_class->dispose = nautilus_theatric_bin_dispose;

    widget_class->snapshot = nautilus_theatric_bin_snapshot;

    properties[PROP_DURATION] = g_param_spec_uint ("duration",
                                                   "Duration",
                                                   "Duration of the flash effect",
                                                   0, UINT_MAX, 333,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT |
                                                   G_PARAM_STATIC_STRINGS);
    properties[PROP_FLASH] = g_param_spec_boolean ("flash",
                                                   "Flash",
                                                   "Whether the flash effect is active",
                                                   FALSE,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT |
                                                   G_PARAM_STATIC_STRINGS);
    properties[PROP_OPACITY] = g_param_spec_double ("opacity",
                                                    "Opacity",
                                                    "Opacity of the flash effect",
                                                    0.0, 1.0, 1.0,
                                                    G_PARAM_READWRITE |
                                                    G_PARAM_CONSTRUCT |
                                                    G_PARAM_STATIC_STRINGS);
    properties[PROP_SCALE] = g_param_spec_double ("scale",
                                                  "Scale",
                                                  "The scale at which the child widget is drawn as part of the flash effect",
                                                  1.0, DBL_MAX, 1.0,
                                                  G_PARAM_READWRITE |
                                                  G_PARAM_CONSTRUCT |
                                                  G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    gtk_widget_class_set_css_name (widget_class, "theatric-bin");
}

void
nautilus_theatric_bin_flash (NautilusTheatricBin *self)
{
    g_return_if_fail (NAUTILUS_IS_THEATRIC_BIN (self));

    g_object_set (self, "flash", TRUE, NULL);
}

GtkWidget *
nautilus_theatric_bin_new (void)
{
    return gtk_widget_new (NAUTILUS_TYPE_THEATRIC_BIN, NULL);
}
