#!/bin/sh

# Fetch the GtkPlacesView files but rename the symbols to avoid symbol clashes
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
        -e 's/gtkplacesview/nautilusgtkplacesview/g' \
        -e 's/gtk_places_view/nautilus_gtk_places_view/g' \
        -e 's/GtkPlacesView/NautilusGtkPlacesView/g' \
        -e 's/GTK_PLACES_VIEW/NAUTILUS_GTK_PLACES_VIEW/g' \
        -e 's/GTK_TYPE_PLACES_VIEW/NAUTILUS_TYPE_GTK_PLACES_VIEW/g' \
        -e 's/GTK_IS_PLACES_VIEW/NAUTILUS_IS_GTK_PLACES_VIEW/g' \
        -e 's/_gtk_marshal_VOID__STRING_STRING/NULL/g' \
        -e '/gtkmarshalers.h/d' \
        -e '/"config.h"/a #include <glib\/gi18n.h>' \
        -e "s/P_(\(.*\))/\1/" \
        -e "s/I_(\(.*\))/\1/" \
        -e '/"config.h"/a #include <gtk\/gtk.h>' \
        -e '/gtktypebuiltins.h/d' \
        -e '/gtkplacessidebar.h/d' \
        -e '/gtkintl.h/d' \
        -e '/gtkbox.h/d' \
        -e '/#error/d' \
        -e 's/gtk\/libgtk/gnome\/nautilus\/gtk/g' \
        > "${_dest}"
}

update_file "${URL}/gtkplacesview.c${SUFIX}" "nautilusgtkplacesview.c"
update_file "${URL}/gtkplacesviewprivate.h${SUFIX}" "nautilusgtkplacesviewprivate.h"
update_file "${URLUI}/gtkplacesview.ui${SUFIX}" "nautilusgtkplacesview.ui"

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

update_file "${URL}/gtkplacesviewrow.c${SUFIX}" "nautilusgtkplacesviewrow.c"
update_file "${URL}/gtkplacesviewrowprivate.h${SUFIX}" "nautilusgtkplacesviewrowprivate.h"
update_file "${URLUI}/gtkplacesviewrow.ui${SUFIX}" "nautilusgtkplacesviewrow.ui"
