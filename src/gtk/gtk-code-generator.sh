#!/bin/sh

set -e

# Fetch the GtkPlacesView files but rename the symbols to avoid symbol clashes
# when using the file chooser inside nautilus i.e. when activating the "move to"
# action.
# Also remove/add the neccesary bits to make it work inside nautilus

URL=https://gitlab.gnome.org/GNOME/gtk/raw/master/gtk/
URLUI=https://gitlab.gnome.org/GNOME/gtk/raw/master/gtk/ui/
SUFFIX=

# Since comments are not allowed inside the sed line, this is what it will do
# by order:
# type substitution
# remove marshalers
# add external localization library after the always there config.h
# and remove the gtk internal P_ and I_ localization, we don't actually
# want localization of this in nautilus
# include gtk.h library after the always there config.h
# and remove all the other types that get included by the general gtk.h
# remove the error when including gtk.h
# load nautilus resources, not gtk resources

update_file () {
    _source="$1"
    _dest="$2"

    curl "${_source}" | sed \
        -e 's/GtkPlacesSidebar/NautilusGtkPlacesSidebar/g'                          \
        -e 's/gtk_places_sidebar/nautilus_gtk_places_sidebar/g'                     \
        -e 's/GTK_PLACES_SIDEBAR/NAUTILUS_GTK_PLACES_SIDEBAR/g'                     \
        -e 's/GTK_TYPE_PLACES_SIDEBAR/NAUTILUS_TYPE_GTK_PLACES_VIEW/g'              \
        -e 's/GTK_IS_PLACES_SIDEBAR/NAUTILUS_GTK_IS_PLACES_SIDEBAR/g'               \
        -e 's/gtkplacesview/nautilusgtkplacesview/g'                                \
        -e 's/gtk_places_view/nautilus_gtk_places_view/g'                           \
        -e 's/GtkPlacesView/NautilusGtkPlacesView/g'                                \
        -e 's/GTK_PLACES_VIEW/NAUTILUS_GTK_PLACES_VIEW/g'                           \
        -e 's/GTK_TYPE_PLACES_VIEW/NAUTILUS_TYPE_GTK_PLACES_VIEW/g'                 \
        -e 's/GTK_IS_PLACES_VIEW/NAUTILUS_IS_GTK_PLACES_VIEW/g'                     \
        -e 's/_gtk_marshal_VOID__STRING_STRING/NULL/g'                              \
        -e '/gtkmarshalers.h/d'                                                     \
        -e '/"config.h"/a #include <glib\/gi18n.h>'                                 \
        -e "s/P_(\(.*\))/\1/"                                                       \
        -e "s/I_(\(.*\))/\1/"                                                       \
        -e '/"config.h"/a #include <gtk\/gtk.h>'                                    \
        -e '/gtktypebuiltins.h/d'                                                   \
        -e '/gtkplacessidebar.h/d'                                                  \
        -e '/gtkintl.h/d'                                                           \
        -e '/gtkbox.h/d'                                                            \
        -e '/#error/d'                                                              \
        -e 's/gtk\/libgtk/gnome\/nautilus\/gtk/g'                                   \
        -e '/#include "gtk.*\.h"/d'                                                 \
        -e 's|<gtk/gtkplacessidebarprivate.h>|"nautilusgtkplacessidebarprivate.h"|' \
        -e 's/GTK_PARAM/G_PARAM/g'                                                  \
        > "${_dest}"
}

update_file "${URL}/gtkplacesview.c${SUFFIX}" "nautilusgtkplacesview.c"
update_file "${URL}/gtkplacesviewprivate.h${SUFFIX}" "nautilusgtkplacesviewprivate.h"
update_file "${URLUI}/gtkplacesview.ui${SUFFIX}" "nautilusgtkplacesview.ui"

# Since comments are not allowed inside the sed line, this is what it will do
# by order:
# type substitution
# use the correct prefixes for type definition
# add external localization library after the always there config.h
# and remove the gtk internal P_ and I_ localization, we don't actually
# want localization of this in nautilus
# include gtk.h library after the always there config.h
# and remove all the other types that get included by the general gtk.h
# remove the error when including gtk.h
# load nautilus resources, not gtk resources
update_file () {
    _source="$1"
    _dest="$2"

    curl "${_source}" | sed \
        -e 's/gtkplacesviewrow/nautilusgtkplacesviewrow/g' \
        -e 's/gtk_places_view_row/nautilus_gtk_places_view_row/g' \
        -e 's/GtkPlacesViewRow/NautilusGtkPlacesViewRow/g' \
        -e 's/GTK_PLACES_VIEW_ROW/NAUTILUS_GTK_PLACES_VIEW_ROW/g' \
        -e 's/GTK_TYPE_PLACES_VIEW_ROW/NAUTILUS_TYPE_GTK_PLACES_VIEW_ROW/g' \
        -e 's/GTK_IS_PLACES_VIEW_ROW/NAUTILUS_IS_GTK_PLACES_VIEW_ROW/g' \
        -e 's/G_DECLARE_FINAL_TYPE (NautilusGtkPlacesViewRow, nautilus_gtk_places_view_row, GTK, PLACES_VIEW_ROW, GtkListBoxRow/ G_DECLARE_FINAL_TYPE (NautilusGtkPlacesViewRow, nautilus_gtk_places_view_row, NAUTILUS, GTK_PLACES_VIEW_ROW, GtkListBoxRow/g' \
        -e '/"config.h"/a #include <glib\/gi18n.h>' \
        -e "s/P_(\(.*\))/\1/" \
        -e "s/I_(\(.*\))/\1/" \
        -e '/"config.h"/a #include <gtk\/gtk.h>' \
        -e '/gtksizegroup.h/d' \
        -e '/gtkwidget.h/d' \
        -e '/gtklistbox.h/d' \
        -e '/#error /d' \
        -e 's/gtk\/libgtk/gnome\/nautilus\/gtk/g' \
 > "${_dest}"
}

