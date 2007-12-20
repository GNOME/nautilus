#include "test.h"

#include <eel/eel-text-caption.h>
#include <libnautilus-private/nautilus-global-preferences.h>

static void
text_caption_update (EelTextCaption *text_caption,
		     const char *name)
{
	g_return_if_fail (EEL_IS_TEXT_CAPTION (text_caption));
	g_return_if_fail (name != NULL);
	
	g_print ("'%s' changed from '%d' to '%d'\n",
		 name,
		 test_text_caption_get_text_as_int (text_caption),
		 eel_preferences_get_integer (name));

	test_text_caption_set_text_for_int_preferences (text_caption, name);
}

static void
green_changed_callback (gpointer callback_data)
{
	text_caption_update (EEL_TEXT_CAPTION (callback_data), "green");
}

static void
yellow_changed_callback (gpointer callback_data)
{
	text_caption_update (EEL_TEXT_CAPTION (callback_data), "yellow");
}

static void
red_changed_callback (gpointer callback_data)
{
	text_caption_update (EEL_TEXT_CAPTION (callback_data), "red");
}

static void
apple_changed_callback (gpointer callback_data)
{
	text_caption_update (EEL_TEXT_CAPTION (callback_data), "fruits/apple");
}

static void
orange_changed_callback (gpointer callback_data)
{
	text_caption_update (EEL_TEXT_CAPTION (callback_data), "fruits/orange");
}

static void
pear_changed_callback (gpointer callback_data)
{
	text_caption_update (EEL_TEXT_CAPTION (callback_data), "fruits/pear");
}

static GtkWidget *
entry_new (const char *name,
	   GtkWidget **caption_out,
	   GtkWidget **default_caption_out)
{
	GtkWidget *hbox;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (caption_out != NULL, NULL);
	g_return_val_if_fail (default_caption_out != NULL, NULL);

	hbox = gtk_hbox_new (TRUE, 2);

	*caption_out = eel_text_caption_new ();
	eel_text_caption_set_editable (EEL_TEXT_CAPTION (*caption_out), FALSE);
	eel_caption_set_title_label (EEL_CAPTION (*caption_out), name);

	*default_caption_out = eel_text_caption_new ();
	eel_text_caption_set_editable (EEL_TEXT_CAPTION (*default_caption_out), FALSE);
	eel_caption_set_title_label (EEL_CAPTION (*default_caption_out), "default:");
	
	gtk_box_pack_start (GTK_BOX (hbox), *caption_out, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), *default_caption_out, FALSE, FALSE, 0);

	gtk_widget_show (*caption_out);
	gtk_widget_show (*default_caption_out);

	return hbox;
}

static GtkWidget *
colors_frame_new (void)
{
	GtkWidget *green_caption;
	GtkWidget *green_default_caption;
	GtkWidget *green_hbox;

	GtkWidget *yellow_caption;
	GtkWidget *yellow_default_caption;
	GtkWidget *yellow_hbox;

	GtkWidget *red_caption;
	GtkWidget *red_default_caption;
	GtkWidget *red_hbox;

	GtkWidget *frame;
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, 2);

	frame = gtk_frame_new ("colors");
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	
	green_hbox = entry_new ("green", &green_caption, &green_default_caption);
	yellow_hbox = entry_new ("yellow", &yellow_caption, &yellow_default_caption);
	red_hbox = entry_new ("red", &red_caption, &red_default_caption);

	test_text_caption_set_text_for_int_preferences (EEL_TEXT_CAPTION (green_caption), "green");
	test_text_caption_set_text_for_int_preferences (EEL_TEXT_CAPTION (yellow_caption), "yellow");
	test_text_caption_set_text_for_int_preferences (EEL_TEXT_CAPTION (red_caption), "red");

	test_text_caption_set_text_for_default_int_preferences (EEL_TEXT_CAPTION (green_default_caption), "green");
	test_text_caption_set_text_for_default_int_preferences (EEL_TEXT_CAPTION (yellow_default_caption), "yellow");
	test_text_caption_set_text_for_default_int_preferences (EEL_TEXT_CAPTION (red_default_caption), "red");
	
	eel_preferences_add_callback ("green", green_changed_callback, green_caption);
	eel_preferences_add_callback ("yellow", yellow_changed_callback, yellow_caption);
	eel_preferences_add_callback ("red", red_changed_callback, red_caption);

	gtk_box_pack_start (GTK_BOX (vbox), green_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), yellow_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), red_hbox, TRUE, TRUE, 0);

	gtk_widget_show_all (frame);

	return frame;
}

static GtkWidget *
fruits_frame_new (void)
{
	GtkWidget *apple_caption;
	GtkWidget *apple_default_caption;
	GtkWidget *apple_hbox;

	GtkWidget *orange_caption;
	GtkWidget *orange_default_caption;
	GtkWidget *orange_hbox;

	GtkWidget *pear_caption;
	GtkWidget *pear_default_caption;
	GtkWidget *pear_hbox;

	GtkWidget *frame;
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, 2);

	frame = gtk_frame_new ("fruits");
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	
	apple_hbox = entry_new ("fruits/apple", &apple_caption, &apple_default_caption);
	orange_hbox = entry_new ("fruits/orange", &orange_caption, &orange_default_caption);
	pear_hbox = entry_new ("fruits/pear", &pear_caption, &pear_default_caption);

	test_text_caption_set_text_for_int_preferences (EEL_TEXT_CAPTION (apple_caption), "fruits/apple");
	test_text_caption_set_text_for_int_preferences (EEL_TEXT_CAPTION (orange_caption), "fruits/orange");
	test_text_caption_set_text_for_int_preferences (EEL_TEXT_CAPTION (pear_caption), "fruits/pear");

	test_text_caption_set_text_for_default_int_preferences (EEL_TEXT_CAPTION (apple_default_caption), "fruits/apple");
	test_text_caption_set_text_for_default_int_preferences (EEL_TEXT_CAPTION (orange_default_caption), "fruits/orange");
	test_text_caption_set_text_for_default_int_preferences (EEL_TEXT_CAPTION (pear_default_caption), "fruits/pear");
	
	eel_preferences_add_callback ("fruits/apple", apple_changed_callback, apple_caption);
	eel_preferences_add_callback ("fruits/orange", orange_changed_callback, orange_caption);
	eel_preferences_add_callback ("fruits/pear", pear_changed_callback, pear_caption);

	gtk_box_pack_start (GTK_BOX (vbox), apple_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), orange_hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), pear_hbox, TRUE, TRUE, 0);

	gtk_widget_show_all (frame);

	return frame;
}

int 
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *vbox;

	GtkWidget *colors_frame;
	GtkWidget *fruits_frame;

	test_init (&argc, &argv);

	nautilus_global_preferences_init ();

	window = test_window_new (NULL, 4);
	test_window_set_title_with_pid (GTK_WINDOW (window), "Preferences Display");

	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	colors_frame = colors_frame_new ();
	fruits_frame = fruits_frame_new ();

	gtk_box_pack_start (GTK_BOX (vbox), colors_frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), fruits_frame, TRUE, TRUE, 0);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
