/* ide-box-theatric.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-box-theatric"

#include <animation/egg-animation.h>
#include <glib/gi18n.h>

#include "animation/ide-box-theatric.h"
#include "animation/ide-cairo.h"

struct _IdeBoxTheatric
{
    GObject parent_instance;

    GtkWidget *target;
    GtkWidget *toplevel;

    GIcon *icon;
    cairo_surface_t *icon_surface;
    guint icon_surface_size;

    GdkRectangle area;
    GdkRectangle last_area;
    GdkRGBA background_rgba;
    gdouble alpha;

    guint draw_handler;

    guint background_set : 1;
    guint pixbuf_failed : 1;
};

enum
{
    PROP_0,
    PROP_ALPHA,
    PROP_BACKGROUND,
    PROP_HEIGHT,
    PROP_ICON,
    PROP_TARGET,
    PROP_WIDTH,
    PROP_X,
    PROP_Y,
    LAST_PROP
};

G_DEFINE_TYPE (IdeBoxTheatric, ide_box_theatric, G_TYPE_OBJECT)

static GParamSpec *properties[LAST_PROP];

static void
get_toplevel_rect (IdeBoxTheatric *theatric,
                   GdkRectangle   *area)
{
    gtk_widget_translate_coordinates (theatric->target, theatric->toplevel,
                                      theatric->area.x, theatric->area.y,
                                      &area->x, &area->y);

    area->width = theatric->area.width;
    area->height = theatric->area.height;
}

static gboolean
on_toplevel_draw (GtkWidget      *widget,
                  cairo_t        *cr,
                  IdeBoxTheatric *self)
{
    GdkRectangle area;

    g_assert (IDE_IS_BOX_THEATRIC (self));

    get_toplevel_rect (self, &area);

#if 0
    g_print ("Drawing on %s at %d,%d %dx%d\n",
             g_type_name (G_TYPE_FROM_INSTANCE (widget)),
             area.x, area.y, area.width, area.height);
#endif

    if (self->background_set)
    {
        GdkRGBA bg;

        bg = self->background_rgba;
        bg.alpha = self->alpha;

        ide_cairo_rounded_rectangle (cr, &area, 3, 3);
        gdk_cairo_set_source_rgba (cr, &bg);
        cairo_fill (cr);
    }

    /* Load an icon if necessary and cache the surface */
    if (self->icon != NULL && self->icon_surface == NULL && !self->pixbuf_failed)
    {
        g_autoptr (GtkIconInfo) icon_info = NULL;
        GtkIconTheme *icon_theme;
        gint width;

        width = area.width * 4;
        icon_theme = gtk_icon_theme_get_default ();
        icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                    self->icon,
                                                    width,
                                                    GTK_ICON_LOOKUP_FORCE_SIZE);

        if (icon_info != NULL)
        {
            GdkWindow *window = gtk_widget_get_window (widget);
            g_autoptr (GdkPixbuf) pixbuf = NULL;
            GtkStyleContext *context;

            context = gtk_widget_get_style_context (self->target);
            pixbuf = gtk_icon_info_load_symbolic_for_context (icon_info, context, NULL, NULL);

            if (pixbuf != NULL)
            {
                self->icon_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 0, window);
                self->icon_surface_size = width;
                self->pixbuf_failed = FALSE;
            }
            else
            {
                self->pixbuf_failed = TRUE;
            }
        }
    }

    if (self->icon_surface != NULL)
    {
        cairo_translate (cr, area.x, area.y);
        cairo_rectangle (cr, 0, 0, area.width, area.height);
        cairo_scale (cr,
                     area.width / (gdouble) self->icon_surface_size,
                     area.height / (gdouble) self->icon_surface_size);
        cairo_set_source_surface (cr, self->icon_surface, 0, 0);
        cairo_paint_with_alpha (cr, self->alpha);
    }

    self->last_area = area;

    return FALSE;
}

static void
ide_box_theatric_notify (GObject    *object,
                         GParamSpec *pspec)
{
    IdeBoxTheatric *self = IDE_BOX_THEATRIC (object);

    if (G_OBJECT_CLASS (ide_box_theatric_parent_class)->notify)
    {
        G_OBJECT_CLASS (ide_box_theatric_parent_class)->notify (object, pspec);
    }

    if (self->target && self->toplevel)
    {
        GdkWindow *window;
        GdkRectangle area;

        get_toplevel_rect (IDE_BOX_THEATRIC (object), &area);

#if 0
        g_print ("  Invalidate : %d,%d %dx%d\n",
                 area.x, area.y, area.width, area.height);
#endif

        window = gtk_widget_get_window (self->toplevel);

        if (window != NULL)
        {
            gdk_window_invalidate_rect (window, &self->last_area, TRUE);
            gdk_window_invalidate_rect (window, &area, TRUE);
        }
    }
}

