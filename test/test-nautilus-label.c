#include <config.h>

#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-picker.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-string-picker.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-text-caption.h>

static char *widget_get_nautilus_background_color (GtkWidget  *widget);
static void  widget_set_nautilus_background_image (GtkWidget  *widget,
						   const char *image_name);
static void  widget_set_nautilus_background_color (GtkWidget  *widget,
						   const char *color);

static void
red_label_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;
	guint32		  color;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);
	
	color = nautilus_label_get_text_color (label);

	color = NAUTILUS_RGBA_COLOR_PACK ((guchar) adjustment->value,
					  NAUTILUS_RGBA_COLOR_GET_G (color),
					  NAUTILUS_RGBA_COLOR_GET_B (color),
					  NAUTILUS_RGBA_COLOR_GET_A (color));
	
	nautilus_label_set_text_color (label, color);
}

static void
green_label_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;
	guint32		  color;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);
	
	color = nautilus_label_get_text_color (label);

	color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
					  (guchar) adjustment->value,
					  NAUTILUS_RGBA_COLOR_GET_B (color),
					  NAUTILUS_RGBA_COLOR_GET_A (color));
	
	nautilus_label_set_text_color (label, color);
}

static void
blue_label_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;
	guint32		  color;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);
	
	color = nautilus_label_get_text_color (label);

	color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
					  NAUTILUS_RGBA_COLOR_GET_G (color),
					  (guchar) adjustment->value,
					  NAUTILUS_RGBA_COLOR_GET_A (color));
	
	nautilus_label_set_text_color (label, color);
}

static void
alpha_label_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);

	nautilus_label_set_text_opacity (NAUTILUS_LABEL (label), (guchar) adjustment->value);
}

static void
red_background_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;
	guint32		  current_color;
	char		  *current_color_spec;
	char		  *new_color_spec;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);

	current_color_spec = widget_get_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (label)));

	current_color = nautilus_parse_rgb_with_white_default (current_color_spec);

	g_free (current_color_spec);

	new_color_spec = g_strdup_printf ("rgb:%04hx/%04hx/%04hx",
					  (guint16) (adjustment->value / 255.0 * 65535.0),
					  (guint16) ((double) NAUTILUS_RGBA_COLOR_GET_G (current_color) / 255.0 * 65535.0),
					  (guint16) ((double) NAUTILUS_RGBA_COLOR_GET_B (current_color) / 255.0 * 65535.0));

	widget_set_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (label)), new_color_spec);
	
	g_free (new_color_spec);
}

static void
green_background_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;
	guint32		  current_color;
	char		  *current_color_spec;
	char		  *new_color_spec;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);

	current_color_spec = widget_get_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (label)));

	current_color = nautilus_parse_rgb_with_white_default (current_color_spec);

	g_free (current_color_spec);

	new_color_spec = g_strdup_printf ("rgb:%04hx/%04hx/%04hx",
					  (guint16) ((double) NAUTILUS_RGBA_COLOR_GET_R (current_color) / 255.0 * 65535.0),
					  (guint16) (adjustment->value / 255.0 * 65535.0),
					  (guint16) ((double) NAUTILUS_RGBA_COLOR_GET_B (current_color) / 255.0 * 65535.0));

	widget_set_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (label)), new_color_spec);
	
	g_free (new_color_spec);
}

static void
blue_background_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;
	guint32		  current_color;
	char		  *current_color_spec;
	char		  *new_color_spec;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	label = NAUTILUS_LABEL (client_data);

	current_color_spec = widget_get_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (label)));

	current_color = nautilus_parse_rgb_with_white_default (current_color_spec);

	g_free (current_color_spec);

	new_color_spec = g_strdup_printf ("rgb:%04hx/%04hx/%04hx",
					  (guint16) ((double) NAUTILUS_RGBA_COLOR_GET_R (current_color) / 255.0 * 65535.0),
					  (guint16) ((double) NAUTILUS_RGBA_COLOR_GET_G (current_color) / 255.0 * 65535.0),
					  (guint16) (adjustment->value / 255.0 * 65535.0));

	widget_set_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (label)), new_color_spec);
	
	g_free (new_color_spec);
}

static void
alpha_background_color_value_changed_callback (GtkAdjustment *adjustment, gpointer client_data)
{
	NautilusLabel	 *label;

	g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));
	
	label = NAUTILUS_LABEL (client_data);

	nautilus_label_set_text_opacity (NAUTILUS_LABEL (label), (guchar) adjustment->value);
}

