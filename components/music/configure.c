#include "mpg123.h"

static GtkWidget *mpg123_configurewin = NULL;
static GtkWidget *vbox, *notebook;
static GtkWidget *decode_vbox, *decode_hbox1;
static GtkWidget *decode_res_frame, *decode_res_vbox, *decode_res_16, *decode_res_8;
static GtkWidget *decode_ch_frame, *decode_ch_vbox, *decode_ch_stereo,
         *decode_ch_mono;
static GtkWidget *decode_freq_frame, *decode_freq_vbox, *decode_freq_1to1,
         *decode_freq_1to2, *decode_freq_1to4;
static GtkWidget *option_frame, *option_vbox, *detect_by_content;
#ifdef USE_3DNOW
static GtkWidget *decoder_frame, *decoder_vbox, *auto_select,
         *decoder_3dnow, *decoder_fpu;
#endif

/* unused
   static GtkWidget *decode_freq_custom,*decode_freq_custom_hbox,*decode_freq_custom_spin,*decode_freq_custom_label;
   static GtkObject *decode_freq_custom_adj;
 */
static GtkObject *streaming_size_adj, *streaming_pre_adj;
static GtkWidget *streaming_proxy_use, *streaming_proxy_host_entry;
static GtkWidget *streaming_proxy_port_entry, *streaming_save_use, *streaming_save_entry;
static GtkWidget *streaming_proxy_auth_use;
static GtkWidget *streaming_proxy_auth_pass_entry, *streaming_proxy_auth_user_entry;
static GtkWidget *streaming_proxy_auth_user_label, *streaming_proxy_auth_pass_label;
static GtkWidget *streaming_cast_title, *streaming_udp_title;
static GtkWidget *streaming_proxy_hbox, *streaming_proxy_auth_hbox, *streaming_save_dirbrowser;
static GtkWidget *streaming_save_hbox, *title_id3_box, *title_id3_desc_box;
static GtkWidget *title_id3_use, *title_id3_entry, *title_id3v2_disable;

MPG123Config mpg123_cfg;

static void mpg123_configurewin_ok(GtkWidget * widget, gpointer data)
{
	ConfigFile *cfg;
	gchar *filename;

	if (GTK_TOGGLE_BUTTON(decode_res_16)->active)
		mpg123_cfg.resolution = 16;
	else if (GTK_TOGGLE_BUTTON(decode_res_8)->active)
		mpg123_cfg.resolution = 8;

	if (GTK_TOGGLE_BUTTON(decode_ch_stereo)->active)
		mpg123_cfg.channels = 2;
	else if (GTK_TOGGLE_BUTTON(decode_ch_mono)->active)
		mpg123_cfg.channels = 1;

	if (GTK_TOGGLE_BUTTON(decode_freq_1to1)->active)
		mpg123_cfg.downsample = 0;
	else if (GTK_TOGGLE_BUTTON(decode_freq_1to2)->active)
		mpg123_cfg.downsample = 1;
	if (GTK_TOGGLE_BUTTON(decode_freq_1to4)->active)
		mpg123_cfg.downsample = 2;
	mpg123_cfg.detect_by_content = GTK_TOGGLE_BUTTON(detect_by_content)->active;
/*
   if(GTK_TOGGLE_BUTTON(decode_freq_custom)->active)
   mpg123_cfg.downsample=3;
   mpg123_cfg.downsample_custom=(gint)GTK_ADJUSTMENT(decode_freq_custom_adj)->value;
 */
#ifdef USE_3DNOW
	if (GTK_TOGGLE_BUTTON(auto_select)->active)
                mpg123_cfg.use_3dnow = 0;
        else if (GTK_TOGGLE_BUTTON(decoder_fpu)->active)
                mpg123_cfg.use_3dnow = 2;
        else mpg123_cfg.use_3dnow = 1;
#endif
	mpg123_cfg.http_buffer_size = (gint) GTK_ADJUSTMENT(streaming_size_adj)->value;
	mpg123_cfg.http_prebuffer = (gint) GTK_ADJUSTMENT(streaming_pre_adj)->value;

	mpg123_cfg.use_proxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_proxy_use));
	g_free(mpg123_cfg.proxy_host);
	mpg123_cfg.proxy_host = g_strdup(gtk_entry_get_text(GTK_ENTRY(streaming_proxy_host_entry)));
	mpg123_cfg.proxy_port = atoi(gtk_entry_get_text(GTK_ENTRY(streaming_proxy_port_entry)));

	mpg123_cfg.proxy_use_auth = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_proxy_auth_use));

	if(mpg123_cfg.proxy_user)
		g_free(mpg123_cfg.proxy_user);
	mpg123_cfg.proxy_user = NULL;
	if(strlen(gtk_entry_get_text(GTK_ENTRY(streaming_proxy_auth_user_entry))) > 0)
		mpg123_cfg.proxy_user = g_strdup(gtk_entry_get_text(GTK_ENTRY(streaming_proxy_auth_user_entry)));

	if(mpg123_cfg.proxy_pass)
		g_free(mpg123_cfg.proxy_pass);
	mpg123_cfg.proxy_pass = NULL;
	if(strlen(gtk_entry_get_text(GTK_ENTRY(streaming_proxy_auth_pass_entry))) > 0)
		mpg123_cfg.proxy_pass = g_strdup(gtk_entry_get_text(GTK_ENTRY(streaming_proxy_auth_pass_entry)));
	
	
	mpg123_cfg.save_http_stream = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_save_use));
	if (mpg123_cfg.save_http_path)
		g_free(mpg123_cfg.save_http_path);
	mpg123_cfg.save_http_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(streaming_save_entry)));

	mpg123_cfg.cast_title_streaming = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_cast_title));
	mpg123_cfg.use_udp_channel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_udp_title));
	
	mpg123_cfg.use_id3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_id3_use));
	mpg123_cfg.disable_id3v2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_id3v2_disable));
	g_free(mpg123_cfg.id3_format);
	mpg123_cfg.id3_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(title_id3_entry)));

	filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
	cfg = xmms_cfg_open_file(filename);
	if (!cfg)
		cfg = xmms_cfg_new();
	xmms_cfg_write_int(cfg, "MPG123", "resolution", mpg123_cfg.resolution);
	xmms_cfg_write_int(cfg, "MPG123", "channels", mpg123_cfg.channels);
	xmms_cfg_write_int(cfg, "MPG123", "downsample", mpg123_cfg.downsample);
