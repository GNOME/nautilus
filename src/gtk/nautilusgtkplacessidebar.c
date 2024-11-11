/* NautilusGtkPlacesSidebar - sidebar widget for places in the filesystem
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * This code is originally from Nautilus.
 *
 * Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *           Cosimo Cecchi <cosimoc@gnome.org>
 *           Federico Mena Quintero <federico@gnome.org>
 *           Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "nautilus-enum-types.h"

#include <gio/gio.h>
#ifdef HAVE_CLOUDPROVIDERS
#include <cloudproviders.h>
#endif

#include "nautilusgtkplacessidebarprivate.h"
#include "nautilusgtksidebarrowprivate.h"
#include "gdk/gdkkeysyms.h"
#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-dnd.h"
#include "nautilus-dbus-launcher.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-properties-window.h"
#include "nautilus-scheme.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window-slot.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#pragma GCC diagnostic ignored "-Wshadow"

/*< private >
 * NautilusGtkPlacesSidebar:
 *
 * NautilusGtkPlacesSidebar is a widget that displays a list of frequently-used places in the
 * file system:  the user’s home directory, the user’s bookmarks, and volumes and drives.
 * This widget is used as a sidebar in GtkFileChooser and may be used by file managers
 * and similar programs.
 *
 * The places sidebar displays drives and volumes, and will automatically mount
 * or unmount them when the user selects them.
 *
 * Applications can hook to various signals in the places sidebar to customize
 * its behavior.  For example, they can add extra commands to the context menu
 * of the sidebar.
 *
 * While bookmarks are completely in control of the user, the places sidebar also
 * allows individual applications to provide extra shortcut folders that are unique
 * to each application.  For example, a Paint program may want to add a shortcut
 * for a Clipart folder.  You can do this with nautilus_gtk_places_sidebar_add_shortcut().
 *
 * To make use of the places sidebar, an application at least needs to connect
 * to the NautilusGtkPlacesSidebar::open-location signal.  This is emitted when the
 * user selects in the sidebar a location to open.  The application should also
 * call nautilus_gtk_places_sidebar_set_location() when it changes the currently-viewed
 * location.
 *
 * # CSS nodes
 *
 * NautilusGtkPlacesSidebar uses a single CSS node with name placessidebar and style
 * class .sidebar.
 *
 * Among the children of the places sidebar, the following style classes can
 * be used:
 * - .sidebar-new-bookmark-row for the 'Add new bookmark' row
 * - .sidebar-placeholder-row for a row that is a placeholder
 * - .has-open-popup when a popup is open for a row
 */

/* These are used when a destination-side DND operation is taking place.
 * Normally, when a common drag action is taking place, the state will be
 * DROP_STATE_NEW_BOOKMARK_ARMED, however, if the client of NautilusGtkPlacesSidebar
 * wants to show hints about the valid targets, we sill set it as
 * DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT, so the sidebar will show drop hints
 * until the client says otherwise
 */
typedef enum {
  DROP_STATE_NORMAL,
  DROP_STATE_NEW_BOOKMARK_ARMED,
  DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT,
} DropState;

struct _NautilusGtkPlacesSidebar {
  GtkWidget parent;

  NautilusWindowSlot *window_slot;
  GSignalGroup *slot_signal_group;

  GtkWidget *swin;
  GtkWidget *list_box;
  GtkWidget *new_bookmark_row;

  NautilusBookmarkList *bookmark_list;

  GActionGroup *row_actions;

#ifdef HAVE_CLOUDPROVIDERS
  CloudProvidersCollector *cloud_manager;
  GList *unready_accounts;
#endif

  GVolumeMonitor    *volume_monitor;
  GtkSettings       *gtk_settings;
  GFile             *current_location;

  GtkWidget *rename_popover;
  GtkWidget *rename_entry;
  GtkWidget *rename_button;
  GtkWidget *rename_error;
  char *rename_uri;

  GtkWidget *trash_row;
  gboolean show_trash;

  /* DND */
  gboolean   dragging_over;
  GtkWidget *drag_row;
  int drag_row_height;
  int drag_row_x;
  int drag_row_y;
  GtkWidget *row_placeholder;
  DropState drop_state;
  guint hover_timer_id;
  graphene_point_t hover_start_point;
  GtkListBoxRow *hover_row;

  GtkWidget *popover;
  NautilusGtkSidebarRow *context_row;

  GDBusProxy *hostnamed_proxy;
  GCancellable *hostnamed_cancellable;
  char *hostname;

  NautilusOpenFlags open_flags;

  guint show_desktop           : 1;
};

struct _NautilusGtkPlacesSidebarClass {
  GtkWidgetClass parent_class;

  GdkDragAction (* drag_action_requested)  (NautilusGtkPlacesSidebar   *sidebar,
                                      GFile              *dest_file,
                                      GSList             *source_file_list);
  GdkDragAction (* drag_action_ask)  (NautilusGtkPlacesSidebar   *sidebar,
                                      GdkDragAction       actions);
  void    (* drag_perform_drop)      (NautilusGtkPlacesSidebar   *sidebar,
                                      GFile              *dest_file,
                                      GList              *source_file_list,
                                      GdkDragAction       action);

  void    (* mount)                  (NautilusGtkPlacesSidebar   *sidebar,
                                      GMountOperation    *mount_operation);
  void    (* unmount)                (NautilusGtkPlacesSidebar   *sidebar,
                                      GMountOperation    *unmount_operation);
};

enum {
  DRAG_ACTION_REQUESTED,
  DRAG_ACTION_ASK,
  DRAG_PERFORM_DROP,
  MOUNT,
  UNMOUNT,
  LAST_SIGNAL
};

enum {
  PROP_LOCATION = 1,
  PROP_OPEN_FLAGS,
  PROP_WINDOW_SLOT,
  NUM_PROPERTIES
};

/* Names for themed icons */
#define ICON_NAME_HOME     "user-home-symbolic"
#define ICON_NAME_DESKTOP  "user-desktop-symbolic"
#define ICON_NAME_EJECT    "media-eject-symbolic"
#define ICON_NAME_NETWORK  "network-workgroup-symbolic"
#define ICON_NAME_NETWORK_VIEW  "network-computer-symbolic"
#define ICON_NAME_FOLDER_NETWORK "folder-remote-symbolic"

static guint places_sidebar_signals [LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static gboolean eject_or_unmount_bookmark  (NautilusGtkSidebarRow *row);
static gboolean eject_or_unmount_selection (NautilusGtkPlacesSidebar *sidebar);
static void  check_unmount_and_eject       (GMount   *mount,
                                            GVolume  *volume,
                                            GDrive   *drive,
                                            gboolean *show_unmount,
                                            gboolean *show_eject);
static void on_row_pressed  (GtkGestureClick *gesture,
                             int                   n_press,
                             double                x,
                             double                y,
                             NautilusGtkSidebarRow        *row);
static void on_row_released (GtkGestureClick *gesture,
                             int                   n_press,
                             double                x,
                             double                y,
                             NautilusGtkSidebarRow        *row);
static void on_row_dragged  (GtkGestureDrag *gesture,
                             double          x,
                             double          y,
                             NautilusGtkSidebarRow  *row);

static void popup_menu_cb    (NautilusGtkSidebarRow   *row);
static void long_press_cb    (GtkGesture      *gesture,
                              double           x,
                              double           y,
                              NautilusGtkPlacesSidebar *sidebar);
static void stop_drop_feedback (NautilusGtkPlacesSidebar *sidebar);
static GMountOperation * get_mount_operation (NautilusGtkPlacesSidebar *sidebar);
static GMountOperation * get_unmount_operation (NautilusGtkPlacesSidebar *sidebar);


G_DEFINE_TYPE (NautilusGtkPlacesSidebar, nautilus_gtk_places_sidebar, GTK_TYPE_WIDGET);

static void
call_open_location (NautilusGtkPlacesSidebar *self,
                    GFile                    *location,
                    NautilusWindowSlot       *preferred_slot,
                    NautilusOpenFlags         open_flags)
{
  if ((open_flags & self->open_flags) == 0)
    open_flags = NAUTILUS_OPEN_FLAG_NORMAL;

  if (open_flags == NAUTILUS_OPEN_FLAG_NEW_TAB)
    {
      open_flags |= NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;
    }

  if (open_flags & (NAUTILUS_OPEN_FLAG_NEW_WINDOW | NAUTILUS_OPEN_FLAG_NEW_TAB))
    {
      nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                               location, open_flags, NULL, NULL, NULL);
    }
  else
    {
      if (!preferred_slot) {
        nautilus_window_slot_open_location_full (self->window_slot, location, open_flags, NULL);
      } else {
        nautilus_window_slot_open_location_full (preferred_slot, location, open_flags, NULL);
      }

      GtkWidget *ancestor = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_OVERLAY_SPLIT_VIEW);
      AdwOverlaySplitView *split_view = ADW_OVERLAY_SPLIT_VIEW (ancestor);

      g_assert (split_view != NULL);

      if (adw_overlay_split_view_get_collapsed (split_view))
        {
          adw_overlay_split_view_set_show_sidebar (split_view, FALSE);
        }
    }
}

static void
show_error_message (NautilusGtkPlacesSidebar *self,
                    const char               *primary,
                    const char               *secondary)
{
  show_dialog (primary,
               secondary,
               GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
               GTK_MESSAGE_ERROR);
}

static void
emit_mount_operation (NautilusGtkPlacesSidebar *sidebar,
                      GMountOperation  *mount_op)
{
  g_signal_emit (sidebar, places_sidebar_signals[MOUNT], 0, mount_op);
}

static void
emit_unmount_operation (NautilusGtkPlacesSidebar *sidebar,
                        GMountOperation  *mount_op)
{
  g_signal_emit (sidebar, places_sidebar_signals[UNMOUNT], 0, mount_op);
}

static GdkDragAction
emit_drag_action_requested (NautilusGtkPlacesSidebar *sidebar,
                            NautilusFile            *dest_file,
                            GSList           *source_file_list)
{
  GdkDragAction ret_action = 0;

  g_signal_emit (sidebar, places_sidebar_signals[DRAG_ACTION_REQUESTED], 0,
                 dest_file, source_file_list, &ret_action);

  return ret_action;
}

static void
emit_drag_perform_drop (NautilusGtkPlacesSidebar *sidebar,
                        GFile            *dest_file,
                        GSList           *source_file_list,
                        GdkDragAction     action)
{
  g_signal_emit (sidebar, places_sidebar_signals[DRAG_PERFORM_DROP], 0,
                 dest_file, source_file_list, action);
}
static void
list_box_header_func (GtkListBoxRow *row,
                      GtkListBoxRow *before,
                      gpointer       user_data)
{
  NautilusGtkPlacesSectionType row_section_type;
  NautilusGtkPlacesSectionType before_section_type;
  GtkWidget *separator;

  gtk_list_box_row_set_header (row, NULL);

  g_object_get (row, "section-type", &row_section_type, NULL);
  if (before)
    {
      g_object_get (before, "section-type", &before_section_type, NULL);
    }
  else
    {
      before_section_type = NAUTILUS_GTK_PLACES_SECTION_INVALID;
    }

  if (before && before_section_type != row_section_type)
    {
      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, separator);
    }
}

static GtkWidget*
add_place (NautilusGtkPlacesSidebar            *sidebar,
           NautilusGtkPlacesPlaceType           place_type,
           NautilusGtkPlacesSectionType         section_type,
           const char                  *name,
           GIcon                       *start_icon,
           GIcon                       *end_icon,
           const char                  *uri,
           GDrive                      *drive,
           GVolume                     *volume,
           GMount                      *mount,
#ifdef HAVE_CLOUDPROVIDERS
           CloudProvidersAccount       *cloud_provider_account,
#else
           gpointer                    *cloud_provider_account,
#endif
           const int                    index,
           const char                  *tooltip)
{
  gboolean show_eject, show_unmount;
  gboolean show_eject_button;
  GtkWidget *row;
  GtkWidget *eject_button;
  GtkGesture *gesture;
  char *eject_tooltip;

  check_unmount_and_eject (mount, volume, drive,
                           &show_unmount, &show_eject);

  if (show_unmount || show_eject)
    g_assert (place_type != NAUTILUS_GTK_PLACES_BOOKMARK);

  show_eject_button = (show_unmount || show_eject);
  if (mount != NULL && volume == NULL && drive == NULL)
    eject_tooltip = _("Disconnect");
  else if (show_eject)
    eject_tooltip = _("Eject");
  else
    eject_tooltip = _("Unmount");

  row = g_object_new (NAUTILUS_TYPE_GTK_SIDEBAR_ROW,
                      "sidebar", sidebar,
                      "start-icon", start_icon,
                      "end-icon", end_icon,
                      "label", name,
                      "tooltip", tooltip,
                      "ejectable", show_eject_button,
                      "eject-tooltip", eject_tooltip,
                      "order-index", index,
                      "section-type", section_type,
                      "place-type", place_type,
                      "uri", uri,
                      "drive", drive,
                      "volume", volume,
                      "mount", mount,
#ifdef HAVE_CLOUDPROVIDERS
                      "cloud-provider-account", cloud_provider_account,
#endif
                      NULL);

  eject_button = nautilus_gtk_sidebar_row_get_eject_button (NAUTILUS_GTK_SIDEBAR_ROW (row));

  g_signal_connect_swapped (eject_button, "clicked",
                            G_CALLBACK (eject_or_unmount_bookmark), row);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (on_row_pressed), row);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (on_row_released), row);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_drag_new ();
  g_signal_connect (gesture, "drag-update",
                    G_CALLBACK (on_row_dragged), row);
  gtk_widget_add_controller (row, GTK_EVENT_CONTROLLER (gesture));

  gtk_list_box_insert (GTK_LIST_BOX (sidebar->list_box), GTK_WIDGET (row), -1);

  return row;
}

static gboolean
recent_files_setting_is_enabled (NautilusGtkPlacesSidebar *sidebar)
{
  GtkSettings *settings;
  gboolean enabled;

  settings = gtk_widget_get_settings (GTK_WIDGET (sidebar));
  g_object_get (settings, "gtk-recent-files-enabled", &enabled, NULL);

  return enabled;
}

static gboolean
recent_scheme_is_supported (void)
{
  const char * const *supported;

  supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
  if (supported != NULL)
    return g_strv_contains (supported, SCHEME_RECENT);

  return FALSE;
}

static gboolean
should_show_recent (NautilusGtkPlacesSidebar *sidebar)
{
  return recent_files_setting_is_enabled (sidebar) && recent_scheme_is_supported ();
}

static gboolean
path_is_home_dir (const char *path)
{
  GFile *home_dir;
  GFile *location;
  const char *home_path;
  gboolean res;

  home_path = g_get_home_dir ();
  if (!home_path)
    return FALSE;

  home_dir = g_file_new_for_path (home_path);
  location = g_file_new_for_path (path);
  res = g_file_equal (home_dir, location);

  g_object_unref (home_dir);
  g_object_unref (location);

  return res;
}

static char *
get_home_directory_uri (void)
{
  const char *home;

  home = g_get_home_dir ();
  if (!home)
    return NULL;

  return g_filename_to_uri (home, NULL, NULL);
}

