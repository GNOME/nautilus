#ifndef NAUTILUS_META_VIEW_H
#define NAUTILUS_META_VIEW_H 1

#include "ntl-view.h"

#define NAUTILUS_TYPE_META_VIEW (nautilus_meta_view_get_type())
#define NAUTILUS_META_VIEW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_META_VIEW, NautilusMetaView))
#define NAUTILUS_META_VIEW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_META_VIEW, NautilusMetaViewClass))
#define NAUTILUS_IS_META_VIEW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_META_VIEW))
#define NAUTILUS_IS_META_VIEW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_META_VIEW))

typedef struct _NautilusMetaView NautilusMetaView;

typedef struct {
  NautilusViewClass parent_spot;

  NautilusViewClass *parent_class;
} NautilusMetaViewClass;

struct _NautilusMetaView {
  NautilusView parent_object;
};

GtkType nautilus_meta_view_get_type(void);
NautilusMetaView *nautilus_meta_view_new(void);
const char *nautilus_meta_view_get_description(NautilusMetaView *nview);

#endif
