#ifndef NTL_TYPES_H
#define NTL_TYPES_H 1

#include <gtk/gtk.h>
#include <libnautilus/libnautilus.h>

typedef char *NautilusLocationReference;

typedef struct {
  Nautilus_NavigationInfo navinfo;

  GtkWidget *requesting_view;

  char *content_iid;
  GSList *meta_iids;
} NautilusNavigationInfo;

#endif
