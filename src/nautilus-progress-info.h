/*
   nautilus-progress-info.h: file operation progress info.
 
   Copyright (C) 2007 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_PROGRESS_INFO (nautilus_progress_info_get_type ())

G_DECLARE_FINAL_TYPE (NautilusProgressInfo, nautilus_progress_info, NAUTILUS, PROGRESS_INFO, GObject)

/* Signals:
   "changed" - status or details changed
   "progress-changed" - the percentage progress changed (or we pulsed if in activity_mode
   "started" - emited on job start
   "finished" - emitted when job is done
   
   All signals are emitted from idles in main loop.
   All methods are threadsafe.
 */

/* Always create progress info's on the main thread, this function is not thread safe */
NautilusProgressInfo *nautilus_progress_info_new (void);

GList *       nautilus_get_all_progress_info (void);

char *        nautilus_progress_info_get_status      (NautilusProgressInfo *info);
char *        nautilus_progress_info_get_details     (NautilusProgressInfo *info);
double        nautilus_progress_info_get_progress    (NautilusProgressInfo *info);
GCancellable *nautilus_progress_info_get_cancellable (NautilusProgressInfo *info);
void          nautilus_progress_info_cancel          (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_started  (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_finished (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_paused   (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_cancelled (NautilusProgressInfo *info);

void          nautilus_progress_info_start           (NautilusProgressInfo *info);
void          nautilus_progress_info_finish          (NautilusProgressInfo *info);
void          nautilus_progress_info_pause           (NautilusProgressInfo *info);
void          nautilus_progress_info_resume          (NautilusProgressInfo *info);
void          nautilus_progress_info_set_status      (NautilusProgressInfo *info,
						      const char           *status);
void          nautilus_progress_info_take_status     (NautilusProgressInfo *info,
						      char                 *status);
void          nautilus_progress_info_set_details     (NautilusProgressInfo *info,
						      const char           *details);
void          nautilus_progress_info_take_details    (NautilusProgressInfo *info,
						      char                 *details);
void          nautilus_progress_info_set_progress    (NautilusProgressInfo *info,
						      double                current,
						      double                total);
void          nautilus_progress_info_pulse_progress  (NautilusProgressInfo *info);

void          nautilus_progress_info_set_remaining_time (NautilusProgressInfo *info,
                                                         gdouble               time);
gdouble       nautilus_progress_info_get_remaining_time (NautilusProgressInfo *info);
void          nautilus_progress_info_set_elapsed_time (NautilusProgressInfo *info,
                                                       gdouble               time);
gdouble       nautilus_progress_info_get_elapsed_time (NautilusProgressInfo *info);
gdouble       nautilus_progress_info_get_total_elapsed_time (NautilusProgressInfo *info);

void nautilus_progress_info_set_destination (NautilusProgressInfo *info,
                                             GFile                *file);
GFile *nautilus_progress_info_get_destination (NautilusProgressInfo *info);