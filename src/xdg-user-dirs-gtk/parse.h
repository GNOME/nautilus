#include <glib.h>

typedef struct {
  char *type;
  char *path;
} XdgDirEntry;

typedef struct {
  char *uri;
  char *label;
} GtkBookmark;

XdgDirEntry *parse_xdg_dirs        (const char *config_file);
char        *parse_xdg_dirs_locale (void);
GList *      parse_gtk_bookmarks   (void);
void         save_gtk_bookmarks    (GList *bookmarks);