static char *
get_desktop_directory_uri (void)
{
  const char *name;

  name = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

  /* "To disable a directory, point it to the homedir."
   * See http://freedesktop.org/wiki/Software/xdg-user-dirs
   */
  if (path_is_home_dir (name))
    return NULL;

  return g_filename_to_uri (name, NULL, NULL);
}

typedef struct {
  NautilusGtkPlacesSidebar *sidebar;
  int index;
  gboolean is_native;
} BookmarkQueryClosure;

static void
update_trash_icon (NautilusGtkPlacesSidebar *sidebar)
{
  if (sidebar->trash_row)
    {
      GIcon *icon;

      icon = nautilus_trash_monitor_get_symbolic_icon ();
      nautilus_gtk_sidebar_row_set_start_icon (NAUTILUS_GTK_SIDEBAR_ROW (sidebar->trash_row), icon);
      g_object_unref (icon);
    }
}

#ifdef HAVE_CLOUDPROVIDERS

static gboolean
create_cloud_provider_account_row (NautilusGtkPlacesSidebar      *sidebar,
                                   CloudProvidersAccount *account)
{
  GIcon *end_icon;
  GIcon *start_icon;
  const char *mount_path;
  const char *name;
  char *mount_uri;
  char *tooltip;
  guint provider_account_status;

  start_icon = cloud_providers_account_get_icon (account);
  name = cloud_providers_account_get_name (account);
  provider_account_status = cloud_providers_account_get_status (account);
  mount_path = cloud_providers_account_get_path (account);
  if (start_icon != NULL
      && name != NULL
      && provider_account_status != CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID
      && mount_path != NULL)
    {
      switch (provider_account_status)
        {
          case CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE:
            end_icon = NULL;
          break;

          case CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING:
            end_icon = g_themed_icon_new ("emblem-synchronizing-symbolic");
          break;

          case CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR:
            end_icon = g_themed_icon_new ("dialog-warning-symbolic");
          break;

          default:
            return FALSE;
        }

      mount_uri = g_strconcat ("file://", mount_path, NULL);

      /* translators: %s is the name of a cloud provider for files */
      tooltip = g_strdup_printf (_("Open %s"), name);

      add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
                 NAUTILUS_GTK_PLACES_SECTION_CLOUD,
                 name, start_icon, end_icon, mount_uri,
                 NULL, NULL, NULL, account, 0,
                 tooltip);

      g_free (tooltip);
      g_free (mount_uri);
      g_clear_object (&end_icon);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
on_account_updated (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
    CloudProvidersAccount *account = CLOUD_PROVIDERS_ACCOUNT (object);
    NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (user_data);

    if (create_cloud_provider_account_row (sidebar, account))
      {
          g_signal_handlers_disconnect_by_data (account, sidebar);
          sidebar->unready_accounts = g_list_remove (sidebar->unready_accounts, account);
          g_object_unref (account);
      }
}

#endif

static void
update_places (NautilusGtkPlacesSidebar *sidebar)
{
  GList *mounts, *l, *ll;
  GMount *mount;
  GList *drives;
  GDrive *drive;
  GList *volumes;
  GVolume *volume;
  GList *bookmarks;
  int index;
  char *original_uri, *name, *identifier;
  GtkListBoxRow *selected;
  char *home_uri;
  GIcon *start_icon;
  GFile *root;
  char *tooltip;
  GList *network_mounts, *network_volumes;
  GIcon *new_bookmark_icon;
  GtkWidget *child;
#ifdef HAVE_CLOUDPROVIDERS
  GList *cloud_providers;
  GList *cloud_providers_accounts;
  CloudProvidersAccount *cloud_provider_account;
  CloudProvidersProvider *cloud_provider;
#endif

  /* save original selection */
  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));
  if (selected)
    g_object_get (selected, "uri", &original_uri, NULL);
  else
    original_uri = NULL;

  /* Reset drag state, just in case we update the places while dragging or
   * ending a drag */
  stop_drop_feedback (sidebar);
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (sidebar->list_box))))
    gtk_list_box_remove (GTK_LIST_BOX (sidebar->list_box), child);

  network_mounts = network_volumes = NULL;

  /* add built-in places */

  /* home folder */
  home_uri = get_home_directory_uri ();
  start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_HOME);
  add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
             NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
             _("Home"), start_icon, NULL, home_uri,
             NULL, NULL, NULL, NULL, 0,
             _("Open Personal Folder"));
  g_object_unref (start_icon);
  g_free (home_uri);

  if (should_show_recent (sidebar))
    {
      start_icon = g_themed_icon_new_with_default_fallbacks ("document-open-recent-symbolic");
      add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
                 NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
                 _("Recent"), start_icon, NULL, SCHEME_RECENT ":///",
                 NULL, NULL, NULL, NULL, 0,
                 _("Recent Files"));
      g_object_unref (start_icon);
    }

  start_icon = g_themed_icon_new_with_default_fallbacks ("starred-symbolic");
  add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
             NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
             _("Starred"), start_icon, NULL, SCHEME_STARRED ":///",
             NULL, NULL, NULL, NULL, 0,
             _("Starred Files"));
  g_object_unref (start_icon);

  /* desktop */
  if (sidebar->show_desktop)
    {
      char *mount_uri = get_desktop_directory_uri ();
      if (mount_uri)
        {
          start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_DESKTOP);
          add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
                     NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
                     _("Desktop"), start_icon, NULL, mount_uri,
                     NULL, NULL, NULL, NULL, 0,
                     _("Open the contents of your desktop in a folder"));
          g_object_unref (start_icon);
          g_free (mount_uri);
        }
    }

  /* Network view */
  start_icon = g_themed_icon_new_with_default_fallbacks (ICON_NAME_NETWORK_VIEW);
  add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
             NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
             _("Network"), start_icon, NULL, SCHEME_NETWORK_VIEW ":///",
             NULL, NULL, NULL, NULL, 0,
             _("Open Network Locations"));
  g_object_unref (start_icon);

  /* Trash */
  if (sidebar->show_trash)
    {
      start_icon = nautilus_trash_monitor_get_symbolic_icon ();
      sidebar->trash_row = add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
                                      NAUTILUS_GTK_PLACES_SECTION_DEFAULT_LOCATIONS,
                                      _("Trash"), start_icon, NULL, SCHEME_TRASH ":///",
                                      NULL, NULL, NULL, NULL, 0,
                                      _("Open Trash"));
      g_object_add_weak_pointer (G_OBJECT (sidebar->trash_row),
                                 (gpointer *) &sidebar->trash_row);
      g_object_unref (start_icon);
    }

  /* Cloud providers */
#ifdef HAVE_CLOUDPROVIDERS
  cloud_providers = cloud_providers_collector_get_providers (sidebar->cloud_manager);
  for (l = sidebar->unready_accounts; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (l->data, sidebar);
    }
  g_list_free_full (sidebar->unready_accounts, g_object_unref);
  sidebar->unready_accounts = NULL;
  for (l = cloud_providers; l != NULL; l = l->next)
    {
      cloud_provider = CLOUD_PROVIDERS_PROVIDER (l->data);
      g_signal_connect_swapped (cloud_provider, "accounts-changed",
                                G_CALLBACK (update_places), sidebar);
      cloud_providers_accounts = cloud_providers_provider_get_accounts (cloud_provider);
      for (ll = cloud_providers_accounts; ll != NULL; ll = ll->next)
        {
          cloud_provider_account = CLOUD_PROVIDERS_ACCOUNT (ll->data);
          if (!create_cloud_provider_account_row (sidebar, cloud_provider_account))
            {

              g_signal_connect (cloud_provider_account, "notify::name",
                                G_CALLBACK (on_account_updated), sidebar);
              g_signal_connect (cloud_provider_account, "notify::status",
                                G_CALLBACK (on_account_updated), sidebar);
              g_signal_connect (cloud_provider_account, "notify::status-details",
                                G_CALLBACK (on_account_updated), sidebar);
              g_signal_connect (cloud_provider_account, "notify::path",
                                G_CALLBACK (on_account_updated), sidebar);
              sidebar->unready_accounts = g_list_append (sidebar->unready_accounts,
                                                         g_object_ref (cloud_provider_account));
              continue;
            }

        }
    }
#endif

  /* go through all connected drives */
  drives = g_volume_monitor_get_connected_drives (sidebar->volume_monitor);

  for (l = drives; l != NULL; l = l->next)
    {
      drive = l->data;

      volumes = g_drive_get_volumes (drive);
      if (volumes != NULL)
        {
          for (ll = volumes; ll != NULL; ll = ll->next)
            {
              volume = ll->data;
              identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

              if (g_strcmp0 (identifier, "network") == 0)
                {
                  g_free (identifier);
                  network_volumes = g_list_prepend (network_volumes, volume);
                  continue;
                }
              g_free (identifier);

              mount = g_volume_get_mount (volume);
              if (mount != NULL)
                {
                  char *mount_uri;

                  /* Show mounted volume in the sidebar */
                  start_icon = g_mount_get_symbolic_icon (mount);
                  root = g_mount_get_default_location (mount);
                  mount_uri = g_file_get_uri (root);
                  name = g_mount_get_name (mount);
                  tooltip = g_file_get_parse_name (root);

                  add_place (sidebar, (is_external_volume (volume) ?
                                       NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT :
                                       NAUTILUS_GTK_PLACES_INTERNAL_MOUNT),
                             NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                             name, start_icon, NULL, mount_uri,
                             drive, volume, mount, NULL, 0, tooltip);
                  g_object_unref (root);
                  g_object_unref (mount);
                  g_object_unref (start_icon);
                  g_free (tooltip);
                  g_free (name);
                  g_free (mount_uri);
                }
              else
                {
                  /* Do show the unmounted volumes in the sidebar;
                   * this is so the user can mount it (in case automounting
                   * is off).
                   *
                   * Also, even if automounting is enabled, this gives a visual
                   * cue that the user should remember to yank out the media if
                   * he just unmounted it.
                   */
                  start_icon = g_volume_get_symbolic_icon (volume);
                  name = g_volume_get_name (volume);
                  tooltip = g_strdup_printf (_("Mount and Open “%s”"), name);

                  add_place (sidebar, (is_external_volume (volume) ?
                                       NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT :
                                       NAUTILUS_GTK_PLACES_INTERNAL_MOUNT),
                             NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                             name, start_icon, NULL, NULL,
                             drive, volume, NULL, NULL, 0, tooltip);
                  g_object_unref (start_icon);
                  g_free (name);
                  g_free (tooltip);
                }
              g_object_unref (volume);
            }
          g_list_free (volumes);
        }
      else
        {
          if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive))
            {
              /* If the drive has no mountable volumes and we cannot detect media change.. we
               * display the drive in the sidebar so the user can manually poll the drive by
               * right clicking and selecting "Rescan..."
               *
               * This is mainly for drives like floppies where media detection doesn't
               * work.. but it's also for human beings who like to turn off media detection
               * in the OS to save battery juice.
               */
              start_icon = g_drive_get_symbolic_icon (drive);
              name = g_drive_get_name (drive);
              tooltip = g_strdup_printf (_("Mount and Open “%s”"), name);

              add_place (sidebar, NAUTILUS_GTK_PLACES_BUILT_IN,
                         NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                         name, start_icon, NULL, NULL,
                         drive, NULL, NULL, NULL, 0, tooltip);
              g_object_unref (start_icon);
              g_free (tooltip);
              g_free (name);
            }
        }
    }
  g_list_free_full (drives, g_object_unref);

  /* add all network volumes that are not associated with a drive, and
   * loop devices
   */
  volumes = g_volume_monitor_get_volumes (sidebar->volume_monitor);
  for (l = volumes; l != NULL; l = l->next)
    {
      volume = l->data;
      drive = g_volume_get_drive (volume);
      if (drive != NULL)
        {
          g_object_unref (volume);
          g_object_unref (drive);
          continue;
        }

      identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

      if (g_strcmp0 (identifier, "network") == 0)
        {
          g_free (identifier);
          network_volumes = g_list_prepend (network_volumes, volume);
          continue;
        }
      g_free (identifier);

      mount = g_volume_get_mount (volume);
      if (mount != NULL)
        {
          char *mount_uri;

          start_icon = g_mount_get_symbolic_icon (mount);
          root = g_mount_get_default_location (mount);
          mount_uri = g_file_get_uri (root);
          tooltip = g_file_get_parse_name (root);
          name = g_mount_get_name (mount);
          add_place (sidebar, (is_external_volume (volume) ?
                               NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT :
                               NAUTILUS_GTK_PLACES_INTERNAL_MOUNT),
                     NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                     name, start_icon, NULL, mount_uri,
                     NULL, volume, mount, NULL, 0, tooltip);
          g_object_unref (mount);
          g_object_unref (root);
          g_object_unref (start_icon);
          g_free (name);
          g_free (tooltip);
          g_free (mount_uri);
        }
      else
        {
          /* see comment above in why we add an icon for an unmounted mountable volume */
          start_icon = g_volume_get_symbolic_icon (volume);
          name = g_volume_get_name (volume);
          add_place (sidebar, (is_external_volume (volume) ?
                               NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT :
                               NAUTILUS_GTK_PLACES_INTERNAL_MOUNT),
                     NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                     name, start_icon, NULL, NULL,
                     NULL, volume, NULL, NULL, 0, name);
          g_object_unref (start_icon);
          g_free (name);
        }
      g_object_unref (volume);
    }
  g_list_free (volumes);

  /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
  mounts = g_volume_monitor_get_mounts (sidebar->volume_monitor);

  for (l = mounts; l != NULL; l = l->next)
    {
      char *mount_uri;

      mount = l->data;
      if (g_mount_is_shadowed (mount))
        {
          g_object_unref (mount);
          continue;
        }
      volume = g_mount_get_volume (mount);
      if (volume != NULL)
        {
          g_object_unref (volume);
          g_object_unref (mount);
          continue;
        }
      root = g_mount_get_default_location (mount);

      if (!g_file_is_native (root))
        {
          network_mounts = g_list_prepend (network_mounts, mount);
          g_object_unref (root);
          continue;
        }

      start_icon = g_mount_get_symbolic_icon (mount);
      mount_uri = g_file_get_uri (root);
      name = g_mount_get_name (mount);
      tooltip = g_file_get_parse_name (root);
      add_place (sidebar, NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT,
                 NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                 name, start_icon, NULL, mount_uri,
                 NULL, NULL, mount, NULL, 0, tooltip);
      g_object_unref (root);
      g_object_unref (mount);
      g_object_unref (start_icon);
      g_free (name);
      g_free (mount_uri);
      g_free (tooltip);
    }
  g_list_free (mounts);

  /* add bookmarks */
  bookmarks = nautilus_bookmark_list_get_all (sidebar->bookmark_list);
  index = 0;

  for (GList *l = bookmarks; l != NULL; l = l->next)
    {
      GtkWidget *row;

      g_autoptr (GFile) location = nautilus_bookmark_get_location (l->data);
      g_autofree char *mount_uri = nautilus_bookmark_get_uri (l->data);

      gboolean is_native = g_file_is_native (location);
      tooltip = is_native ? g_file_get_path (location) : g_uri_unescape_string (mount_uri, NULL);

      row = add_place (sidebar, NAUTILUS_GTK_PLACES_BOOKMARK,
                       NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS,
                       nautilus_bookmark_get_name (l->data),
                       NULL,
                       NULL, mount_uri, NULL, NULL, NULL, NULL, index, tooltip);
      g_object_bind_property (l->data, "symbolic-icon", row, "start-icon", G_BINDING_SYNC_CREATE);
      index++;

      g_free (tooltip);
    }

  /* Add new bookmark row */
  new_bookmark_icon = g_themed_icon_new ("bookmark-new-symbolic");
  sidebar->new_bookmark_row = add_place (sidebar, NAUTILUS_GTK_PLACES_DROP_FEEDBACK,
                                         NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS,
                                         _("New bookmark"), new_bookmark_icon, NULL, NULL,
                                         NULL, NULL, NULL, NULL, 0,
                                         _("Add a new bookmark"));
  gtk_widget_add_css_class (sidebar->new_bookmark_row, "sidebar-new-bookmark-row");
  g_object_unref (new_bookmark_icon);

  /* network */
  network_volumes = g_list_reverse (network_volumes);
  for (l = network_volumes; l != NULL; l = l->next)
    {
      volume = l->data;
      mount = g_volume_get_mount (volume);

      if (mount != NULL)
        {
          network_mounts = g_list_prepend (network_mounts, mount);
          continue;
        }
      else
        {
          start_icon = g_volume_get_symbolic_icon (volume);
          name = g_volume_get_name (volume);
          tooltip = g_strdup_printf (_("Mount and Open “%s”"), name);

          add_place (sidebar, NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT,
                     NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                     name, start_icon, NULL, NULL,
                     NULL, volume, NULL, NULL, 0, tooltip);
          g_object_unref (start_icon);
          g_free (name);
          g_free (tooltip);
        }
    }

  network_mounts = g_list_reverse (network_mounts);
  for (l = network_mounts; l != NULL; l = l->next)
    {
      char *mount_uri;

      mount = l->data;
      root = g_mount_get_default_location (mount);
      start_icon = g_mount_get_symbolic_icon (mount);
      mount_uri = g_file_get_uri (root);
      name = g_mount_get_name (mount);
      tooltip = g_file_get_parse_name (root);
      add_place (sidebar, NAUTILUS_GTK_PLACES_EXTERNAL_MOUNT,
                 NAUTILUS_GTK_PLACES_SECTION_MOUNTS,
                 name, start_icon, NULL, mount_uri,
                 NULL, NULL, mount, NULL, 0, tooltip);
      g_object_unref (root);
      g_object_unref (start_icon);
      g_free (name);
      g_free (mount_uri);
      g_free (tooltip);
    }
  

  g_list_free_full (network_volumes, g_object_unref);
  g_list_free_full (network_mounts, g_object_unref);

  /* We want this hidden by default, but need to do it after the show_all call */
  nautilus_gtk_sidebar_row_hide (NAUTILUS_GTK_SIDEBAR_ROW (sidebar->new_bookmark_row), TRUE);

  /* restore original selection */
  if (original_uri)
    {
      GFile *restore;

      restore = g_file_new_for_uri (original_uri);
      nautilus_gtk_places_sidebar_set_location (sidebar, restore);
      g_object_unref (restore);
      g_free (original_uri);
    }
}

