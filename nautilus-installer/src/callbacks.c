#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "installer.h"

#include "Banner_Left.xpm"
#include "Step_Two_Top.xpm"
#include "Step_Three_Top.xpm"
#include "Step_One_Top.xpm"
#include "Final_Top.xpm"

void
druid_cancel                           (GnomeDruid      *gnomedruid,
                                        gpointer         data)
{
	exit (1);
}


void
begin_install                          (GtkButton       *button,
                                        gpointer         window)
{
	GnomeDruid *druid;

	druid = GNOME_DRUID (gtk_object_get_data (GTK_OBJECT (window), "druid"));
	gnome_druid_set_buttons_sensitive(druid,TRUE,FALSE,TRUE);

	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

	if (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (window), 
						    "fullbutton"))->active) {
		g_message ("full install");
		installer (window, FULL_INST);
	} else if (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (window),
							   "nautilus_only_button"))->active) {
		g_message ("nautilus only");
		installer (window, NAUTILUS_ONLY);
	} else if (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (window), 
							   "services_only_button"))->active) {
		g_message ("services only");
		installer (window, SERVICES_ONLY);
	} else if (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (window), 
							   "upgrade_button"))->active) {
		g_message ("upgrade");
		installer (window, UPGRADE);
	} else if (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (window), 
							   "uninstall_button"))->active) {
		g_message ("uninstall");
		installer (window, UNINSTALL);
	} 

	gnome_druid_set_buttons_sensitive(druid,TRUE,TRUE,TRUE);
}


void
druid_finish                           (GnomeDruidPage  *gnomedruidpage,
                                        gpointer         arg1,
                                        gpointer         user_data)
{
	exit (0);
}

void
prep_install                           (GnomeDruidPage  *gnomedruidpage,
                                        gpointer         arg1,
                                        gpointer         window)
{
	GnomeDruid *druid;
	GtkButton *button;

	button = GTK_BUTTON (gtk_object_get_data (GTK_OBJECT (window), "begin_button"));
	gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
	druid = GNOME_DRUID (gtk_object_get_data (GTK_OBJECT (window), "druid"));
	gnome_druid_set_buttons_sensitive(druid,TRUE,FALSE,TRUE);
}

void
set_images  (GtkWidget *window)
{

	GnomeDruidPage *page;
/*
	gtk_rc_parse_string ("style \"default\" "
			     "{"
			     "bg[SELECTED] = { 0.80, 0.80, 0.80 }"
			     "fg[SELECTED] = { 0.00, 0.33, 0.99 }"
			     "bg[ACTIVE] = { 0.80, 0.80, 0.80 }"
			     "fg[ACTIVE] = { 0.00, 0.33, 0.99 }"
			     
			     "bg[NORMAL] = { 0.99, 0.99, 0.99 }"
			     "bg[PRELIGHT] = { 0.94, 0.94, 0.94 }"
			     "fg[PRELIGHT] = { 0.00, 0.33, 0.99 }"
			     "bg[INSENSITIVE] = { 0.80, 0.80, 0.80 }"
			     
			     "fg[NORMAL] = { 0.00, 0.00, 0.00 }"
			     
			     "}"
			     
			     "widget_class \"*\" style \"default\""
		);
	*/
	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "start_page"));
	gnome_druid_page_start_set_logo (GNOME_DRUID_PAGE_START (page), gdk_imlib_create_image_from_xpm_data (step_one_top));
	gnome_druid_page_start_set_watermark (GNOME_DRUID_PAGE_START (page), gdk_imlib_create_image_from_xpm_data (banner_left));

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "what_to_do_page"));
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page), gdk_imlib_create_image_from_xpm_data (step_two_top));

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "install_page"));
	gnome_druid_page_standard_set_logo (GNOME_DRUID_PAGE_STANDARD (page), gdk_imlib_create_image_from_xpm_data (step_three_top));

	page = GNOME_DRUID_PAGE (gtk_object_get_data(GTK_OBJECT (window), "finish_page"));
	gnome_druid_page_finish_set_logo (GNOME_DRUID_PAGE_FINISH (page), gdk_imlib_create_image_from_xpm_data (final_top));
	gnome_druid_page_finish_set_watermark (GNOME_DRUID_PAGE_FINISH (page), gdk_imlib_create_image_from_xpm_data (banner_left));
}


void set_white_stuff (GtkWidget *w) {
	GtkStyle *style;
	GdkColor *color;

	style = gtk_style_copy (w->style);
	style->bg[GTK_STATE_NORMAL].red = 65000;
	style->bg[GTK_STATE_NORMAL].blue = 65000;
	style->bg[GTK_STATE_NORMAL].green = 65000;
	gtk_widget_set_style (w, style);
        gtk_style_unref (style);
}
