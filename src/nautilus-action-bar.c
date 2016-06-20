/* nautilus-action-bar.c
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#include "nautilus-action-bar.h"
#include "nautilus-clipboard.h"
#include "nautilus-clipboard-monitor.h"
#include "nautilus-file.h"
#include "nautilus-mime-actions.h"
#include "nautilus-previewer.h"

#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#define               UPDATE_STATUS_TIMEOUT  200 //ms

typedef enum
{
  MODE_NO_SELECTION,
  MODE_FILES_ONLY,
  MODE_FOLDERS_ONLY,
  MODE_MIXED
} ActionBarMode;

struct _NautilusActionBar
{
  GtkFrame            parent;

  GtkWidget          *loading_label;
  GtkWidget          *stack;

  /* No selection buttons */
  GtkWidget          *new_folder_0_button;
  GtkWidget          *paste_button;
  GtkWidget          *select_all_button;
  GtkWidget          *no_selection_overflow_button;

  /* Folders buttons */
  GtkWidget          *open_file_box;
  GtkWidget          *open_folders_button;
  GtkWidget          *move_folders_button;
  GtkWidget          *move_trash_folders_button;
  GtkWidget          *copy_folders_button;
  GtkWidget          *rename_folders_button;
  GtkWidget          *properties_folders_button;
  GtkWidget          *folders_overflow_button;

  GtkWidget          *no_selection_widgets    [3];
  GtkWidget          *files_folders_widgets   [5];

  /* Labels */
  GtkWidget          *selection_label;
  GtkWidget          *size_label;

  /* Default app widgets */
  GtkWidget          *default_app_button;
  GtkWidget          *default_app_icon;
  GtkWidget          *default_app_label;

  NautilusView       *view;
  ActionBarMode       mode;
  gint                update_status_timeout_id;
};

G_DEFINE_TYPE (NautilusActionBar, nautilus_action_bar, GTK_TYPE_FRAME)

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

static void
update_paste_button (NautilusActionBar *self)
{
  NautilusClipboardMonitor *monitor;
  NautilusClipboardInfo *info;

  monitor = nautilus_clipboard_monitor_get ();
  info = nautilus_clipboard_monitor_get_clipboard_info (monitor);

  gtk_widget_set_visible (self->paste_button, info != NULL);

  if (info)
    {
      gchar *label;
      gint length;

      length = g_list_length (info->files);

      if (info->cut)
        label = g_strdup_printf (g_dngettext(NULL, "Move %d file", "Move %d files", length), length);
      else
        label = g_strdup_printf (g_dngettext(NULL, "Paste %d file", "Paste %d files", length), length);

      gtk_button_set_label (GTK_BUTTON (self->paste_button), label);

      g_free (label);
    }
}

