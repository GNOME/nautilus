#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "installer.h"

void
druid_cancel                           (GnomeDruid      *gnomedruid,
                                        gpointer         data)
{
	g_message ("Installation cancelled");
	exit (1);
}


void
begin_install                          (GtkButton       *button,
                                        gpointer         window)
{
	GnomeDruid *druid;
	g_message ("Begin install");

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
	g_message ("Installation completed");
	exit (0);
}





void
prep_install                           (GnomeDruidPage  *gnomedruidpage,
                                        gpointer         arg1,
                                        gpointer         window)
{
	GnomeDruid *druid;
	GtkButton *button;
	g_message ("in prep_install");

	button = GTK_BUTTON (gtk_object_get_data (GTK_OBJECT (window), "begin_button"));
	gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
	druid = GNOME_DRUID (gtk_object_get_data (GTK_OBJECT (window), "druid"));
	gnome_druid_set_buttons_sensitive(druid,TRUE,FALSE,TRUE);
}


