
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

GtkWidget*
create_what_to_do_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *what_to_do_page;
	GdkColor what_to_do_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor what_to_do_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor what_to_do_page_title_color = { 0, 65535, 65535, 65535 };
	GtkWidget *druid_vbox1;
	GtkWidget *vbox3;
	GtkWidget *label10;
	GtkWidget *fixed3;
	GSList *fixed3_group = NULL;
	GtkWidget *fullbutton;
	GtkWidget *nautilus_only_button;
	GtkWidget *services_only_button;
	GtkWidget *upgrade_button;
	GtkWidget *uninstall_button;

	what_to_do_page = gnome_druid_page_standard_new_with_vals ("", NULL);

	set_white_stuff (GTK_WIDGET (what_to_do_page));

	gtk_widget_set_name (what_to_do_page, "what_to_do_page");
	gtk_widget_ref (what_to_do_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "what_to_do_page", what_to_do_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (what_to_do_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (what_to_do_page));
	gnome_druid_page_standard_set_bg_color (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), 
						&what_to_do_page_bg_color);
	gnome_druid_page_standard_set_logo_bg_color (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), 
						     &what_to_do_page_logo_bg_color);
	gnome_druid_page_standard_set_title_color (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), 
						   &what_to_do_page_title_color);
	gnome_druid_page_standard_set_title (GNOME_DRUID_PAGE_STANDARD (what_to_do_page), _("What to do ?"));

	druid_vbox1 = GNOME_DRUID_PAGE_STANDARD (what_to_do_page)->vbox;
	set_white_stuff (GTK_WIDGET (druid_vbox1));
	gtk_widget_set_name (druid_vbox1, "druid_vbox1");
	gtk_widget_ref (druid_vbox1);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid_vbox1", druid_vbox1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid_vbox1);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_name (vbox3, "vbox3");
	gtk_widget_ref (vbox3);
	gtk_object_set_data_full (GTK_OBJECT (window), "vbox3", vbox3,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (druid_vbox1), vbox3, TRUE, TRUE, 0);

	label10 = gtk_label_new (_("You have several choices for what you would like the installer to do.\n"
				   "Please choose one and click on the \"Next\" button to begin install."));
	gtk_widget_set_name (label10, "label10");
	gtk_widget_ref (label10);
	gtk_object_set_data_full (GTK_OBJECT (window), "label10", label10,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label10);
	gtk_box_pack_start (GTK_BOX (vbox3), label10, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label10), GTK_JUSTIFY_LEFT);

	fixed3 = gtk_fixed_new ();
	set_white_stuff (GTK_WIDGET (fixed3));
	gtk_widget_set_name (fixed3, "fixed3");
	gtk_widget_ref (fixed3);
	gtk_object_set_data_full (GTK_OBJECT (window), "fixed3", fixed3,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (fixed3);
	gtk_box_pack_start (GTK_BOX (vbox3), fixed3, TRUE, TRUE, 0);

	fullbutton = gtk_radio_button_new_with_label (fixed3_group, _("Most recent build"));
	fixed3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (fullbutton));
	gtk_widget_set_name (fullbutton, "fullbutton");
	gtk_widget_ref (fullbutton);
	gtk_object_set_data_full (GTK_OBJECT (window), "fullbutton", fullbutton,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (fullbutton);
	gtk_fixed_put (GTK_FIXED (fixed3), fullbutton, 72, 24);
	gtk_widget_set_uposition (fullbutton, 72, 24);
	gtk_widget_set_usize (fullbutton, 0, 0);

	nautilus_only_button = gtk_radio_button_new_with_label (fixed3_group, _("Stable Nautilus"));
	fixed3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (nautilus_only_button));
	gtk_widget_set_name (nautilus_only_button, "nautilus_only_button");
	gtk_widget_ref (nautilus_only_button);
	gtk_object_set_data_full (GTK_OBJECT (window), "nautilus_only_button", nautilus_only_button,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (nautilus_only_button);
	gtk_fixed_put (GTK_FIXED (fixed3), nautilus_only_button, 72, 56);
	gtk_widget_set_uposition (nautilus_only_button, 72, 56);
	gtk_widget_set_usize (nautilus_only_button, 0, 0);
/*
	services_only_button = gtk_radio_button_new_with_label (fixed3_group, _("*"));
	fixed3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (services_only_button));
	gtk_widget_set_name (services_only_button, "services_only_button");
	gtk_widget_ref (services_only_button);
	gtk_object_set_data_full (GTK_OBJECT (window), "services_only_button", services_only_button,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (services_only_button);
	gtk_fixed_put (GTK_FIXED (fixed3), services_only_button, 72, 88);
	gtk_widget_set_uposition (services_only_button, 72, 88);
	gtk_widget_set_usize (services_only_button, 0, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (services_only_button), FALSE);

	upgrade_button = gtk_radio_button_new_with_label (fixed3_group, _("*"));
	fixed3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (upgrade_button));
	gtk_widget_set_name (upgrade_button, "upgrade_button");
	gtk_widget_ref (upgrade_button);
	gtk_object_set_data_full (GTK_OBJECT (window), "upgrade_button", upgrade_button,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (upgrade_button);
	gtk_fixed_put (GTK_FIXED (fixed3), upgrade_button, 72, 120);
	gtk_widget_set_uposition (upgrade_button, 72, 120);
	gtk_widget_set_usize (upgrade_button, 0, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (upgrade_button), FALSE);
*/
	uninstall_button = gtk_radio_button_new_with_label (fixed3_group, _("Uninstall"));
	fixed3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (uninstall_button));
	gtk_widget_set_name (uninstall_button, "uninstall_button");
	gtk_widget_ref (uninstall_button);
	gtk_object_set_data_full (GTK_OBJECT (window), "uninstall_button", uninstall_button,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (uninstall_button);
	gtk_fixed_put (GTK_FIXED (fixed3), uninstall_button, 72, 152);
	gtk_widget_set_uposition (uninstall_button, 72, 152);
	gtk_widget_set_usize (uninstall_button, 0, 0);
	gtk_widget_set_sensitive (GTK_WIDGET (uninstall_button), FALSE);
	
	return what_to_do_page;
}

