#include "ntl-meta-view.h"
#include <gtk/gtksignal.h>

static void nautilus_meta_view_class_init (NautilusMetaViewClass *klass);
static void nautilus_meta_view_init (NautilusMetaView *view);

GtkType
nautilus_meta_view_get_type(void)
{
  static guint view_type = 0;

  if (!view_type)
    {
      GtkTypeInfo view_info = {
	"NautilusMetaView",
	sizeof(NautilusMetaView),
	sizeof(NautilusMetaViewClass),
	(GtkClassInitFunc) nautilus_meta_view_class_init,
	(GtkObjectInitFunc) nautilus_meta_view_init
      };

      view_type = gtk_type_unique (nautilus_view_get_type(), &view_info);
    }

  return view_type;
}

static void
nautilus_meta_view_class_init (NautilusMetaViewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
}

static void
nautilus_meta_view_init (NautilusMetaView *view)
{
}

NautilusMetaView *
nautilus_meta_view_new(void)
{
  return NAUTILUS_META_VIEW (gtk_type_new (nautilus_meta_view_get_type()));
}

const char *
nautilus_meta_view_get_description(NautilusMetaView *nview)
{
  return NULL;
}
