
#ifndef TEST_H
#define TEST_H

#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include <libnautilus-extensions/nautilus-art-extensions.h>
#include <libnautilus-extensions/nautilus-art-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-debug-drawing.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-image-with-background.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label-with-background.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

#include <libnautilus-extensions/nautilus-text-caption.h>
#include <libnautilus-extensions/nautilus-string-picker.h>

void       test_init                            (int                         *argc,
						 char                      ***argv);
int        test_quit                            (int                          exit_code);
void       test_delete_event                    (GtkWidget                   *widget,
						 GdkEvent                    *event,
						 gpointer                     callback_data);
GtkWidget *test_window_new                      (const char                  *title,
						 guint                        border_width);
void       test_gtk_widget_set_background_image (GtkWidget                   *widget,
						 const char                  *image_name);
void       test_gtk_widget_set_background_color (GtkWidget                   *widget,
						 const char                  *color_spec);
GdkPixbuf *test_pixbuf_new_named                (const char                  *name,
						 float                        scale);
GtkWidget *test_image_new                       (const char                  *pixbuf_name,
						 const char                  *tile_name,
						 float                        scale,
						 gboolean                     with_background);
GtkWidget *test_label_new                       (const char                  *text,
						 const char                  *tile_name,
						 gboolean                     with_background,
						 int                          num_sizes_larger);
void       test_pixbuf_draw_rectangle_tiled     (GdkPixbuf                   *pixbuf,
						 const char                  *tile_name,
						 int                          x0,
						 int                          y0,
						 int                          x1,
						 int                          y1,
						 int                          opacity);
void       test_window_set_title_with_pid       (GtkWindow                   *window,
						 const char                  *title);
int        test_text_caption_get_text_as_int    (const NautilusTextCaption   *text_caption);

/* Preferences hacks */
void test_text_caption_set_text_for_int_preferences            (NautilusTextCaption       *text_caption,
								const char                *name);
void test_text_caption_set_text_for_string_preferences         (NautilusTextCaption       *text_caption,
								const char                *name);
void test_text_caption_set_text_for_default_int_preferences    (NautilusTextCaption       *text_caption,
								const char                *name);
void test_text_caption_set_text_for_default_string_preferences (NautilusTextCaption       *text_caption,
								const char                *name);

#endif /* TEST_H */
