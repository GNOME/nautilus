#ifndef NTL_TYPES_H
#define NTL_TYPES_H 1

#include <gtk/gtk.h>

typedef char *NautilusLocationReference;

typedef struct {
  NautilusLocationReference requested_uri, actual_uri;
  NautilusLocationReference referring_uri, actual_referring_uri;
  char *content_type;
  GtkWidget *requesting_view;
} NautilusNavigationInfo;

#endif
