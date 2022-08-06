/*
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 * Copyright (C) 2004  Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <glib/gi18n-lib.h>

#define GST_USE_UNSTABLE_API 1
#include <gst/tag/tag.h>
#include <gst/pbutils/pbutils.h>

#include "totem-properties-view.h"
#include <math.h>

struct TotemPropertiesViewPriv {
	NautilusPropertiesModel *model;
	GListStore *store;
	GstDiscoverer *disco;
};

static GObjectClass *parent_class = NULL;
static void totem_properties_view_finalize (GObject *object);

G_DEFINE_TYPE (TotemPropertiesView, totem_properties_view, G_TYPE_OBJECT)

void
totem_properties_view_register_type (GTypeModule *module)
{
	totem_properties_view_get_type ();
}

static void
totem_properties_view_class_init (TotemPropertiesViewClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	G_OBJECT_CLASS (class)->finalize = totem_properties_view_finalize;
}

static void
append_item (TotemPropertiesView *props,
             const char          *name,
             const char          *value)
{
    g_autoptr (NautilusPropertiesItem) item = NULL;

    item = nautilus_properties_item_new (name, value);

    g_list_store_append (props->priv->store, item);
}

/* Copied from bacon-video-widget-properties.c
 * Copyright (C) 2002 Bastien Nocera
 */
static char *
time_to_string_text (gint64 msecs)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour, _time;

	_time = (int) (msecs / 1000);
	sec = _time % 60;
	_time = _time - sec;
	min = (_time % (60*60)) / 60;
	_time = _time - (min * 60);
	hour = _time / (60*60);

	hours = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d hour", "%d hours", hour), hour);

	mins = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d minute",
					  "%d minutes", min), min);

	secs = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d second",
					  "%d seconds", sec), sec);

	if (hour > 0)
	{
		/* 5 hours 2 minutes 12 seconds */
		string = g_strdup_printf (C_("time", "%s %s %s"), hours, mins, secs);
	} else if (min > 0) {
		/* 2 minutes 12 seconds */
		string = g_strdup_printf (C_("time", "%s %s"), mins, secs);
	} else if (sec > 0) {
		/* 10 seconds */
		string = g_strdup (secs);
	} else {
		/* 0 seconds */
		string = g_strdup (_("0 seconds"));
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
}

static void
update_general (TotemPropertiesView *props,
		const GstTagList    *list)
{
	struct {
		const char *tag_name;
		const char *title;
	} items[] = {
		{ GST_TAG_TITLE, N_("Title") },
		{ GST_TAG_ARTIST, N_("Artist") },
		{ GST_TAG_ALBUM, N_("Album") },
	};
	guint i;
        GDate *date;
	GstDateTime *datetime;
	gchar *comment;

	for (i = 0; i < G_N_ELEMENTS(items); i++) {
		char *string;

		if (gst_tag_list_get_string_index (list, items[i].tag_name, 0, &string) != FALSE) {
			append_item (props, gettext (items[i].title), string);
			g_free (string);
		}
	}
	
	/* Comment else use Description defined by:
	 * http://xiph.org/vorbis/doc/v-comment.html */
	if (gst_tag_list_get_string (list, GST_TAG_COMMENT, &comment) ||
		gst_tag_list_get_string (list, GST_TAG_DESCRIPTION, &comment)) {

		append_item (props, _("Comment"), comment);
		g_free (comment);
        }
	
	/* Date */
        if (gst_tag_list_get_date (list, GST_TAG_DATE, &date)) {
		char *string;

		string = g_strdup_printf ("%d", g_date_get_year (date));
		g_date_free (date);
		append_item (props, _("Year"), string);
		g_free (string);
        } else if (gst_tag_list_get_date_time (list, GST_TAG_DATE_TIME, &datetime)) {
		char *string;

		string = g_strdup_printf ("%d", gst_date_time_get_year (datetime));
		gst_date_time_unref (datetime);
		append_item (props, _("Year"), string);
		g_free (string);
	}
}

static void
set_codec (TotemPropertiesView     *props,
	   GstDiscovererStreamInfo *info,
	   const char              *title)
{
	GstCaps *caps;
	const char *nick;

	nick = gst_discoverer_stream_info_get_stream_type_nick (info);
	if (g_str_equal (nick, "audio") == FALSE &&
	    g_str_equal (nick, "video") == FALSE &&
	    g_str_equal (nick, "container") == FALSE)
		return;

	caps = gst_discoverer_stream_info_get_caps (info);
	if (caps) {
		if (gst_caps_is_fixed (caps)) {
			char *string;

			string = gst_pb_utils_get_codec_description (caps);
			append_item (props, title, string);
			g_free (string);
		}
		gst_caps_unref (caps);
	}
}

static void
set_bitrate (TotemPropertiesView    *props,
	     guint                   bitrate,
	     const char             *title)
{
	char *string;

	if (!bitrate) {
		return;
	}
	string = g_strdup_printf (_("%d kbps"), bitrate / 1000);
	append_item (props, title, string);
	g_free (string);
}

static void
update_video (TotemPropertiesView    *props,
	      GstDiscovererVideoInfo *info)
{
	guint width, height;
	guint fps_n, fps_d;
	float framerate = 0.0;
	char *string;

	width = gst_discoverer_video_info_get_width (info);
	height = gst_discoverer_video_info_get_height (info);
	string = g_strdup_printf (N_("%d × %d"), width, height);
	append_item (props, _("Dimensions"), string);
	g_free (string);

	set_codec (props, (GstDiscovererStreamInfo *) info, _("Video Codec"));
	set_bitrate (props, gst_discoverer_video_info_get_bitrate (info), _("Video Bit Rate"));

	/* Round up/down to the nearest integer framerate */
	fps_n = gst_discoverer_video_info_get_framerate_num (info);
	fps_d = gst_discoverer_video_info_get_framerate_denom (info);
	if (fps_d > 0.0) {
		framerate = (float) fps_n / (float) fps_d;
	}

	if (framerate > 1.0) {
		string = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                       "%0.2f frame per second",
                                                       "%0.2f frames per second",
                                                       (int) (ceilf (framerate))),
                                          framerate);
		append_item (props, _("Frame Rate"), string);
		g_free (string);
	}
}

