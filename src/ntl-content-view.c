#include "ntl-content-view.h"
#include <gtk/gtksignal.h>

static void nautilus_content_view_class_init (NautilusContentViewClass *klass);
static void nautilus_content_view_init (NautilusContentView *view);

GtkType
nautilus_content_view_get_type(void)
{
  static guint view_type = 0;

  if (!view_type)
    {
      GtkTypeInfo view_info = {
	"NautilusContentView",
	sizeof(NautilusContentView),
	sizeof(NautilusContentViewClass),
	(GtkClassInitFunc) nautilus_content_view_class_init,
	(GtkObjectInitFunc) nautilus_content_view_init
      };

      view_type = gtk_type_unique (nautilus_view_get_type(), &view_info);
    }

  return view_type;
}

static void
nautilus_content_view_class_init (NautilusContentViewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
}

static void
nautilus_content_view_init (NautilusContentView *view)
{
}

NautilusContentView *
nautilus_content_view_new(void)
{
  return NAUTILUS_CONTENT_VIEW (gtk_type_new (nautilus_content_view_get_type()));
}