void
nautilus_gtk_places_sidebar_set_show_trash (NautilusGtkPlacesSidebar *sidebar,
                                            gboolean                  show_trash)
{
  if (sidebar->show_trash == show_trash)
    return;

  sidebar->show_trash = show_trash;

  if (!sidebar->show_trash)
    g_clear_weak_pointer (&sidebar->trash_row);

  update_places (sidebar);
}

static gboolean
hover_timer (gpointer user_data)
{
  NautilusGtkPlacesSidebar *sidebar = user_data;
  gboolean open_folder_on_hover;
  g_autofree gchar *uri = NULL;
  g_autoptr (GFile) location = NULL;

  open_folder_on_hover = g_settings_get_boolean (nautilus_preferences,
                                                 NAUTILUS_PREFERENCES_OPEN_FOLDER_ON_DND_HOVER);
  sidebar->hover_timer_id = 0;

  if (open_folder_on_hover &&
      sidebar->hover_row != NULL &&
      gtk_widget_get_sensitive (GTK_WIDGET (sidebar->hover_row)))
    {
      g_object_get (sidebar->hover_row, "uri", &uri, NULL);
      if (uri != NULL && g_strcmp0 (uri, SCHEME_TRASH ":///") != 0)
        {
          location = g_file_new_for_uri (uri);
          call_open_location (sidebar, location, NULL, 0);
        }
    }

  return G_SOURCE_REMOVE;
}

static gboolean
check_valid_drop_target (NautilusGtkPlacesSidebar *sidebar,
                         NautilusGtkSidebarRow    *row,
                         const GValue     *value)
{
  NautilusGtkPlacesPlaceType place_type;
  NautilusGtkPlacesSectionType section_type;
  g_autoptr (NautilusFile) dest_file = NULL;
  gboolean valid = FALSE;
  char *uri;
  int drag_action;

  g_return_val_if_fail (value != NULL, TRUE);

  if (row == NULL)
    return FALSE;

  g_object_get (row,
                "place-type", &place_type,
                "section_type", &section_type,
                "uri", &uri,
                "file", &dest_file,
                NULL);

  if (place_type == NAUTILUS_GTK_PLACES_DROP_FEEDBACK)
    {
      g_free (uri);
      return TRUE;
    }

  /* Disallow drops on recent:/// */
  if (place_type == NAUTILUS_GTK_PLACES_BUILT_IN)
    {
      if (g_strcmp0 (uri, SCHEME_RECENT ":///") == 0)
        {
          g_free (uri);
          return FALSE;
        }
    }

  /* Dragging a bookmark? */
  if (G_VALUE_HOLDS (value, NAUTILUS_TYPE_GTK_SIDEBAR_ROW))
    {
      /* Don't allow reordering bookmarks into non-bookmark areas */
      valid = section_type == NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS;
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      /* Dragging a file */
      if (uri != NULL)
        {
          drag_action = emit_drag_action_requested (sidebar, dest_file, g_value_get_boxed (value));
          valid = drag_action > 0;
        }
      else
        {
          valid = FALSE;
        }
    }
  else
    {
      g_assert_not_reached ();
      valid = TRUE;
    }

  g_free (uri);
  return valid;
}

static void
update_possible_drop_targets (NautilusGtkPlacesSidebar *sidebar,
                              const GValue     *value)
{
  GtkWidget *row;

  for (row = gtk_widget_get_first_child (GTK_WIDGET (sidebar->list_box));
       row != NULL;
       row = gtk_widget_get_next_sibling (row))
    {
      gboolean sensitive;

      if (!GTK_IS_LIST_BOX_ROW (row))
        continue;

      sensitive = value == NULL ||
                  check_valid_drop_target (sidebar, NAUTILUS_GTK_SIDEBAR_ROW (row), value);
      gtk_widget_set_sensitive (row, sensitive);
    }
}

static void
start_drop_feedback (NautilusGtkPlacesSidebar *sidebar,
                     const GValue     *value)
{
  if (value != NULL && G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GSList *source_list = g_value_get_boxed (value);
      if (g_slist_length (source_list) == 1)
        {
          g_autoptr (NautilusFile) file = NULL;
          file = nautilus_file_get (source_list->data);
          if (nautilus_file_is_directory (file))
            nautilus_gtk_sidebar_row_reveal (NAUTILUS_GTK_SIDEBAR_ROW (sidebar->new_bookmark_row));
        }
    }
  if (value && !G_VALUE_HOLDS (value, NAUTILUS_TYPE_GTK_SIDEBAR_ROW))
    {
      /* If the state is permanent, don't change it. The application controls it. */
      if (sidebar->drop_state != DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT)
        sidebar->drop_state = DROP_STATE_NEW_BOOKMARK_ARMED;
    }

  update_possible_drop_targets (sidebar, value);
}

static void
stop_drop_feedback (NautilusGtkPlacesSidebar *sidebar)
{
  update_possible_drop_targets (sidebar, NULL);

  if (sidebar->drop_state != DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT &&
      sidebar->new_bookmark_row != NULL)
    {
      nautilus_gtk_sidebar_row_hide (NAUTILUS_GTK_SIDEBAR_ROW (sidebar->new_bookmark_row), FALSE);
      sidebar->drop_state = DROP_STATE_NORMAL;
    }

  if (sidebar->drag_row != NULL)
    {
      gtk_widget_set_visible (sidebar->drag_row, TRUE);
      sidebar->drag_row = NULL;
    }

  if (sidebar->row_placeholder != NULL)
    {
      if (gtk_widget_get_parent (sidebar->row_placeholder) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (sidebar->list_box), sidebar->row_placeholder);
      sidebar->row_placeholder = NULL;
    }

  sidebar->dragging_over = FALSE;
}

static GtkWidget *
create_placeholder_row (NautilusGtkPlacesSidebar *sidebar)
{
  return g_object_new (NAUTILUS_TYPE_GTK_SIDEBAR_ROW, "placeholder", TRUE, NULL);
}

static GdkDragAction
drag_motion_callback (GtkDropTarget    *target,
                      double            x,
                      double            y,
                      NautilusGtkPlacesSidebar *sidebar)
{
  GdkDragAction action;
  GtkListBoxRow *row;
  NautilusGtkPlacesPlaceType place_type;
  char *drop_target_uri = NULL;
  int row_index;
  int row_placeholder_index;
  const GValue *value;
  graphene_point_t start;
  graphene_point_t point_in_row;

  sidebar->dragging_over = TRUE;
  action = 0;
  row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (sidebar->list_box), y);

  start = sidebar->hover_start_point;
  if (row != NULL &&
      sidebar->drag_row == NULL &&
      (row != sidebar->hover_row ||
      gtk_drag_check_threshold (GTK_WIDGET (sidebar), start.x, start.y, x, y)))
    {
      g_clear_handle_id (&sidebar->hover_timer_id, g_source_remove);
      g_set_weak_pointer (&sidebar->hover_row, row);
      sidebar->hover_timer_id = g_timeout_add (HOVER_TIMEOUT, hover_timer, sidebar);
      sidebar->hover_start_point.x = x;
      sidebar->hover_start_point.y = y;
    }

  /* Workaround https://gitlab.gnome.org/GNOME/gtk/-/issues/5023 */
  gtk_list_box_drag_unhighlight_row (GTK_LIST_BOX (sidebar->list_box));

  /* Nothing to do if no value yet */
  value = gtk_drop_target_get_value (target);
  if (value == NULL)
    goto out;

  /* Nothing to do if the target is not valid drop destination */
  if (!check_valid_drop_target (sidebar, NAUTILUS_GTK_SIDEBAR_ROW (row), value))
    goto out;

  if (G_VALUE_HOLDS (value, NAUTILUS_TYPE_GTK_SIDEBAR_ROW))
    {
      /* Dragging bookmarks always moves them to another position in the bookmarks list */
      action = GDK_ACTION_MOVE;
      if (sidebar->row_placeholder == NULL)
        {
          sidebar->row_placeholder = create_placeholder_row (sidebar);
          g_object_ref_sink (sidebar->row_placeholder);
        }
      else if (GTK_WIDGET (row) == sidebar->row_placeholder)
        {
          goto out;
        }

      if (gtk_widget_get_parent (sidebar->row_placeholder) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (sidebar->list_box), sidebar->row_placeholder);

      if (row != NULL)
        {
          g_object_get (row, "order-index", &row_index, NULL);
          g_object_get (sidebar->row_placeholder, "order-index", &row_placeholder_index, NULL);
          /* We order the bookmarks sections based on the bookmark index that we
           * set on the row as order-index property, but we have to deal with
           * the placeholder row wanting to be between two consecutive bookmarks,
           * with two consecutive order-index values which is the usual case.
           * For that, in the list box sort func we give priority to the placeholder row,
           * that means that if the index-order is the same as another bookmark
           * the placeholder row goes before. However if we want to show it after
           * the current row, for instance when the cursor is in the lower half
           * of the row, we need to increase the order-index.
           */
          row_placeholder_index = row_index;


          if (gtk_widget_compute_point (sidebar->list_box, GTK_WIDGET (row),
                                        &GRAPHENE_POINT_INIT (x, y), &point_in_row) &&
              point_in_row.y > sidebar->drag_row_height / 2 && row_index > 0)
            row_placeholder_index++;
        }
      else
        {
          /* If the user is dragging over an area that has no row, place the row
           * placeholder in the last position
           */
          row_placeholder_index = G_MAXINT32;
        }

      g_object_set (sidebar->row_placeholder, "order-index", row_placeholder_index, NULL);

      gtk_list_box_prepend (GTK_LIST_BOX (sidebar->list_box),
                            sidebar->row_placeholder);
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      NautilusFile *file;
      gtk_list_box_drag_highlight_row (GTK_LIST_BOX (sidebar->list_box), row);

      g_object_get (row,
                    "place-type", &place_type,
                    "uri", &drop_target_uri,
                    "file", &file,
                    NULL);
      /* URIs are being dragged.  See if the caller wants to handle a
       * file move/copy operation itself, or if we should only try to
       * create bookmarks out of the dragged URIs.
       */
      if (place_type == NAUTILUS_GTK_PLACES_DROP_FEEDBACK)
        {
          action = GDK_ACTION_COPY;
        }
      else
        {
          /* uri may be NULL for unmounted volumes, for example, so we don't allow drops there */
          if (drop_target_uri != NULL)
            {
              GFile *dest_file = g_file_new_for_uri (drop_target_uri);

              action = emit_drag_action_requested (sidebar, file, g_value_get_boxed (value));

              g_object_unref (dest_file);
            }
        }

      nautilus_file_unref (file);
      g_free (drop_target_uri);
    }
  else
    {
      g_assert_not_reached ();
    }

 out:
  start_drop_feedback (sidebar, value);
  return action;
}

/* Reorders the bookmark to the specified position */
static void
reorder_bookmarks (NautilusGtkPlacesSidebar *sidebar,
                   NautilusGtkSidebarRow    *row,
                   int               new_position)
{
  char *uri;
  GFile *file;
  guint old_position;

  g_object_get (row, "uri", &uri, NULL);
  file = g_file_new_for_uri (uri);
  nautilus_bookmark_list_item_with_location (sidebar->bookmark_list, file, &old_position);
  nautilus_bookmark_list_move_item (sidebar->bookmark_list, old_position, new_position);

  g_object_unref (file);
  g_free (uri);
}

/* Creates bookmarks for the specified files at the given position in the bookmarks list */
static void
drop_files_as_bookmarks (NautilusGtkPlacesSidebar *sidebar,
                         GSList           *files,
                         int               position)
{
  GSList *l;

  for (l = files; l; l = l->next)
    {
      GFile *f = G_FILE (l->data);
      GFileInfo *info = g_file_query_info (f,
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           NULL,
                                           NULL);

      if (info)
        {
          if ((g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY ||
               g_file_info_get_file_type (info) == G_FILE_TYPE_MOUNTABLE ||
               g_file_info_get_file_type (info) == G_FILE_TYPE_SHORTCUT ||
               g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK))
            {
              g_autoptr (NautilusBookmark) bookmark = nautilus_bookmark_new (f, NULL);
              nautilus_bookmark_list_insert_item (sidebar->bookmark_list, bookmark, position++);
            }

          g_object_unref (info);
        }
    }
}

static gboolean
drag_drop_callback (GtkDropTarget    *target,
                    const GValue     *value,
                    double            x,
                    double            y,
                    NautilusGtkPlacesSidebar *sidebar)
{
  int target_order_index;
  NautilusGtkPlacesPlaceType target_place_type;
  NautilusGtkPlacesSectionType target_section_type;
  char *target_uri;
  GtkListBoxRow *target_row;
  gboolean result;

  target_row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (sidebar->list_box), y);
  if (target_row == NULL)
    return FALSE;

  if (!check_valid_drop_target (sidebar, NAUTILUS_GTK_SIDEBAR_ROW (target_row), value))
    return FALSE;

  g_object_get (target_row,
                "place-type", &target_place_type,
                "section-type", &target_section_type,
                "order-index", &target_order_index,
                "uri", &target_uri,
                NULL);
  result = FALSE;

  if (G_VALUE_HOLDS (value, NAUTILUS_TYPE_GTK_SIDEBAR_ROW))
    {
      GtkWidget *source_row;
      /* A bookmark got reordered */
      if (target_section_type != NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS)
        goto out;

      source_row = g_value_get_object (value);

      if (sidebar->row_placeholder != NULL)
        g_object_get (sidebar->row_placeholder, "order-index", &target_order_index, NULL);

      reorder_bookmarks (sidebar, NAUTILUS_GTK_SIDEBAR_ROW (source_row), target_order_index);
      result = TRUE;
    }
  else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      /* Dropping URIs! */
      if (target_place_type == NAUTILUS_GTK_PLACES_DROP_FEEDBACK)
        {
          drop_files_as_bookmarks (sidebar, g_value_get_boxed (value), target_order_index);
        }
      else
        {
          GFile *dest_file = g_file_new_for_uri (target_uri);
          GdkDragAction actions;

          actions = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));

          #ifdef GDK_WINDOWING_X11
          if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (sidebar))))
          {
              /* Temporary workaround until the below GTK MR (or equivalend fix)
               * is merged.  Without this fix, the preferred action isn't set correctly.
               * https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4982 */
              GdkDrag *drag = gdk_drop_get_drag (gtk_drop_target_get_current_drop (target));
              actions = gdk_drag_get_selected_action (drag);
          }
          #endif
          
          emit_drag_perform_drop (sidebar,
                                  dest_file,
                                  g_value_get_boxed (value),
                                  actions);

          g_object_unref (dest_file);
        }
      result = TRUE;
    }
  else
    {
      g_assert_not_reached ();
    }

out:
  stop_drop_feedback (sidebar);
  g_free (target_uri);
  return result;
}

static void
dnd_finished_cb (GdkDrag          *drag,
                 NautilusGtkPlacesSidebar *sidebar)
{
  stop_drop_feedback (sidebar);
}

static void
dnd_cancel_cb (GdkDrag             *drag,
               GdkDragCancelReason  reason,
               NautilusGtkPlacesSidebar    *sidebar)
{
  stop_drop_feedback (sidebar);
}

/* This functions is called every time the drag source leaves
 * the sidebar widget.
 * The problem is that, we start showing hints for drop when the source
 * start being above the sidebar or when the application request so show
 * drop hints, but at some moment we need to restore to normal
 * state.
 * One could think that here we could simply call stop_drop_feedback,
 * but that's not true, because this function is called also before drag_drop,
 * which needs the data from the drag so we cannot free the drag data here.
 * So now one could think we could just do nothing here, and wait for
 * drag-end or drag-cancel signals and just stop_drop_feedback there. But that
 * is also not true, since when the drag comes from a different widget than the
 * sidebar, when the drag stops the last drag signal we receive is drag-leave.
 * So here what we will do is restore the state of the sidebar as if no drag
 * is being done (and if the application didn't request for permanent hints with
 * nautilus_gtk_places_sidebar_show_drop_hints) and we will free the drag data next time
 * we build new drag data in drag_data_received.
 */
static void
drag_leave_callback (GtkDropTarget *dest,
                     gpointer       user_data)
{
  NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (user_data);

  gtk_list_box_drag_unhighlight_row (GTK_LIST_BOX (sidebar->list_box));

  if (sidebar->drop_state != DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT)
    {
      update_possible_drop_targets (sidebar, FALSE);
      nautilus_gtk_sidebar_row_hide (NAUTILUS_GTK_SIDEBAR_ROW (sidebar->new_bookmark_row), FALSE);
      sidebar->drop_state = DROP_STATE_NORMAL;
    }

  g_clear_handle_id (&sidebar->hover_timer_id, g_source_remove);
  sidebar->dragging_over = FALSE;
}

static void
check_unmount_and_eject (GMount   *mount,
                         GVolume  *volume,
                         GDrive   *drive,
                         gboolean *show_unmount,
                         gboolean *show_eject)
{
  *show_unmount = FALSE;
  *show_eject = FALSE;

  if (drive != NULL)
    *show_eject = g_drive_can_eject (drive);

  if (volume != NULL)
    *show_eject |= g_volume_can_eject (volume);

  if (mount != NULL)
    {
      *show_eject |= g_mount_can_eject (mount);
      *show_unmount = g_mount_can_unmount (mount) && !*show_eject;
    }
}

static void
drive_start_from_bookmark_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar;
  GError *error;
  char *primary;
  char *name;

  sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (user_data);

  error = NULL;
  if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to start “%s”"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }
}

typedef struct {
  NautilusGtkSidebarRow *row;
  NautilusWindowSlot *window_slot; /* weak reference */
  NautilusOpenFlags open_flags;
} VolumeMountCallbackData;

static void
volume_mount_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  g_autofree VolumeMountCallbackData *callback_data = user_data;
  NautilusGtkSidebarRow *row = NAUTILUS_GTK_SIDEBAR_ROW (callback_data->row);
  NautilusGtkPlacesSidebar *sidebar;
  GVolume *volume;
  GError *error;
  char *primary;
  char *name;
  GMount *mount;

  volume = G_VOLUME (source_object);
  g_object_get (row, "sidebar", &sidebar, NULL);

  error = NULL;
  if (!g_volume_mount_finish (volume, result, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED &&
          error->code != G_IO_ERROR_ALREADY_MOUNTED)
        {
          name = g_volume_get_name (G_VOLUME (source_object));
          if (g_str_has_prefix (error->message, "Error unlocking"))
            /* Translators: This means that unlocking an encrypted storage
             * device failed. %s is the name of the device.
             */
            primary = g_strdup_printf (_("Error unlocking “%s”"), name);
          else
            primary = g_strdup_printf (_("Unable to access “%s”"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  nautilus_gtk_sidebar_row_set_busy (row, FALSE);

  mount = g_volume_get_mount (volume);
  if (mount != NULL)
    {
      GFile *location;

      location = g_mount_get_default_location (mount);
      call_open_location (sidebar, location, callback_data->window_slot, callback_data->open_flags);

      g_object_unref (G_OBJECT (location));
      g_object_unref (G_OBJECT (mount));
    }

  g_object_unref (row);
  g_object_unref (sidebar);
  g_clear_weak_pointer (&callback_data->window_slot);
}

static void
mount_volume (NautilusGtkSidebarRow *row,
              GVolume               *volume,
              NautilusOpenFlags      open_flags)
{
  NautilusGtkPlacesSidebar *sidebar;
  g_autoptr (GMountOperation) mount_op = NULL;
  VolumeMountCallbackData *callback_data = NULL;

  g_object_get (row, "sidebar", &sidebar, NULL);

  mount_op = get_mount_operation (sidebar);
  g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);

  g_object_ref (sidebar);
  callback_data = g_new0 (VolumeMountCallbackData, 1);
  g_set_weak_pointer (&callback_data->window_slot, sidebar->window_slot);
  callback_data->row = g_object_ref (row);
  callback_data->open_flags = open_flags;

  g_volume_mount (volume, 0, mount_op, NULL, volume_mount_cb, callback_data);
}

static void
open_drive (NautilusGtkSidebarRow      *row,
            GDrive             *drive,
            NautilusOpenFlags  open_flags)
{
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  if (drive != NULL &&
      (g_drive_can_start (drive) || g_drive_can_start_degraded (drive)))
    {
      GMountOperation *mount_op;

      nautilus_gtk_sidebar_row_set_busy (row, TRUE);
      mount_op = get_mount_operation (sidebar);
      g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_from_bookmark_cb, NULL);
      g_object_unref (mount_op);
    }
}

static void
open_volume (NautilusGtkSidebarRow      *row,
             GVolume            *volume,
             NautilusOpenFlags  open_flags)
{
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  if (volume != NULL)
    {
      nautilus_gtk_sidebar_row_set_busy (row, TRUE);
      mount_volume (row, volume, open_flags);
    }
}

static void
open_uri (NautilusGtkPlacesSidebar   *sidebar,
          const char         *uri,
          NautilusOpenFlags  open_flags)
{
  GFile *location;

  location = g_file_new_for_uri (uri);
  call_open_location (sidebar, location, NULL, open_flags);
  g_object_unref (location);
}

static void
open_row (NautilusGtkSidebarRow      *row,
          NautilusOpenFlags  open_flags)
{
  char *uri;
  GDrive *drive;
  GVolume *volume;
  NautilusGtkPlacesPlaceType place_type;
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "uri", &uri,
                "place-type", &place_type,
                "drive", &drive,
                "volume", &volume,
                NULL);

  if (uri != NULL)
    {
      open_uri (sidebar, uri, open_flags);
    }
  else if (volume != NULL)
    {
      open_volume (row, volume, open_flags);
    }
  else if (drive != NULL)
    {
      open_drive (row, drive, open_flags);
    }

  g_object_unref (sidebar);
  if (drive)
    g_object_unref (drive);
  if (volume)
    g_object_unref (volume);
  g_free (uri);
}

/* Callback used for the "Open" menu items in the context menu */
static void
open_shortcut_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  NautilusOpenFlags flags;

  flags = (NautilusOpenFlags)g_variant_get_int32 (parameter);
  open_row (sidebar->context_row, flags);
}

static void
rename_entry_changed (GtkEntry         *entry,
                      NautilusGtkPlacesSidebar *sidebar)
{
  NautilusGtkPlacesPlaceType type;
  char *name;
  char *uri;
  const char *new_name;
  gboolean found = FALSE;
  GtkWidget *row;

  new_name = gtk_editable_get_text (GTK_EDITABLE (sidebar->rename_entry));

  if (strcmp (new_name, "") == 0)
    {
      gtk_widget_set_sensitive (sidebar->rename_button, FALSE);
      gtk_label_set_label (GTK_LABEL (sidebar->rename_error), "");
      return;
    }

  for (row = gtk_widget_get_first_child (GTK_WIDGET (sidebar->list_box));
       row != NULL && !found;
       row = gtk_widget_get_next_sibling (row))
    {
      if (!GTK_IS_LIST_BOX_ROW (row))
        continue;

      g_object_get (row,
                    "place-type", &type,
                    "uri", &uri,
                    "label", &name,
                    NULL);

      if (type == NAUTILUS_GTK_PLACES_BOOKMARK &&
          strcmp (uri, sidebar->rename_uri) != 0 &&
          strcmp (new_name, name) == 0)
        found = TRUE;

      g_free (uri);
      g_free (name);
    }

  gtk_widget_set_sensitive (sidebar->rename_button, !found);
  gtk_label_set_label (GTK_LABEL (sidebar->rename_error),
                       found ? _("This name is already taken") : "");
}

static void
do_rename (GtkButton        *button,
           NautilusGtkPlacesSidebar *sidebar)
{
  char *new_text;
  GFile *file;
  g_autoptr (NautilusBookmark) bookmark = NULL;

  new_text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (sidebar->rename_entry)));

  file = g_file_new_for_uri (sidebar->rename_uri);
  bookmark = nautilus_bookmark_list_item_with_location (sidebar->bookmark_list, file, NULL);
  if (!bookmark)
    {
      bookmark = nautilus_bookmark_new (file, new_text);
      nautilus_bookmark_list_append (sidebar->bookmark_list, bookmark);
    }
  else
    nautilus_bookmark_set_name (g_steal_pointer (&bookmark), new_text);

  if (sidebar->rename_popover)
    {
      gtk_popover_popdown (GTK_POPOVER (sidebar->rename_popover));
    }

  g_object_unref (file);
  g_free (new_text);

  g_clear_pointer (&sidebar->rename_uri, g_free);

}

static void
on_rename_popover_destroy (GtkWidget        *rename_popover,
                           NautilusGtkPlacesSidebar *sidebar)
{
  if (sidebar)
    {
      sidebar->rename_popover = NULL;
      sidebar->rename_entry = NULL;
      sidebar->rename_button = NULL;
      sidebar->rename_error = NULL;
    }
}

static void
create_rename_popover (NautilusGtkPlacesSidebar *sidebar)
{
  GtkWidget *popover;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *error;
  char *str;

  if (sidebar->rename_popover)
    return;

  popover = gtk_popover_new ();
  gtk_widget_set_parent (popover, GTK_WIDGET (sidebar));
  /* Clean sidebar pointer when its destroyed, most of the times due to its
   * relative_to associated row being destroyed */
  g_signal_connect (popover, "destroy", G_CALLBACK (on_rename_popover_destroy), sidebar);
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_RIGHT);
  grid = gtk_grid_new ();
  gtk_popover_set_child (GTK_POPOVER (popover), grid);
  g_object_set (grid,
                "margin-start", 10,
                "margin-end", 10,
                "margin-top", 10,
                "margin-bottom", 10,
                "row-spacing", 6,
                "column-spacing", 6,
                NULL);
  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  g_signal_connect (entry, "changed", G_CALLBACK (rename_entry_changed), sidebar);
  str = g_strdup_printf ("<b>%s</b>", _("Name"));
  label = gtk_label_new (str);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  g_free (str);
  button = gtk_button_new_with_mnemonic (_("_Rename"));
  gtk_widget_add_css_class (button, "suggested-action");
  g_signal_connect (button, "clicked", G_CALLBACK (do_rename), sidebar);
  error = gtk_label_new ("");
  gtk_widget_set_halign (error, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), entry, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), button,1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), error, 0, 2, 2, 1);
  gtk_popover_set_default_widget (GTK_POPOVER (popover), button);

  sidebar->rename_popover = popover;
  sidebar->rename_entry = entry;
  sidebar->rename_button = button;
  sidebar->rename_error = error;
}

/* Style the row differently while we show a popover for it.
 * Otherwise, the popover is 'pointing to nothing'. Since the
 * main popover and the rename popover interleave their hiding
 * and showing, we have to count to ensure that we don't loose
 * the state before the last popover is gone.
 *
 * This would be nicer as a state, but reusing hover for this
 * interferes with the normal handling of this state, so just
 * use a style class.
 */
static void
update_popover_shadowing (GtkWidget *row,
                          gboolean   shown)
{
  int count;

  count = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "popover-count"));
  count = shown ? count + 1 : count - 1;
  g_object_set_data (G_OBJECT (row), "popover-count", GINT_TO_POINTER (count));

  if (count > 0)
    gtk_widget_add_css_class (row, "has-open-popup");
  else
    gtk_widget_remove_css_class (row, "has-open-popup");
}

static void
set_prelight (NautilusGtkSidebarRow *row)
{
  update_popover_shadowing (GTK_WIDGET (row), TRUE);
}

static void
unset_prelight (NautilusGtkSidebarRow *row)
{
  update_popover_shadowing (GTK_WIDGET (row), FALSE);
}

static void
setup_popover_shadowing (GtkWidget                *popover,
                         NautilusGtkPlacesSidebar *sidebar)
{
  g_signal_connect_object (popover, "map",
                           G_CALLBACK (set_prelight), sidebar->context_row,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (popover, "unmap",
                            G_CALLBACK (unset_prelight), sidebar->context_row,
                            G_CONNECT_SWAPPED);
}

static void
_popover_set_pointing_to_widget (GtkPopover *popover,
                                 GtkWidget  *target)
{
  GtkWidget *parent;
  double w, h;
  graphene_point_t target_origin;

  parent = gtk_widget_get_parent (GTK_WIDGET (popover));

  if (!gtk_widget_compute_point (target, parent,
                                 &GRAPHENE_POINT_INIT (0, 0),
                                 &target_origin))
  	return;

  w = gtk_widget_get_width (GTK_WIDGET (target));
  h = gtk_widget_get_height (GTK_WIDGET (target));

  gtk_popover_set_pointing_to (popover, &(GdkRectangle){target_origin.x, target_origin.y, w, h});
}

static void
show_rename_popover (NautilusGtkSidebarRow *row)
{
  char *name;
  char *uri;
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "label", &name,
                "uri", &uri,
                NULL);

  create_rename_popover (sidebar);

  g_set_str (&sidebar->rename_uri, uri);

  gtk_editable_set_text (GTK_EDITABLE (sidebar->rename_entry), name);

  _popover_set_pointing_to_widget (GTK_POPOVER (sidebar->rename_popover),
                                   GTK_WIDGET (row));

  setup_popover_shadowing (sidebar->rename_popover, sidebar);

  gtk_popover_popup (GTK_POPOVER (sidebar->rename_popover));
  gtk_widget_grab_focus (sidebar->rename_entry);

  g_free (name);
  g_free (uri);
  g_object_unref (sidebar);
}

static void
rename_bookmark (NautilusGtkSidebarRow *row)
{
  NautilusGtkPlacesPlaceType type;

  g_object_get (row, "place-type", &type, NULL);

  if (type != NAUTILUS_GTK_PLACES_BOOKMARK)
    return;

  show_rename_popover (row);
}

static void
rename_shortcut_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;

  rename_bookmark (sidebar->context_row);
}

static void
properties_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GList *list;
  NautilusFile *file;
  g_autofree gchar *uri = NULL;

  g_object_get (sidebar->context_row, "uri", &uri, NULL);
  file = nautilus_file_get_by_uri (uri);
  list = g_list_append (NULL, file);
  nautilus_properties_window_present (list, GTK_WIDGET (sidebar), NULL, NULL, NULL);

  nautilus_file_list_free (list);
}

static void
empty_trash_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  nautilus_file_operations_empty_trash (GTK_WIDGET (sidebar), TRUE, NULL);
}

static void
remove_bookmark (NautilusGtkSidebarRow *row)
{
  NautilusGtkPlacesPlaceType type;
  char *uri;
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "place-type", &type,
                "uri", &uri,
                NULL);

  if (type == NAUTILUS_GTK_PLACES_BOOKMARK)
    {
      nautilus_bookmark_list_delete_items_with_uri (sidebar->bookmark_list, uri);
    }

  g_free (uri);
  g_object_unref (sidebar);
}

static void
remove_shortcut_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;

  remove_bookmark (sidebar->context_row);
}

static void
mount_shortcut_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GVolume *volume;

  g_object_get (sidebar->context_row,
                "volume", &volume,
                NULL);

  if (volume != NULL)
    mount_volume (sidebar->context_row, volume, NAUTILUS_OPEN_FLAG_NORMAL);

  g_object_unref (volume);
}

static GMountOperation *
get_mount_operation (NautilusGtkPlacesSidebar *sidebar)
{
  GMountOperation *mount_op;

  mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (sidebar))));

  emit_mount_operation (sidebar, mount_op);

  return mount_op;
}

static GMountOperation *
get_unmount_operation (NautilusGtkPlacesSidebar *sidebar)
{
  GMountOperation *mount_op;

  mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (sidebar))));

  emit_unmount_operation (sidebar, mount_op);

  return mount_op;
}

static void
do_unmount (GMount           *mount,
            NautilusGtkPlacesSidebar *sidebar)
{
  if (mount != NULL)
    {
      GMountOperation *mount_op;
      GtkWindow *parent;

      mount_op = get_unmount_operation (sidebar);
      parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));
      nautilus_file_operations_unmount_mount_full (parent, mount, mount_op,
                                                   FALSE, TRUE, NULL, NULL);
      g_object_unref (mount_op);
    }
}

static void
unmount_shortcut_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GMount *mount;

  g_object_get (sidebar->context_row,
                "mount", &mount,
                NULL);

  do_unmount (mount, sidebar);

  if (mount)
    g_object_unref (mount);
}

static void
drive_stop_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar;
  GError *error;
  char *primary;
  char *name;

  sidebar = user_data;

  error = NULL;
  if (!g_drive_stop_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to stop “%s”"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
drive_eject_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar;
  GError *error;
  char *primary;
  char *name;

  sidebar = user_data;

  error = NULL;
  if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to eject “%s”"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
volume_eject_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar;
  GError *error;
  char *primary;
  char *name;

  sidebar = user_data;

  error = NULL;
  if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_volume_get_name (G_VOLUME (source_object));
          primary = g_strdup_printf (_("Unable to eject “%s”"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
do_eject (GMount           *mount,
          GVolume          *volume,
          GDrive           *drive,
          NautilusGtkPlacesSidebar *sidebar)
{
  GMountOperation *mount_op;
  GtkWindow *parent;

  mount_op = get_unmount_operation (sidebar);

  if (mount != NULL)
    {
      parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));
      nautilus_file_operations_unmount_mount_full (parent, mount, mount_op,
                                                   TRUE, TRUE, NULL, NULL);
    }
  /* This code path is probably never reached since mount always exists,
   * and if it doesn't exists we don't offer a way to eject a volume or
   * drive in the UI. Do it for defensive programming
   */
  else if (volume != NULL)
    g_volume_eject_with_operation (volume, 0, mount_op, NULL, volume_eject_cb,
                                   g_object_ref (sidebar));
  /* This code path is probably never reached since mount always exists,
   * and if it doesn't exists we don't offer a way to eject a volume or
   * drive in the UI. Do it for defensive programming
   */
  else if (drive != NULL)
    {
      if (g_drive_can_stop (drive))
        g_drive_stop (drive, 0, mount_op, NULL, drive_stop_cb,
                      g_object_ref (sidebar));
      else
        g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb,
                                      g_object_ref (sidebar));
    }
  g_object_unref (mount_op);
}

static void
eject_shortcut_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;

  g_object_get (sidebar->context_row,
                "mount", &mount,
                "volume", &volume,
                "drive", &drive,
                NULL);

  do_eject (mount, volume, drive, sidebar);

  if (mount)
    g_object_unref (mount);
  if (volume)
    g_object_unref (volume);
  if (drive)
    g_object_unref (drive);
}

static gboolean
eject_or_unmount_bookmark (NautilusGtkSidebarRow *row)
{
  gboolean can_unmount, can_eject;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;
  gboolean ret;
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row,
                "sidebar", &sidebar,
                "mount", &mount,
                "volume", &volume,
                "drive", &drive,
                NULL);
  ret = FALSE;

  check_unmount_and_eject (mount, volume, drive, &can_unmount, &can_eject);
  /* if we can eject, it has priority over unmount */
  if (can_eject)
    {
      do_eject (mount, volume, drive, sidebar);
      ret = TRUE;
    }
  else if (can_unmount)
    {
      do_unmount (mount, sidebar);
      ret = TRUE;
    }

  g_object_unref (sidebar);
  if (mount)
    g_object_unref (mount);
  if (volume)
    g_object_unref (volume);
  if (drive)
    g_object_unref (drive);

  return ret;
}

static gboolean
eject_or_unmount_selection (NautilusGtkPlacesSidebar *sidebar)
{
  gboolean ret;
  GtkListBoxRow *row;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));
  ret = eject_or_unmount_bookmark (NAUTILUS_GTK_SIDEBAR_ROW (row));

  return ret;
}

static void
drive_poll_for_media_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar;
  GError *error;
  char *primary;
  char *name;

  sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (user_data);

  error = NULL;
  if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to poll “%s” for media changes"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
rescan_shortcut_cb (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GDrive *drive;

  g_object_get (sidebar->context_row,
                "drive", &drive,
                NULL);

  if (drive != NULL)
    {
      g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, g_object_ref (sidebar));
      g_object_unref (drive);
    }
}

static void
drive_start_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar;
  GError *error;
  char *primary;
  char *name;

  sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (user_data);

  error = NULL;
  if (!g_drive_start_finish (G_DRIVE (source_object), res, &error))
    {
      if (error->code != G_IO_ERROR_FAILED_HANDLED)
        {
          name = g_drive_get_name (G_DRIVE (source_object));
          primary = g_strdup_printf (_("Unable to start “%s”"), name);
          g_free (name);
          show_error_message (sidebar, primary, error->message);
          g_free (primary);
        }
      g_error_free (error);
    }

  g_object_unref (sidebar);
}

static void
start_shortcut_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GDrive  *drive;

  g_object_get (sidebar->context_row,
                "drive", &drive,
                NULL);

  if (drive != NULL)
    {
      GMountOperation *mount_op;

      mount_op = get_mount_operation (sidebar);

      g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_cb, g_object_ref (sidebar));

      g_object_unref (mount_op);
      g_object_unref (drive);
    }
}

static void
stop_shortcut_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       data)
{
  NautilusGtkPlacesSidebar *sidebar = data;
  GDrive  *drive;

  g_object_get (sidebar->context_row,
                "drive", &drive,
                NULL);

  if (drive != NULL)
    {
      GMountOperation *mount_op;

      mount_op = get_unmount_operation (sidebar);
      g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
                    g_object_ref (sidebar));

      g_object_unref (mount_op);
      g_object_unref (drive);
    }
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                NautilusGtkPlacesSidebar      *sidebar)
{
  guint modifiers;
  GtkListBoxRow *row;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));
  if (row)
    {
      modifiers = gtk_accelerator_get_default_mod_mask ();

      if (keyval == GDK_KEY_Return ||
          keyval == GDK_KEY_KP_Enter ||
          keyval == GDK_KEY_ISO_Enter ||
          keyval == GDK_KEY_space)
        {
          NautilusOpenFlags open_flags = NAUTILUS_OPEN_FLAG_NORMAL;

          if ((state & modifiers) == GDK_SHIFT_MASK)
            open_flags = NAUTILUS_OPEN_FLAG_NEW_TAB;
          else if ((state & modifiers) == GDK_CONTROL_MASK)
            open_flags = NAUTILUS_OPEN_FLAG_NEW_WINDOW;

          open_row (NAUTILUS_GTK_SIDEBAR_ROW (row), open_flags);

          return TRUE;
        }

      if (keyval == GDK_KEY_Down &&
          (state & modifiers) == GDK_ALT_MASK)
        return eject_or_unmount_selection (sidebar);

      if ((keyval == GDK_KEY_Delete ||
           keyval == GDK_KEY_KP_Delete) &&
          (state & modifiers) == 0)
        {
          remove_bookmark (NAUTILUS_GTK_SIDEBAR_ROW (row));
          return TRUE;
        }

      if ((keyval == GDK_KEY_F2) &&
          (state & modifiers) == 0)
        {
          rename_bookmark (NAUTILUS_GTK_SIDEBAR_ROW (row));
          return TRUE;
        }

      if ((keyval == GDK_KEY_Menu) ||
          ((keyval == GDK_KEY_F10) &&
           (state & modifiers) == GDK_SHIFT_MASK))
        {
          popup_menu_cb (NAUTILUS_GTK_SIDEBAR_ROW (row));
          return TRUE;
        }
    }

  return FALSE;
}

static void
format_cb (GSimpleAction *action,
           GVariant      *variant,
           gpointer       data)
{
    NautilusGtkPlacesSidebar *sidebar = data;
    g_autoptr (GVolume) volume = NULL;
    g_autofree gchar *device_identifier = NULL;
    GVariant *parameters;

    g_object_get (sidebar->context_row, "volume", &volume, NULL);
    device_identifier = g_volume_get_identifier (volume,
                                                 G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

    parameters = g_variant_new_parsed ("(objectpath '/org/gnome/DiskUtility', @aay [], "
                                       "{'options': <{'block-device': <%s>, "
                                       "'format-device': <true> }> })", device_identifier);

    nautilus_dbus_launcher_call (nautilus_dbus_launcher_get(),
                                 NAUTILUS_DBUS_LAUNCHER_DISKS,
                                 "CommandLine",
                                 parameters,
                                 GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (sidebar))));

}

static GActionEntry entries[] = {
  { .name = "open", .activate = open_shortcut_cb, .parameter_type = "i"},
  { .name = "open-other", .activate = open_shortcut_cb, .parameter_type = "i"},
  { .name = "remove", .activate = remove_shortcut_cb},
  { .name = "rename", .activate = rename_shortcut_cb},
  { .name = "mount", .activate = mount_shortcut_cb},
  { .name = "unmount", .activate = unmount_shortcut_cb},
  { .name = "eject", .activate = eject_shortcut_cb},
  { .name = "rescan", .activate = rescan_shortcut_cb},
  { .name = "start", .activate = start_shortcut_cb},
  { .name = "stop", .activate = stop_shortcut_cb},
  { .name = "properties", .activate = properties_cb},
  { .name = "empty-trash", .activate = empty_trash_cb},
  { .name = "format", .activate = format_cb},
};

static gboolean
should_show_format_command (GVolume *volume)
{
    g_autofree gchar *unix_device_id = NULL;
    gboolean disks_available;
    g_autoptr (GFile) activation_root = NULL;

    if (volume == NULL || !G_IS_VOLUME (volume))
      return FALSE;

    /* Don't show format command for MTP and GPhoto2 devices. */
    activation_root = g_volume_get_activation_root (volume);
    if (activation_root != NULL && !g_file_is_native (activation_root))
      return FALSE;

    unix_device_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    disks_available = nautilus_dbus_launcher_is_available (nautilus_dbus_launcher_get(),
                                                           NAUTILUS_DBUS_LAUNCHER_DISKS);

    return unix_device_id != NULL && disks_available;
}

static void
on_row_popover_destroy (GtkWidget        *row_popover,
                        NautilusGtkPlacesSidebar *sidebar)
{
  if (sidebar)
    sidebar->popover = NULL;
}

