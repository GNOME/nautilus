#pragma once

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_LIST_ZOOM_LEVEL_N_ENTRIES (NAUTILUS_LIST_ZOOM_LEVEL_LARGER + 1)
#define NAUTILUS_GRID_ZOOM_LEVEL_N_ENTRIES (NAUTILUS_GRID_ZOOM_LEVEL_LARGEST + 1)

/* Maximum size of an icon that the icon factory will ever produce */
#define NAUTILUS_ICON_MAXIMUM_SIZE     320

#define NAUTILUS_TYPE_ICON_INFO (nautilus_icon_info_get_type ())
G_DECLARE_FINAL_TYPE (NautilusIconInfo, nautilus_icon_info, NAUTILUS, ICON_INFO, GObject)

NautilusIconInfo *    nautilus_icon_info_new_for_pixbuf               (GdkPixbuf         *pixbuf,
								       int                scale);
NautilusIconInfo *    nautilus_icon_info_lookup                       (GIcon             *icon,
								       int                size,
								       int                scale);
gboolean              nautilus_icon_info_is_fallback                  (NautilusIconInfo  *icon);
GdkPaintable *        nautilus_icon_info_get_paintable                (NautilusIconInfo  *icon);
GdkTexture *          nautilus_icon_info_get_texture                  (NautilusIconInfo  *icon);
const char *          nautilus_icon_info_get_used_name                (NautilusIconInfo  *icon);

void                  nautilus_icon_info_clear_caches                 (void);

G_END_DECLS
