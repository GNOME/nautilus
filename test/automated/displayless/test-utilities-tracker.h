#include <gio/gio.h>

#pragma once

typedef struct _TrackerFilesProcessedWatcher TrackerFilesProcessedWatcher;

TrackerFilesProcessedWatcher *tracker_files_processed_watcher_new (void);

void tracker_files_processed_watcher_free (TrackerFilesProcessedWatcher *data);

void tracker_files_processed_watcher_await_file (TrackerFilesProcessedWatcher *watcher,
                                                 GFile                        *file);