#ifdef HAVE_CLOUDPROVIDERS
static void
build_popup_menu_using_gmenu (NautilusGtkSidebarRow *row)
{
  CloudProvidersAccount *cloud_provider_account;
  NautilusGtkPlacesSidebar *sidebar;
  GMenuModel *cloud_provider_menu;
  GActionGroup *cloud_provider_action_group;

  g_object_get (row,
                "sidebar", &sidebar,
                "cloud-provider-account", &cloud_provider_account,
                NULL);

  /* Cloud provider account */
  if (cloud_provider_account)
    {
      GMenu *menu = g_menu_new ();
      GMenuItem *item;
      item = g_menu_item_new (_("_Open"), "row.open");
      g_menu_item_set_action_and_target_value (item, "row.open",
                                               g_variant_new_int32 (NAUTILUS_OPEN_FLAG_NORMAL));
      g_menu_append_item (menu, item);
      g_object_unref (item);

      if (sidebar->open_flags & NAUTILUS_OPEN_FLAG_NEW_TAB)
        {
          item = g_menu_item_new (_("Open in New _Tab"), "row.open-other");
          g_menu_item_set_action_and_target_value (item, "row.open-other", g_variant_new_int32(NAUTILUS_OPEN_FLAG_NEW_TAB));
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }
      if (sidebar->open_flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW)
        {
          item = g_menu_item_new (_("Open in New _Window"), "row.open-other");
          g_menu_item_set_action_and_target_value (item, "row.open-other", g_variant_new_int32(NAUTILUS_OPEN_FLAG_NEW_WINDOW));
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }
      cloud_provider_menu = cloud_providers_account_get_menu_model (cloud_provider_account);
      cloud_provider_action_group = cloud_providers_account_get_action_group (cloud_provider_account);
      if (cloud_provider_menu != NULL && cloud_provider_action_group != NULL)
        {
          g_menu_append_section (menu, NULL, cloud_provider_menu);
          gtk_widget_insert_action_group (GTK_WIDGET (sidebar),
                                          "cloudprovider",
                                          G_ACTION_GROUP (cloud_provider_action_group));
        }
      if (sidebar->popover)
        gtk_widget_unparent (sidebar->popover);

      sidebar->popover = gtk_popover_menu_new_from_model_full (G_MENU_MODEL (menu),
                                                               GTK_POPOVER_MENU_NESTED);
      g_object_unref (menu);
      gtk_widget_set_parent (sidebar->popover, GTK_WIDGET (sidebar));
      gtk_widget_set_halign (sidebar->popover, GTK_ALIGN_START);
      gtk_popover_set_has_arrow (GTK_POPOVER (sidebar->popover), FALSE);
      g_signal_connect (sidebar->popover, "destroy",
                        G_CALLBACK (on_row_popover_destroy), sidebar);

      setup_popover_shadowing (sidebar->popover, sidebar);

      g_object_unref (sidebar);
      g_object_unref (cloud_provider_account);
    }
}
#endif

/* Constructs the popover for the sidebar row if needed */
static void
create_row_popover (NautilusGtkPlacesSidebar *sidebar,
                    NautilusGtkSidebarRow    *row)
{
  NautilusGtkPlacesPlaceType type;
  GMenu *menu, *section;
  GMenuItem *item;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;
  GAction *action;
  gboolean show_unmount, show_eject;
  gboolean show_stop;
  g_autofree gchar *uri = NULL;
  g_autoptr (GFile) file = NULL;
  gboolean show_properties;
  g_autoptr (GFile) trash = NULL;
  gboolean is_trash;
#ifdef HAVE_CLOUDPROVIDERS
  CloudProvidersAccount *cloud_provider_account;
#endif

  g_object_get (row,
                "place-type", &type,
                "drive", &drive,
                "volume", &volume,
                "mount", &mount,
                "uri", &uri,
                NULL);

  check_unmount_and_eject (mount, volume, drive, &show_unmount, &show_eject);
  if (uri != NULL)
    {
      file = g_file_new_for_uri (uri);
      trash = g_file_new_for_uri (SCHEME_TRASH ":///");
      is_trash = g_file_equal (trash, file);
      show_properties = (g_file_is_native (file) || is_trash || mount != NULL);
    }
  else
    {
      show_properties = FALSE;
      is_trash = FALSE;
    }

#ifdef HAVE_CLOUDPROVIDERS
  g_object_get (row, "cloud-provider-account", &cloud_provider_account, NULL);

  if (cloud_provider_account)
    {
      build_popup_menu_using_gmenu (row);
       return;
    }
#endif

  action = g_action_map_lookup_action (G_ACTION_MAP (sidebar->row_actions), "remove");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), (type == NAUTILUS_GTK_PLACES_BOOKMARK));
  action = g_action_map_lookup_action (G_ACTION_MAP (sidebar->row_actions), "rename");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), (type == NAUTILUS_GTK_PLACES_BOOKMARK));
  action = g_action_map_lookup_action (G_ACTION_MAP (sidebar->row_actions), "open");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (row)));
  action = g_action_map_lookup_action (G_ACTION_MAP (sidebar->row_actions), "empty-trash");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !nautilus_trash_monitor_is_empty());

  menu = g_menu_new ();
  section = g_menu_new ();

  item = g_menu_item_new (_("_Open"), "row.open");
  g_menu_item_set_action_and_target_value (item, "row.open",
                                           g_variant_new_int32 (NAUTILUS_OPEN_FLAG_NORMAL));
  g_menu_append_item (section, item);
  g_object_unref (item);

  if (sidebar->open_flags & NAUTILUS_OPEN_FLAG_NEW_TAB)
    {
      item = g_menu_item_new (_("Open in New _Tab"), "row.open-other");
      g_menu_item_set_action_and_target_value (item, "row.open-other",
                                               g_variant_new_int32 (NAUTILUS_OPEN_FLAG_NEW_TAB));
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  if (sidebar->open_flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW)
    {
      item = g_menu_item_new (_("Open in New _Window"), "row.open-other");
      g_menu_item_set_action_and_target_value (item, "row.open-other",
                                               g_variant_new_int32 (NAUTILUS_OPEN_FLAG_NEW_WINDOW));
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();

  item = g_menu_item_new (_("_Remove from Bookmarks"), "row.remove");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new (_("_Rename"), "row.rename");
  g_menu_append_item (section, item);
  g_object_unref (item);

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  if (is_trash) {
    section = g_menu_new ();
    item = g_menu_item_new (_("_Empty Trash…"), "row.empty-trash");
    g_menu_append_item (section, item);
    g_object_unref (item);

    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);
  }

  section = g_menu_new ();

  if (volume != NULL && mount == NULL &&
      g_volume_can_mount (volume))
    {
      item = g_menu_item_new (_("_Mount"), "row.mount");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  show_stop = (drive != NULL && g_drive_can_stop (drive));

  if (show_unmount && !show_stop)
    {
      item = g_menu_item_new (_("_Unmount"), "row.unmount");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  if (show_eject)
    {
      item = g_menu_item_new (_("_Eject"), "row.eject");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  if (drive != NULL &&
      g_drive_is_media_removable (drive) &&
      !g_drive_is_media_check_automatic (drive) &&
      g_drive_can_poll_for_media (drive))
    {
      item = g_menu_item_new (_("_Detect Media"), "row.rescan");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  if (drive != NULL &&
      (g_drive_can_start (drive) || g_drive_can_start_degraded (drive)))
    {
      const guint ss_type = g_drive_get_start_stop_type (drive);
      const char *start_label = _("_Start");

      if (ss_type == G_DRIVE_START_STOP_TYPE_SHUTDOWN) start_label = _("_Power On");
      else if (ss_type == G_DRIVE_START_STOP_TYPE_NETWORK) start_label = _("_Connect Drive");
      else if (ss_type == G_DRIVE_START_STOP_TYPE_MULTIDISK) start_label = _("_Start Multi-disk Device");
      else if (ss_type == G_DRIVE_START_STOP_TYPE_PASSWORD) start_label = _("_Unlock Device");

      item = g_menu_item_new (start_label, "row.start");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  if (show_stop)
    {
      const guint ss_type = g_drive_get_start_stop_type (drive);
      const char *stop_label = _("_Stop");

      if (ss_type == G_DRIVE_START_STOP_TYPE_SHUTDOWN) stop_label = _("_Safely Remove Drive");
      else if (ss_type == G_DRIVE_START_STOP_TYPE_NETWORK) stop_label = _("_Disconnect Drive");
      else if (ss_type == G_DRIVE_START_STOP_TYPE_MULTIDISK) stop_label = _("_Stop Multi-disk Device");
      else if (ss_type == G_DRIVE_START_STOP_TYPE_PASSWORD) stop_label = _("_Lock Device");

      item = g_menu_item_new (stop_label, "row.stop");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  if (should_show_format_command (volume))
    {
      item = g_menu_item_new (_("Format…"), "row.format");
      g_menu_append_item (section, item);
      g_object_unref (item);
    }

  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  if (show_properties) {
    section = g_menu_new ();
    item = g_menu_item_new (_("Properties"), "row.properties");
    g_menu_append_item (section, item);
    g_object_unref (item);

    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    g_object_unref (section);
  }

  sidebar->popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
  g_object_unref (menu);
  gtk_widget_set_parent (sidebar->popover, GTK_WIDGET (sidebar));
  gtk_widget_set_halign (sidebar->popover, GTK_ALIGN_START);
  gtk_popover_set_has_arrow (GTK_POPOVER (sidebar->popover), FALSE);
  g_signal_connect (sidebar->popover, "destroy", G_CALLBACK (on_row_popover_destroy), sidebar);

  setup_popover_shadowing (sidebar->popover, sidebar);
}

static void
show_row_popover (NautilusGtkSidebarRow *row,
                  double x,
                  double y)
{
  NautilusGtkPlacesSidebar *sidebar;
  graphene_point_t p_in_sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  g_clear_pointer (&sidebar->popover, gtk_widget_unparent);

  sidebar->context_row = row;
  create_row_popover (sidebar, row);

  if ((x == -1 && y == -1) ||
      !gtk_widget_compute_point (GTK_WIDGET (row), GTK_WIDGET (sidebar),
                                 &GRAPHENE_POINT_INIT (x, y),
                                 &p_in_sidebar))
    _popover_set_pointing_to_widget (GTK_POPOVER (sidebar->popover), GTK_WIDGET (row));
  else
    {
      gtk_popover_set_pointing_to (GTK_POPOVER (sidebar->popover),
                                   &(GdkRectangle){p_in_sidebar.x, p_in_sidebar.y, 0, 0});
    }

  gtk_popover_popup (GTK_POPOVER (sidebar->popover));

  g_object_unref (sidebar);
}

static void
on_row_activated (GtkListBox    *list_box,
                  GtkListBoxRow *row,
                  gpointer       user_data)
{
  NautilusGtkSidebarRow *selected_row;

  /* Avoid to open a location if the user is dragging. Changing the location
   * while dragging usually makes clients changing the view of the files, which
   * is confusing while the user has the attention on the drag
   */
  if (NAUTILUS_GTK_PLACES_SIDEBAR (user_data)->dragging_over)
    return;

  selected_row = NAUTILUS_GTK_SIDEBAR_ROW (gtk_list_box_get_selected_row (list_box));
  open_row (selected_row, 0);
}

static void
on_row_pressed (GtkGestureClick *gesture,
                int              n_press,
                double           x,
                double           y,
                NautilusGtkSidebarRow   *row)
{
  NautilusGtkPlacesSidebar *sidebar;
  NautilusGtkPlacesSectionType section_type;
  NautilusGtkPlacesPlaceType row_type;

  g_object_get (row,
                "sidebar", &sidebar,
                "section_type", &section_type,
                "place-type", &row_type,
                NULL);

  if (section_type == NAUTILUS_GTK_PLACES_SECTION_BOOKMARKS)
    {
      sidebar->drag_row = GTK_WIDGET (row);
      sidebar->drag_row_x = (int)x;
      sidebar->drag_row_y = (int)y;
    }

  g_object_unref (sidebar);
}

static void
on_row_released (GtkGestureClick *gesture,
                 int              n_press,
                 double           x,
                 double           y,
                 NautilusGtkSidebarRow   *row)
{
  NautilusGtkPlacesSidebar *sidebar;
  NautilusGtkPlacesSectionType section_type;
  NautilusGtkPlacesPlaceType row_type;
  guint button, state;

  g_object_get (row,
                "sidebar", &sidebar,
                "section_type", &section_type,
                "place-type", &row_type,
                NULL);

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

  if (row)
    {
      if (button == 2)
        {
          NautilusOpenFlags open_flags = NAUTILUS_OPEN_FLAG_NORMAL;

          open_flags = (state & GDK_CONTROL_MASK) ?
            NAUTILUS_OPEN_FLAG_NEW_WINDOW :
            NAUTILUS_OPEN_FLAG_NEW_TAB;

          open_row (NAUTILUS_GTK_SIDEBAR_ROW (row), open_flags);
          gtk_gesture_set_state (GTK_GESTURE (gesture),
                                 GTK_EVENT_SEQUENCE_CLAIMED);
        }
      else if (button == 3)
        {
          show_row_popover (NAUTILUS_GTK_SIDEBAR_ROW (row), x, y);
        }
    }
}

static void
on_row_dragged (GtkGestureDrag *gesture,
                double          x,
                double          y,
                NautilusGtkSidebarRow  *row)
{
  NautilusGtkPlacesSidebar *sidebar;

  g_object_get (row, "sidebar", &sidebar, NULL);

  if (sidebar->drag_row == NULL || sidebar->dragging_over)
    {
      g_object_unref (sidebar);
      return;
    }

  if (gtk_drag_check_threshold (GTK_WIDGET (row), 0, 0, x, y))
    {
      double start_x, start_y;
      GdkContentProvider *content;
      GdkSurface *surface;
      GdkDevice *device;
      GtkAllocation allocation;
      GtkWidget *drag_widget;
      GdkDrag *drag;
      graphene_point_t drag_point;

      gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);
      if (!gtk_widget_compute_point (GTK_WIDGET (row), GTK_WIDGET (sidebar),
                                     &GRAPHENE_POINT_INIT (start_x, start_y),
                                     &drag_point))
        g_return_if_reached ();

      sidebar->dragging_over = TRUE;

      content = gdk_content_provider_new_typed (NAUTILUS_TYPE_GTK_SIDEBAR_ROW, sidebar->drag_row);

      surface = gtk_native_get_surface (gtk_widget_get_native (GTK_WIDGET (sidebar)));
      device = gtk_gesture_get_device (GTK_GESTURE (gesture));

      drag = gdk_drag_begin (surface, device, content, GDK_ACTION_MOVE, drag_point.x, drag_point.y);

      g_object_unref (content);

      g_signal_connect (drag, "dnd-finished", G_CALLBACK (dnd_finished_cb), sidebar);
      g_signal_connect (drag, "cancel", G_CALLBACK (dnd_cancel_cb), sidebar);

      gtk_widget_get_allocation (sidebar->drag_row, &allocation);
      gtk_widget_set_visible (sidebar->drag_row, FALSE);

      drag_widget = GTK_WIDGET (nautilus_gtk_sidebar_row_clone (NAUTILUS_GTK_SIDEBAR_ROW (sidebar->drag_row)));
      sidebar->drag_row_height = allocation.height;
      gtk_widget_set_size_request (drag_widget, allocation.width, allocation.height);
      gtk_widget_set_opacity (drag_widget, 0.8);

      gtk_drag_icon_set_child (GTK_DRAG_ICON (gtk_drag_icon_get_for_drag (drag)), drag_widget);

      g_object_unref (drag);
    }

  g_object_unref (sidebar);
}

static void
popup_menu_cb (NautilusGtkSidebarRow *row)
{
  NautilusGtkPlacesPlaceType row_type;

  g_object_get (row, "place-type", &row_type, NULL);

  show_row_popover (row, -1, -1);
}

static void
long_press_cb (GtkGesture       *gesture,
               double            x,
               double            y,
               NautilusGtkPlacesSidebar *sidebar)
{
  GtkWidget *row;

  row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (sidebar->list_box), y));
  if (NAUTILUS_IS_GTK_SIDEBAR_ROW (row))
    popup_menu_cb (NAUTILUS_GTK_SIDEBAR_ROW (row));
}

static int
list_box_sort_func (GtkListBoxRow *row1,
                    GtkListBoxRow *row2,
                    gpointer       user_data)
{
  NautilusGtkPlacesSectionType section_type_1, section_type_2;
  NautilusGtkPlacesPlaceType place_type_1, place_type_2;
  char *label_1, *label_2;
  int index_1, index_2;
  int retval = 0;

  g_object_get (row1,
                "label", &label_1,
                "place-type", &place_type_1,
                "section-type", &section_type_1,
                "order-index", &index_1,
                NULL);
  g_object_get (row2,
                "label", &label_2,
                "place-type", &place_type_2,
                "section-type", &section_type_2,
                "order-index", &index_2,
                NULL);

  if (section_type_1 == section_type_2)
    {
      if (section_type_1 == NAUTILUS_GTK_PLACES_SECTION_MOUNTS)
        {
          if (place_type_1 == place_type_2)
            {
              retval = g_utf8_collate (label_1, label_2);
            }
          else
            {
              /* Sort internals last */
              retval = (place_type_1 == NAUTILUS_GTK_PLACES_INTERNAL_MOUNT) ? 1 : -1;
            }
        }
      else if ((place_type_1 == NAUTILUS_GTK_PLACES_BOOKMARK || place_type_2 == NAUTILUS_GTK_PLACES_DROP_FEEDBACK) &&
               (place_type_1 == NAUTILUS_GTK_PLACES_DROP_FEEDBACK || place_type_2 == NAUTILUS_GTK_PLACES_BOOKMARK))
        {
          retval = index_1 - index_2;
        }
      /* We order the bookmarks sections based on the bookmark index that we
       * set on the row as an order-index property, but we have to deal with
       * the placeholder row wanted to be between two consecutive bookmarks,
       * with two consecutive order-index values which is the usual case.
       * For that, in the list box sort func we give priority to the placeholder row,
       * that means that if the index-order is the same as another bookmark
       * the placeholder row goes before. However if we want to show it after
       * the current row, for instance when the cursor is in the lower half
       * of the row, we need to increase the order-index.
       */
      else if (place_type_1 == NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER && place_type_2 == NAUTILUS_GTK_PLACES_BOOKMARK)
        {
          if (index_1 == index_2)
            retval =  index_1 - index_2 - 1;
          else
            retval = index_1 - index_2;
        }
      else if (place_type_1 == NAUTILUS_GTK_PLACES_BOOKMARK && place_type_2 == NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER)
        {
          if (index_1 == index_2)
            retval =  index_1 - index_2 + 1;
          else
            retval = index_1 - index_2;
        }
      /* Placeholder for dropping a row comes before the "New Bookmark" row. */
      else if (place_type_1 == NAUTILUS_GTK_PLACES_DROP_FEEDBACK && place_type_2 == NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER)
        {
          retval = 1;
        }
      else if (place_type_1 == NAUTILUS_GTK_PLACES_BOOKMARK_PLACEHOLDER && place_type_2 == NAUTILUS_GTK_PLACES_DROP_FEEDBACK)
        {
          retval = -1;
        }
    }
  else
    {
      /* Order by section. That means the order in the enum of section types
       * define the actual order of them in the list */
      retval = section_type_1 - section_type_2;
    }

  g_free (label_1);
  g_free (label_2);

  return retval;
}

static void
update_hostname (NautilusGtkPlacesSidebar *sidebar)
{
  GVariant *variant;
  gsize len;
  const char *hostname;

  if (sidebar->hostnamed_proxy == NULL)
    return;

  variant = g_dbus_proxy_get_cached_property (sidebar->hostnamed_proxy,
                                              "PrettyHostname");
  if (variant == NULL)
    return;

  hostname = g_variant_get_string (variant, &len);
  if (len > 0 &&
      g_set_str (&sidebar->hostname, hostname))
    {
      update_places (sidebar);
    }

  g_variant_unref (variant);
}

static void
hostname_proxy_new_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  NautilusGtkPlacesSidebar *sidebar = user_data;
  GError *error = NULL;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  sidebar->hostnamed_proxy = proxy;
  g_clear_object (&sidebar->hostnamed_cancellable);

  if (error != NULL)
    {
      g_debug ("Failed to create D-Bus proxy: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect_swapped (sidebar->hostnamed_proxy,
                            "g-properties-changed",
                            G_CALLBACK (update_hostname),
                            sidebar);
  update_hostname (sidebar);
}

static void
create_volume_monitor (NautilusGtkPlacesSidebar *sidebar)
{
  g_assert (sidebar->volume_monitor == NULL);

  sidebar->volume_monitor = g_volume_monitor_get ();

  g_signal_connect_object (sidebar->volume_monitor, "volume_added",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "mount_added",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
  g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
                           G_CALLBACK (update_places), sidebar, G_CONNECT_SWAPPED);
}

static void
shell_shows_desktop_changed (GtkSettings *settings,
                             GParamSpec  *pspec,
                             gpointer     user_data)
{
  NautilusGtkPlacesSidebar *sidebar = user_data;
  gboolean show_desktop;

  g_assert (settings == sidebar->gtk_settings);

  g_object_get (settings, "gtk-shell-shows-desktop", &show_desktop, NULL);

  if (show_desktop != sidebar->show_desktop)
    {
      sidebar->show_desktop = show_desktop;
      update_places (sidebar);
    }
}

static void
update_location (NautilusGtkPlacesSidebar *self)
{
  GFile *location = NULL;

  if (self->window_slot != NULL &&
      !nautilus_window_slot_get_search_global (self->window_slot))
    {
      location = nautilus_window_slot_get_location (self->window_slot);
    }

  nautilus_gtk_places_sidebar_set_location (self, location);
}

static void
nautilus_gtk_places_sidebar_init (NautilusGtkPlacesSidebar *sidebar)
{
  GtkDropTarget *target;
  gboolean show_desktop;
  GtkEventController *controller;
  GtkGesture *gesture;

  create_volume_monitor (sidebar);

  sidebar->slot_signal_group = g_signal_group_new (NAUTILUS_TYPE_WINDOW_SLOT);
  g_signal_group_connect_object (sidebar->slot_signal_group, "notify::location",
                                 G_CALLBACK (update_location), sidebar,
                                 G_CONNECT_SWAPPED);
  g_signal_group_connect_object (sidebar->slot_signal_group, "notify::search-global",
                                 G_CALLBACK (update_location), sidebar,
                                 G_CONNECT_SWAPPED);

  sidebar->open_flags = NAUTILUS_OPEN_FLAG_NORMAL;

  NautilusApplication *app = NAUTILUS_APPLICATION (g_application_get_default ());
  sidebar->bookmark_list = nautilus_application_get_bookmarks (app);
  g_signal_connect_object (sidebar->bookmark_list, "changed",
                           G_CALLBACK (update_places), sidebar,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (nautilus_trash_monitor_get (), "trash-state-changed",
                           G_CALLBACK (update_trash_icon), sidebar,
                           G_CONNECT_SWAPPED);

  sidebar->swin = gtk_scrolled_window_new ();
  gtk_widget_set_parent (sidebar->swin, GTK_WIDGET (sidebar));

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar->swin),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  /* list box */
  sidebar->list_box = gtk_list_box_new ();
  gtk_widget_add_css_class (sidebar->list_box, "navigation-sidebar");

  gtk_list_box_set_header_func (GTK_LIST_BOX (sidebar->list_box),
                                list_box_header_func, sidebar, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (sidebar->list_box),
                              list_box_sort_func, NULL, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (sidebar->list_box), GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (sidebar->list_box), TRUE);

  g_signal_connect (sidebar->list_box, "row-activated",
                    G_CALLBACK (on_row_activated), sidebar);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (on_key_pressed), sidebar);
  gtk_widget_add_controller (sidebar->list_box, controller);

  gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (long_press_cb), sidebar);
  gtk_widget_add_controller (GTK_WIDGET (sidebar), GTK_EVENT_CONTROLLER (gesture));

  /* DND support */
  target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);
  gtk_drop_target_set_preload (target, TRUE);
  gtk_drop_target_set_gtypes (target, (GType[2]) { NAUTILUS_TYPE_GTK_SIDEBAR_ROW, GDK_TYPE_FILE_LIST }, 2);
  g_signal_connect (target, "enter", G_CALLBACK (drag_motion_callback), sidebar);
  g_signal_connect (target, "motion", G_CALLBACK (drag_motion_callback), sidebar);
  g_signal_connect (target, "drop", G_CALLBACK (drag_drop_callback), sidebar);
  g_signal_connect (target, "leave", G_CALLBACK (drag_leave_callback), sidebar);
  gtk_widget_add_controller (sidebar->list_box, GTK_EVENT_CONTROLLER (target));

  sidebar->drag_row = NULL;
  sidebar->row_placeholder = NULL;
  sidebar->dragging_over = FALSE;

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sidebar->swin), sidebar->list_box);

  sidebar->hostname = g_strdup (_("Device"));
  sidebar->hostnamed_cancellable = g_cancellable_new ();
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                            NULL,
                            "org.freedesktop.hostname1",
                            "/org/freedesktop/hostname1",
                            "org.freedesktop.hostname1",
                            sidebar->hostnamed_cancellable,
                            hostname_proxy_new_cb,
                            sidebar);

  sidebar->drop_state = DROP_STATE_NORMAL;

  /* Don't bother trying to trace this across hierarchy changes... */
  sidebar->gtk_settings = gtk_settings_get_default ();
  g_signal_connect (sidebar->gtk_settings, "notify::gtk-shell-shows-desktop",
                    G_CALLBACK (shell_shows_desktop_changed), sidebar);
  g_object_get (sidebar->gtk_settings, "gtk-shell-shows-desktop", &show_desktop, NULL);
  sidebar->show_desktop = show_desktop;

  /* Cloud providers */
