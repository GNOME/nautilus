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

#define REMAINDER_LEFT_BUMPER		"summary-service-remainder-left-bumper.png"
#define REMAINDER_FILL			"summary-service-remainder-fill.png"
#define REMAINDER_RIGHT_BUMPER		"summary-service-remainder-right-bumper.png"

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
	   gint xpadding,
	   gint ypadding,
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

typedef struct
{
	GtkWidget *buffered_widget;
	char *tile_name;
	char *prelight_tile_name;
	GdkPixbuf *tile_pixbuf;
	GdkPixbuf *prelight_tile_pixbuf;
} PrelightLabelData;

static gint
label_enter_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightLabelData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (PrelightLabelData *) client_data;

	g_return_val_if_fail (data->prelight_tile_name, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (data->buffered_widget), TRUE);

	//g_print ("%s(%p)\n", __FUNCTION__, data);

	if (data->prelight_tile_pixbuf == NULL) {
		data->prelight_tile_pixbuf = pixbuf_new_from_name (data->prelight_tile_name);
	}

	if (data->prelight_tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (data->buffered_widget),
							  data->prelight_tile_pixbuf);
	}

	return TRUE;
}

static gint
label_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightLabelData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (PrelightLabelData *) client_data;

	g_return_val_if_fail (data->tile_name, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (data->buffered_widget), TRUE);

	if (data->tile_pixbuf == NULL) {
		data->tile_pixbuf = pixbuf_new_from_name (data->tile_name);
	}
	
	if (data->tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (data->buffered_widget),
							  data->tile_pixbuf);
	}

	return TRUE;
}

static void
label_free_data (GtkWidget *event_box,
		 gpointer client_data)
{
	PrelightLabelData *data;

	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (client_data != NULL);

	data = (PrelightLabelData *) client_data;

	g_free (data->tile_name);
	g_free (data->prelight_tile_name);
	nautilus_gdk_pixbuf_unref_if_not_null (data->tile_pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (data->prelight_tile_pixbuf);
	g_free (data);
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

static void
buffered_widget_add_prelighting (GtkWidget *buffered_widget,
				 GtkWidget *event_box,
				 const char *tile_name,
				 const char *prelight_tile_name)
{
	PrelightLabelData *data;

	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget));
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (tile_name != NULL);
	g_return_if_fail (prelight_tile_name != NULL);

	data = g_new0 (PrelightLabelData, 1);
	
	data->buffered_widget = buffered_widget;
	data->tile_name = g_strdup (tile_name);
	data->prelight_tile_name = g_strdup (prelight_tile_name);
	
	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (label_enter_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (label_leave_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (label_free_data), data);
}

static GtkWidget *
header_new (const char *text)
{
	GtkWidget *hbox;
 	GtkWidget *label;
 	GtkWidget *middle;
 	GtkWidget *logo;
	
 	label = label_new (text,
			   18,
			   1, 
			   4,
			   0,
			   10,
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
footer_item_new (const char *text, gboolean has_left_bumper, gboolean has_right_bumper)
{
	GtkWidget *hbox;
	GtkWidget *event_box;
  	GtkWidget *label;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);

	event_box = gtk_event_box_new ();
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (event_box), hbox);

	if (has_left_bumper) {
		GtkWidget *left;
		left = image_new_from_name (NORMAL_LEFT_BUMPER, NORMAL_FILL, BACKGROUND_COLOR_RGBA);
		buffered_widget_add_prelighting (left, event_box, NORMAL_FILL, PRELIGHT_FILL);
		gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
	}

 	label = label_new (text,
			   13,
			   1,
			   2,
			   0,
			   8,
			   0,
			   BACKGROUND_COLOR_RGBA,
			   DROP_SHADOW_COLOR_RGBA,
			   TEXT_COLOR_RGBA,
			   NORMAL_FILL);

	buffered_widget_add_prelighting (label, event_box, NORMAL_FILL, PRELIGHT_FILL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	if (has_right_bumper) {
		GtkWidget *right;
		right = image_new_from_name (NORMAL_RIGHT_BUMPER, NORMAL_FILL, BACKGROUND_COLOR_RGBA);
		buffered_widget_add_prelighting (right, event_box, NORMAL_FILL, PRELIGHT_FILL);
		gtk_box_pack_start (GTK_BOX (hbox), right, FALSE, FALSE, 0);
	}

	return event_box;
}

static GtkWidget *
footer_remainder_new (void)
{
	GtkWidget *hbox;
	GtkWidget *left;
	GtkWidget *fill;
	GtkWidget *right;

	hbox = gtk_hbox_new (FALSE, 0);

	left = image_new_from_name (REMAINDER_LEFT_BUMPER, NULL, BACKGROUND_COLOR_RGBA);
	fill = image_new_from_name (NULL, REMAINDER_FILL, BACKGROUND_COLOR_RGBA);
	right = image_new_from_name (REMAINDER_RIGHT_BUMPER, NULL, BACKGROUND_COLOR_RGBA);

	gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), fill, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), right, FALSE, FALSE, 0);

	return hbox;
}

static GtkWidget *
footer_new (const char *items[], guint num_items)
{
	GtkWidget *hbox;
	GtkWidget *remainder;
	GtkWidget *date;
	guint i;

	g_return_val_if_fail (items != NULL, NULL);
	g_return_val_if_fail (num_items > 0, NULL);

	hbox = gtk_hbox_new (FALSE, 0);

	for (i = 0; i < num_items; i++) {
		GtkWidget *item;
		
		item = footer_item_new (items[i], i > 0, i < (num_items - 1));
		
		gtk_box_pack_start (GTK_BOX (hbox), item, FALSE, FALSE, 0);
	}

	remainder = footer_remainder_new ();
	gtk_box_pack_start (GTK_BOX (hbox), remainder, TRUE, TRUE, 0);

 	date = label_new ("Friday, October 13th",
			  13,
			  1,
			  2,
			  0,
			  8,
			  0,
			  BACKGROUND_COLOR_RGBA,
			  DROP_SHADOW_COLOR_RGBA,
			  TEXT_COLOR_RGBA,
			  NORMAL_FILL);

	gtk_box_pack_start (GTK_BOX (hbox), date, FALSE, FALSE, 0);

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
