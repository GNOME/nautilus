#ifndef NAUTILUS_CONTAINER_MAX_WIDTH_H
#define NAUTILUS_CONTAINER_MAX_WIDTH_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_CONTAINER_MAX_WIDTH (nautilus_container_max_width_get_type())

G_DECLARE_FINAL_TYPE (NautilusContainerMaxWidth, nautilus_container_max_width, NAUTILUS, CONTAINER_MAX_WIDTH, GtkBin)

NautilusContainerMaxWidth *nautilus_container_max_width_new (void);

void nautilus_container_max_width_set_max_width (NautilusContainerMaxWidth *self,
                                                 guint                      max_width);
guint nautilus_container_max_width_get_max_width (NautilusContainerMaxWidth *self);

G_END_DECLS

#endif /* NAUTILUS_CONTAINER_MAX_WIDTH_H */

