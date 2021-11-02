/* fm-icon-container.h - the container widget for file manager icons
 *
 *  Copyright (C) 2002 Sun Microsystems, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Author: Michael Meeks <michael@ximian.com>
 */

#include "nautilus-canvas-view-container.h"

#include <eel/eel-glib-extensions.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "nautilus-canvas-view.h"
#include "nautilus-enums.h"
#include "nautilus-global-preferences.h"
#include "nautilus-thumbnails.h"

struct _NautilusCanvasViewContainer
{
    NautilusCanvasContainer parent;

    NautilusCanvasView *view;
};

G_DEFINE_TYPE (NautilusCanvasViewContainer, nautilus_canvas_view_container, NAUTILUS_TYPE_CANVAS_CONTAINER);

static GQuark attribute_none_q;

static NautilusCanvasView *
get_canvas_view (NautilusCanvasContainer *container)
{
    /* Type unsafe comparison for performance */
    return ((NautilusCanvasViewContainer *) container)->view;
}

static NautilusIconInfo *
nautilus_canvas_view_container_get_icon_images (NautilusCanvasContainer *container,
                                                NautilusCanvasIconData  *data,
                                                int                      size,
                                                gboolean                 for_drag_accept,
                                                gboolean                *is_thumbnail)
{
    NautilusCanvasView *canvas_view;
    NautilusFile *file;
    NautilusFileIconFlags flags;
    NautilusIconInfo *icon_info;
    gint scale;

    file = (NautilusFile *) data;

    g_assert (NAUTILUS_IS_FILE (file));
    canvas_view = get_canvas_view (container);
    g_return_val_if_fail (canvas_view != NULL, NULL);

    flags = NAUTILUS_FILE_ICON_FLAGS_USE_EMBLEMS |
            NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS;

    if (for_drag_accept)
    {
        flags |= NAUTILUS_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
    }

    scale = gtk_widget_get_scale_factor (GTK_WIDGET (canvas_view));
    icon_info = nautilus_file_get_icon (file, size, scale, flags);

    if (is_thumbnail != NULL)
    {
        g_autofree gchar *thumbnail_path = NULL;

        thumbnail_path = nautilus_file_get_thumbnail_path (file);
        *is_thumbnail = (thumbnail_path != NULL &&
                         nautilus_file_should_show_thumbnail (file));
    }

    return icon_info;
}

static char *
nautilus_canvas_view_container_get_icon_description (NautilusCanvasContainer *container,
                                                     NautilusCanvasIconData  *data)
{
    NautilusFile *file;
    char *mime_type;
    const char *description;

    file = NAUTILUS_FILE (data);
    g_assert (NAUTILUS_IS_FILE (file));

    mime_type = nautilus_file_get_mime_type (file);
    description = g_content_type_get_description (mime_type);
    g_free (mime_type);
    return g_strdup (description);
}

static void
nautilus_canvas_view_container_prioritize_thumbnailing (NautilusCanvasContainer *container,
                                                        NautilusCanvasIconData  *data)
{
    NautilusFile *file;
    char *uri;

    file = (NautilusFile *) data;

    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_thumbnailing (file))
    {
        uri = nautilus_file_get_uri (file);
        nautilus_thumbnail_prioritize (uri);
        g_free (uri);
    }
}

static GQuark *
get_quark_from_strv (gchar **value)
{
    GQuark *quark;
    int i;

    quark = g_new0 (GQuark, g_strv_length (value) + 1);
    for (i = 0; value[i] != NULL; ++i)
    {
        quark[i] = g_quark_from_string (value[i]);
    }

    return quark;
}

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static GQuark *
nautilus_canvas_view_container_get_icon_text_attributes_from_preferences (void)
{
    GQuark *attributes;
    gchar **value;

    value = g_settings_get_strv (nautilus_icon_view_preferences,
                                 NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);
    attributes = get_quark_from_strv (value);
    g_strfreev (value);

    /* We don't need to sanity check the attributes list even though it came
     * from preferences.
     *
     * There are 2 ways that the values in the list could be bad.
     *
     * 1) The user picks "bad" values.  "bad" values are those that result in
     *    there being duplicate attributes in the list.
     *
     * 2) Value stored in GConf are tampered with.  Its possible physically do
     *    this by pulling the rug underneath GConf and manually editing its
     *    config files.  Its also possible to use a third party GConf key
     *    editor and store garbage for the keys in question.
     *
     * Thankfully, the Nautilus preferences machinery deals with both of
     * these cases.
     *
     * In the first case, the preferences dialog widgetry prevents
     * duplicate attributes by making "bad" choices insensitive.
     *
     * In the second case, the preferences getter (and also the auto storage) for
     * string_array values are always valid members of the enumeration associated
     * with the preference.
     *
     * So, no more error checking on attributes is needed here and we can return
     * a the auto stored value.
     */
    return attributes;
}

static int
quarkv_length (GQuark *attributes)
{
    int i;
    i = 0;
    while (attributes[i] != 0)
    {
        i++;
    }
    return i;
}

/**
 * nautilus_canvas_view_get_icon_text_attribute_names:
 *
 * Get a list representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Don't free the result.
 * @view: NautilusCanvasView to query.
 *
 **/
