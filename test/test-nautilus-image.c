#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-theme.h>

#define BACKGROUND_COLOR_STRING		"white"
#define BACKGROUND_COLOR_RGBA		nautilus_parse_rgb_with_white_default (BACKGROUND_COLOR_STRING)
#define DROP_SHADOW_COLOR_RGBA		NAUTILUS_RGB_COLOR_BLACK
#define TEXT_COLOR_RGBA			NAUTILUS_RGB_COLOR_WHITE

#define LOGO_LEFT_SIDE_REPEAT_ICON	"eazel-logo-left-side-repeat.png"
#define LOGO_RIGHT_SIDE_ICON		"eazel-logo-right-side-logo.png"

#define NORMAL_FILL			"summary-service-normal-fill.png"
#define NORMAL_LEFT_BUMPER		"summary-service-normal-left-bumper.png"
#define NORMAL_RIGHT_BUMPER		"summary-service-normal-right-bumper.png"

#define PRELIGHT_FILL			"summary-service-prelight-fill.png"
#define PRELIGHT_LEFT_BUMPER		"summary-service-prelight-left-bumper.png"
#define PRELIGHT_RIGHT_BUMPER		"summary-service-prelight-right-bumper.png"

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}

static char *
icon_get_path (const char *icon_name)
{
	char *icon_path;

	g_return_val_if_fail (icon_name, NULL);

	icon_path = nautilus_theme_get_image_path (icon_name);

	return icon_path;
}

static GdkPixbuf *
pixbuf_new_from_name (const char *name)
{
	char *path;
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (name != NULL, NULL);

	path = icon_get_path (name);
	
	g_return_val_if_fail (path != NULL, NULL);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	return pixbuf;
}

static GtkWidget *
label_new (const char *text,
	   guint font_size,
	   guint drop_shadow_offset,
	   guint vertical_offset,
	   guint horizontal_offset,
	   guint xpadding,
	   guint ypadding,
	   guint32 background_color,
	   guint32 drop_shadow_color,
	   guint32 text_color,
	   const char *tile_name)
{
	GtkWidget *label;

	g_return_val_if_fail (text != NULL, NULL);
	
 	label = nautilus_label_new (text);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), font_size);
	nautilus_label_set_drop_shadow_offset (NAUTILUS_LABEL (label), drop_shadow_offset);
	nautilus_buffered_widget_set_background_type (NAUTILUS_BUFFERED_WIDGET (label), NAUTILUS_BACKGROUND_SOLID);
	nautilus_buffered_widget_set_background_color (NAUTILUS_BUFFERED_WIDGET (label), background_color);
	nautilus_label_set_drop_shadow_color (NAUTILUS_LABEL (label), drop_shadow_color);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), background_color);

	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (label), vertical_offset);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (label), horizontal_offset);

	gtk_misc_set_padding (GTK_MISC (label), xpadding, ypadding);

	if (tile_name != NULL) {
		GdkPixbuf *tile_pixbuf;

		tile_pixbuf = pixbuf_new_from_name (tile_name);

		if (tile_pixbuf != NULL) {
			nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (label), tile_pixbuf);
		}

		nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);
	}
	
	return label;
}

static gint
label_enter_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	NautilusLabel *label;
	GdkPixbuf *tile_pixbuf;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (client_data), TRUE);

	label = NAUTILUS_LABEL (client_data);

	tile_pixbuf = pixbuf_new_from_name (PRELIGHT_FILL);

	if (tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (label), tile_pixbuf);
	}

	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return TRUE;
}

static gint
label_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	NautilusLabel *label;
	GdkPixbuf *tile_pixbuf;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (client_data), TRUE);

	label = NAUTILUS_LABEL (client_data);

	tile_pixbuf = pixbuf_new_from_name (NORMAL_FILL);

	if (tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (label), tile_pixbuf);
	}

	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return TRUE;
}

static GtkWidget *
label_new_with_prelight (const char *text,
			 guint font_size,
			 guint drop_shadow_offset,
			 guint vertical_offset,
			 guint horizontal_offset,
			 guint xpadding,
			 guint ypadding,
			 guint32 background_color,
			 guint32 drop_shadow_color,
			 guint32 text_color,
			 const char *tile_name)
{
	GtkWidget *label;
	GtkWidget *event_box;

	g_return_val_if_fail (text != NULL, NULL);
	
	event_box = gtk_event_box_new ();
	
 	label = label_new (text,
			   font_size,
			   drop_shadow_offset,
			   vertical_offset,
			   horizontal_offset,
			   xpadding,
			   ypadding,
			   background_color,
			   drop_shadow_color,
			   text_color,
			   tile_name);

	gtk_container_add (GTK_CONTAINER (event_box), label);

	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (label_enter_event), label);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (label_leave_event), label);

	return event_box;
}