static void
font_size_changed_callback (NautilusStringPicker *string_picker, gpointer client_data)
{
 	char *string;
	int   size;

	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

 	string = nautilus_string_picker_get_selected_string (string_picker);

	if (nautilus_eat_str_to_int (string, &size)) {
		nautilus_label_set_smooth_font_size (NAUTILUS_LABEL (client_data), (guint) size);
	}

	g_free (string);
}

static void
font_changed_callback (NautilusFontPicker *font_picker, gpointer client_data)
{
	NautilusScalableFont *font;
	char *family;
	char *weight;
	char *slant;
	char *set_width;

	g_return_if_fail (NAUTILUS_IS_FONT_PICKER (font_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

	family = nautilus_font_picker_get_selected_family (NAUTILUS_FONT_PICKER (font_picker));
	weight = nautilus_font_picker_get_selected_weight (NAUTILUS_FONT_PICKER (font_picker));
	slant = nautilus_font_picker_get_selected_slant (NAUTILUS_FONT_PICKER (font_picker));
	set_width = nautilus_font_picker_get_selected_set_width (NAUTILUS_FONT_PICKER (font_picker));

	g_print ("%s (%s,%s,%s,%s)\n", __FUNCTION__, family, weight, slant, set_width);

	font = nautilus_scalable_font_new (family, weight, slant, set_width);
	g_assert (font != NULL);

	nautilus_label_set_smooth_font (NAUTILUS_LABEL (client_data), font);

	g_free (family);
	g_free (weight);
	g_free (slant);
	g_free (set_width);

	gtk_object_unref (GTK_OBJECT (font));
}

static void
text_caption_changed_callback (NautilusTextCaption *text_caption, gpointer client_data)
{
	NautilusLabel *label;
 	char *text;

	g_return_if_fail (NAUTILUS_IS_TEXT_CAPTION (text_caption));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

 	text = nautilus_text_caption_get_text (text_caption);

	label = NAUTILUS_LABEL (client_data);

	nautilus_label_set_text (NAUTILUS_LABEL (label), text);

	g_free (text);
}

static GtkWidget*
create_value_scale (guint min,
		    guint max,
		    guint value,
		    const char *color_spec,
		    GtkSignalFunc callback,
		    gpointer callback_data)
{
	GtkAdjustment	*adjustment;
	GtkWidget	*scale;

	g_assert (max > min);
	g_assert (callback > 0);
	
	adjustment = (GtkAdjustment *) gtk_adjustment_new (value,
							   min,
							   max,
							   1,
							   (max - min) / 10,
							   0);
	
	scale = gtk_hscale_new (adjustment);

	if (color_spec != NULL) {
		nautilus_gtk_widget_set_background_color (scale, color_spec);
	}

	gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);

	gtk_widget_set_usize (scale, 150, 0);
	
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed", callback, callback_data);
	
	return scale;
}

static GtkWidget*
create_value_scale_caption (const gchar *title,
			    guint min,
			    guint max,
			    guint value,
			    const char *color_spec,
			    GtkSignalFunc callback,
			    gpointer callback_data)
{
	GtkWidget	*hbox;
	GtkWidget	*label;
	GtkWidget	*scale;

	scale = create_value_scale (min, max, value, color_spec, callback, callback_data);
	hbox = gtk_hbox_new (FALSE, 0);
	label = gtk_label_new (title);
	
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	gtk_box_pack_end (GTK_BOX (hbox), scale, FALSE, FALSE, 4);

	gtk_widget_show (label);
	gtk_widget_show (scale);

	return hbox;
}

static GtkWidget*
create_color_picker_frame (const char		*title,
			   GtkSignalFunc	red_callback,
			   GtkSignalFunc	green_callback,
			   GtkSignalFunc	blue_callback,
			   GtkSignalFunc	alpha_callback,
			   gpointer		callback_data,
			   guint32		current_color)
{
	GtkWidget *red_scale;
	GtkWidget *green_scale;
	GtkWidget *blue_scale;
	GtkWidget *alpha_scale;
	GtkWidget *frame;
	GtkWidget *vbox;

	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (red_callback != NULL, NULL);
	g_return_val_if_fail (green_callback != NULL, NULL);
	g_return_val_if_fail (blue_callback != NULL, NULL);
	g_return_val_if_fail (alpha_callback != NULL, NULL);
	
	frame = gtk_frame_new (title);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);

	red_scale = create_value_scale_caption ("Red",
						0,
						255,
						NAUTILUS_RGBA_COLOR_GET_R (current_color),
						"red",
						red_callback,
						callback_data);

	green_scale = create_value_scale_caption ("Green",
						  0,
						  255,
						  NAUTILUS_RGBA_COLOR_GET_R (current_color),
						  "green",
						  green_callback,
						  callback_data);
	
	blue_scale = create_value_scale_caption ("Blue",
						 0,
						 255,
						 NAUTILUS_RGBA_COLOR_GET_R (current_color),
						 "blue",
						 blue_callback,
						 callback_data);
	
	alpha_scale = create_value_scale_caption ("Alpha",
						  0,
						  255,
						  NAUTILUS_RGBA_COLOR_GET_R (current_color),
						  NULL,
						  alpha_callback,
						  callback_data);

	gtk_container_add (GTK_CONTAINER (frame), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), red_scale, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (vbox), green_scale, TRUE, TRUE, 1);
	gtk_box_pack_start (GTK_BOX (vbox), blue_scale, TRUE, TRUE, 1);
	gtk_box_pack_end (GTK_BOX (vbox), alpha_scale, TRUE, TRUE, 2);

	gtk_widget_show_all (vbox);

	return frame;
}

