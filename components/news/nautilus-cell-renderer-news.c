/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus News Item Cell Renderer
 *
 *  Copyright (C) 2000  Red Hat, Inc., Ximian Inc., Jonathan Blandford
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Dave Camp <dave@ximian.com> 
 *          based on the text cell renderer by Jonathan Blandford
 *          <jrb@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-cell-renderer-news.h"

#include <stdlib.h>
#include <pango/pango-layout.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <libgnome/gnome-i18n.h>

struct _NautilusCellRendererNewsPrivate {
	char *text;
	PangoFontDescription *font;
	
	PangoAttrList *extra_attrs;

	GdkPixbuf *bullet;
	GdkPixbuf *prelit_bullet;
	
	int wrap_at;
};

static void nautilus_cell_renderer_news_init       (NautilusCellRendererNews      *cellnews);
static void nautilus_cell_renderer_news_class_init (NautilusCellRendererNewsClass *class);
static void nautilus_cell_renderer_news_finalize   (GObject                  *object);

static void nautilus_cell_renderer_news_get_property  (GObject                  *object,
                                                       guint                     param_id,
                                                       GValue                   *value,
                                                       GParamSpec               *pspec);
static void nautilus_cell_renderer_news_set_property  (GObject                  *object,
                                                       guint                     param_id,
                                                       const GValue             *value,
                                                       GParamSpec               *pspec);
static void nautilus_cell_renderer_news_get_size   (GtkCellRenderer          *cell,
                                                    GtkWidget                *widget,
                                                    GdkRectangle             *cell_area,
                                                    int                      *x_offset,
                                                    int                      *y_offset,
                                                    int                      *width,
                                                    int                      *height);
static void nautilus_cell_renderer_news_render     (GtkCellRenderer          *cell,
                                                    GdkWindow                *window,
                                                    GtkWidget                *widget,
                                                    GdkRectangle             *background_area,
                                                    GdkRectangle             *cell_area,
                                                    GdkRectangle             *expose_area,
                                                    guint                     flags);

enum {
        PROP_0,
        
        PROP_MARKUP,
        PROP_BULLET,
        PROP_PRELIT_BULLET,
        PROP_WRAP_AT,
};

static gpointer parent_class;

#define PAD 2

GType
nautilus_cell_renderer_news_get_type (void)
{
        static GtkType cell_news_type = 0;

        if (!cell_news_type) {
                static const GTypeInfo cell_news_info = {
                        sizeof (NautilusCellRendererNewsClass),
                        NULL,		/* base_init */
                        NULL,		/* base_finalize */
                        (GClassInitFunc) nautilus_cell_renderer_news_class_init,
                        NULL,		/* class_finalize */
                        NULL,		/* class_data */
                        sizeof (NautilusCellRendererNews),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) nautilus_cell_renderer_news_init,
                };
                
                cell_news_type = g_type_register_static (GTK_TYPE_CELL_RENDERER, "NautilusCellRendererNews", &cell_news_info, 0);
        }
        
        return cell_news_type;
}

static void
nautilus_cell_renderer_news_init (NautilusCellRendererNews *cellnews)
{
        GTK_CELL_RENDERER (cellnews)->xalign = 0.0;
        GTK_CELL_RENDERER (cellnews)->yalign = 0.5;
        GTK_CELL_RENDERER (cellnews)->xpad = 2;
        GTK_CELL_RENDERER (cellnews)->ypad = 2;

        cellnews->priv = g_new0 (NautilusCellRendererNewsPrivate, 1);
        cellnews->priv->wrap_at = -1;
        cellnews->priv->font = pango_font_description_new ();
}