static GtkWidget *
image_new (GdkPixbuf *pixbuf, GdkPixbuf *tile_pixbuf, guint32 background_color)
{
	GtkWidget *image;

	g_return_val_if_fail (pixbuf || tile_pixbuf, NULL);

	image = nautilus_image_new ();

	nautilus_buffered_widget_set_background_type (NAUTILUS_BUFFERED_WIDGET (image), NAUTILUS_BACKGROUND_SOLID);
	nautilus_buffered_widget_set_background_color (NAUTILUS_BUFFERED_WIDGET (image), background_color);

	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);
	}
	
	if (tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (image), tile_pixbuf);
	}

	return image;
}

static GtkWidget *
image_new_from_name (const char *icon_name, const char *tile_name, guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	g_return_val_if_fail (icon_name || tile_name, NULL);

	if (icon_name) {
		pixbuf = pixbuf_new_from_name (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = pixbuf_new_from_name (tile_name);
	}

	g_return_val_if_fail (pixbuf || tile_pixbuf, NULL);

	image = image_new (pixbuf, tile_pixbuf, background_color);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

static GtkWidget *
header_new (const char *text)
{
	GtkWidget *hbox;
 	GtkWidget *label;
 	GtkWidget *middle;
 	GtkWidget *logo;
	
 	label = label_new (text,
			   20,
			   1, 
			   0,
			   0,
			   0,
			   0,
			   BACKGROUND_COLOR_RGBA,
			   DROP_SHADOW_COLOR_RGBA,
			   TEXT_COLOR_RGBA,
			   LOGO_LEFT_SIDE_REPEAT_ICON);

	middle = image_new_from_name (NULL, LOGO_LEFT_SIDE_REPEAT_ICON, BACKGROUND_COLOR_RGBA);

	logo = image_new_from_name (LOGO_RIGHT_SIDE_ICON, NULL, BACKGROUND_COLOR_RGBA);

	hbox = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), middle, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), logo, FALSE, FALSE, 0);

	return hbox;
}

static GtkWidget *
footer_new (const char *items[], guint num_items)
{
	GtkWidget *hbox;
//  	GtkWidget *label;
//  	GtkWidget *middle;
//  	GtkWidget *logo;

	guint i;

	g_return_val_if_fail (items != NULL, NULL);
	g_return_val_if_fail (num_items > 0, NULL);

	hbox = gtk_hbox_new (FALSE, 0);

	for (i = 0; i < num_items; i++) {
		GtkWidget *label;

		g_assert (items[i] != NULL);
		g_assert (items[i][0] != '\0');
	
		label = label_new_with_prelight (items[i],
						 16,
						 1, 
						 10,
						 10,
						 10,
						 10,
						 BACKGROUND_COLOR_RGBA,
						 DROP_SHADOW_COLOR_RGBA,
						 TEXT_COLOR_RGBA,
						 NORMAL_FILL);

		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	}

// 	middle = image_new_from_name (NULL, LOGO_LEFT_SIDE_REPEAT_ICON, BACKGROUND_COLOR_RGBA);

// 	logo = image_new_from_name (LOGO_RIGHT_SIDE_ICON, NULL, BACKGROUND_COLOR_RGBA);


// 	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
// 	gtk_box_pack_start (GTK_BOX (hbox), middle, TRUE, TRUE, 0);
// 	gtk_box_pack_end (GTK_BOX (hbox), logo, FALSE, FALSE, 0);

	return hbox;
}

static const char *footer_items[] = 
{
	"Register",
	"Login",
	"Terms of Use",
	"Privacy Statement"
};

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *header;
	GtkWidget *footer;
	GtkWidget *content;
	
	gtk_init (&argc, &argv);
	gdk_rgb_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	nautilus_gtk_widget_set_background_color (window, BACKGROUND_COLOR_STRING);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Nautilus Wrapped Label Test");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);
	gtk_widget_set_usize (window, 640, 480);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	header = header_new ("Welcome back, Arlo!");
	content = gtk_vbox_new (FALSE, 0);
	footer = footer_new (footer_items, NAUTILUS_N_ELEMENTS (footer_items));
	
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), content, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), footer, FALSE, FALSE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
