#ifndef NAUTILUS_CONTENT_VIEW_H
#define NAUTILUS_CONTENT_VIEW_H 1

#include "ntl-view.h"

#define NAUTILUS_TYPE_CONTENT_VIEW (nautilus_content_view_get_type())
#define NAUTILUS_CONTENT_VIEW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CONTENT_VIEW, NautilusContentView))
#define NAUTILUS_CONTENT_VIEW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONTENT_VIEW, NautilusContentViewClass))
#define NAUTILUS_IS_CONTENT_VIEW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW))
#define NAUTILUS_IS_CONTENT_VIEW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CONTENT_VIEW))

typedef struct {
  NautilusViewClass parent_spot;

  NautilusViewClass *parent_class;
} NautilusContentViewClass;

typedef struct {
  NautilusView parent_object;
} NautilusContentView;

GtkType nautilus_content_view_get_type(void);
NautilusContentView *nautilus_content_view_new(void);

#endif