#ifdef HAVE_CLOUDPROVIDERS
  sidebar->cloud_manager = cloud_providers_collector_dup_singleton ();
  g_signal_connect_swapped (sidebar->cloud_manager,
                            "providers-changed",
                            G_CALLBACK (update_places),
                            sidebar);
#endif

  sidebar->show_trash = TRUE;

  /* populate the sidebar */
  update_places (sidebar);

  sidebar->row_actions = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (sidebar->row_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   sidebar);
  gtk_widget_insert_action_group (GTK_WIDGET (sidebar), "row", sidebar->row_actions);

  gtk_accessible_update_property (GTK_ACCESSIBLE (sidebar),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Sidebar"),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                  _("List of common shortcuts, mountpoints, and bookmarks."), -1);
}

static void
nautilus_gtk_places_sidebar_set_property (GObject      *obj,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (obj);

  switch (property_id)
    {
    case PROP_LOCATION:
      nautilus_gtk_places_sidebar_set_location (sidebar, g_value_get_object (value));
      break;

    case PROP_OPEN_FLAGS:
      nautilus_gtk_places_sidebar_set_open_flags (sidebar, g_value_get_flags (value));
      break;

    case PROP_WINDOW_SLOT:
      if (g_set_object (&sidebar->window_slot, g_value_get_object (value)))
        {
          g_signal_group_set_target (sidebar->slot_signal_group, sidebar->window_slot);
          update_location (sidebar);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
nautilus_gtk_places_sidebar_get_property (GObject    *obj,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (obj);

  switch (property_id)
    {
    case PROP_LOCATION:
      g_value_take_object (value, nautilus_gtk_places_sidebar_get_location (sidebar));
      break;

    case PROP_OPEN_FLAGS:
      g_value_set_flags (value, nautilus_gtk_places_sidebar_get_open_flags (sidebar));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
nautilus_gtk_places_sidebar_dispose (GObject *object)
{
  NautilusGtkPlacesSidebar *sidebar;
#ifdef HAVE_CLOUDPROVIDERS
  GList *l;
#endif

  sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (object);

  g_clear_object (&sidebar->window_slot);
  g_clear_object (&sidebar->slot_signal_group);

  g_clear_pointer (&sidebar->popover, gtk_widget_unparent);

  if (sidebar->rename_popover)
    {
      gtk_widget_unparent (sidebar->rename_popover);
      sidebar->rename_popover = NULL;
      sidebar->rename_entry = NULL;
      sidebar->rename_button = NULL;
      sidebar->rename_error = NULL;
    }

  if (sidebar->trash_row)
    {
      g_object_remove_weak_pointer (G_OBJECT (sidebar->trash_row),
                                    (gpointer *) &sidebar->trash_row);
      sidebar->trash_row = NULL;
    }

  if (sidebar->volume_monitor != NULL)
    {
      g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
                                            update_places, sidebar);
      g_clear_object (&sidebar->volume_monitor);
    }

  if (sidebar->hostnamed_cancellable != NULL)
    {
      g_cancellable_cancel (sidebar->hostnamed_cancellable);
      g_clear_object (&sidebar->hostnamed_cancellable);
    }

  g_clear_object (&sidebar->hostnamed_proxy);
  g_free (sidebar->hostname);
  sidebar->hostname = NULL;

  if (sidebar->gtk_settings)
    {
      g_signal_handlers_disconnect_by_func (sidebar->gtk_settings, shell_shows_desktop_changed, sidebar);
      sidebar->gtk_settings = NULL;
    }

  g_clear_object (&sidebar->current_location);
  g_clear_pointer (&sidebar->rename_uri, g_free);

  g_clear_handle_id (&sidebar->hover_timer_id, g_source_remove);

#ifdef HAVE_CLOUDPROVIDERS
  for (l = sidebar->unready_accounts; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (l->data, sidebar);
    }
  g_list_free_full (sidebar->unready_accounts, g_object_unref);
  sidebar->unready_accounts = NULL;
  if (sidebar->cloud_manager)
    {
      g_signal_handlers_disconnect_by_data (sidebar->cloud_manager, sidebar);
      for (l = cloud_providers_collector_get_providers (sidebar->cloud_manager);
           l != NULL; l = l->next)
        {
          g_signal_handlers_disconnect_by_data (l->data, sidebar);
        }
      g_object_unref (sidebar->cloud_manager);
      sidebar->cloud_manager = NULL;
    }
#endif

  G_OBJECT_CLASS (nautilus_gtk_places_sidebar_parent_class)->dispose (object);
}

static void
nautilus_gtk_places_sidebar_finalize (GObject *object)
{
  NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (object);

  g_clear_object (&sidebar->row_actions);

  g_clear_pointer (&sidebar->swin, gtk_widget_unparent);

  G_OBJECT_CLASS (nautilus_gtk_places_sidebar_parent_class)->finalize (object);
}

static void
nautilus_gtk_places_sidebar_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (widget);

  gtk_widget_measure (sidebar->swin,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
nautilus_gtk_places_sidebar_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (widget);

  gtk_widget_size_allocate (sidebar->swin,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);

  if (sidebar->popover)
    gtk_popover_present (GTK_POPOVER (sidebar->popover));

  if (sidebar->rename_popover)
    gtk_popover_present (GTK_POPOVER (sidebar->rename_popover));
}

static void
nautilus_gtk_places_sidebar_class_init (NautilusGtkPlacesSidebarClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);


  gobject_class->dispose = nautilus_gtk_places_sidebar_dispose;
  gobject_class->finalize = nautilus_gtk_places_sidebar_finalize;
  gobject_class->set_property = nautilus_gtk_places_sidebar_set_property;
  gobject_class->get_property = nautilus_gtk_places_sidebar_get_property;

  widget_class->measure = nautilus_gtk_places_sidebar_measure;
  widget_class->size_allocate = nautilus_gtk_places_sidebar_size_allocate;

  /*
   * NautilusGtkPlacesSidebar::drag-action-requested:
   * @sidebar: the object which received the signal.
   * @drop: (type Gdk.Drop): GdkDrop with information about the drag operation
   * @dest_file: (type Gio.File): GFile with the tentative location that is being hovered for a drop
   * @source_file_list: (type GLib.SList) (element-type GFile) (transfer none):
   *   List of GFile that are being dragged
   *
   * When the user starts a drag-and-drop operation and the sidebar needs
   * to ask the application for which drag action to perform, then the
   * sidebar will emit this signal.
   *
   * The application can evaluate the @context for customary actions, or
   * it can check the type of the files indicated by @source_file_list against the
   * possible actions for the destination @dest_file.
   *
   * The drag action to use must be the return value of the signal handler.
   *
   * Returns: The drag action to use, for example, GDK_ACTION_COPY
   * or GDK_ACTION_MOVE, or 0 if no action is allowed here (i.e. drops
   * are not allowed in the specified @dest_file).
   */
  places_sidebar_signals [DRAG_ACTION_REQUESTED] =
          g_signal_new ("drag-action-requested",
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET (NautilusGtkPlacesSidebarClass, drag_action_requested),
                        NULL, NULL,
                        NULL,
                        GDK_TYPE_DRAG_ACTION, 2,
                        G_TYPE_OBJECT,
                        GDK_TYPE_FILE_LIST);

  /*
   * NautilusGtkPlacesSidebar::drag-action-ask:
   * @sidebar: the object which received the signal.
   * @actions: Possible drag actions that need to be asked for.
   *
   * The places sidebar emits this signal when it needs to ask the application
   * to pop up a menu to ask the user for which drag action to perform.
   *
   * Returns: the final drag action that the sidebar should pass to the drag side
   * of the drag-and-drop operation.
   */
  places_sidebar_signals [DRAG_ACTION_ASK] =
          g_signal_new ("drag-action-ask",
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET (NautilusGtkPlacesSidebarClass, drag_action_ask),
                        NULL, NULL,
                        NULL,
                        GDK_TYPE_DRAG_ACTION, 1,
                        GDK_TYPE_DRAG_ACTION);

  /*
   * NautilusGtkPlacesSidebar::drag-perform-drop:
   * @sidebar: the object which received the signal.
   * @dest_file: (type Gio.File): Destination GFile.
   * @source_file_list: (type GLib.SList) (element-type GFile) (transfer none):
   *   GSList of GFile that got dropped.
   * @action: Drop action to perform.
   *
   * The places sidebar emits this signal when the user completes a
   * drag-and-drop operation and one of the sidebar's items is the
   * destination.  This item is in the @dest_file, and the
   * @source_file_list has the list of files that are dropped into it and
   * which should be copied/moved/etc. based on the specified @action.
   */
  places_sidebar_signals [DRAG_PERFORM_DROP] =
          g_signal_new ("drag-perform-drop",
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (NautilusGtkPlacesSidebarClass, drag_perform_drop),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE, 3,
                        G_TYPE_OBJECT,
                        GDK_TYPE_FILE_LIST,
                        GDK_TYPE_DRAG_ACTION);

  /*
   * NautilusGtkPlacesSidebar::mount:
   * @sidebar: the object which received the signal.
   * @mount_operation: the GMountOperation that is going to start.
   *
   * The places sidebar emits this signal when it starts a new operation
   * because the user clicked on some location that needs mounting.
   * In this way the application using the NautilusGtkPlacesSidebar can track the
   * progress of the operation and, for example, show a notification.
   */
  places_sidebar_signals [MOUNT] =
          g_signal_new ("mount",
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (NautilusGtkPlacesSidebarClass, mount),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE,
                        1,
                        G_TYPE_MOUNT_OPERATION);
  /*
   * NautilusGtkPlacesSidebar::unmount:
   * @sidebar: the object which received the signal.
   * @mount_operation: the GMountOperation that is going to start.
   *
   * The places sidebar emits this signal when it starts a new operation
   * because the user for example ejected some drive or unmounted a mount.
   * In this way the application using the NautilusGtkPlacesSidebar can track the
   * progress of the operation and, for example, show a notification.
   */
  places_sidebar_signals [UNMOUNT] =
          g_signal_new ("unmount",
                        G_OBJECT_CLASS_TYPE (gobject_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (NautilusGtkPlacesSidebarClass, unmount),
                        NULL, NULL,
                        NULL,
                        G_TYPE_NONE,
                        1,
                        G_TYPE_MOUNT_OPERATION);

  properties[PROP_LOCATION] =
          g_param_spec_object ("location",
                               "Location to Select",
                               "The location to highlight in the sidebar",
                               G_TYPE_FILE,
                               G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB);
  properties[PROP_OPEN_FLAGS] =
          g_param_spec_flags ("open-flags",
                              "Open Flags",
                              "Modes in which the calling app can open locations selected in the sidebar",
                              NAUTILUS_TYPE_OPEN_FLAGS,
                              NAUTILUS_OPEN_FLAG_NORMAL,
                              G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB);
  properties[PROP_WINDOW_SLOT] =
          g_param_spec_object ("window-slot", NULL, NULL,
                               NAUTILUS_TYPE_WINDOW_SLOT,
                               G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_css_name (widget_class, "placessidebar");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LIST);
}

/*
 * nautilus_gtk_places_sidebar_new:
 *
 * Creates a new NautilusGtkPlacesSidebar widget.
 *
 * The application should connect to at least the
 * NautilusGtkPlacesSidebar::open-location signal to be notified
 * when the user makes a selection in the sidebar.
 *
 * Returns: a newly created NautilusGtkPlacesSidebar
 */
GtkWidget *
nautilus_gtk_places_sidebar_new (void)
{
  return GTK_WIDGET (g_object_new (nautilus_gtk_places_sidebar_get_type (), NULL));
}

/*
 * nautilus_gtk_places_sidebar_set_open_flags:
 * @sidebar: a places sidebar
 * @flags: Bitmask of modes in which the calling application can open locations
 *
 * Sets the way in which the calling application can open new locations from
 * the places sidebar.  For example, some applications only open locations
 * “directly” into their main view, while others may support opening locations
 * in a new notebook tab or a new window.
 *
 * This function is used to tell the places @sidebar about the ways in which the
 * application can open new locations, so that the sidebar can display (or not)
 * the “Open in new tab” and “Open in new window” menu items as appropriate.
 *
 * When the NautilusGtkPlacesSidebar::open-location signal is emitted, its flags
 * argument will be set to one of the @flags that was passed in
 * nautilus_gtk_places_sidebar_set_open_flags().
 *
 * Passing 0 for @flags will cause NAUTILUS_OPEN_FLAG_NORMAL to always be sent
 * to callbacks for the “open-location” signal.
 */
void
nautilus_gtk_places_sidebar_set_open_flags (NautilusGtkPlacesSidebar   *sidebar,
                                   NautilusOpenFlags  flags)
{
  g_return_if_fail (NAUTILUS_IS_GTK_PLACES_SIDEBAR (sidebar));

  if (sidebar->open_flags != flags)
    {
      sidebar->open_flags = flags;
      g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_OPEN_FLAGS]);
    }
}

/*
 * nautilus_gtk_places_sidebar_get_open_flags:
 * @sidebar: a NautilusGtkPlacesSidebar
 *
 * Gets the open flags.
 *
 * Returns: the NautilusOpenFlags of @sidebar
 */
NautilusOpenFlags
nautilus_gtk_places_sidebar_get_open_flags (NautilusGtkPlacesSidebar *sidebar)
{
  g_return_val_if_fail (NAUTILUS_IS_GTK_PLACES_SIDEBAR (sidebar), 0);

  return sidebar->open_flags;
}

/*
 * nautilus_gtk_places_sidebar_set_location:
 * @sidebar: a places sidebar
 * @location: (nullable): location to select, or %NULL for no current path
 *
 * Sets the location that is being shown in the widgets surrounding the
 * @sidebar, for example, in a folder view in a file manager.  In turn, the
 * @sidebar will highlight that location if it is being shown in the list of
 * places, or it will unhighlight everything if the @location is not among the
 * places in the list.
 */
void
nautilus_gtk_places_sidebar_set_location (NautilusGtkPlacesSidebar *sidebar,
                                 GFile            *location)
{
  GtkWidget *row;
  char *row_uri;
  char *uri;
  gboolean found = FALSE;

  g_return_if_fail (NAUTILUS_IS_GTK_PLACES_SIDEBAR (sidebar));

  gtk_list_box_unselect_all (GTK_LIST_BOX (sidebar->list_box));

  g_set_object (&sidebar->current_location, location);

  if (location == NULL)
    goto out;

  uri = g_file_get_uri (location);

  for (row = gtk_widget_get_first_child (GTK_WIDGET (sidebar->list_box));
       row != NULL && !found;
       row = gtk_widget_get_next_sibling (row))
    {
      if (!GTK_IS_LIST_BOX_ROW (row))
        continue;

      g_object_get (row, "uri", &row_uri, NULL);
      if (row_uri != NULL && g_strcmp0 (row_uri, uri) == 0)
        {
          gtk_list_box_select_row (GTK_LIST_BOX (sidebar->list_box),
                                   GTK_LIST_BOX_ROW (row));
          found = TRUE;
        }

      g_free (row_uri);
    }

  g_free (uri);

 out:
  g_object_notify_by_pspec (G_OBJECT (sidebar), properties[PROP_LOCATION]);
}

/*
 * nautilus_gtk_places_sidebar_get_location:
 * @sidebar: a places sidebar
 *
 * Gets the currently selected location in the @sidebar. This can be %NULL when
 * nothing is selected, for example, when nautilus_gtk_places_sidebar_set_location() has
 * been called with a location that is not among the sidebar’s list of places to
 * show.
 *
 * You can use this function to get the selection in the @sidebar.
 *
 * Returns: (nullable) (transfer full): a GFile with the selected location, or
 * %NULL if nothing is visually selected.
 */
GFile *
nautilus_gtk_places_sidebar_get_location (NautilusGtkPlacesSidebar *sidebar)
{
  GtkListBoxRow *selected;
  GFile *file;

  g_return_val_if_fail (sidebar != NULL, NULL);

  file = NULL;
  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));

  if (selected)
    {
      char *uri;

      g_object_get (selected, "uri", &uri, NULL);
      file = g_file_new_for_uri (uri);
      g_free (uri);
    }

  return file;
}