static void
setup_selection_label (NautilusActionBar *self)
{
  NautilusFile *file;
  goffset total_size;
  gboolean total_size_known;
  guint non_folder_count, folder_count, folder_item_count;
  gboolean folder_item_count_known;
  guint file_item_count;
  GList *selection;
  GList *p;
  gchar *first_item_name;
  gchar *non_folder_count_str;
  gchar *total_size_string;
  gchar *folder_count_str;
  gchar *folder_item_count_str;
  gchar *status;

  selection = nautilus_view_get_selection (self->view);

  folder_item_count_known = TRUE;
  folder_count = 0;
  folder_item_count = 0;
  non_folder_count = 0;
  total_size_known = FALSE;
  first_item_name = NULL;
  folder_count_str = NULL;
  folder_item_count_str = NULL;
  non_folder_count_str = NULL;
  total_size_string = NULL;
  total_size = 0;

  for (p = selection; p != NULL; p = p->next)
    {
      file = p->data;

      if (!nautilus_file_can_get_size (file))
        {
          total_size_known = TRUE;
          total_size += nautilus_file_get_size (file);
        }

      if (nautilus_file_is_directory (file))
        {
          folder_count++;

          if (nautilus_file_get_directory_item_count (file, &file_item_count, NULL))
            folder_item_count += file_item_count;
          else
            folder_item_count_known = FALSE;
        }
      else
        {
          non_folder_count++;
        }

      if (first_item_name == NULL)
        first_item_name = nautilus_file_get_display_name (file);
    }

  /*
   * Break out cases for localization's sake. But note that there are still pieces
   * being assembled in a particular order, which may be a problem for some localizers.
   */
  if (folder_count != 0)
    {
      if (folder_count == 1 && non_folder_count == 0)
        {
          folder_count_str = g_strdup_printf (_("“%s” selected"), first_item_name);
        }
      else
        {
          folder_count_str = g_strdup_printf (ngettext("%'d folder selected",
                                                       "%'d folders selected",
                                                       folder_count),
                                              folder_count);
        }

      if (folder_count == 1)
        {
          if (!folder_item_count_known)
            folder_item_count_str = g_strdup ("");
          else
            folder_item_count_str = g_strdup_printf (ngettext("(containing %'d item)", "(containing %'d items)", folder_item_count),
                                                     folder_item_count);
        }
      else
        {
          if (!folder_item_count_known)
            {
              folder_item_count_str = g_strdup ("");
            }
          else
            {
              /* translators: this is preceded with a string of form 'N folders' (N more than 1) */
              folder_item_count_str = g_strdup_printf (ngettext("(containing a total of %'d item)",
                                                                "(containing a total of %'d items)",
                                                                folder_item_count),
                                                       folder_item_count);
            }
        }
    }

  if (non_folder_count != 0)
    {
      if (folder_count == 0)
        {
          if (non_folder_count == 1) {
                  non_folder_count_str = g_strdup_printf (_("“%s” selected"), first_item_name);
          } else {
                  non_folder_count_str = g_strdup_printf (ngettext("%'d item selected",
                                                                   "%'d items selected",
                                                                   non_folder_count),
                                                          non_folder_count);
          }
       }
      else
        {
          /* Folders selected also, use "other" terminology */
          non_folder_count_str = g_strdup_printf (ngettext("%'d other item selected",
                                                           "%'d other items selected",
                                                           non_folder_count),
                                                  non_folder_count);
        }
    }

  if (total_size_known)
    total_size_string = g_format_size (total_size);

  if (folder_count == 0 && non_folder_count == 0)
    {
      status = NULL;
    }
  else if (folder_count == 0)
    {
      status = g_strdup_printf ("%s", non_folder_count_str);
    }
  else if (non_folder_count == 0)
    {
      status = g_strdup_printf ("%s %s", folder_count_str, folder_item_count_str);
    }
  else
    {
      /* This is marked for translation in case a localizer
       * needs to change ", " to something else. The comma
       * is between the message about the number of folders
       * and the number of items in those folders and the
       * message about the number of other items and the
       * total size of those items.
       */
      status = g_strdup_printf (_("%s %s, %s"),
                                folder_count_str,
                                folder_item_count_str,
                                non_folder_count_str);
    }

  gtk_label_set_label (GTK_LABEL (self->selection_label), status);
  gtk_label_set_label (GTK_LABEL (self->size_label), total_size_string);

  g_free (first_item_name);
  g_free (folder_count_str);
  g_free (folder_item_count_str);
  g_free (non_folder_count_str);
  g_free (total_size_string);
  g_free (status);
}

static void
setup_file_button (NautilusActionBar *self)
{
  GAppInfo *app;
  GIcon *icon;
  GList *selection, *l;
  gchar *label;
  gboolean show_app, show_run;

  selection = nautilus_view_get_selection (self->view);
  app = nautilus_mime_get_default_application_for_files (selection);
  show_app = show_run = g_list_length (selection) > 0;
  icon = NULL;

  for (l = selection; l != NULL; l = l->next)
    {
      NautilusFile *file;

      file = NAUTILUS_FILE (selection->data);

      if (!nautilus_mime_file_opens_in_external_app (file))
        show_app = FALSE;

      if (!nautilus_mime_file_launches (file))
        show_run = FALSE;

      if (!show_app && !show_run)
        break;
    }

  if (app)
    {
      label = g_strdup_printf (_("Open With %s"), g_app_info_get_name (app));
      icon = g_app_info_get_icon (app);

      if (icon)
        g_object_ref (icon);

      g_clear_object (&app);
    }
  else if (show_run)
    {
      label = g_strdup (_("Run"));
    }
  else
    {
      label = g_strdup (_("Open"));
    }

  /*
   * Change the visibility of the icon widget because otherwise the box
   * will calculate the spacing considering it.
   */
  gtk_widget_set_visible (self->default_app_icon, icon != NULL);

  gtk_image_set_from_gicon (GTK_IMAGE (self->default_app_icon), icon, GTK_ICON_SIZE_MENU);
  gtk_label_set_label (GTK_LABEL (self->default_app_label), label);

  g_free (label);
}

static void
set_internal_mode (NautilusActionBar *self,
                   ActionBarMode      mode)
{
  self->mode = mode;

  switch (mode)
    {
    case MODE_NO_SELECTION:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "no-selection");
      break;

    case MODE_FILES_ONLY:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "files-folders");
      setup_selection_label (self);
      setup_file_button (self);
      break;

    case MODE_FOLDERS_ONLY:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "files-folders");
      setup_selection_label (self);
      break;

    case MODE_MIXED:
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "files-folders");
      setup_selection_label (self);
      break;
    }

  gtk_widget_queue_resize (self->stack);
}

