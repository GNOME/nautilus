#include <gio/gio.h>

#pragma once

#define TRACKER_MINERFS_BUS_NAME "org.freedesktop.Tracker3.Miner.Files"
#define TRACKER_MINERFS_OBJECT_PATH "/org/freedesktop/Tracker3/Miner/Files"

#define TRACKER_EXTRACT_BUS_NAME "org.freedesktop.Tracker3.Miner.Extract"
#define TRACKER_EXTRACT_OBJECT_PATH "/org/freedesktop/Tracker3/Miner/Extract"

typedef struct _TrackerFilesProcessedWatcher TrackerFilesProcessedWatcher;

TrackerFilesProcessedWatcher *tracker_files_processed_watcher_new (const gchar *bus_name,
                                                                   const gchar *object_path);

void tracker_files_processed_watcher_free (TrackerFilesProcessedWatcher *data);

void tracker_files_processed_watcher_await_file (TrackerFilesProcessedWatcher *watcher,
                                                 GFile                        *file);