update_file "${URL}/gtkplacesviewrow.c${SUFFIX}" "nautilusgtkplacesviewrow.c"
update_file "${URL}/gtkplacesviewrowprivate.h${SUFFIX}" "nautilusgtkplacesviewrowprivate.h"
update_file "${URLUI}/gtkplacesviewrow.ui${SUFFIX}" "nautilusgtkplacesviewrow.ui"

update_file () {
    _source="$1"
    _dest="$2"

    curl "${_source}" | sed                                                  \
        -e 's/GtkPlacesSidebar/NautilusGtkPlacesSidebar/g'                   \
        -e 's/gtk_places_sidebar/nautilus_gtk_places_sidebar/g'              \
        -e 's/GTK_PLACES_SIDEBAR/NAUTILUS_GTK_PLACES_SIDEBAR/g'              \
        -e 's/GTK_TYPE_PLACES_SIDEBAR/NAUTILUS_TYPE_GTK_PLACES_SIDEBAR/g'    \
        -e 's/GTK_IS_PLACES_SIDEBAR/NAUTILUS_GTK_IS_PLACES_SIDEBAR/g'        \
                                                                             \
        -e 's/GtkTrashMonitor/NautilusGtkTrashMonitor/g'                     \
        -e 's/gtk_trash_monitor/nautilus_gtk_trash_monitor/g'                \
        -e 's/GTK_TRASH_MONITOR/NAUTILUS_GTK_TRASH_MONITOR/g'                \
        -e 's/GTK_TYPE_TRASH_MONITOR/NAUTILUS_TYPE_GTK_TRASH_MONITOR/g'      \
        -e 's/GTK_IS_TRASH_MONITOR/NAUTILUS_GTK_IS_TRASH_MONITOR/g'          \
                                                                             \
        -e 's/gtksidebarrow/nautilusgtksidebarrow/g'                         \
        -e 's/GtkSidebarRow/NautilusGtkSidebarRow/g'                         \
        -e 's/gtk_sidebar_row/nautilus_gtk_sidebar_row/g'                    \
        -e 's/GTK_SIDEBAR_ROW/NAUTILUS_GTK_SIDEBAR_ROW/g'                    \
        -e 's/GTK_TYPE_SIDEBAR_ROW/NAUTILUS_TYPE_GTK_SIDEBAR_ROW/g'          \
        -e 's/GTK_IS_SIDEBAR_ROW/NAUTILUS_GTK_IS_SIDEBAR_ROW/g'              \
                                                                             \
        -e 's|<gtk/.*\.h>|<gtk/gtk.h>|g'                                     \
        -e '/#include <glib\/gi18n-lib\.h>/d'                                \
        -e '/"config.h"/a #include <glib\/gi18n.h>'                          \
        -e '/"config.h"/a #include <gtk\/gtk.h>'                             \
        -e 's/gtkbookmarksmanager\.h/nautilusgtkbookmarksmanager.h/'         \
        -e 's/gtkfilesystem\.h/nautilusgtkfilesystem.h/'                     \
        -e 's/gtkplacessidebarprivate\.h/nautilusgtkplacessidebarprivate.h/' \
        -e 's/gtktrashmonitor\.h/nautilusgtktrashmonitor.h/'                 \
        -e 's|#include "gtklistbox\.h"|#include <gtk/gtk.h>|'                \
        -e '/#include "g[dt]k.*\.h"/d'                                       \
        -e 's/_gtk_marshal_.*,/NULL,/g'                                      \
        -e "s/I_(\(.*\))/\1/"                                                \
        -e "s/P_(\(.*\))/\1/"                                                \
        -e 's/GTK_PARAM/G_PARAM/g'                                           \
        -e 's/gtk\/libgtk/gnome\/nautilus\/gtk/g'                            \
    > "${_dest}"
}
update_file "${URL}/gtkplacessidebar.c${SUFFIX}" "nautilusgtkplacessidebar.c"
update_file "${URL}/gtkplacessidebarprivate.h${SUFFIX}" "nautilusgtkplacessidebarprivate.h"

update_file "${URL}/gtksidebarrow.c${SUFFIX}" "nautilusgtksidebarrow.c"
update_file "${URL}/gtksidebarrowprivate.h${SUFFIX}" "nautilusgtksidebarrowprivate.h"
update_file "${URLUI}/gtksidebarrow.ui${SUFFIX}" "nautilusgtksidebarrow.ui"

update_file "${URL}/gtkbookmarksmanager.c${SUFFIX}" "nautilusgtkbookmarksmanager.c"
update_file "${URL}/gtkbookmarksmanager.h${SUFFIX}" "nautilusgtkbookmarksmanager.h"

update_file "${URL}/gtktrashmonitor.c${SUFFIX}" "nautilusgtktrashmonitor.c"
update_file "${URL}/gtktrashmonitor.h${SUFFIX}" "nautilusgtktrashmonitor.h"

update_file "${URL}/gtkfilesystem.c${SUFFIX}" "nautilusgtkfilesystem.c"
update_file "${URL}/gtkfilesystem.h${SUFFIX}" "nautilusgtkfilesystem.h"