static gboolean
real_update_status (gpointer data)
{
  NautilusActionBar *actionbar = data;

  if (nautilus_view_is_loading (actionbar->view))
    {
      gtk_label_set_label (GTK_LABEL (actionbar->loading_label),
                           nautilus_view_is_searching (actionbar->view) ? _("Searching") : _("Loading"));

      gtk_stack_set_visible_child_name (GTK_STACK (actionbar->stack), "loading");
    }
  else
    {
      GList *selection, *l;
      gint number_of_files, number_of_folders;

      selection = nautilus_view_get_selection (actionbar->view);
      number_of_files = number_of_folders = 0;

      /* Count the number of selected files and folders */
      for (l = selection; l != NULL; l = l->next)
        {
          if (nautilus_file_is_directory (l->data))
            number_of_folders++;
          else
            number_of_files++;
        }

      if (number_of_files > 0 && number_of_folders > 0)
        set_internal_mode (actionbar, MODE_MIXED);
      else if (number_of_files > 0)
        set_internal_mode (actionbar, MODE_FILES_ONLY);
      else if (number_of_folders > 0)
        set_internal_mode (actionbar, MODE_FOLDERS_ONLY);
      else
        set_internal_mode (actionbar, MODE_NO_SELECTION);

      nautilus_file_list_free (selection);
    }

  actionbar->update_status_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
update_status (NautilusActionBar *actionbar)
{
  if (actionbar->update_status_timeout_id > 0)
    {
      g_source_remove (actionbar->update_status_timeout_id);
      actionbar->update_status_timeout_id = 0;
    }

  actionbar->update_status_timeout_id = g_timeout_add (UPDATE_STATUS_TIMEOUT,
                                                       real_update_status,
                                                       actionbar);
}

static void
clear_selection_cb (NautilusActionBar *self)
{
  nautilus_view_set_selection (self->view, NULL);
}

static void
nautilus_action_bar_finalize (GObject *object)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  if (self->update_status_timeout_id > 0)
    {
      g_source_remove (self->update_status_timeout_id);
      self->update_status_timeout_id = 0;
    }

  g_signal_handlers_disconnect_by_func (nautilus_clipboard_monitor_get (), update_paste_button, self);
  g_signal_handlers_disconnect_by_func (self->view, update_status, self);

  g_clear_object (&self->view);

  G_OBJECT_CLASS (nautilus_action_bar_parent_class)->finalize (object);
}

static void
nautilus_action_bar_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_action_bar_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      if (g_set_object (&self->view, g_value_get_object (value)))
        {
          g_signal_connect_swapped (self->view, "notify::selection", G_CALLBACK (update_status), self);
          g_signal_connect_swapped (self->view, "notify::is-loading", G_CALLBACK (update_status), self);
          g_signal_connect_swapped (self->view, "notify::is-searching", G_CALLBACK (update_status), self);
          g_object_notify (object, "view");
        }

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_visible_buttons (GtkWidget **widgets,
                     GtkWidget  *overflow,
                     gint        length,
                     gint        visible_items)
{
  gint i;

  gtk_widget_set_visible (overflow, visible_items < length);

  for (i = 0; i < length; i++)
    gtk_widget_set_visible (widgets[i], i < visible_items);
}

static void
nautilus_action_bar_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  NautilusActionBar *self;
  GtkWidget **widgets;
  GtkWidget *overflow_button, *reference_button;
  gint visible_items, button_width, overflow_button_width, static_button_width;
  gint max_width, max_items;

  self = NAUTILUS_ACTION_BAR (widget);
  max_width = 2 * allocation->width / 3 - 2 * gtk_container_get_border_width (GTK_CONTAINER (self->stack));
  reference_button = overflow_button = NULL;
  static_button_width = 0;
  max_items = 5;
  widgets = NULL;

  switch (self->mode)
    {
    case MODE_NO_SELECTION:
      overflow_button = self->no_selection_overflow_button;
      reference_button = self->new_folder_0_button;
      widgets = self->no_selection_widgets;
      max_items = 3;
      break;

    case MODE_FILES_ONLY:
      overflow_button = self->folders_overflow_button;
      reference_button = self->move_folders_button;
      widgets = self->files_folders_widgets;

      gtk_widget_get_preferred_width (self->open_file_box, &static_button_width, NULL);

      gtk_widget_show (self->open_file_box);
      gtk_widget_hide (self->open_folders_button);
      break;

    case MODE_FOLDERS_ONLY:
      overflow_button = self->folders_overflow_button;
      reference_button = self->move_folders_button;
      widgets = self->files_folders_widgets;

      gtk_widget_get_preferred_width (self->open_folders_button, &static_button_width, NULL);

      gtk_widget_hide (self->open_file_box);
      gtk_widget_show (self->open_folders_button);
      break;

    case MODE_MIXED:
      overflow_button = self->folders_overflow_button;
      reference_button = self->move_folders_button;
      widgets = self->files_folders_widgets;

      gtk_widget_hide (self->open_file_box);
      gtk_widget_hide (self->open_folders_button);
      break;
    }

  gtk_widget_get_preferred_width (reference_button, &button_width, NULL);
  gtk_widget_get_preferred_width (overflow_button, &overflow_button_width, NULL);

  /*
   * When in the files-only mode and selecting 2+ files with different
   * types, the "Open" label shrinks way too much. Because of that, we
   * have to setup a reasonable size request here.
   */
  if (self->mode == MODE_FILES_ONLY &&
      static_button_width < button_width)
    {
      static_button_width = button_width;

      gtk_widget_set_size_request (self->default_app_button, button_width, -1);
    }

  /* Number of visible widgets */
  visible_items = CLAMP ((max_width - overflow_button_width - static_button_width) / MAX (button_width, 1), 0, max_items);

  if (visible_items > 0)
    set_visible_buttons (widgets, overflow_button, max_items, visible_items);

  /* Let GtkBox allocate and position the widgets */
  GTK_WIDGET_CLASS (nautilus_action_bar_parent_class)->size_allocate (widget, allocation);
}

