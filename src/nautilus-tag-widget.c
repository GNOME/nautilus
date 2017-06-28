/* nautilus-tag-widget.c
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "nautilus-tag-widget.h"
#include "nautilus-tag-manager.h"

struct _NautilusTagWidget
{
    GtkEventBox parent;

    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *circle;

    gboolean cursor_over;

    gchar *color;
    TagData *tag_data;
};

G_DEFINE_TYPE (NautilusTagWidget, nautilus_tag_widget, GTK_TYPE_EVENT_BOX);

GtkWidget*
nautilus_tag_widget_queue_get_tag_with_name (GQueue      *queue,
                                             const gchar *tag_name)
{
    GList *l;
    NautilusTagWidget *tag_widget;

    for (l = g_queue_peek_head_link (queue); l != NULL; l = l->next)
    {
        tag_widget = NAUTILUS_TAG_WIDGET (l->data);

        if (g_strcmp0 (tag_name, tag_widget->tag_data->name) == 0)
        {
            return GTK_WIDGET (tag_widget);
        }
    }

    return NULL;
}

gboolean
nautilus_tag_widget_queue_contains_tag (GQueue      *queue,
                                        const gchar *tag_name)
{
    GList *l;
    NautilusTagWidget *tag_widget;

    for (l = g_queue_peek_head_link (queue); l != NULL; l = l->next)
    {
        tag_widget = NAUTILUS_TAG_WIDGET (l->data);

        if (g_strcmp0 (tag_name, tag_widget->tag_data->name) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

const gchar*
nautilus_tag_widget_get_tag_name (NautilusTagWidget *self)
{
    return self->tag_data->name;
}

const gchar*
nautilus_tag_widget_get_tag_id (NautilusTagWidget *self)
{
    return self->tag_data->id;
}

static gboolean
paint_circle (GtkWidget *widget,
              cairo_t   *cr,
              gpointer   data)
{
    guint width, height;
    GdkRGBA color;
    GtkStyleContext *context;
    NautilusTagWidget *tag_widget;
    /*GtkIconInfo *info;
    GdkPixbuf *pixbuf;
    gint icon_size;
    gint scale_factor;*/

    tag_widget = NAUTILUS_TAG_WIDGET (data);

    context = gtk_widget_get_style_context (widget);

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

    gtk_render_background (context, cr, 0, 0, width, height);

    cairo_arc (cr,
               width / 2.0, height / 2.0,
               MIN (width, height) / 2.0,
               0, 2 * G_PI);

    gtk_style_context_get_color (context,
                                 gtk_style_context_get_state (context),
                                 &color);


    gdk_rgba_parse (&color, tag_widget->color);

    gdk_cairo_set_source_rgba (cr, &color);

    cairo_fill (cr);

    if (tag_widget->cursor_over)
    {

        /*gtk_icon_size_lookup (GTK_ICON_SIZE_MENU,
                              &icon_size, NULL);
        scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (tag_widget->label));

        info = gtk_icon_theme_lookup_icon_for_scale (gtk_icon_theme_get_default (),
                                                     "window-close-symbolic",
                                                     icon_size, scale_factor,
                                                     GTK_ICON_LOOKUP_GENERIC_FALLBACK);

        pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, NULL);
        g_print("%d %d\n", width, height);
        width = 0;
        height = 0;
        gdk_cairo_set_source_pixbuf (cr, pixbuf, width / 2, height / 2);

        cairo_paint (cr);

        g_object_unref (info);
        g_object_unref (pixbuf);*/

        gtk_style_context_get_color (context,
                                     gtk_style_context_get_state (context),
                                     &color);

        gdk_cairo_set_source_rgba (cr, &color);

        cairo_move_to (cr,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 * -1 + width / 2.0,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 + height / 2.0);
        cairo_line_to (cr,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 + width / 2.0,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 * -1 + height / 2.0);

        cairo_stroke (cr);

        cairo_move_to (cr,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 + width / 2.0,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 + height / 2.0);
        cairo_line_to (cr,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 * -1 + width / 2.0,
                       (MIN (width, height) - MIN (width, height) * 0.25) / 2.0 * G_SQRT2 / 2 * -1 + height / 2.0);

        cairo_stroke (cr);
    }

    return FALSE;
}

gboolean
on_leave_event (GtkWidget *widget,
                GdkEvent  *event,
                gpointer   user_data)
{
    NautilusTagWidget *self;

    self = NAUTILUS_TAG_WIDGET (widget);

    if (self->cursor_over == TRUE)
    {
        gtk_widget_queue_draw (self->circle);

        gtk_label_set_label (GTK_LABEL (self->label), self->tag_data->name);
    }

    self->cursor_over = FALSE;

    return FALSE;
}

gboolean
on_motion_event (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
    NautilusTagWidget *self;
    g_autofree gchar *markup = NULL;

    self = NAUTILUS_TAG_WIDGET (widget);

    if (self->cursor_over == FALSE)
    {
        gtk_widget_queue_draw (self->circle);

        markup = g_markup_printf_escaped ("<u>%s</u>", self->tag_data->name);
        gtk_label_set_label (GTK_LABEL (self->label), markup);
    }

    self->cursor_over = TRUE;

    return FALSE;
}


static void
nautilus_tag_widget_finalize (GObject *object)
{
    NautilusTagWidget *self;

    self = NAUTILUS_TAG_WIDGET (object);

    g_free (self->color);
    nautilus_tag_data_free (self->tag_data);

    G_OBJECT_CLASS (nautilus_tag_widget_parent_class)->finalize (object);
}

static void
nautilus_tag_widget_class_init (NautilusTagWidgetClass *klass)
{
    //GtkWidgetClass *widget_class;
    GObjectClass *oclass;

    //widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_tag_widget_finalize;
}

GtkWidget* nautilus_tag_widget_new (const gchar   *tag_label,
                                    const gchar   *tag_id,
                                    gboolean       can_close)
{
    NautilusTagWidget *self;
    self = g_object_new (NAUTILUS_TYPE_TAG_WIDGET,
                         NULL);

    self->color = parse_color_from_tag_id (tag_id);

    self->tag_data = nautilus_tag_data_new (tag_id, tag_label, NULL);

    self->label = gtk_label_new (tag_label);
    gtk_label_set_use_markup (GTK_LABEL (self->label), TRUE);

    self->box = g_object_new (GTK_TYPE_BOX,
                              "orientation",
                              GTK_ORIENTATION_HORIZONTAL,
                              "spacing",
                              5,
                              NULL);

    gtk_widget_add_events (GTK_WIDGET (self), GDK_POINTER_MOTION_MASK);

    if (can_close)
    {
        g_signal_connect (self,
                          "motion-notify-event",
                          G_CALLBACK (on_motion_event),
                          NULL);

        g_signal_connect (self,
                          "leave-notify-event",
                          G_CALLBACK (on_leave_event),
                          NULL);
    }

    self->circle = gtk_drawing_area_new ();
    gtk_widget_set_size_request (self->circle, 15, 15);
    g_signal_connect (self->circle, "draw",
                      G_CALLBACK (paint_circle), self);

    gtk_box_pack_start (GTK_BOX (self->box), self->circle, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (self->box), self->label, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (self), self->box);

    return GTK_WIDGET (self);
}

static void
nautilus_tag_widget_init (NautilusTagWidget *self)
{

}
