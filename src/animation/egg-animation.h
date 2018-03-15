/* egg-animation.h
 *
 * Copyright (C) 2010-2016 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define EGG_TYPE_ANIMATION      (egg_animation_get_type())
#define EGG_TYPE_ANIMATION_MODE (egg_animation_mode_get_type())

G_DECLARE_FINAL_TYPE (EggAnimation, egg_animation,
                      EGG, ANIMATION, GInitiallyUnowned)

typedef enum   _EggAnimationMode    EggAnimationMode;

enum _EggAnimationMode
{
  EGG_ANIMATION_LINEAR,
  EGG_ANIMATION_EASE_IN_QUAD,
  EGG_ANIMATION_EASE_OUT_QUAD,
  EGG_ANIMATION_EASE_IN_OUT_QUAD,
  EGG_ANIMATION_EASE_IN_CUBIC,
  EGG_ANIMATION_EASE_OUT_CUBIC,
  EGG_ANIMATION_EASE_IN_OUT_CUBIC,

  EGG_ANIMATION_LAST
};

GType         egg_animation_mode_get_type (void);
void          egg_animation_start         (EggAnimation     *animation);
void          egg_animation_stop          (EggAnimation     *animation);
void          egg_animation_add_property  (EggAnimation     *animation,
                                           GParamSpec       *pspec,
                                           const GValue     *value);

EggAnimation *egg_object_animatev         (gpointer          object,
                                           EggAnimationMode  mode,
                                           guint             duration_msec,
                                           GdkFrameClock    *frame_clock,
                                           const gchar      *first_property,
                                           va_list           args);
EggAnimation* egg_object_animate          (gpointer          object,
                                           EggAnimationMode  mode,
                                           guint             duration_msec,
                                           GdkFrameClock    *frame_clock,
                                           const gchar      *first_property,
                                           ...) G_GNUC_NULL_TERMINATED;
EggAnimation* egg_object_animate_full     (gpointer          object,
                                           EggAnimationMode  mode,
                                           guint             duration_msec,
                                           GdkFrameClock    *frame_clock,
                                           GDestroyNotify    notify,
                                           gpointer          notify_data,
                                           const gchar      *first_property,
                                           ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS