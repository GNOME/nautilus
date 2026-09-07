/*
 * SPDX-FileCopyrightText: 2003 Andrew Sobala <aes@gnome.org>
 * SPDX-FileCopyrightText: 2005 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <nautilus-extension.h>

#define AUDIO_VIDEO_TYPE_PROPERTIES_MODEL (audio_video_properties_model_get_type ())

G_DECLARE_FINAL_TYPE (AudioVideoPropertiesModel, audio_video_properties_model,
                      AUDIO_VIDEO, PROPERTIES_MODEL, GObject)

GType                    audio_video_properties_model_get_type      (void);
void                     audio_video_properties_model_register_type (GTypeModule *module);

NautilusPropertiesModel *
audio_video_properties_model_new (const char *location);
