#!/bin/sh

# Fetch the GtkPlaces* files but rename the symbols to avoid symbol clashes
# when using the file chooser inside nautilus i.e. when activating the "move to"
# action.
# Also remove/add the neccesary bits to make it work inside nautilus

URL=https://gitlab.gnome.org/GNOME/gtk/raw/gtk-3-24/gtk/
URLUI=https://gitlab.gnome.org/GNOME/gtk/raw/gtk-3-24/gtk/ui/
SUFIX=?h=gtk-3-24

# Since comments are not allowed inside the sed line, this is what it will do
# by order:
# type substitution
# remove marshalers
# replace GtkTrashMonitor API with NautilusTrashMonitor API
# add external localization library after the always there config.h
# and remove the gtk internal P_ and I_ localization, we don't actually
# want localization of this in nautilus
# include gtk.h library after the always there config.h
# and remove all the other types that get included by the general gtk.h
# remove the error when including gtk.h
# load nautilus resources, not gtk resources
# use local sidebar header instead of private gtk one
# in-line replace private gtkfilesystem.h helper function
# ignore shadowed variable which we would treat as compile error

update_file () {
    _source="$1"
    _dest="$2"

    curl "${_source}" | sed \
        -e 's/gtkplacesview/nautilusgtkplacesview/g' \
        -e 's/gtk_places_view/nautilus_gtk_places_view/g' \
        -e 's/GtkPlacesView/NautilusGtkPlacesView/g' \
        -e 's/GTK_PLACES_VIEW/NAUTILUS_GTK_PLACES_VIEW/g' \
        -e 's/GTK_TYPE_PLACES_VIEW/NAUTILUS_TYPE_GTK_PLACES_VIEW/g' \
        -e 's/GTK_IS_PLACES_VIEW/NAUTILUS_IS_GTK_PLACES_VIEW/g' \
        -e 's/G_DECLARE_FINAL_TYPE (NautilusGtkPlacesViewRow, nautilus_gtk_places_view_row, GTK, PLACES_VIEW_ROW, GtkListBoxRow/ G_DECLARE_FINAL_TYPE (NautilusGtkPlacesViewRow, nautilus_gtk_places_view_row, NAUTILUS, GTK_PLACES_VIEW_ROW, GtkListBoxRow/g' \
        -e 's/gtkplacessidebar/nautilusgtkplacessidebar/g' \
        -e 's/gtk_places_sidebar/nautilus_gtk_places_sidebar/g' \
        -e 's/GtkPlacesSidebar/NautilusGtkPlacesSidebar/g' \
        -e 's/GTK_PLACES_SIDEBAR/NAUTILUS_GTK_PLACES_SIDEBAR/g' \
        -e 's/GTK_TYPE_PLACES_SIDEBAR/NAUTILUS_TYPE_GTK_PLACES_SIDEBAR/g' \
        -e 's/GTK_IS_PLACES_SIDEBAR/NAUTILUS_IS_GTK_PLACES_SIDEBAR/g' \
        -e 's/GtkPlacesOpen/NautilusGtkPlacesOpen/g' \
        -e 's/GTK_PLACES_OPEN/NAUTILUS_GTK_PLACES_OPEN/g' \
        -e 's/gtkbookmarksmanager/nautilusgtkbookmarksmanager/g' \
        -e 's/gtk_bookmarks_manager/nautilus_gtk_bookmarks_manager/g' \
        -e 's/GtkBookmarksManager/NautilusGtkBookmarksManager/g' \
        -e 's/GTK_BOOKMARKS_MANAGER/NAUTILUS_GTK_BOOKMARKS_MANAGER/g' \
        -e 's/GTK_TYPE_BOOKMARKS_MANAGER/NAUTILUS_TYPE_GTK_BOOKMARKS_MANAGER/g' \
        -e 's/GTK_IS_BOOKMARKS_MANAGER/NAUTILUS_IS_GTK_BOOKMARKS_MANAGER/g' \
        -e 's/gtksidebarrow/nautilusgtksidebarrow/g' \
        -e 's/gtk_sidebar_row/nautilus_gtk_sidebar_row/g' \
        -e 's/GtkSidebarRow/NautilusGtkSidebarRow/g' \
        -e 's/GTK_SIDEBAR_ROW/NAUTILUS_GTK_SIDEBAR_ROW/g' \
        -e 's/GTK_TYPE_SIDEBAR_ROW/NAUTILUS_TYPE_GTK_SIDEBAR_ROW/g' \
        -e 's/GTK_IS_SIDEBAR_ROW/NAUTILUS_IS_GTK_SIDEBAR_ROW/g' \
        -e 's/G_DECLARE_FINAL_TYPE (NautilusGtkSidebarRow, nautilus_gtk_sidebar_row, GTK, SIDEBAR_ROW, GtkListBoxRow/ G_DECLARE_FINAL_TYPE (NautilusGtkSidebarRow, nautilus_gtk_sidebar_row, NAUTILUS, GTK_SIDEBAR_ROW, GtkListBoxRow/g' \
        -e 's/_gtk_marshal_VOID__STRING_STRING/NULL/g' \
        -e '/gtkmarshalers.h/d' \
        -e '/g_signal_set_va_marshaller/,+2d' \
        -e 's/_gtk_marshal_VOID__OBJECT_FLAGS/NULL/g' \
        -e 's/_gtk_marshal_VOID__OBJECT_POINTER_INT/NULL/g' \
        -e 's/_gtk_marshal_VOID__OBJECT_OBJECT_OBJECT/NULL/g' \
        -e 's/_gtk_marshal_INT__OBJECT_OBJECT_POINTER/NULL/g' \
        -e 's/_gtk_marshal_INT__INT/NULL/g' \
        -e 's/gtktrashmonitor.h/nautilus-trash-monitor.h/g' \
        -e 's/GtkTrashMonitor/NautilusTrashMonitor/g' \
        -e "s/_gtk_trash_monitor_get_icon (\(.*\))/nautilus_trash_monitor_get_symbolic_icon ()/g" \
        -e "s/_gtk_trash_monitor_get_has_trash (\(.*\))/!nautilus_trash_monitor_is_empty ()/g" \
        -e 's/_gtk_trash_monitor_get/nautilus_trash_monitor_get/g' \
        -e '/"config.h"/a #include <glib\/gi18n.h>' \
        -e "s/P_(\(.*\))/\1/" \
        -e "s/I_(\(.*\))/\1/" \
        -e '/"config.h"/a #include <gtk\/gtk.h>' \
        -e '/gtktypebuiltins.h/d' \
        -e '/gtkintl.h/d' \
        -e '/<gtk\/gtkbox.h>/d' \
        -e '/"gtkbox\(.*\).h"/d' \
        -e '/"gtkbu\(.*\).h"/d' \
        -e '/"gtkc\(.*\).h"/d' \
        -e '/"gtkd\(.*\).h"/d' \
        -e '/"gtke\(.*\).h"/d' \
        -e '/"gtkf\(.*\).h"/d' \
        -e '/"gtkg\(.*\).h"/d' \
        -e '/"gtki\(.*\).h"/d' \
        -e '/"gtkl\(.*\).h"/d' \
        -e '/"gtkm\(.*\).h"/d' \
        -e '/"gtkpo\(.*\).h"/d' \
        -e '/"gtkr\(.*\).h"/d' \
        -e '/"gtksc\(.*\).h"/d' \
        -e '/"gtkse\(.*\).h"/d' \
        -e '/"gtksi\(.*\).h"/d' \
        -e '/"gtksp\(.*\).h"/d' \
        -e '/"gtkst\(.*\).h"/d' \
        -e '/"gtkt\(.*\).h"/d' \
        -e '/"gtkw\(.*\).h"/d' \
        -e '/#error/d' \
        -e 's/gtk\/libgtk/gnome\/nautilus\/gtk/g' \
        -e 's/<gtk\/nautilusgtkplacessidebar.h>/"nautilusgtkplacessidebar.h"'/g \
        -e 's/_gtk_file_info_consider_as_directory (info)/(g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY || g_file_info_get_file_type (info) == G_FILE_TYPE_MOUNTABLE || g_file_info_get_file_type (info) == G_FILE_TYPE_SHORTCUT)/g' \
        -e '/#include "nautilus-trash-monitor.h"/a #pragma GCC diagnostic ignored "-Wshadow"' \
        > "${_dest}"
}

update_file "${URL}/gtkplacesview.c${SUFIX}" "nautilusgtkplacesview.c"
update_file "${URL}/gtkplacesviewprivate.h${SUFIX}" "nautilusgtkplacesviewprivate.h"
update_file "${URLUI}/gtkplacesview.ui${SUFIX}" "nautilusgtkplacesview.ui"
update_file "${URL}/gtkplacessidebar.c${SUFIX}" "nautilusgtkplacessidebar.c"
update_file "${URL}/gtkplacessidebarprivate.h${SUFIX}" "nautilusgtkplacessidebarprivate.h"
update_file "${URL}/gtkplacessidebar.h${SUFIX}" "nautilusgtkplacessidebar.h"
update_file "${URL}/gtkbookmarksmanager.c${SUFIX}" "nautilusgtkbookmarksmanager.c"
update_file "${URL}/gtkbookmarksmanager.h${SUFIX}" "nautilusgtkbookmarksmanager.h"
update_file "${URL}/gtkplacesviewrow.c${SUFIX}" "nautilusgtkplacesviewrow.c"
update_file "${URL}/gtkplacesviewrowprivate.h${SUFIX}" "nautilusgtkplacesviewrowprivate.h"
update_file "${URLUI}/gtkplacesviewrow.ui${SUFIX}" "nautilusgtkplacesviewrow.ui"
update_file "${URL}/gtksidebarrow.c${SUFIX}" "nautilusgtksidebarrow.c"
update_file "${URL}/gtksidebarrowprivate.h${SUFIX}" "nautilusgtksidebarrowprivate.h"
update_file "${URLUI}/gtksidebarrow.ui${SUFIX}" "nautilusgtksidebarrow.ui"