static GtkWidget*
create_font_picker_frame (const char		*title,
			  GtkSignalFunc		changed_callback,
			  GtkSignalFunc		size_changed_callback,
			  gpointer		callback_data)
{
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget		*font_picker;
	GtkWidget		*font_size_picker;
	NautilusStringList	*font_size_list;

	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (changed_callback != NULL, NULL);
	g_return_val_if_fail (size_changed_callback != NULL, NULL);
	
	frame = gtk_frame_new (title);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);

	gtk_container_add (GTK_CONTAINER (frame), hbox);

	font_size_picker = nautilus_string_picker_new ();
	nautilus_caption_set_show_title (NAUTILUS_CAPTION (font_size_picker), FALSE);
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (font_size_picker), "Size");

	gtk_signal_connect (GTK_OBJECT (font_size_picker), "changed", size_changed_callback, callback_data);

	font_size_list = nautilus_string_list_new_from_tokens ("5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,"
							       "30,40,50,60,70,80,90,100,110,120,130,140,"
							       "200,400,800", ",", TRUE);

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (font_size_picker), font_size_list);
	nautilus_string_list_free (font_size_list);

	font_picker = nautilus_font_picker_new ();
	gtk_signal_connect (GTK_OBJECT (font_picker), "selected_font_changed", changed_callback, callback_data);
	
	gtk_box_pack_start (GTK_BOX (hbox), font_picker, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), font_size_picker, FALSE, FALSE, 5);

	gtk_widget_show_all (hbox);

	return frame;
}

static GtkWidget*
create_text_caption_frame (const char		*title,
			   GtkSignalFunc	changed_callback,
			   gpointer		callback_data)
{
	GtkWidget *frame;
	GtkWidget *text_caption;

	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (changed_callback != NULL, NULL);
	
	frame = gtk_frame_new (title);

	text_caption = nautilus_text_caption_new ();
	gtk_container_set_border_width (GTK_CONTAINER (text_caption), 6);

 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (text_caption), FALSE);
	nautilus_caption_set_title_label (NAUTILUS_CAPTION (text_caption), title);

	gtk_signal_connect (GTK_OBJECT (text_caption), "changed", changed_callback, callback_data);

	gtk_container_add (GTK_CONTAINER (frame), text_caption);

	gtk_widget_show (text_caption);

	return frame;
}

static void
widget_set_nautilus_background_image (GtkWidget *widget, const char *image_name)
{
	NautilusBackground	*background;
	char			*background_uri;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (image_name != NULL);

	background = nautilus_get_widget_background (widget);
	
	background_uri = g_strdup_printf ("file://%s/patterns/%s", NAUTILUS_DATADIR, image_name);

	nautilus_background_reset (background);
	nautilus_background_set_image_uri (background, background_uri);

	g_free (background_uri);
}

static void
widget_set_nautilus_background_color (GtkWidget *widget, const char *color)
{
	NautilusBackground	*background;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (color != NULL);

	background = nautilus_get_widget_background (widget);
	
	nautilus_background_reset (background);
	nautilus_background_set_color (background, color);
}

static char *
widget_get_nautilus_background_color (GtkWidget *widget)
{
	NautilusBackground *background;
	
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	background = nautilus_get_widget_background (widget);

	return nautilus_background_get_color (background);
}

static void
widget_set_background_reset (GtkWidget *widget)
{
	NautilusBackground	*background;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	background = nautilus_get_widget_background (widget);

	nautilus_background_reset (background);
}

