#include <libnautilus/libnautilus.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>

typedef struct {
  NautilusViewFrame *view_frame;

  GtkWidget *ctree;
  HyperbolaDocTree *doc_tree;

  gint notify_count;
} HyperbolaNavigationIndex;

BonoboObject *hyperbola_navigation_index_new(void)
{
  return NULL;
}