GtkWidget*
create_install_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *install_page;
	GdkColor install_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor install_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor install_page_title_color = { 0, 65535, 65535, 65535 };
	GtkWidget *druid_vbox2;
	GtkWidget *vbox5;
	GtkWidget *label11;
	GtkWidget *table2;
	GtkWidget *label12;
	GtkWidget *label13;
	GtkWidget *action_label;
	GtkWidget *progressbar1;
	GtkWidget *progressbar2;
	GtkWidget *package_label;
	GtkWidget *fixed1;
	GtkWidget *textbox;
	GtkWidget *scrolledwindow;
	const char *download_description;
	int download_description_length;
	
	download_description = g_strdup (_("Currently downloading packages required to "
					   "install Nautilus\n"));
	download_description_length = strlen (download_description);

	install_page = gnome_druid_page_standard_new_with_vals ("", NULL);
	set_white_stuff (GTK_WIDGET (install_page));
	gtk_widget_set_name (install_page, "install_page");
	gtk_widget_ref (install_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "install_page", install_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show_all (install_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (install_page));
	gnome_druid_page_standard_set_bg_color (GNOME_DRUID_PAGE_STANDARD (install_page), 
						&install_page_bg_color);
	gnome_druid_page_standard_set_logo_bg_color (GNOME_DRUID_PAGE_STANDARD (install_page), 
						     &install_page_logo_bg_color);
	gnome_druid_page_standard_set_title_color (GNOME_DRUID_PAGE_STANDARD (install_page), 
						   &install_page_title_color);
	gnome_druid_page_standard_set_title (GNOME_DRUID_PAGE_STANDARD (install_page), _("Progress..."));

	druid_vbox2 = GNOME_DRUID_PAGE_STANDARD (install_page)->vbox;
	gtk_widget_set_name (druid_vbox2, "druid_vbox2");
	gtk_widget_ref (druid_vbox2);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid_vbox2", druid_vbox2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid_vbox2);

	vbox5 = gtk_vbox_new (FALSE, 16);
	set_white_stuff (GTK_WIDGET (vbox5));
	gtk_widget_set_name (vbox5, "vbox5");
	gtk_widget_ref (vbox5);
	gtk_object_set_data_full (GTK_OBJECT (window), "vbox5", vbox5,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox5);
	gtk_box_pack_start (GTK_BOX (druid_vbox2), vbox5, FALSE, FALSE, 16);

	table2 = gtk_table_new (3, 2, FALSE);
	set_white_stuff (GTK_WIDGET (table2));
	gtk_widget_set_name (table2, "table2");
	gtk_widget_ref (table2);
	gtk_object_set_data_full (GTK_OBJECT (window), "table2", table2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (table2);
	gtk_box_pack_start (GTK_BOX (vbox5), table2, FALSE, TRUE, 16);
	gtk_table_set_row_spacings (GTK_TABLE (table2), 16);

	progressbar1 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar1, "progressbar_single");
	gtk_widget_ref (progressbar1);
	gtk_object_set_data_full (GTK_OBJECT (window), "progressbar_single", progressbar1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_progress_set_show_text (GTK_PROGRESS (progressbar1), TRUE);		  
	gtk_widget_show (progressbar1);
	gtk_table_attach (GTK_TABLE (table2), progressbar1, 1, 2, 1, 2,
			  /* GTK_EXPAND */ 0,
			  /* GTK_EXPAND */ GTK_SHRINK,
			  0, 0);

	progressbar2 = gtk_progress_bar_new ();
	gtk_widget_set_name (progressbar2, "progressbar_overall");
	gtk_widget_ref (progressbar2);
	gtk_object_set_data_full (GTK_OBJECT (window), "progressbar_overall", progressbar2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_progress_set_format_string (GTK_PROGRESS (progressbar2), "Waiting for download...");
	gtk_progress_set_show_text (GTK_PROGRESS (progressbar2), TRUE);		  
	/* gtk_widget_show (progressbar2); */
	gtk_table_attach (GTK_TABLE (table2), progressbar2, 1, 2, 2, 3,
			  /* GTK_EXPAND */ 0,
			  /* GTK_EXPAND */ GTK_SHRINK,
			  0, 0);

	package_label = gtk_label_new (_("                                                     "));
	gtk_widget_set_name (package_label, "package_label");
	gtk_widget_ref (package_label);
	gtk_object_set_data_full (GTK_OBJECT (window), "package_label", package_label,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (package_label);
	gtk_table_attach (GTK_TABLE (table2), package_label, 1, 2, 0, 1,
			  GTK_EXPAND,
			  GTK_EXPAND,
			  0, 0);

	fixed1 = gtk_fixed_new ();
	gtk_widget_set_name (fixed1, "fixed1");
	set_white_stuff (GTK_WIDGET (fixed1));
	gtk_widget_ref (fixed1);
	gtk_object_set_data_full (GTK_OBJECT (window), "fixed1", fixed1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (fixed1);
	gtk_box_pack_start (GTK_BOX (vbox5), fixed1, TRUE, TRUE, 16);	

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_name (scrolledwindow, "scrolledwindow");
	gtk_widget_ref (scrolledwindow);
	gtk_object_set_data_full (GTK_OBJECT (window), "scrolledwindow", scrolledwindow,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scrolledwindow);
	gtk_box_pack_start (GTK_BOX (vbox5), scrolledwindow, TRUE, TRUE, 16);	
    

	textbox = gtk_text_new (NULL, NULL);
	gtk_widget_set_name (textbox, "summary");
	gtk_widget_ref (textbox);
	gtk_text_set_editable (GTK_TEXT (textbox), FALSE);
	gtk_text_set_word_wrap (GTK_TEXT (textbox), TRUE);
	gtk_object_set_data_full (GTK_OBJECT (window), "summary", textbox,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_text_insert (GTK_TEXT (textbox), NULL, NULL, NULL,
			 download_description, download_description_length);
	gtk_widget_show (textbox);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), 
					       textbox);
	
	return install_page;
}

GtkWidget*
create_finish_page (GtkWidget *druid, GtkWidget *window)
{
	GtkWidget *finish_page;
	GdkColor finish_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor finish_page_textbox_color = { 0, 65535, 65535, 65535 };
	GdkColor finish_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor finish_page_title_color = { 0, 65535, 65535, 65535 };

	finish_page = gnome_druid_page_finish_new ();
	gtk_widget_set_name (finish_page, "finish_page");
	gtk_widget_ref (finish_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "finish_page", finish_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (finish_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (finish_page));
	gnome_druid_page_finish_set_bg_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
					      &finish_page_bg_color);
	gnome_druid_page_finish_set_textbox_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
						   &finish_page_textbox_color);
	gnome_druid_page_finish_set_logo_bg_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
						   &finish_page_logo_bg_color);
	gnome_druid_page_finish_set_title_color (GNOME_DRUID_PAGE_FINISH (finish_page), 
						 &finish_page_title_color);
	gnome_druid_page_finish_set_title (GNOME_DRUID_PAGE_FINISH (finish_page), _("Finished"));
	gnome_druid_page_finish_set_text (GNOME_DRUID_PAGE_FINISH (finish_page), 
					  _("If the installation was successfull, you can\n"
					    "find the nautilus icon in the applications menu.\n"
					    "If you restart GMC, you can also use the desktop icon, which\n"
					    "I'll install when you click \"Finish\".\n\n"
					    "Thanks for taking the time to try out Nautilus.\n\n"
					    "May your life be a healthy and happy one."));

	return finish_page;
}

GtkWidget*
create_window (void)
{
	GtkWidget *window;
	GtkWidget *druid;
	GtkWidget *start_page;
	GdkColor start_page_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor start_page_textbox_color = { 0, 65535, 65535, 65535 };
	GdkColor start_page_logo_bg_color = { 0, 3341, 23130, 26214 };
	GdkColor start_page_title_color = { 0, 65535, 65535, 65535 };
	GtkWidget *what_to_do_page;
	GtkWidget *install_page;
	GtkWidget *finish_page;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name (window, "window");
	gtk_object_set_data (GTK_OBJECT (window), "window", window);
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus install tool"));

	druid = gnome_druid_new ();
	gtk_widget_set_name (druid, "druid");
	gtk_widget_ref (druid);
	gtk_object_set_data_full (GTK_OBJECT (window), "druid", druid,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (druid);
	gtk_container_add (GTK_CONTAINER (window), druid);

	start_page = gnome_druid_page_start_new ();
	gtk_widget_set_name (start_page, "start_page");
	gtk_widget_ref (start_page);
	gtk_object_set_data_full (GTK_OBJECT (window), "start_page", start_page,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (start_page);
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));
	gnome_druid_set_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));
	gnome_druid_page_start_set_bg_color (GNOME_DRUID_PAGE_START (start_page), 
					     &start_page_bg_color);
	gnome_druid_page_start_set_textbox_color (GNOME_DRUID_PAGE_START (start_page), 
						  &start_page_textbox_color);
	gnome_druid_page_start_set_logo_bg_color (GNOME_DRUID_PAGE_START (start_page), 
						  &start_page_logo_bg_color);
	gnome_druid_page_start_set_title_color (GNOME_DRUID_PAGE_START (start_page), 
						&start_page_title_color);
	gnome_druid_page_start_set_title (GNOME_DRUID_PAGE_START (start_page), _("Step one:"));
	gnome_druid_page_start_set_text (GNOME_DRUID_PAGE_START (start_page), 
					 _("This is the internal Nautilus installer.\n\n"
					   "Lots of text should go here letting you know what you need\n"
					   "to have installed before you should even begin to think about\n"
					   "using this. For example:\n"
					   "\n"
					   "  * Stuff\n"
					   "  * More stuff\n"
					   "  * Other stuff\n"
					   "\n"
					   "If you meet these requirements, hit the \"Next\" button to continue!\n\n"));

	what_to_do_page = create_what_to_do_page (druid, window);
	install_page = create_install_page (druid, window);
	finish_page = create_finish_page (druid, window);

	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
			    GTK_SIGNAL_FUNC (druid_cancel),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (install_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (install_page), "prepare",
			    GTK_SIGNAL_FUNC (prep_install),
			    window);
	gtk_signal_connect (GTK_OBJECT (finish_page), "finish",
			    GTK_SIGNAL_FUNC (druid_finish),
			    NULL);

	return window;
}