static void
background_changed_callback (NautilusStringPicker *string_picker, gpointer client_data)
{
 	char *string;

	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

 	string = nautilus_string_picker_get_selected_string (string_picker);

	if (nautilus_str_has_prefix (string, "Image - ")) {
		widget_set_nautilus_background_image (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (client_data)),
					     string + strlen ("Image - "));
	}
	else if (nautilus_str_has_prefix (string, "Gradient - ")) {
		widget_set_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (client_data)),
					     string + strlen ("Gradient - "));
	}
	else if (nautilus_str_has_prefix (string, "Solid - ")) {
		widget_set_nautilus_background_color (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (client_data)),
					     string + strlen ("Solid - "));
	}
	else if (nautilus_str_has_prefix (string, "Reset")) {
		widget_set_background_reset (nautilus_gtk_widget_find_windowed_ancestor (GTK_WIDGET (client_data)));
	}

	g_free (string);
}

static void
justification_changed_callback (NautilusStringPicker *string_picker, gpointer client_data)
{
	GtkJustification justification;
	char *string;

	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

 	string = nautilus_string_picker_get_selected_string (string_picker);

	if (nautilus_str_is_equal (string, "Left")) {
		justification = GTK_JUSTIFY_LEFT;
	} else if (nautilus_str_has_prefix (string, "Center")) {
		justification = GTK_JUSTIFY_CENTER;
	} else if (nautilus_str_has_prefix (string, "Right")) {
		justification = GTK_JUSTIFY_RIGHT;
	} else {
		g_assert_not_reached ();
		justification = GTK_JUSTIFY_LEFT;
	}

	nautilus_label_set_justify (NAUTILUS_LABEL (client_data), justification);
	
	g_free (string);
}

static void
drop_shadow_offset_changed_callback (NautilusStringPicker *string_picker, gpointer client_data)
{
	char *string;
	int   drop_shadow_offset;

	g_return_if_fail (NAUTILUS_IS_STRING_PICKER (string_picker));
	g_return_if_fail (NAUTILUS_IS_LABEL (client_data));

 	string = nautilus_string_picker_get_selected_string (string_picker);

	if (nautilus_eat_str_to_int (string, &drop_shadow_offset)) {
		nautilus_label_set_smooth_drop_shadow_offset (NAUTILUS_LABEL (client_data), drop_shadow_offset);
	}

	g_free (string);
}

static GtkWidget*
create_background_frame (const char	*title,
			 GtkSignalFunc	background_changed_callback,
			 gpointer	callback_data)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *background_picker;

	g_return_val_if_fail (title != NULL, NULL);
 	g_return_val_if_fail (background_changed_callback != NULL, NULL);
	
	vbox = gtk_vbox_new (FALSE, 0);
	frame = gtk_frame_new (title);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	background_picker = nautilus_string_picker_new ();
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Image - pale_coins.png");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Image - bubbles.png");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Image - irish_spring.png");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Image - white_ribs.png");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "-----------------------");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Gradient - rgb:bbbb/bbbb/eeee-rgb:ffff/ffff/ffff:h");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Gradient - rgb:bbbb/bbbb/eeee-rgb:ffff/ffff/ffff");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "-----------------------");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Solid - rgb:bbbb/bbbb/eeee");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "-----------------------");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (background_picker), "Reset");

 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (background_picker), FALSE);

	gtk_signal_connect (GTK_OBJECT (background_picker), "changed", background_changed_callback, callback_data);

	gtk_box_pack_start (GTK_BOX (vbox), background_picker, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);

	return frame;
}

static GtkWidget*
create_justification_frame (const char		*title,
			    GtkSignalFunc	justification_changed_callback,
			    gpointer		callback_data)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *justification_picker;

	g_return_val_if_fail (title != NULL, NULL);
 	g_return_val_if_fail (justification_changed_callback != NULL, NULL);
	
	vbox = gtk_vbox_new (FALSE, 0);
	frame = gtk_frame_new (title);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	justification_picker = nautilus_string_picker_new ();
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (justification_picker), "Left");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (justification_picker), "Center");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (justification_picker), "Right");

 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (justification_picker), FALSE);

	gtk_signal_connect (GTK_OBJECT (justification_picker), "changed", justification_changed_callback, callback_data);

	gtk_box_pack_start (GTK_BOX (vbox), justification_picker, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);

	return frame;
}