static GQuark *
nautilus_canvas_view_container_get_icon_text_attribute_names (NautilusCanvasContainer *container,
                                                              int                     *len)
{
    GQuark *attributes;
    int piece_count;

    const int pieces_by_level[] =
    {
        1,              /* NAUTILUS_ZOOM_LEVEL_SMALL */
        2,              /* NAUTILUS_ZOOM_LEVEL_STANDARD */
        3,              /* NAUTILUS_ZOOM_LEVEL_LARGE */
        3,              /* NAUTILUS_ZOOM_LEVEL_LARGER */
    };

    piece_count = pieces_by_level[nautilus_canvas_container_get_zoom_level (container)];

    attributes = nautilus_canvas_view_container_get_icon_text_attributes_from_preferences ();

    *len = MIN (piece_count, quarkv_length (attributes));

    return attributes;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
nautilus_canvas_view_container_get_icon_text (NautilusCanvasContainer  *container,
                                              NautilusCanvasIconData   *data,
                                              char                    **editable_text,
                                              char                    **additional_text,
                                              gboolean                  include_invisible)
{
    GQuark *attributes;
    char *text_array[4];
    int i, j, num_attributes;
    NautilusCanvasView *canvas_view;
    NautilusFile *file;
    gboolean use_additional;

    file = NAUTILUS_FILE (data);

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (editable_text != NULL);
    canvas_view = get_canvas_view (container);
    g_return_if_fail (canvas_view != NULL);

    use_additional = (additional_text != NULL);

    /* Strip the suffix for nautilus object xml files. */
    *editable_text = nautilus_file_get_display_name (file);

    if (!use_additional)
    {
        return;
    }

    /* Find out what attributes go below each icon. */
    attributes = nautilus_canvas_view_container_get_icon_text_attribute_names (container,
                                                                               &num_attributes);

    /* Get the attributes. */
    j = 0;
    for (i = 0; i < num_attributes; ++i)
    {
        char *text;
        if (attributes[i] == attribute_none_q)
        {
            continue;
        }
        text = nautilus_file_get_string_attribute_q (file, attributes[i]);
        if (text == NULL)
        {
            continue;
        }
        text_array[j++] = text;
    }
    text_array[j] = NULL;

    /* Return them. */
    if (j == 0)
    {
        *additional_text = NULL;
    }
    else if (j == 1)
    {
        /* Only one item, avoid the strdup + free */
        *additional_text = text_array[0];
    }
    else
    {
        *additional_text = g_strjoinv ("\n", text_array);

        for (i = 0; i < j; i++)
        {
            g_free (text_array[i]);
        }
    }

    g_free (attributes);
}

static int
nautilus_canvas_view_container_compare_icons (NautilusCanvasContainer *container,
                                              NautilusCanvasIconData  *icon_a,
                                              NautilusCanvasIconData  *icon_b)
{
    NautilusCanvasView *canvas_view;

    canvas_view = get_canvas_view (container);
    g_return_val_if_fail (canvas_view != NULL, 0);

    /* Type unsafe comparisons for performance */
    return nautilus_canvas_view_compare_files (canvas_view,
                                               (NautilusFile *) icon_a,
                                               (NautilusFile *) icon_b);
}

static int
nautilus_canvas_view_container_compare_icons_by_name (NautilusCanvasContainer *container,
                                                      NautilusCanvasIconData  *icon_a,
                                                      NautilusCanvasIconData  *icon_b)
{
    return nautilus_file_compare_for_sort
               (NAUTILUS_FILE (icon_a),
               NAUTILUS_FILE (icon_b),
               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
               FALSE, FALSE);
}

static void
nautilus_canvas_view_container_class_init (NautilusCanvasViewContainerClass *klass)
{
    NautilusCanvasContainerClass *ic_class;

    ic_class = &klass->parent_class;

    attribute_none_q = g_quark_from_static_string ("none");

    ic_class->get_icon_text = nautilus_canvas_view_container_get_icon_text;
    ic_class->get_icon_images = nautilus_canvas_view_container_get_icon_images;
    ic_class->get_icon_description = nautilus_canvas_view_container_get_icon_description;
    ic_class->prioritize_thumbnailing = nautilus_canvas_view_container_prioritize_thumbnailing;

    ic_class->compare_icons = nautilus_canvas_view_container_compare_icons;
    ic_class->compare_icons_by_name = nautilus_canvas_view_container_compare_icons_by_name;
}

static void
nautilus_canvas_view_container_init (NautilusCanvasViewContainer *canvas_container)
{
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (canvas_container)),
                                 GTK_STYLE_CLASS_VIEW);
}

NautilusCanvasContainer *
nautilus_canvas_view_container_construct (NautilusCanvasViewContainer *canvas_container,
                                          NautilusCanvasView          *view)
{
    AtkObject *atk_obj;

    g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), NULL);

    canvas_container->view = view;
    atk_obj = gtk_widget_get_accessible (GTK_WIDGET (canvas_container));
    atk_object_set_name (atk_obj, _("Icon View"));

    return NAUTILUS_CANVAS_CONTAINER (canvas_container);
}

NautilusCanvasContainer *
nautilus_canvas_view_container_new (NautilusCanvasView *view)
{
    return nautilus_canvas_view_container_construct
               (g_object_new (NAUTILUS_TYPE_CANVAS_VIEW_CONTAINER, NULL),
               view);
}
