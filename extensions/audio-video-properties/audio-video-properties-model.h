/*
 * SPDX-FileCopyrightText: 2003 Andrew Sobala <aes@gnome.org>
 * SPDX-FileCopyrightText: 2005 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AUDIO_VIDEO_PROPERTIES_VIEW_H
#define AUDIO_VIDEO_PROPERTIES_VIEW_H

#include <nautilus-extension.h>

#define AUDIO_VIDEO_TYPE_PROPERTIES_VIEW (audio_video_properties_model_get_type ())
#define AUDIO_VIDEO_PROPERTIES_VIEW(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), AUDIO_VIDEO_TYPE_PROPERTIES_VIEW, TotemPropertiesView))
#define AUDIO_VIDEO_PROPERTIES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), AUDIO_VIDEO_TYPE_PROPERTIES_VIEW, TotemPropertiesViewClass))
#define AUDIO_VIDEO_IS_PROPERTIES_VIEW(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AUDIO_VIDEO_TYPE_PROPERTIES_VIEW))
#define AUDIO_VIDEO_IS_PROPERTIES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AUDIO_VIDEO_TYPE_PROPERTIES_VIEW))

typedef struct TotemPropertiesViewPriv TotemPropertiesViewPriv;

typedef struct {
	GObject parent;
	TotemPropertiesViewPriv *priv;
} TotemPropertiesView;

typedef struct {
	GObjectClass parent;
} TotemPropertiesViewClass;

GType                    audio_video_properties_model_get_type      (void);
void                     audio_video_properties_model_register_type (GTypeModule *module);

NautilusPropertiesModel *
audio_video_properties_model_new (const char *location);

#endif /* AUDIO_VIDEO_PROPERTIES_VIEW_H */
