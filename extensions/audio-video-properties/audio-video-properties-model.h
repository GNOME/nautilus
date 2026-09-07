/*
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 * Copyright (C) 2005  Bastien Nocera <hadess@hadess.net>
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
#pragma once

#include <nautilus-extension.h>

#define AUDIO_VIDEO_TYPE_PROPERTIES_MODEL (audio_video_properties_model_get_type ())

G_DECLARE_FINAL_TYPE (AudioVideoPropertiesModel, av_properties_model,
                      AUDIO_VIDEO, PROPERTIES_MODEL, GObject)

GType                    audio_video_properties_model_get_type      (void);
void                     audio_video_properties_model_register_type (GTypeModule *module);

NautilusPropertiesModel *
audio_video_properties_model_new (const char *location);