/*      xmms_cfg_write_int(cfg,"MPG123","downsample_custom",mpg123_cfg.downsample_custom); */
	xmms_cfg_write_int(cfg, "MPG123", "http_buffer_size", mpg123_cfg.http_buffer_size);
	xmms_cfg_write_int(cfg, "MPG123", "http_prebuffer", mpg123_cfg.http_prebuffer);
	xmms_cfg_write_boolean(cfg, "MPG123", "use_proxy", mpg123_cfg.use_proxy);
	xmms_cfg_write_string(cfg, "MPG123", "proxy_host", mpg123_cfg.proxy_host);
	xmms_cfg_write_int(cfg, "MPG123", "proxy_port", mpg123_cfg.proxy_port);
	xmms_cfg_write_boolean(cfg, "MPG123", "proxy_use_auth", mpg123_cfg.proxy_use_auth);
	if(mpg123_cfg.proxy_user)
		xmms_cfg_write_string(cfg, "MPG123", "proxy_user", mpg123_cfg.proxy_user);
	else
		xmms_cfg_remove_key(cfg, "MPG123", "proxy_user");
	if(mpg123_cfg.proxy_pass)
		xmms_cfg_write_string(cfg, "MPG123", "proxy_pass", mpg123_cfg.proxy_pass);
	else
		xmms_cfg_remove_key(cfg, "MPG123", "proxy_pass");
	xmms_cfg_write_boolean(cfg, "MPG123", "save_http_stream", mpg123_cfg.save_http_stream);
	xmms_cfg_write_string(cfg, "MPG123", "save_http_path", mpg123_cfg.save_http_path);
	xmms_cfg_write_boolean(cfg, "MPG123", "cast_title_streaming", mpg123_cfg.cast_title_streaming);
	xmms_cfg_write_boolean(cfg, "MPG123", "use_udp_channel", mpg123_cfg.use_udp_channel);
	xmms_cfg_write_boolean(cfg, "MPG123", "use_id3", mpg123_cfg.use_id3);
	xmms_cfg_write_boolean(cfg, "MPG123", "disable_id3v2", mpg123_cfg.disable_id3v2);
	xmms_cfg_write_string(cfg, "MPG123", "id3_format", mpg123_cfg.id3_format);
	xmms_cfg_write_boolean(cfg, "MPG123", "detect_by_content", mpg123_cfg.detect_by_content);
#ifdef USE_3DNOW
	xmms_cfg_write_int(cfg, "MPG123", "use_3dnow", mpg123_cfg.use_3dnow);
#endif
	xmms_cfg_write_file(cfg, filename);
	xmms_cfg_free(cfg);
	g_free(filename);
	gtk_widget_destroy(mpg123_configurewin);
}

#ifdef USE_3DNOW
static void auto_select_cb(GtkWidget * w, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(auto_select)) == TRUE) {
	  gtk_widget_set_sensitive(decoder_fpu, FALSE);
	  gtk_widget_set_sensitive(decoder_3dnow, FALSE);
	  if (support_3dnow() == TRUE) 
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decoder_3dnow), TRUE);
	  else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decoder_fpu), TRUE);
	} else {
	    gtk_widget_set_sensitive(decoder_3dnow, TRUE);
	    gtk_widget_set_sensitive(decoder_fpu, TRUE);
	}
}