char *
nautilus_gtk_places_sidebar_get_location_title (NautilusGtkPlacesSidebar *sidebar)
{
  GtkListBoxRow *selected;
  char *title;

  g_return_val_if_fail (sidebar != NULL, NULL);

  title = NULL;
  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (sidebar->list_box));

  if (selected)
    g_object_get (selected, "label", &title, NULL);

  return title;
}

/*
 * nautilus_gtk_places_sidebar_get_nth_bookmark:
 * @sidebar: a places sidebar
 * @n: index of the bookmark to query
 *
 * This function queries the bookmarks added by the user to the places sidebar,
 * and returns one of them.  This function is used by GtkFileChooser to implement
 * the “Alt-1”, “Alt-2”, etc. shortcuts, which activate the corresponding bookmark.
 *
 * Returns: (nullable) (transfer full): The bookmark specified by the index @n, or
 * %NULL if no such index exist.  Note that the indices start at 0, even though
 * the file chooser starts them with the keyboard shortcut "Alt-1".
 */
GFile *
nautilus_gtk_places_sidebar_get_nth_bookmark (NautilusGtkPlacesSidebar *sidebar,
                                     int               n)
{
  GtkWidget *row;
  int k;
  GFile *file;

  g_return_val_if_fail (NAUTILUS_IS_GTK_PLACES_SIDEBAR (sidebar), NULL);

  file = NULL;
  k = 0;
  for (row = gtk_widget_get_first_child (GTK_WIDGET (sidebar->list_box));
       row != NULL;
       row = gtk_widget_get_next_sibling (row))
    {
      NautilusGtkPlacesPlaceType place_type;
      char *uri;

      if (!GTK_IS_LIST_BOX_ROW (row))
        continue;

      g_object_get (row,
                    "place-type", &place_type,
                    "uri", &uri,
                    NULL);
      if (place_type == NAUTILUS_GTK_PLACES_BOOKMARK)
        {
          if (k == n)
            {
              file = g_file_new_for_uri (uri);
              g_free (uri);
              break;
            }
          k++;
        }
      g_free (uri);
    }

  return file;
}

/*
 * nautilus_gtk_places_sidebar_set_drop_targets_visible:
 * @sidebar: a places sidebar.
 * @visible: whether to show the valid targets or not.
 *
 * Make the NautilusGtkPlacesSidebar show drop targets, so it can show the available
 * drop targets and a "new bookmark" row. This improves the Drag-and-Drop
 * experience of the user and allows applications to show all available
 * drop targets at once.
 *
 * This needs to be called when the application is aware of an ongoing drag
 * that might target the sidebar. The drop-targets-visible state will be unset
 * automatically if the drag finishes in the NautilusGtkPlacesSidebar. You only need
 * to unset the state when the drag ends on some other widget on your application.
 */
void
nautilus_gtk_places_sidebar_set_drop_targets_visible (NautilusGtkPlacesSidebar *sidebar,
                                             gboolean          visible)
{
  g_return_if_fail (NAUTILUS_IS_GTK_PLACES_SIDEBAR (sidebar));

  if (visible)
    {
      sidebar->drop_state = DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT;
      start_drop_feedback (sidebar, NULL);
    }
  else
    {
      if (sidebar->drop_state == DROP_STATE_NEW_BOOKMARK_ARMED_PERMANENT ||
          sidebar->drop_state == DROP_STATE_NEW_BOOKMARK_ARMED)
        {
          if (!sidebar->dragging_over)
            {
              sidebar->drop_state = DROP_STATE_NORMAL;
              stop_drop_feedback (sidebar);
            }
          else
            {
              /* In case this is called while we are dragging we need to mark the
               * drop state as no permanent so the leave timeout can do its job.
               * This will only happen in applications that call this in a wrong
               * time */
              sidebar->drop_state = DROP_STATE_NEW_BOOKMARK_ARMED;
            }
        }
    }
}
