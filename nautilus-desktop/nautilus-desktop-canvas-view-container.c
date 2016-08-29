/* nautilus-desktop-canvas-view-container.c
 *
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
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

#include "nautilus-desktop-canvas-view-container.h"
#include "nautilus-desktop-icon-file.h"

struct _NautilusDesktopCanvasViewContainer
{
    NautilusCanvasViewContainer parent_instance;
};

G_DEFINE_TYPE (NautilusDesktopCanvasViewContainer, nautilus_desktop_canvas_view_container, NAUTILUS_TYPE_CANVAS_VIEW_CONTAINER)

/* Sort as follows:
 *   0) home link
 *   1) network link
 *   2) mount links
 *   3) other
 *   4) trash link
 */
typedef enum
{
    SORT_HOME_LINK,
    SORT_NETWORK_LINK,
    SORT_MOUNT_LINK,
    SORT_OTHER,
    SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (NautilusFile *file)
{
    NautilusDesktopLink *link;
    SortCategory category;

    category = SORT_OTHER;

    if (NAUTILUS_IS_DESKTOP_ICON_FILE (file))
    {
        link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));
        if (link != NULL)
        {
            switch (nautilus_desktop_link_get_link_type (link))
            {
                case NAUTILUS_DESKTOP_LINK_HOME:
                {
                    category = SORT_HOME_LINK;
                }
                break;

                case NAUTILUS_DESKTOP_LINK_MOUNT:
                {
                    category = SORT_MOUNT_LINK;
                }
                break;

                case NAUTILUS_DESKTOP_LINK_TRASH:
                {
                    category = SORT_TRASH_LINK;
                }
                break;

                case NAUTILUS_DESKTOP_LINK_NETWORK:
                {
                    category = SORT_NETWORK_LINK;
                }
                break;

                default:
                {
                    category = SORT_OTHER;
                }
                break;
            }
            g_object_unref (link);
        }
    }

    return category;
}

static int
real_compare_icons (NautilusCanvasContainer *container,
                    NautilusCanvasIconData  *data_a,
                    NautilusCanvasIconData  *data_b)
{
    NautilusFile *file_a;
    NautilusFile *file_b;
    NautilusFilesView *directory_view;
    SortCategory category_a, category_b;

    file_a = (NautilusFile *) data_a;
    file_b = (NautilusFile *) data_b;

    directory_view = NAUTILUS_FILES_VIEW (NAUTILUS_CANVAS_VIEW_CONTAINER (container)->view);
    g_return_val_if_fail (directory_view != NULL, 0);

    category_a = get_sort_category (file_a);
    category_b = get_sort_category (file_b);

    if (category_a == category_b)
    {
        return nautilus_file_compare_for_sort (file_a,
                                               file_b,
                                               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                               nautilus_files_view_should_sort_directories_first (directory_view),
                                               FALSE);
    }

    if (category_a < category_b)
    {
        return -1;
    }
    else
    {
        return +1;
    }
}

static void
real_get_icon_text (NautilusCanvasContainer  *container,
                    NautilusCanvasIconData   *data,
                    char                    **editable_text,
                    char                    **additional_text,
                    gboolean                  include_invisible)
{
    NautilusFile *file;
    gboolean use_additional;

    file = NAUTILUS_FILE (data);

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (editable_text != NULL);

    use_additional = (additional_text != NULL);

    /* Strip the suffix for nautilus object xml files. */
    *editable_text = nautilus_file_get_display_name (file);

    if (!use_additional)
    {
        return;
    }

    if (NAUTILUS_IS_DESKTOP_ICON_FILE (file) ||
        nautilus_file_is_nautilus_link (file))
    {
        /* Don't show the normal extra information for desktop icons,
         * or desktop files, it doesn't make sense.
         */
        *additional_text = NULL;

        return;
    }

    return NAUTILUS_CANVAS_CONTAINER_CLASS (nautilus_desktop_canvas_view_container_parent_class)->get_icon_text (container,
                                                                                                                 data,
                                                                                                                 editable_text,
                                                                                                                 additional_text,
                                                                                                                 include_invisible);
}

static char *
real_get_icon_description (NautilusCanvasContainer *container,
                           NautilusCanvasIconData  *data)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (data);
    g_assert (NAUTILUS_IS_FILE (file));

    if (NAUTILUS_IS_DESKTOP_ICON_FILE (file))
    {
        return NULL;
    }

    return NAUTILUS_CANVAS_CONTAINER_CLASS (nautilus_desktop_canvas_view_container_parent_class)->get_icon_description (container,
                                                                                                                        data);
}

NautilusDesktopCanvasViewContainer *
nautilus_desktop_canvas_view_container_new (void)
{
    return g_object_new (NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW_CONTAINER, NULL);
}

static void
nautilus_desktop_canvas_view_container_class_init (NautilusDesktopCanvasViewContainerClass *klass)
{
    NautilusCanvasContainerClass *container_class = NAUTILUS_CANVAS_CONTAINER_CLASS (klass);

    container_class->get_icon_description = real_get_icon_description;
    container_class->get_icon_text = real_get_icon_text;
    container_class->compare_icons = real_compare_icons;
}

static void
nautilus_desktop_canvas_view_container_init (NautilusDesktopCanvasViewContainer *self)
{
}