static void use_3dnow_cb(GtkWidget * w, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(decoder_3dnow)) == TRUE) {
		mpg123_cfg.resolution = 16;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_res_16), TRUE);
		gtk_widget_set_sensitive(decode_res_8, FALSE);

		mpg123_cfg.channels = 2;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_ch_stereo), TRUE);
		gtk_widget_set_sensitive(decode_ch_mono, FALSE);

		mpg123_cfg.downsample = 0;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to1), TRUE);
		gtk_widget_set_sensitive(decode_freq_1to2, FALSE);
		gtk_widget_set_sensitive(decode_freq_1to4, FALSE);
	}
}

static void use_fpu_cb(GtkWidget * w, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(decoder_fpu)) == TRUE) {
		gtk_widget_set_sensitive(decode_res_8, TRUE);

		gtk_widget_set_sensitive(decode_ch_mono, TRUE);

		gtk_widget_set_sensitive(decode_freq_1to2, TRUE);
		gtk_widget_set_sensitive(decode_freq_1to4, TRUE);
	}
}
#endif

static void proxy_use_cb(GtkWidget * w, gpointer data)
{
	gboolean use_proxy, use_proxy_auth;

	use_proxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_proxy_use));
	use_proxy_auth = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_proxy_auth_use));
		
	gtk_widget_set_sensitive(streaming_proxy_hbox, use_proxy);
	gtk_widget_set_sensitive(streaming_proxy_auth_use, use_proxy);
	gtk_widget_set_sensitive(streaming_proxy_auth_hbox, use_proxy && use_proxy_auth);
}

static void proxy_auth_use_cb(GtkWidget *w, gpointer data)
{
	gboolean use_proxy, use_proxy_auth;

	use_proxy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_proxy_use));
	use_proxy_auth = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_proxy_auth_use));

	gtk_widget_set_sensitive(streaming_proxy_auth_hbox, use_proxy && use_proxy_auth);
}

static void streaming_save_dirbrowser_cb(gchar * dir)
{
	gtk_entry_set_text(GTK_ENTRY(streaming_save_entry), dir);
}

static void streaming_save_browse_cb(GtkWidget * w, gpointer data)
{
	if (!streaming_save_dirbrowser)
	{
		streaming_save_dirbrowser = xmms_create_dir_browser(_("Select the directory where you want to store the MPEG streams:"),
								    mpg123_cfg.save_http_path, GTK_SELECTION_SINGLE, streaming_save_dirbrowser_cb);
		gtk_signal_connect(GTK_OBJECT(streaming_save_dirbrowser), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &streaming_save_dirbrowser);
		gtk_window_set_transient_for(GTK_WINDOW(streaming_save_dirbrowser), GTK_WINDOW(mpg123_configurewin));
		gtk_widget_show(streaming_save_dirbrowser);
	}
}

static void streaming_save_use_cb(GtkWidget * w, gpointer data)
{
	gboolean save_stream;

	save_stream = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(streaming_save_use));

	gtk_widget_set_sensitive(streaming_save_hbox, save_stream);
}

static void title_id3_use_cb(GtkWidget * w, gpointer data)
{
	gboolean use_id3;

	use_id3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_id3_use));

	gtk_widget_set_sensitive(title_id3v2_disable, use_id3);
	gtk_widget_set_sensitive(title_id3_box, use_id3);
	gtk_widget_set_sensitive(title_id3_desc_box, use_id3);
}

static void configure_destroy(GtkWidget * w, gpointer data)
{
	if (streaming_save_dirbrowser)
		gtk_widget_destroy(streaming_save_dirbrowser);
}

void mpg123_configure(void)
{
	GtkWidget *streaming_vbox;
	GtkWidget *streaming_buf_frame, *streaming_buf_hbox;
	GtkWidget *streaming_size_box, *streaming_size_label, *streaming_size_spin;
	GtkWidget *streaming_pre_box, *streaming_pre_label, *streaming_pre_spin;
	GtkWidget *streaming_proxy_frame, *streaming_proxy_vbox;
	GtkWidget *streaming_proxy_port_label, 	*streaming_proxy_host_label;
	GtkWidget *streaming_save_frame, *streaming_save_vbox;
	GtkWidget *streaming_save_label, *streaming_save_browse;
	GtkWidget *streaming_cast_frame, *streaming_cast_vbox;
	GtkWidget *title_frame, *title_id3_vbox, *title_id3_label;
	GtkWidget *title_id3_desc_label1, *title_id3_desc_label2;
	GtkWidget *bbox, *ok, *cancel;

	gchar *temp;

	if (mpg123_configurewin != NULL)
	{
		gdk_window_raise(mpg123_configurewin->window);
		return;
	}
	mpg123_configurewin = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect(GTK_OBJECT(mpg123_configurewin), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &mpg123_configurewin);
	gtk_signal_connect(GTK_OBJECT(mpg123_configurewin), "destroy", GTK_SIGNAL_FUNC(configure_destroy), &mpg123_configurewin);
	gtk_window_set_title(GTK_WINDOW(mpg123_configurewin), _("MPG123 Configuration"));
	gtk_window_set_wmclass(GTK_WINDOW(mpg123_configurewin), "mpg123_configuration", "Nautilus");
	gtk_window_set_policy(GTK_WINDOW(mpg123_configurewin), FALSE, FALSE, FALSE);
	/*  gtk_window_set_position(GTK_WINDOW(mpg123_configurewin), GTK_WIN_POS_MOUSE); */
	gtk_container_border_width(GTK_CONTAINER(mpg123_configurewin), 10);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(mpg123_configurewin), vbox);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	decode_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(decode_vbox), 5);

	decode_hbox1 = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(decode_vbox), decode_hbox1, FALSE, FALSE, 0);

	decode_res_frame = gtk_frame_new(_("Resolution:"));
	gtk_box_pack_start(GTK_BOX(decode_hbox1), decode_res_frame, TRUE, TRUE, 0);

	decode_res_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(decode_res_vbox), 5);
	gtk_container_add(GTK_CONTAINER(decode_res_frame), decode_res_vbox);

	decode_res_16 = gtk_radio_button_new_with_label(NULL, _("16 bit"));
	if (mpg123_cfg.resolution == 16)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_res_16), TRUE);
	gtk_box_pack_start(GTK_BOX(decode_res_vbox), decode_res_16, FALSE, FALSE, 0);
	gtk_widget_show(decode_res_16);

	decode_res_8 = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(decode_res_16)), _("8 bit"));
	if (mpg123_cfg.resolution == 8)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_res_8), TRUE);

	gtk_box_pack_start(GTK_BOX(decode_res_vbox), decode_res_8, FALSE, FALSE, 0);
	gtk_widget_show(decode_res_8);

