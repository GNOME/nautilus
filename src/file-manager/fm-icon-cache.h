#ifndef FM_ICON_CACHE_H
#define FM_ICON_CACHE_H 1

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glib.h>

typedef struct _FMIconCache FMIconCache;

FMIconCache *fm_icon_cache_new(const char *theme_name);
void fm_icon_cache_destroy(FMIconCache *fmic);

void fm_icon_cache_set_theme(FMIconCache *fmic, const char *theme_name);
GdkPixbuf *fm_icon_cache_get_icon(FMIconCache *fmic, const GnomeVFSFileInfo *info); /* Ownership of a refcount in
										       this pixbuf comes with the deal */
#endif