static void
ide_box_theatric_dispose (GObject *object)
{
    IdeBoxTheatric *self = IDE_BOX_THEATRIC (object);

    if (self->target)
    {
        if (self->draw_handler && self->toplevel)
        {
            g_signal_handler_disconnect (self->toplevel, self->draw_handler);
            self->draw_handler = 0;
        }
        g_object_remove_weak_pointer (G_OBJECT (self->target),
                                      (gpointer *) &self->target);
        self->target = NULL;
    }

    g_clear_pointer (&self->icon_surface, cairo_surface_destroy);
    g_clear_object (&self->icon);

    G_OBJECT_CLASS (ide_box_theatric_parent_class)->dispose (object);
}

static void
ide_box_theatric_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    IdeBoxTheatric *theatric = IDE_BOX_THEATRIC (object);

    switch (prop_id)
    {
        case PROP_ALPHA:
        {
            g_value_set_double (value, theatric->alpha);
        }
        break;

        case PROP_BACKGROUND:
        {
            g_value_take_string (value,
                                 gdk_rgba_to_string (&theatric->background_rgba));
        }
        break;

        case PROP_HEIGHT:
        {
            g_value_set_int (value, theatric->area.height);
        }
        break;

        case PROP_ICON:
        {
            g_value_set_object (value, theatric->icon);
        }
        break;

        case PROP_TARGET:
        {
            g_value_set_object (value, theatric->target);
        }
        break;

        case PROP_WIDTH:
        {
            g_value_set_int (value, theatric->area.width);
        }
        break;

        case PROP_X:
        {
            g_value_set_int (value, theatric->area.x);
        }
        break;

        case PROP_Y:
        {
            g_value_set_int (value, theatric->area.y);
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_box_theatric_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    IdeBoxTheatric *theatric = IDE_BOX_THEATRIC (object);

    switch (prop_id)
    {
        case PROP_ALPHA:
        {
            theatric->alpha = g_value_get_double (value);
        }
        break;

        case PROP_BACKGROUND:
        {
            const gchar *str = g_value_get_string (value);

            if (str == NULL)
            {
                gdk_rgba_parse (&theatric->background_rgba, "#000000");
                theatric->background_rgba.alpha = 0;
                theatric->background_set = FALSE;
            }
            else
            {
                gdk_rgba_parse (&theatric->background_rgba, str);
                theatric->background_set = TRUE;
            }
        }
        break;

        case PROP_HEIGHT:
        {
            theatric->area.height = g_value_get_int (value);
        }
        break;

        case PROP_ICON:
        {
            g_clear_pointer (&theatric->icon_surface, cairo_surface_destroy);
            g_clear_object (&theatric->icon);
            theatric->icon = g_value_dup_object (value);
            theatric->pixbuf_failed = FALSE;
        }
        break;

        case PROP_TARGET:
        {
            theatric->target = g_value_get_object (value);
            theatric->toplevel = gtk_widget_get_toplevel (theatric->target);
            g_object_add_weak_pointer (G_OBJECT (theatric->target),
                                       (gpointer *) &theatric->target);
            theatric->draw_handler =
                g_signal_connect_after (theatric->toplevel,
                                        "draw",
                                        G_CALLBACK (on_toplevel_draw),
                                        theatric);
        }
        break;

        case PROP_WIDTH:
        {
            theatric->area.width = g_value_get_int (value);
        }
        break;

        case PROP_X:
        {
            theatric->area.x = g_value_get_int (value);
        }
        break;

        case PROP_Y:
        {
            theatric->area.y = g_value_get_int (value);
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

    g_object_notify_by_pspec (object, pspec);
}

static void
ide_box_theatric_class_init (IdeBoxTheatricClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = ide_box_theatric_dispose;
    object_class->notify = ide_box_theatric_notify;
    object_class->get_property = ide_box_theatric_get_property;
    object_class->set_property = ide_box_theatric_set_property;

    properties[PROP_ALPHA] =
        g_param_spec_double ("alpha",
                             "Alpha",
                             "Alpha",
                             0.0,
                             1.0,
                             1.0,
                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    properties[PROP_BACKGROUND] =
        g_param_spec_string ("background",
                             "background",
                             "background",
                             "#000000",
                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    properties[PROP_HEIGHT] =
        g_param_spec_int ("height",
                          "height",
                          "height",
                          0,
                          G_MAXINT,
                          0,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    properties [PROP_ICON] =
        g_param_spec_object ("icon",
                             "Icon",
                             "The GIcon to render over the background",
                             G_TYPE_ICON,
                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    properties[PROP_TARGET] =
        g_param_spec_object ("target",
                             "Target",
                             "Target",
                             GTK_TYPE_WIDGET,
                             (G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT_ONLY |
                              G_PARAM_STATIC_STRINGS));

    properties[PROP_WIDTH] =
        g_param_spec_int ("width",
                          "width",
                          "width",
                          0,
                          G_MAXINT,
                          0,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    properties[PROP_X] =
        g_param_spec_int ("x",
                          "x",
                          "x",
                          G_MININT,
                          G_MAXINT,
                          0,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    properties[PROP_Y] =
        g_param_spec_int ("y",
                          "y",
                          "y",
                          G_MININT,
                          G_MAXINT,
                          0,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_box_theatric_init (IdeBoxTheatric *theatric)
{
}