#ifdef USE_3DNOW
	if (((support_3dnow() == TRUE) && (mpg123_cfg.use_3dnow !=2 )) ||
		((support_3dnow() == FALSE) && (mpg123_cfg.use_3dnow == 1)))
	{
		mpg123_cfg.resolution = 16;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_res_16), TRUE);
		gtk_widget_set_sensitive(decode_res_8, FALSE);
	}
#endif
	gtk_widget_show(decode_res_vbox);
	gtk_widget_show(decode_res_frame);

	decode_ch_frame = gtk_frame_new(_("Channels:"));
	gtk_box_pack_start(GTK_BOX(decode_hbox1), decode_ch_frame, TRUE, TRUE, 0);

	decode_ch_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(decode_ch_vbox), 5);
	gtk_container_add(GTK_CONTAINER(decode_ch_frame), decode_ch_vbox);

	decode_ch_stereo = gtk_radio_button_new_with_label(NULL, _("Stereo (if available)"));
	if (mpg123_cfg.channels == 2)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_ch_stereo), TRUE);

	gtk_box_pack_start(GTK_BOX(decode_ch_vbox), decode_ch_stereo, FALSE, FALSE, 0);
	gtk_widget_show(decode_ch_stereo);

	decode_ch_mono = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(decode_ch_stereo)), _("Mono"));
	if (mpg123_cfg.channels == 1)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_ch_mono), TRUE);

	gtk_box_pack_start(GTK_BOX(decode_ch_vbox), decode_ch_mono, FALSE, FALSE, 0);
	gtk_widget_show(decode_ch_mono);

#ifdef USE_3DNOW
	if (((support_3dnow() == TRUE) && (mpg123_cfg.use_3dnow !=2 )) ||
		((support_3dnow() == FALSE) && (mpg123_cfg.use_3dnow == 1)))
	{
		mpg123_cfg.channels = 2;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_ch_stereo), TRUE);
		gtk_widget_set_sensitive(decode_ch_mono, FALSE);
	}
#endif
	gtk_widget_show(decode_ch_vbox);
	gtk_widget_show(decode_ch_frame);
	gtk_widget_show(decode_hbox1);

	decode_freq_frame = gtk_frame_new(_("Down sample:"));
	gtk_box_pack_start(GTK_BOX(decode_vbox), decode_freq_frame, FALSE, FALSE, 0);

	decode_freq_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(decode_freq_vbox), 5);
	gtk_container_add(GTK_CONTAINER(decode_freq_frame), decode_freq_vbox);

	decode_freq_1to1 = gtk_radio_button_new_with_label(NULL, _("1:1 (44 kHz)"));
	if (mpg123_cfg.downsample == 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to1), TRUE);
	gtk_box_pack_start(GTK_BOX(decode_freq_vbox), decode_freq_1to1, FALSE, FALSE, 0);
	gtk_widget_show(decode_freq_1to1);

	decode_freq_1to2 = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(decode_freq_1to1)), _("1:2 (22 kHz)"));
	if (mpg123_cfg.downsample == 1)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to2), TRUE);
	gtk_box_pack_start(GTK_BOX(decode_freq_vbox), decode_freq_1to2, FALSE, FALSE, 0);
	gtk_widget_show(decode_freq_1to2);

	decode_freq_1to4 = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(decode_freq_1to1)), _("1:4 (11 kHz)"));
	if (mpg123_cfg.downsample == 2)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to4), TRUE);

	gtk_box_pack_start(GTK_BOX(decode_freq_vbox), decode_freq_1to4, FALSE, FALSE, 0);
	gtk_widget_show(decode_freq_1to4);
