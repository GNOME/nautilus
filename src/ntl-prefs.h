#ifndef NTL_PREFS_H
#define NTL_PREFS_H 1

#include <gtk/gtk.h>

typedef struct {
  guchar window_alwaysnew : 1;
  guchar window_search_existing : 1;

  GSList *global_meta_views;
} NautilusPrefsOld;

extern NautilusPrefsOld nautilus_prefs;

void nautilus_prefs_load(void);
void nautilus_prefs_save(void);
void nautilus_prefs_ui_show(GtkWindow *transient_for);

#endif
