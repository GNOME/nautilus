
#ifndef TEST_H
#define TEST_H

#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include <eel/eel-art-extensions.h>
#include <eel/eel-art-gtk-extensions.h>
#include <eel/eel-background.h>
#include <eel/eel-debug-drawing.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-image-with-background.h>
#include <eel/eel-image.h>
#include <eel/eel-label-with-background.h>
#include <eel/eel-label.h>
#include <eel/eel-string-list.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-file-utilities.h>

#include <eel/eel-text-caption.h>
#include <eel/eel-string-picker.h>

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
int        test_text_caption_get_text_as_int    (const EelTextCaption   *text_caption);

/* Preferences hacks */
void test_text_caption_set_text_for_int_preferences            (EelTextCaption       *text_caption,
								const char                *name);
void test_text_caption_set_text_for_string_preferences         (EelTextCaption       *text_caption,
								const char                *name);
void test_text_caption_set_text_for_default_int_preferences    (EelTextCaption       *text_caption,
								const char                *name);
void test_text_caption_set_text_for_default_string_preferences (EelTextCaption       *text_caption,
								const char                *name);

#endif /* TEST_H */