static GtkWidget*
create_drop_shadow_offset_frame (const char	*title,
				 GtkSignalFunc	drop_shadow_changed_callback,
				 gpointer	callback_data)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *drop_shadow_offset_picker;

	g_return_val_if_fail (title != NULL, NULL);
 	g_return_val_if_fail (drop_shadow_changed_callback != NULL, NULL);
	
	vbox = gtk_vbox_new (FALSE, 0);
	frame = gtk_frame_new (title);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	drop_shadow_offset_picker = nautilus_string_picker_new ();
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (drop_shadow_offset_picker), "0");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (drop_shadow_offset_picker), "1");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (drop_shadow_offset_picker), "2");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (drop_shadow_offset_picker), "3");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (drop_shadow_offset_picker), "4");
	nautilus_string_picker_insert_string (NAUTILUS_STRING_PICKER (drop_shadow_offset_picker), "5");

 	nautilus_caption_set_show_title (NAUTILUS_CAPTION (drop_shadow_offset_picker), FALSE);

	gtk_signal_connect (GTK_OBJECT (drop_shadow_offset_picker), "changed", drop_shadow_changed_callback, callback_data);

	gtk_box_pack_start (GTK_BOX (vbox), drop_shadow_offset_picker, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);

	return frame;
}

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	GtkWidget		*window;
	GtkWidget		*main_box;
	GtkWidget		*bottom_box;
	GtkWidget		*tool_box1;
	GtkWidget		*tool_box2;
	GtkWidget		*tool_box3;
	GtkWidget		*color_tool_box;
	GtkWidget		*label;
	GtkWidget		*label_color_picker_frame;
	GtkWidget		*background_color_picker_frame;
	GtkWidget		*font_picker_frame;
	GtkWidget		*text_caption_frame;
	GtkWidget		*background_frame;
	GtkWidget		*justification_frame;
	GtkWidget		*drop_shadow_offset_frame;
	GtkWidget		*middle_box;

	gtk_init (&argc, &argv);
	gdk_rgb_init ();
	gnome_vfs_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event", GTK_SIGNAL_FUNC (delete_event), NULL);
	gtk_window_set_title (GTK_WINDOW (window), "Label Test");
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_box);

 	label = nautilus_label_new ("Foo");

	bottom_box = gtk_vbox_new (FALSE, 4);

	tool_box1 = gtk_hbox_new (FALSE, 0);
	tool_box2 = gtk_hbox_new (FALSE, 0);
	tool_box3 = gtk_hbox_new (FALSE, 0);

	color_tool_box = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (bottom_box), tool_box1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (bottom_box), tool_box2, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (bottom_box), tool_box3, TRUE, TRUE, 10);

	gtk_box_pack_start (GTK_BOX (main_box), label, TRUE, TRUE, 10);
	gtk_box_pack_end (GTK_BOX (main_box), bottom_box, TRUE, TRUE, 10);

	widget_set_nautilus_background_image (nautilus_gtk_widget_find_windowed_ancestor (label), "pale_coins.png");
	
	label_color_picker_frame = create_color_picker_frame ("Label Color",
							      red_label_color_value_changed_callback,
							      green_label_color_value_changed_callback,
							      blue_label_color_value_changed_callback,
							      alpha_label_color_value_changed_callback,
							      label,
							      nautilus_label_get_text_color (NAUTILUS_LABEL (label)));

	background_color_picker_frame = create_color_picker_frame ("Background Color",
								   red_background_color_value_changed_callback,
								   green_background_color_value_changed_callback,
								   blue_background_color_value_changed_callback,
								   alpha_background_color_value_changed_callback,
								   label,
								   nautilus_label_get_text_color (NAUTILUS_LABEL (label)));

	font_picker_frame = create_font_picker_frame ("Font",
						      font_changed_callback,
						      font_size_changed_callback,
						      label);

	text_caption_frame = create_text_caption_frame ("Text",
							text_caption_changed_callback,
							label);
	
	background_frame = create_background_frame ("Background",
						    background_changed_callback,
						    label);

	justification_frame = create_justification_frame ("Justification",
							  justification_changed_callback,
							  label);

	drop_shadow_offset_frame = create_drop_shadow_offset_frame ("Drop Shadow Offset",
								    drop_shadow_offset_changed_callback,
								    label);
	
	middle_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (middle_box), background_frame, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (middle_box), drop_shadow_offset_frame, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (middle_box), justification_frame, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (color_tool_box), label_color_picker_frame, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (color_tool_box), middle_box, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (color_tool_box), background_color_picker_frame, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (tool_box1), color_tool_box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (tool_box2), font_picker_frame, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (tool_box3), text_caption_frame, TRUE, TRUE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	gnome_vfs_shutdown ();
	
	return 0;
}