static void
nautilus_action_bar_get_preferred_width (GtkWidget *widget,
                                         gint      *minimum,
                                         gint      *natural)
{
  NautilusActionBar *self;
  gint button_width;

  self = NAUTILUS_ACTION_BAR (widget);

  switch (self->mode)
    {
    case MODE_NO_SELECTION:
      gtk_widget_get_preferred_width (self->new_folder_0_button, &button_width, NULL);
      break;

    case MODE_FILES_ONLY:
      gtk_widget_get_preferred_width (self->open_folders_button, &button_width, NULL);
      break;

    case MODE_FOLDERS_ONLY:
      gtk_widget_get_preferred_width (self->open_folders_button, &button_width, NULL);
      break;

    case MODE_MIXED:
      gtk_widget_get_preferred_width (self->open_folders_button, &button_width, NULL);
      break;
    }

  button_width += 2 * gtk_container_get_border_width (GTK_CONTAINER (self->stack));

  if (minimum)
    *minimum = button_width;

  if (natural)
    *natural = button_width;
}

static void
nautilus_action_bar_class_init (NautilusActionBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nautilus_action_bar_finalize;
  object_class->get_property = nautilus_action_bar_get_property;
  object_class->set_property = nautilus_action_bar_set_property;

  widget_class->size_allocate = nautilus_action_bar_size_allocate;
  widget_class->get_preferred_width = nautilus_action_bar_get_preferred_width;

  /**
   * NautilusActionBar::view:
   *
   * The view related to this actionbar.
   */
  g_object_class_install_property (object_class,
                                   PROP_VIEW,
                                   g_param_spec_object ("view",
                                                        "View of the actionbar",
                                                        "The view related to this actionbar",
                                                        NAUTILUS_TYPE_VIEW,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-action-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, copy_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, default_app_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, default_app_icon);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, default_app_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, loading_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, folders_overflow_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, move_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, move_trash_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, new_folder_0_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, no_selection_overflow_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, open_file_box);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, open_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, paste_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, properties_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, rename_folders_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, select_all_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, selection_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, size_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, stack);

  gtk_widget_class_bind_template_callback (widget_class, clear_selection_cb);

  gtk_widget_class_set_css_name (widget_class, "actionbar");
}

static void
nautilus_action_bar_init (NautilusActionBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* No selection widgets */
  self->no_selection_widgets[0] = self->new_folder_0_button;
  self->no_selection_widgets[1] = self->paste_button;
  self->no_selection_widgets[2] = self->select_all_button;

  /* Folder- and folder-only widgets */
  self->files_folders_widgets[0] = self->move_folders_button;
  self->files_folders_widgets[1] = self->copy_folders_button;
  self->files_folders_widgets[2] = self->rename_folders_button;
  self->files_folders_widgets[3] = self->move_trash_folders_button;
  self->files_folders_widgets[4] = self->properties_folders_button;

  update_paste_button (self);

  g_signal_connect_swapped (nautilus_clipboard_monitor_get (), "clipboard-changed",
                            G_CALLBACK (update_paste_button), self);
}

/**
 * nautilus_action_bar_new:
 * @view: a #NautilusView
 *
 * Creates a new actionbar related to @view.
 *
 * Returns: (transfer full): a #NautilusActionBar
 */
GtkWidget*
nautilus_action_bar_new (NautilusView *view)
{
  return g_object_new (NAUTILUS_TYPE_ACTION_BAR,
                       "view", view,
                       NULL);
}