#ifdef USE_3DNOW
	if (((support_3dnow() == TRUE) && (mpg123_cfg.use_3dnow !=2 )) ||
		((support_3dnow() == FALSE) && (mpg123_cfg.use_3dnow == 1)))
	{
		mpg123_cfg.downsample = 0;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_1to1), TRUE);
		gtk_widget_set_sensitive(decode_freq_1to2, FALSE);
		gtk_widget_set_sensitive(decode_freq_1to4, FALSE);
	}
#endif
	/*decode_freq_custom_hbox=gtk_hbox_new(FALSE,5);
	   gtk_box_pack_start(GTK_BOX(decode_freq_vbox),decode_freq_custom_hbox,FALSE,FALSE,0);

	   decode_freq_custom=gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(decode_freq_1to1)),_("Custom"));
	   if(mpg123_cfg.downsample==3)
	   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decode_freq_custom),TRUE);
	   gtk_box_pack_start(GTK_BOX(decode_freq_custom_hbox),decode_freq_custom,FALSE,FALSE,0);
	   gtk_widget_show(decode_freq_custom);

	   decode_freq_custom_adj=gtk_adjustment_new(mpg123_cfg.downsample_custom,8000,48000,25,25,25);
	   decode_freq_custom_spin=gtk_spin_button_new(GTK_ADJUSTMENT(decode_freq_custom_adj),25,0);
	   gtk_widget_set_usize(decode_freq_custom_spin,60,-1);
	   gtk_box_pack_start(GTK_BOX(decode_freq_custom_hbox),decode_freq_custom_spin,FALSE,FALSE,0);
	   gtk_widget_show(decode_freq_custom_spin);

	   decode_freq_custom_label=gtk_label_new(_("Hz"));
	   gtk_box_pack_start(GTK_BOX(decode_freq_custom_hbox),decode_freq_custom_label,FALSE,FALSE,0);
	   gtk_widget_show(decode_freq_custom_label);

	   gtk_widget_show(decode_freq_custom_hbox); */


#ifdef USE_3DNOW
	decoder_frame = gtk_frame_new(_("Decoder:"));
	gtk_box_pack_start(GTK_BOX(decode_vbox), decoder_frame, FALSE, FALSE, 0);

	decoder_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(decoder_vbox), 5);
	gtk_container_add(GTK_CONTAINER(decoder_frame), decoder_vbox);

	auto_select = gtk_check_button_new_with_label(_("Enable Automatic detection"));
	if (mpg123_cfg.use_3dnow == 0)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_select), TRUE);
	gtk_box_pack_start(GTK_BOX(decoder_vbox), auto_select, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(auto_select), "clicked", GTK_SIGNAL_FUNC(auto_select_cb), NULL);
	gtk_widget_show(auto_select);

	decoder_3dnow = gtk_radio_button_new_with_label(NULL, _("use 3DNow! optimized decoder"));
	if (((support_3dnow() == TRUE) && mpg123_cfg.use_3dnow != 2) || (mpg123_cfg.use_3dnow == 1))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decoder_3dnow), TRUE);
	gtk_box_pack_start(GTK_BOX(decoder_vbox), decoder_3dnow, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(decoder_3dnow), "clicked", GTK_SIGNAL_FUNC(use_3dnow_cb), NULL);
	gtk_widget_show(decoder_3dnow);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(auto_select)) == TRUE)
	  gtk_widget_set_sensitive(decoder_3dnow, FALSE);

	decoder_fpu = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(decoder_3dnow)), _("use FPU decoder"));
	if (((support_3dnow() == FALSE) && mpg123_cfg.use_3dnow != 1) || (mpg123_cfg.use_3dnow == 2))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(decoder_fpu), TRUE);
	gtk_box_pack_start(GTK_BOX(decoder_vbox), decoder_fpu, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(decoder_fpu), "clicked", GTK_SIGNAL_FUNC(use_fpu_cb), NULL);
	gtk_widget_show(decoder_fpu);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(auto_select)) == TRUE)
	  gtk_widget_set_sensitive(decoder_fpu, FALSE);

	gtk_widget_show(decoder_vbox);
	gtk_widget_show(decoder_frame);
