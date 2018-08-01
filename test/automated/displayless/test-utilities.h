#include <gio/gio.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-global-preferences.h>
#include <src/nautilus-search-engine.h>
#include <gtk/gtk.h>
#include <locale.h>

#pragma once

void create_search_file_hierarchy (gchar *search_engine);

void delete_search_file_hierarchy (gchar *search_engine);