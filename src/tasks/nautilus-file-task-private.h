/* nautilus-file-task-private.h - private methods for file tasks.
 *
 * Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ernestas Kulik <ernestask@gnome.org>
 */

#ifndef NAUTILUS_FILE_TASK_PRIVATE_H
#define NAUTILUS_FILE_TASK_PRIVATE_H

#include "nautilus-file-task.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-progress-info.h"

#include <gio/gio.h>

#define SECONDS_NEEDED_FOR_APPROXIMATE_TRANSFER_RATE 1
#define SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE 8

#define IS_IO_ERROR(__error, KIND) (((__error)->domain == G_IO_ERROR && (__error)->code == G_IO_ERROR_ ## KIND))

#define CANCEL _("_Cancel")
#define SKIP _("_Skip")
#define SKIP_ALL _("S_kip All")
#define RETRY _("_Retry")
#define DELETE _("_Delete")
#define DELETE_ALL _("Delete _All")
#define REPLACE _("_Replace")
#define REPLACE_ALL _("Replace _All")
#define MERGE _("_Merge")
#define MERGE_ALL _("Merge _All")
#define COPY_FORCE _("Copy _Anyway")

GtkWindow *nautilus_file_task_get_parent_window (NautilusFileTask *self);

NautilusProgressInfo *nautilus_file_task_get_progress_info (NautilusFileTask *self);

int nautilus_file_task_prompt_error (NautilusFileTask *self,
                                     char             *primary_text,
                                     char             *secondary_text,
                                     const char       *details_text,
                                     gboolean          show_all,
                                     ...);

int nautilus_file_task_prompt_warning (NautilusFileTask *self,
                                       char             *primary_text,
                                       char             *secondary_text,
                                       const char       *details_text,
                                       gboolean          show_all,
                                       ...);

gchar *nautilus_file_task_get_basename (GFile *file);

gchar *nautilus_file_task_get_formatted_time (int seconds);

int nautilus_file_task_seconds_count_format_time_units (int seconds);

void nautilus_file_task_inhibit_power_manager (NautilusFileTask *self,
                                               const char       *message);

#endif
