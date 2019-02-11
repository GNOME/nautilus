/* bacon-video-widget-properties.c

   Copyright (C) 2002 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "totem-interface.h"

#include "bacon-video-widget-properties.h"

static void bacon_video_widget_properties_dispose (GObject *object);

struct BaconVideoWidgetPropertiesPrivate {
	GtkBuilder *xml;
	int time;
};

#define BACON_VIDEO_WIDGET_PROPERTIES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BACON_TYPE_VIDEO_WIDGET_PROPERTIES, BaconVideoWidgetPropertiesPrivate))

G_DEFINE_TYPE (BaconVideoWidgetProperties, bacon_video_widget_properties, GTK_TYPE_BOX)

static void
bacon_video_widget_properties_class_init (BaconVideoWidgetPropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BaconVideoWidgetPropertiesPrivate));

	object_class->dispose = bacon_video_widget_properties_dispose;
}

static void
bacon_video_widget_properties_init (BaconVideoWidgetProperties *props)
{
	props->priv = G_TYPE_INSTANCE_GET_PRIVATE (props, BACON_TYPE_VIDEO_WIDGET_PROPERTIES, BaconVideoWidgetPropertiesPrivate);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (props), GTK_ORIENTATION_VERTICAL);
}

static void
bacon_video_widget_properties_dispose (GObject *object)
{
	BaconVideoWidgetPropertiesPrivate *priv = BACON_VIDEO_WIDGET_PROPERTIES_GET_PRIVATE (object);

	if (priv->xml != NULL)
		g_object_unref (priv->xml);
	priv->xml = NULL;

	G_OBJECT_CLASS (bacon_video_widget_properties_parent_class)->dispose (object);
}

void
bacon_video_widget_properties_set_label (BaconVideoWidgetProperties *props,
					 const char                 *name,
					 const char                 *text)
{
	GtkLabel *item;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));
	g_return_if_fail (name != NULL);

	item = GTK_LABEL (gtk_builder_get_object (props->priv->xml, name));
	g_return_if_fail (item != NULL);
	gtk_label_set_text (item, text);
}

void
bacon_video_widget_properties_reset (BaconVideoWidgetProperties *props)
{
	GtkWidget *item;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	item = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "video_vbox"));
	gtk_widget_show (item);
	item = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "video"));
	gtk_widget_set_sensitive (item, FALSE);
	item = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "audio"));
	gtk_widget_set_sensitive (item, FALSE);

	/* Title */
	bacon_video_widget_properties_set_label (props, "title", _("Unknown"));
	/* Artist */
	bacon_video_widget_properties_set_label (props, "artist", _("Unknown"));
	/* Album */
	bacon_video_widget_properties_set_label (props, "album", _("Unknown"));
	/* Year */
	bacon_video_widget_properties_set_label (props, "year", _("Unknown"));
	/* Duration */
	bacon_video_widget_properties_set_duration (props, 0);
	/* Comment */
	bacon_video_widget_properties_set_label (props, "comment", "");
	/* Container */
	bacon_video_widget_properties_set_label (props, "container", _("Unknown"));

	/* Dimensions */
	bacon_video_widget_properties_set_label (props, "dimensions", C_("Dimensions", "N/A"));
	/* Video Codec */
	bacon_video_widget_properties_set_label (props, "vcodec", C_("Video codec", "N/A"));
	/* Video Bitrate */
	bacon_video_widget_properties_set_label (props, "video_bitrate",
			C_("Video bit rate", "N/A"));
	/* Framerate */
	bacon_video_widget_properties_set_label (props, "framerate",
			C_("Frame rate", "N/A"));

	/* Audio Bitrate */
	bacon_video_widget_properties_set_label (props, "audio_bitrate",
			C_("Audio bit rate", "N/A"));
	/* Audio Codec */
	bacon_video_widget_properties_set_label (props, "acodec", C_("Audio codec", "N/A"));
	/* Sample rate */
	bacon_video_widget_properties_set_label (props, "samplerate", _("0 Hz"));
	/* Channels */
	bacon_video_widget_properties_set_label (props, "channels", _("0 Channels"));
}

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

void
bacon_video_widget_properties_set_duration (BaconVideoWidgetProperties *props,
					    int _time)
{
	char *string;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	if (_time == props->priv->time)
		return;

	string = time_to_string_text (_time);
	bacon_video_widget_properties_set_label (props, "duration", string);
	g_free (string);

	props->priv->time = _time;
}

void
bacon_video_widget_properties_set_has_type (BaconVideoWidgetProperties *props,
					    gboolean                    has_video,
					    gboolean                    has_audio)
{
	GtkWidget *item;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	/* Video */
	item = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "video"));
	gtk_widget_set_sensitive (item, has_video);
	item = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "video_vbox"));
	gtk_widget_set_visible (item, has_video);

	/* Audio */
	item = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "audio"));
	gtk_widget_set_sensitive (item, has_audio);
}

void
bacon_video_widget_properties_set_framerate (BaconVideoWidgetProperties *props,
					     int                         framerate)
{
	gchar *temp;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	/* The FPS has to be done differently because it's a plural string */
	if (framerate != 0) {
		temp = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d frame per second", "%d frames per second", framerate),
					framerate);
	} else {
		temp = g_strdup (C_("Frame rate", "N/A"));
	}
	bacon_video_widget_properties_set_label (props, "framerate", temp);
	g_free (temp);
}

GtkWidget*
bacon_video_widget_properties_new (void)
{
	BaconVideoWidgetProperties *props;
	GtkBuilder *xml;
	GtkWidget *vbox;
	GtkSizeGroup *group;
	const char *labels[] = { "title_label", "artist_label", "album_label",
			"year_label", "duration_label", "comment_label", "container_label",
			"dimensions_label", "vcodec_label", "framerate_label",
			"vbitrate_label", "abitrate_label", "acodec_label",
			"samplerate_label", "channels_label" };
	guint i;

	xml = gtk_builder_new ();
	gtk_builder_set_translation_domain (xml, GETTEXT_PACKAGE);
	if (gtk_builder_add_from_file (xml, DATADIR"/properties.ui", NULL) == 0) {
		g_object_unref (xml);
		return NULL;
	}

	props = BACON_VIDEO_WIDGET_PROPERTIES (g_object_new
			(BACON_TYPE_VIDEO_WIDGET_PROPERTIES, NULL));

	props->priv->xml = xml;
	vbox = GTK_WIDGET (gtk_builder_get_object (props->priv->xml, "vbox1"));
	gtk_box_pack_start (GTK_BOX (props), vbox, FALSE, FALSE, 0);

	bacon_video_widget_properties_reset (props);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (i = 0; i < G_N_ELEMENTS (labels); i++)
		gtk_size_group_add_widget (group, GTK_WIDGET (gtk_builder_get_object (xml, labels[i])));

	g_object_unref (group);

	gtk_widget_show_all (GTK_WIDGET (props));

	return GTK_WIDGET (props);
}