static void
nautilus_cell_renderer_news_class_init (NautilusCellRendererNewsClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

        parent_class = g_type_class_peek_parent (class);
  
        object_class->finalize = nautilus_cell_renderer_news_finalize;
  
        object_class->get_property = nautilus_cell_renderer_news_get_property;
        object_class->set_property = nautilus_cell_renderer_news_set_property;

        cell_class->get_size = nautilus_cell_renderer_news_get_size;
        cell_class->render = nautilus_cell_renderer_news_render;
  
        g_object_class_install_property (object_class,
                                         PROP_WRAP_AT,
                                         g_param_spec_int ("wrap_at",
                                                           _("Wrap at"),
                                                           _("Width the cell should wrap to."),
                                                           -1, G_MAXINT,
                                                           -1, 
                                                           G_PARAM_READWRITE));


        g_object_class_install_property (object_class,
                                         PROP_MARKUP,
                                         g_param_spec_string ("markup",
                                                              _("Markup"),
                                                              _("Marked up text to display"),
                                                              "",
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_BULLET,
                                         g_param_spec_pointer ("bullet",
                                                               _("Bullet"),
                                                               _("Bullet to display"),
                                                               G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_PRELIT_BULLET,
                                         g_param_spec_pointer ("prelit_bullet",
                                                               _("Prelight Bullet"),
                                                               _("Bullet to display when prelit"),
                                                               G_PARAM_READWRITE));
}

static void
nautilus_cell_renderer_news_finalize (GObject *object)
{
        NautilusCellRendererNews *cellnews = NAUTILUS_CELL_RENDERER_NEWS (object);

        pango_font_description_free (cellnews->priv->font);

        if (cellnews->priv->text) {
                g_free (cellnews->priv->text);
        }

        if (cellnews->priv->extra_attrs) {
                pango_attr_list_unref (cellnews->priv->extra_attrs);
        }
        
        if (cellnews->priv->bullet) {
                g_object_unref (cellnews->priv->bullet);
        }

        if (cellnews->priv->prelit_bullet) {
                g_object_unref (cellnews->priv->prelit_bullet);
        }

        g_free (cellnews->priv);

        (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
nautilus_cell_renderer_news_get_property (GObject        *object,
                                          guint           param_id,
                                          GValue         *value,
                                          GParamSpec     *pspec)
{
        NautilusCellRendererNews *cellnews = NAUTILUS_CELL_RENDERER_NEWS (object);

        switch (param_id) {
        case PROP_WRAP_AT :
                g_value_set_int (value, cellnews->priv->wrap_at);
                break;
        case PROP_MARKUP :
                g_value_set_pointer (value, cellnews->priv->text);
                break;
        case PROP_BULLET :
                g_value_set_pointer (value, cellnews->priv->bullet);
                break;
        case PROP_PRELIT_BULLET :
                g_value_set_pointer (value, cellnews->priv->prelit_bullet);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
nautilus_cell_renderer_news_set_property (GObject      *object,
                                          guint         param_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
        NautilusCellRendererNews *cellnews = NAUTILUS_CELL_RENDERER_NEWS (object);
        GdkPixbuf *pixbuf;
        const gchar *str;
        char *text = NULL;
        GError *error = NULL;
        PangoAttrList *attrs = NULL;
        

        switch (param_id) {
        case PROP_WRAP_AT: 
                cellnews->priv->wrap_at = g_value_get_int (value);
                g_object_notify (object, "wrap_at");
                break;
        case PROP_BULLET: 
                pixbuf = g_value_get_pointer (value);

                if (cellnews->priv->bullet) {
                        g_object_unref (cellnews->priv->bullet);
                }
                cellnews->priv->bullet = pixbuf;
                if (cellnews->priv->bullet) {
                        g_object_ref (cellnews->priv->bullet);
                }
                g_object_notify (object, "bullet");
                break;
        case PROP_PRELIT_BULLET:
                pixbuf = g_value_get_pointer (value);

                if (cellnews->priv->prelit_bullet) {
                        g_object_unref (cellnews->priv->prelit_bullet);
                }
                cellnews->priv->prelit_bullet = pixbuf;
                if (cellnews->priv->prelit_bullet) {
                        g_object_ref (cellnews->priv->prelit_bullet);
                }

                g_object_notify (object, "prelit_bullet");
                break;
        case PROP_MARKUP: 
                str = g_value_get_string (value);
                
                if (cellnews->priv->extra_attrs) {
                        pango_attr_list_unref (cellnews->priv->extra_attrs);
                }
                
                if (str && !pango_parse_markup (str,
                                                -1,
                                                0,
                                                &attrs,
                                                &text,
                                                NULL,
                                                &error)) {
                        g_warning ("Failed to set cell news from markup due to error parsing markup: %s",
                                   error->message);
                        g_error_free (error);
                        return;
                }
	
                cellnews->priv->text = text;
                cellnews->priv->extra_attrs = attrs;

                g_object_notify (object, "markup");
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

/**
 * nautilus_cell_renderer_news_new:
 * 
 * Creates a new #NautilusCellRendererNews. Adjust how news is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "news" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
nautilus_cell_renderer_news_new (void)
{
        return GTK_CELL_RENDERER (g_object_new (nautilus_cell_renderer_news_get_type (), NULL));
}

static void
add_attr (PangoAttrList  *attr_list,
          PangoAttribute *attr)
{
        attr->start_index = 0;
        attr->end_index = G_MAXINT;
  
        pango_attr_list_insert (attr_list, attr);
}

static PangoLayout*
get_layout (NautilusCellRendererNews *cellnews,
            GtkWidget           *widget,
            gboolean             will_render,
            GtkCellRendererState flags)
{
        PangoAttrList *attr_list;
        PangoLayout *layout;
        int wrap_width;
  
        layout = gtk_widget_create_pango_layout (widget, cellnews->priv->text);

        if (cellnews->priv->extra_attrs)
                attr_list = pango_attr_list_copy (cellnews->priv->extra_attrs);
        else
                attr_list = pango_attr_list_new ();

        add_attr (attr_list, pango_attr_font_desc_new (cellnews->priv->font));
  
        if ((flags & GTK_CELL_RENDERER_PRELIT) != 0) {
                add_attr (attr_list,
                          pango_attr_foreground_new (0, 0, 65535 / 2));
        }

        pango_layout_set_attributes (layout, attr_list);

        if (cellnews->priv->wrap_at != -1) {
                wrap_width = cellnews->priv->wrap_at;
                if (cellnews->priv->bullet) {
                        wrap_width -= (gdk_pixbuf_get_width (cellnews->priv->bullet) + PAD);
                }
                
                pango_layout_set_wrap (layout, PANGO_WRAP_WORD);
                pango_layout_set_width (layout, 
                                        wrap_width * PANGO_SCALE);
        }

        pango_attr_list_unref (attr_list);
  
        return layout;
}

static void
nautilus_cell_renderer_news_get_size (GtkCellRenderer *cell,
                                      GtkWidget       *widget,
                                      GdkRectangle    *cell_area,
                                      int             *x_offset,
                                      int             *y_offset,
                                      int             *width,
                                      int             *height)
{
        NautilusCellRendererNews *cellnews = (NautilusCellRendererNews *) cell;
        PangoRectangle rect;
        PangoLayout *layout;

        layout = get_layout (cellnews, widget, FALSE, 0);
        pango_layout_get_pixel_extents (layout, NULL, &rect);

        if (width) {
                *width = GTK_CELL_RENDERER (cellnews)->xpad * 2 + rect.width;
                if (cellnews->priv->bullet) {
                        *width += gdk_pixbuf_get_width (cellnews->priv->bullet) + PAD;
                }
        }
        
        if (height) {
                *height = GTK_CELL_RENDERER (cellnews)->ypad * 2 + rect.height;
                if (cellnews->priv->bullet) {
                        *height = MAX (*height,
                                       gdk_pixbuf_get_height (cellnews->priv->bullet) + GTK_CELL_RENDERER (cellnews)->ypad * 2);
                }
        }

        if (cell_area) {
                if (x_offset) {
                        *x_offset = cell->xalign * (cell_area->width - rect.width - (2 * cell->xpad));
                        *x_offset = MAX (*x_offset, 0);
                }
                if (y_offset) {
                        *y_offset = cell->yalign * (cell_area->height - rect.height - (2 * cell->ypad));
                        *y_offset = MAX (*y_offset, 0);
                }
        }

        g_object_unref (layout);
}

static void
nautilus_cell_renderer_news_render (GtkCellRenderer    *cell,
                                    GdkWindow          *window,
                                    GtkWidget          *widget,
                                    GdkRectangle       *background_area,
                                    GdkRectangle       *cell_area,
                                    GdkRectangle       *expose_area,
                                    guint               flags)

{
        NautilusCellRendererNews *cellnews = (NautilusCellRendererNews *) cell;
        PangoLayout *layout;
        GtkStateType state;
        int x_offset;
        int y_offset;
        int height;
        int width;
        GdkPixbuf *pixbuf;

        layout = get_layout (cellnews, widget, TRUE, flags);

        nautilus_cell_renderer_news_get_size (cell, widget, cell_area, &x_offset, &y_offset, &height, &width);

        if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED) {
                if (GTK_WIDGET_HAS_FOCUS (widget)) {
                        state = GTK_STATE_SELECTED;
                } else {
                        state = GTK_STATE_ACTIVE;
                }
        } else {
                if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE)
                        state = GTK_STATE_INSENSITIVE;
                else
                        state = GTK_STATE_NORMAL;
        }

        if (cellnews->priv->prelit_bullet 
            && (flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT) {
                pixbuf = cellnews->priv->prelit_bullet;
        } else {
                pixbuf = cellnews->priv->bullet;
        }

        if (pixbuf) {
                gdk_pixbuf_render_to_drawable_alpha 
                        (pixbuf, 
                         window,
                         0, 0,
                         cell_area->x + cell->xpad,
                         cell_area->y + cell->ypad,
                         gdk_pixbuf_get_width (pixbuf),
                         gdk_pixbuf_get_height (pixbuf),
                         GDK_PIXBUF_ALPHA_FULL, 
                         0, 
                         GDK_RGB_DITHER_NORMAL,
                         0, 0);
                x_offset += gdk_pixbuf_get_width (pixbuf) + PAD;
        }
        gtk_paint_layout (widget->style,
                          window,
                          state,
                          TRUE,
                          cell_area,
                          widget,
                          "cellrenderernews",
                          cell_area->x + x_offset + cell->xpad,
                          cell_area->y + y_offset + cell->ypad,
                          layout);

        g_object_unref (layout);
}