#endif

	gtk_widget_show(decode_freq_vbox);
	gtk_widget_show(decode_freq_frame);

	option_frame = gtk_frame_new(_("Options"));
	gtk_box_pack_start(GTK_BOX(decode_vbox), option_frame, FALSE, FALSE, 0);

	option_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(option_frame), option_vbox);

	detect_by_content = gtk_check_button_new_with_label(_("Detect files by content (instead of file extention)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(detect_by_content), mpg123_cfg.detect_by_content);
	gtk_box_pack_start(GTK_BOX(option_vbox), detect_by_content, FALSE, FALSE, 0);

	gtk_widget_show(detect_by_content);
	gtk_widget_show(option_vbox);
	gtk_widget_show(option_frame);
	
	gtk_widget_show(decode_vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), decode_vbox, gtk_label_new(_("Decoder")));

	streaming_vbox = gtk_vbox_new(FALSE, 0);

	streaming_buf_frame = gtk_frame_new(_("Buffering:"));
	gtk_container_set_border_width(GTK_CONTAINER(streaming_buf_frame), 5);
	gtk_box_pack_start(GTK_BOX(streaming_vbox), streaming_buf_frame, FALSE, FALSE, 0);

	streaming_buf_hbox = gtk_hbox_new(TRUE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(streaming_buf_hbox), 5);
	gtk_container_add(GTK_CONTAINER(streaming_buf_frame), streaming_buf_hbox);

	streaming_size_box = gtk_hbox_new(FALSE, 5);
	/*gtk_table_attach_defaults(GTK_TABLE(streaming_buf_table),streaming_size_box,0,1,0,1); */
	gtk_box_pack_start(GTK_BOX(streaming_buf_hbox), streaming_size_box, TRUE, TRUE, 0);
	streaming_size_label = gtk_label_new(_("Buffer size (kb):"));
	gtk_box_pack_start(GTK_BOX(streaming_size_box), streaming_size_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_size_label);
	streaming_size_adj = gtk_adjustment_new(mpg123_cfg.http_buffer_size, 4, 4096, 4, 4, 4);
	streaming_size_spin = gtk_spin_button_new(GTK_ADJUSTMENT(streaming_size_adj), 8, 0);
	gtk_widget_set_usize(streaming_size_spin, 60, -1);
	gtk_box_pack_start(GTK_BOX(streaming_size_box), streaming_size_spin, FALSE, FALSE, 0);
	gtk_widget_show(streaming_size_spin);
	gtk_widget_show(streaming_size_box);

	streaming_pre_box = gtk_hbox_new(FALSE, 5);
	/*gtk_table_attach_defaults(GTK_TABLE(streaming_buf_table),streaming_pre_box,1,2,0,1); */
	gtk_box_pack_start(GTK_BOX(streaming_buf_hbox), streaming_pre_box, TRUE, TRUE, 0);
	streaming_pre_label = gtk_label_new(_("Pre-buffer (percent):"));
	gtk_box_pack_start(GTK_BOX(streaming_pre_box), streaming_pre_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_pre_label);
	streaming_pre_adj = gtk_adjustment_new(mpg123_cfg.http_prebuffer, 0, 90, 1, 1, 1);
	streaming_pre_spin = gtk_spin_button_new(GTK_ADJUSTMENT(streaming_pre_adj), 1, 0);
	gtk_widget_set_usize(streaming_pre_spin, 60, -1);
	gtk_box_pack_start(GTK_BOX(streaming_pre_box), streaming_pre_spin, FALSE, FALSE, 0);
	gtk_widget_show(streaming_pre_spin);
	gtk_widget_show(streaming_pre_box);

	gtk_widget_show(streaming_buf_hbox);
	gtk_widget_show(streaming_buf_frame);

 	/*
 	 * Proxy config.
 	 */
	streaming_proxy_frame = gtk_frame_new(_("Proxy:"));
	gtk_container_set_border_width(GTK_CONTAINER(streaming_proxy_frame), 5);
	gtk_box_pack_start(GTK_BOX(streaming_vbox), streaming_proxy_frame, FALSE, FALSE, 0);

	streaming_proxy_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(streaming_proxy_vbox), 5);
	gtk_container_add(GTK_CONTAINER(streaming_proxy_frame), streaming_proxy_vbox);

	streaming_proxy_use = gtk_check_button_new_with_label(_("Use proxy"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(streaming_proxy_use), mpg123_cfg.use_proxy);
	gtk_signal_connect(GTK_OBJECT(streaming_proxy_use), "clicked", GTK_SIGNAL_FUNC(proxy_use_cb), NULL);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_vbox), streaming_proxy_use, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_use);

	streaming_proxy_hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_set_sensitive(streaming_proxy_hbox, mpg123_cfg.use_proxy);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_vbox), streaming_proxy_hbox, FALSE, FALSE, 0);

	streaming_proxy_host_label = gtk_label_new(_("Host:"));
	gtk_box_pack_start(GTK_BOX(streaming_proxy_hbox), streaming_proxy_host_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_host_label);

	streaming_proxy_host_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(streaming_proxy_host_entry), mpg123_cfg.proxy_host);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_hbox), streaming_proxy_host_entry, TRUE, TRUE, 0);
	gtk_widget_show(streaming_proxy_host_entry);

	streaming_proxy_port_label = gtk_label_new(_("Port:"));
	gtk_box_pack_start(GTK_BOX(streaming_proxy_hbox), streaming_proxy_port_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_port_label);

	streaming_proxy_port_entry = gtk_entry_new();
	gtk_widget_set_usize(streaming_proxy_port_entry, 50, -1);
	temp = g_strdup_printf("%d", mpg123_cfg.proxy_port);
	gtk_entry_set_text(GTK_ENTRY(streaming_proxy_port_entry), temp);
	g_free(temp);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_hbox), streaming_proxy_port_entry, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_port_entry);

	gtk_widget_show(streaming_proxy_hbox);
	
	streaming_proxy_auth_use = gtk_check_button_new_with_label(_("Use authentication"));
	gtk_widget_set_sensitive(streaming_proxy_auth_use, mpg123_cfg.use_proxy);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(streaming_proxy_auth_use), mpg123_cfg.proxy_use_auth);
	gtk_signal_connect(GTK_OBJECT(streaming_proxy_auth_use), "clicked", GTK_SIGNAL_FUNC(proxy_auth_use_cb), NULL);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_vbox), streaming_proxy_auth_use, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_auth_use);

	streaming_proxy_auth_hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_set_sensitive(streaming_proxy_auth_hbox, mpg123_cfg.use_proxy && mpg123_cfg.proxy_use_auth);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_vbox), streaming_proxy_auth_hbox, FALSE, FALSE, 0);
	
	streaming_proxy_auth_user_label = gtk_label_new(_("Username:"));
	gtk_box_pack_start(GTK_BOX(streaming_proxy_auth_hbox), streaming_proxy_auth_user_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_auth_user_label);

	streaming_proxy_auth_user_entry = gtk_entry_new();
	if(mpg123_cfg.proxy_user)
		gtk_entry_set_text(GTK_ENTRY(streaming_proxy_auth_user_entry), mpg123_cfg.proxy_user);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_auth_hbox), streaming_proxy_auth_user_entry, TRUE, TRUE, 0);
	gtk_widget_show(streaming_proxy_auth_user_entry);

	streaming_proxy_auth_pass_label = gtk_label_new(_("Password:"));
	gtk_box_pack_start(GTK_BOX(streaming_proxy_auth_hbox), streaming_proxy_auth_pass_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_proxy_auth_pass_label);

	streaming_proxy_auth_pass_entry = gtk_entry_new();
	if(mpg123_cfg.proxy_pass)
		gtk_entry_set_text(GTK_ENTRY(streaming_proxy_auth_pass_entry), mpg123_cfg.proxy_pass);
	gtk_entry_set_visibility(GTK_ENTRY(streaming_proxy_auth_pass_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(streaming_proxy_auth_hbox), streaming_proxy_auth_pass_entry, TRUE, TRUE, 0);
	gtk_widget_show(streaming_proxy_auth_pass_entry);

	gtk_widget_show(streaming_proxy_auth_hbox);
	gtk_widget_show(streaming_proxy_vbox);
	gtk_widget_show(streaming_proxy_frame);


	/*
	 * Save to disk config.
	 */
	streaming_save_frame = gtk_frame_new(_("Save stream to disk:"));
	gtk_container_set_border_width(GTK_CONTAINER(streaming_save_frame), 5);
	gtk_box_pack_start(GTK_BOX(streaming_vbox), streaming_save_frame, FALSE, FALSE, 0);

	streaming_save_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(streaming_save_vbox), 5);
	gtk_container_add(GTK_CONTAINER(streaming_save_frame), streaming_save_vbox);

	streaming_save_use = gtk_check_button_new_with_label(_("Save stream to disk"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(streaming_save_use), mpg123_cfg.save_http_stream);
	gtk_signal_connect(GTK_OBJECT(streaming_save_use), "clicked", GTK_SIGNAL_FUNC(streaming_save_use_cb), NULL);
	gtk_box_pack_start(GTK_BOX(streaming_save_vbox), streaming_save_use, FALSE, FALSE, 0);
	gtk_widget_show(streaming_save_use);

	streaming_save_hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_set_sensitive(streaming_save_hbox, mpg123_cfg.save_http_stream);
	gtk_box_pack_start(GTK_BOX(streaming_save_vbox), streaming_save_hbox, FALSE, FALSE, 0);

	streaming_save_label = gtk_label_new(_("Path:"));
	gtk_box_pack_start(GTK_BOX(streaming_save_hbox), streaming_save_label, FALSE, FALSE, 0);
	gtk_widget_show(streaming_save_label);

	streaming_save_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(streaming_save_entry), mpg123_cfg.save_http_path);
	gtk_box_pack_start(GTK_BOX(streaming_save_hbox), streaming_save_entry, TRUE, TRUE, 0);
	gtk_widget_show(streaming_save_entry);

	streaming_save_browse = gtk_button_new_with_label(_("Browse"));
	gtk_signal_connect(GTK_OBJECT(streaming_save_browse), "clicked", GTK_SIGNAL_FUNC(streaming_save_browse_cb), NULL);
	gtk_box_pack_start(GTK_BOX(streaming_save_hbox), streaming_save_browse, FALSE, FALSE, 0);
	gtk_widget_show(streaming_save_browse);

	gtk_widget_show(streaming_save_hbox);
	gtk_widget_show(streaming_save_vbox);
	gtk_widget_show(streaming_save_frame);

	streaming_cast_frame = gtk_frame_new(_("SHOUT/Icecast:"));
	gtk_container_set_border_width(GTK_CONTAINER(streaming_cast_frame), 5);
	gtk_box_pack_start(GTK_BOX(streaming_vbox), streaming_cast_frame, FALSE, FALSE, 0);

	streaming_cast_vbox = gtk_vbox_new(5, FALSE);
	gtk_container_add(GTK_CONTAINER(streaming_cast_frame), streaming_cast_vbox);
	
	streaming_cast_title = gtk_check_button_new_with_label(_("Enable SHOUT/Icecast title streaming"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(streaming_cast_title), mpg123_cfg.cast_title_streaming);
	gtk_box_pack_start(GTK_BOX(streaming_cast_vbox), streaming_cast_title, FALSE, FALSE, 0);
	gtk_widget_show(streaming_cast_title);

	streaming_udp_title = gtk_check_button_new_with_label(_("Enable Icecast Metadata UDP Channel"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(streaming_udp_title), mpg123_cfg.use_udp_channel);
	gtk_box_pack_start(GTK_BOX(streaming_cast_vbox), streaming_udp_title, FALSE, FALSE, 0);
	gtk_widget_show(streaming_udp_title);

	gtk_widget_show(streaming_cast_vbox);
	gtk_widget_show(streaming_cast_frame);

	gtk_widget_show(streaming_vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), streaming_vbox, gtk_label_new(_("Streaming")));

	title_frame = gtk_frame_new(_("ID3 Tags:"));
	gtk_container_border_width(GTK_CONTAINER(title_frame), 5);

	title_id3_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_border_width(GTK_CONTAINER(title_id3_vbox), 5);
	gtk_container_add(GTK_CONTAINER(title_frame), title_id3_vbox);

	title_id3_use = gtk_check_button_new_with_label(_("Use ID3 tags"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_id3_use), mpg123_cfg.use_id3);
	gtk_signal_connect(GTK_OBJECT(title_id3_use), "clicked", title_id3_use_cb, NULL);
	gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_id3_use, FALSE, FALSE, 0);
	gtk_widget_show(title_id3_use);

	title_id3v2_disable = gtk_check_button_new_with_label(_("Disable ID3V2 tags"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_id3v2_disable),
				     mpg123_cfg.disable_id3v2);
	gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_id3v2_disable, FALSE, FALSE, 0);
	gtk_widget_show(title_id3v2_disable);

	title_id3_box = gtk_hbox_new(FALSE, 5);
	gtk_widget_set_sensitive(title_id3_box, mpg123_cfg.use_id3);
	gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_id3_box, FALSE, FALSE, 0);

	title_id3_label = gtk_label_new(_("ID3 format:"));
	gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_label, FALSE, FALSE, 0);
	gtk_widget_show(title_id3_label);

	title_id3_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(title_id3_entry), mpg123_cfg.id3_format);
	gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_entry, TRUE, TRUE, 0);
	gtk_widget_show(title_id3_entry);
	gtk_widget_show(title_id3_box);

	title_id3_desc_box = gtk_hbox_new(TRUE, 5);
	gtk_widget_set_sensitive(title_id3_desc_box, mpg123_cfg.use_id3);
	gtk_box_pack_start(GTK_BOX(title_id3_vbox), title_id3_desc_box, FALSE, FALSE, 0);

	title_id3_desc_label1 = gtk_label_new(_("%1 = Artist\n%3 = Album\n%5 = Comment\n%7 = File name\n%9 = File extension"));
	gtk_misc_set_alignment(GTK_MISC(title_id3_desc_label1), 0, 0);
	gtk_label_set_justify(GTK_LABEL(title_id3_desc_label1), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(title_id3_desc_box), title_id3_desc_label1, TRUE, TRUE, 0);
	gtk_widget_show(title_id3_desc_label1);

	title_id3_desc_label2 = gtk_label_new(_("%2 = Title\n%4 = Year\n%6 = Genre\n%8 = Path"));
	gtk_label_set_justify(GTK_LABEL(title_id3_desc_label2), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(title_id3_desc_label2), 0, 0);
	gtk_box_pack_start(GTK_BOX(title_id3_desc_box), title_id3_desc_label2, TRUE, TRUE, 0);
	gtk_widget_show(title_id3_desc_label2);
	gtk_widget_show(title_id3_desc_box);
	gtk_widget_show(title_id3_vbox);
	gtk_widget_show(title_frame);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), title_frame, gtk_label_new(_("Title")));

	gtk_widget_show(notebook);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	ok = gtk_button_new_with_label(_("Ok"));
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(mpg123_configurewin_ok), NULL);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
	gtk_widget_show(ok);
	gtk_widget_grab_default(ok);

	cancel = gtk_button_new_with_label(_("Cancel"));
	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(mpg123_configurewin));
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
	gtk_widget_show(cancel);

	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_widget_show(mpg123_configurewin);
}