static void
update_audio (TotemPropertiesView    *props,
	      GstDiscovererAudioInfo *info)
{
	guint samplerate, channels;

	set_codec (props, (GstDiscovererStreamInfo *) info, _("Audio Codec"));

	set_bitrate (props, gst_discoverer_audio_info_get_bitrate (info), _("Audio Bit Rate"));

	samplerate = gst_discoverer_audio_info_get_sample_rate (info);
	if (samplerate) {
		char *string;
		string = g_strdup_printf (_("%d Hz"), samplerate);
		append_item (props, _("Sample Rate"), string);
		g_free (string);
	}

	channels = gst_discoverer_audio_info_get_channels (info);
	if (channels) {
		char *string;

		if (channels > 2) {
			string = g_strdup_printf ("%s %d.1", _("Surround"), channels - 1);
		} else if (channels == 1) {
			string = g_strdup (_("Mono"));
		} else if (channels == 2) {
			string = g_strdup (_("Stereo"));
		} else {
			string = g_strdup (""); //Should not happen
		}
		append_item (props, _("Channels"), string);
		g_free (string);
	}
}

static void
discovered_cb (GstDiscoverer       *discoverer,
	       GstDiscovererInfo   *info,
	       GError              *error,
	       TotemPropertiesView *props)
{
	GList *video_streams, *audio_streams;
	const GstTagList *taglist;
	gboolean has_audio, has_video;
	const char *label;
        GstClockTime duration;
	g_autofree char *duration_string = NULL;
        GstDiscovererStreamInfo *sinfo;

	if (error) {
		g_warning ("Couldn't get information about '%s': %s",
			   gst_discoverer_info_get_uri (info),
			   error->message);
		return;
	}

	video_streams = gst_discoverer_info_get_video_streams (info);
	has_video = (video_streams != NULL);
	audio_streams = gst_discoverer_info_get_audio_streams (info);
	has_audio = (audio_streams != NULL);

	if (has_audio == has_video)
		label = _("Audio and Video Properties");
	else if (has_audio)
		label = _("Audio Properties");
	else
		label = _("Video Properties");

	nautilus_properties_model_set_title (props->priv->model, label);

	/* General */
        duration = gst_discoverer_info_get_duration (info);
        duration_string = time_to_string_text (duration / GST_SECOND * 1000);
	append_item (props, _("Duration"), duration_string);

        sinfo = gst_discoverer_info_get_stream_info (info);
        if (sinfo) {
		set_codec (props, sinfo, _("Container"));
		gst_discoverer_stream_info_unref (sinfo);
	}

	taglist = gst_discoverer_info_get_tags (info);
	update_general (props, taglist);

	/* Video and Audio */
	if (video_streams)
		update_video (props, video_streams->data);
	if (audio_streams)
		update_audio (props, audio_streams->data);

	gst_discoverer_stream_info_list_free (video_streams);
	gst_discoverer_stream_info_list_free (audio_streams);
}

static void
totem_properties_view_init (TotemPropertiesView *props)
{
	GError *err = NULL;

	props->priv = g_new0 (TotemPropertiesViewPriv, 1);

	props->priv->store = g_list_store_new (NAUTILUS_TYPE_PROPERTIES_ITEM);

        props->priv->model = nautilus_properties_model_new (_("Audio/Video Properties"),
                                                            G_LIST_MODEL (props->priv->store));

	props->priv->disco = gst_discoverer_new (GST_SECOND * 60, &err);
	if (props->priv->disco == NULL) {
		g_warning ("Could not create discoverer object: %s", err->message);
		g_error_free (err);
		return;
	}
	g_signal_connect (props->priv->disco, "discovered",
			  G_CALLBACK (discovered_cb), props);
}

static void
totem_properties_view_finalize (GObject *object)
{
	TotemPropertiesView *props;

	props = TOTEM_PROPERTIES_VIEW (object);

	if (props->priv != NULL) {
		if (props->priv->disco) {
			g_signal_handlers_disconnect_by_func (props->priv->disco,
							      discovered_cb,
							      props);
			gst_discoverer_stop (props->priv->disco);
			g_clear_object (&props->priv->disco);
		}
		g_free (props->priv);
	}
	props->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
totem_properties_view_set_location (TotemPropertiesView *props,
				    const char          *location)
{
	g_assert (TOTEM_IS_PROPERTIES_VIEW (props));

	if (props->priv->disco)
		gst_discoverer_stop (props->priv->disco);

	if (location != NULL && props->priv->disco != NULL) {
		gst_discoverer_start (props->priv->disco);

		if (gst_discoverer_discover_uri_async (props->priv->disco, location) == FALSE) {
			g_warning ("Couldn't add %s to list", location);
			return;
		}
	}
}

NautilusPropertiesModel *
totem_properties_view_new (const char *location)
{
	TotemPropertiesView *props;

	props = g_object_new (TOTEM_TYPE_PROPERTIES_VIEW, NULL);

	totem_properties_view_set_location (props, location);

	g_object_weak_ref (G_OBJECT (props->priv->model), (GWeakNotify) g_object_unref, props);

	return props->priv->model;
}
