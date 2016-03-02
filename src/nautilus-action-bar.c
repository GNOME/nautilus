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
#include "nautilus-file.h"
#include "nautilus-previewer.h"

#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#define               UPDATE_STATUS_TIMEOUT  200 //ms

struct _NautilusActionBar
{
  GtkFrame            parent;

  GtkWidget          *loading_label;
  GtkWidget          *paste_button;
  GtkWidget          *primary_label;
  GtkWidget          *secondary_label;
  GtkWidget          *stack;

  NautilusView       *view;
  gint                update_status_timeout_id;
};

G_DEFINE_TYPE (NautilusActionBar, nautilus_action_bar, GTK_TYPE_FRAME)

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

static void
open_preview_cb (NautilusActionBar *actionbar)
{
  GtkWidget *toplevel;
  GdkWindow *window;
  GList *selection;
  gchar *uri;
  guint xid;

  xid = 0;
  uri = NULL;
  selection = nautilus_view_get_selection (actionbar->view);

  /* Only preview if exact 1 file is selected */
  if (g_list_length (selection) != 1)
    goto out;

  uri = nautilus_file_get_uri (selection->data);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (actionbar));

#ifdef GDK_WINDOWING_X11
        window = gtk_widget_get_window (toplevel);
        if (GDK_IS_X11_WINDOW (window))
          xid = gdk_x11_window_get_xid (gtk_widget_get_window (toplevel));
#endif

  nautilus_previewer_call_show_file (uri, xid, TRUE);

out:
  g_clear_pointer (&selection, nautilus_file_list_free);
  g_clear_pointer (&uri, g_free);
}

#if 0
static void
update_paste_button (NautilusActionBar *self)
{
  NautilusClipboardMonitor *monitor;
  NautilusClipboardInfo *info;

  monitor = nautilus_clipboard_monitor_get ();
  info = nautilus_clipboard_monitor_get_clipboard_info (monitor);

  if (info)
    {
      gchar *label;
      gint length;

      length = g_list_length (info->files);

      if (info->cut)
        label = g_strdup_printf (g_dngettext(NULL, "Move %d file", "Move %d files", length), length);
      else
        label = g_strdup_printf (g_dngettext(NULL, "Paste %d file", "Paste %d files", length), length);

      gtk_widget_set_tooltip_text (self->paste_button, label);

      g_free (label);
    }
}
#endif

static void
setup_multiple_files_selection (NautilusActionBar *actionbar,
                                GList             *selection)
{
  NautilusFile *file;
  goffset non_folder_size;
  gboolean non_folder_size_known;
  guint non_folder_count, folder_count, folder_item_count;
  gboolean folder_item_count_known;
  guint file_item_count;
  GList *p;
  char *first_item_name;
  char *non_folder_count_str;
  char *non_folder_item_count_str;
  char *folder_count_str;
  char *folder_item_count_str;
  gchar *primary_text;
  gchar *secondary_text;

  folder_item_count_known = TRUE;
  folder_count = 0;
  folder_item_count = 0;
  non_folder_count = 0;
  non_folder_size_known = FALSE;
  non_folder_size = 0;
  first_item_name = NULL;
  folder_count_str = NULL;
  folder_item_count_str = NULL;
  non_folder_count_str = NULL;
  non_folder_item_count_str = NULL;

  for (p = selection; p != NULL; p = p->next)
    {
      file = p->data;

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

          if (!nautilus_file_can_get_size (file))
            {
              non_folder_size_known = TRUE;
              non_folder_size += nautilus_file_get_size (file);
            }
        }

      if (first_item_name == NULL)
        first_item_name = nautilus_file_get_display_name (file);
    }

  nautilus_file_list_free (selection);

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

      if (non_folder_size_known)
        {
          char *size_string;

          size_string = g_format_size (non_folder_size);
          /* This is marked for translation in case a localiser
           * needs to use something other than parentheses. The
           * the message in parentheses is the size of the selected items.
           */
          non_folder_item_count_str = g_strdup_printf (_("(%s)"), size_string);
          g_free (size_string);
        }
      else
        {
          non_folder_item_count_str = g_strdup ("");
        }
    }

  if (folder_count == 0 && non_folder_count == 0)
    {
      primary_text = secondary_text = NULL;
    }
  else if (folder_count == 0)
    {
      primary_text = g_strdup_printf ("%s, %s", non_folder_count_str, non_folder_item_count_str);
      secondary_text = NULL;
    }
  else if (non_folder_count == 0)
    {
      primary_text = g_strdup_printf ("%s %s", folder_count_str, folder_item_count_str);
      secondary_text = NULL;
    }
  else
    {
      primary_text = g_strdup_printf ("%s %s", folder_count_str, folder_item_count_str);
      secondary_text = g_strdup_printf ("%s, %s", non_folder_count_str, non_folder_item_count_str);
    }

  gtk_label_set_label (GTK_LABEL (actionbar->primary_label), primary_text ? primary_text : "");
  gtk_label_set_label (GTK_LABEL (actionbar->secondary_label), secondary_text ? secondary_text : "");

  g_free (first_item_name);
  g_free (folder_count_str);
  g_free (folder_item_count_str);
  g_free (non_folder_count_str);
  g_free (non_folder_item_count_str);
  g_free (secondary_text);
  g_free (primary_text);
}

static void
setup_single_file_selection (NautilusActionBar *actionbar,
                             NautilusFile      *file)
{
  gboolean is_directory;
  gchar *description;

  description = NULL;
  is_directory = nautilus_file_is_directory (file);

  /* Primary label is the file name */
  gtk_label_set_label (GTK_LABEL (actionbar->primary_label), nautilus_file_get_display_name (file));

  /*
   * If the selected item is a folder, display the number of
   * children. Otherwise, display the file size.
   */
  if (is_directory)
    {
      guint folder_children;

      if (nautilus_file_get_directory_item_count (file, &folder_children, NULL))
        {
          description = g_strdup_printf (ngettext("Contains %'d item", "Contains %'d items", folder_children),
                                         folder_children);
        }
    }
  else
    {
      description = g_format_size (nautilus_file_get_size (file));
    }

  /*
   * If there is no description available, we hide the second label so
   * the filename is vertically centralized against the icon.
   */
  gtk_widget_set_visible (actionbar->secondary_label, description != NULL);
  gtk_label_set_label (GTK_LABEL (actionbar->secondary_label), description ? description : "");

  g_clear_pointer (&description, g_free);
  g_clear_pointer (&file, nautilus_file_unref);
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
      GList *selection;
      gint number_of_files;

      selection = nautilus_view_get_selection (actionbar->view);
      number_of_files = g_list_length (selection);

      if (number_of_files == 0)
        {
          gtk_label_set_label (GTK_LABEL (actionbar->primary_label), "");
          gtk_label_set_label (GTK_LABEL (actionbar->secondary_label), "");
        }
      else if (number_of_files == 1)
        {
          setup_single_file_selection (actionbar, selection->data);
        }
      else
        {
          setup_multiple_files_selection (actionbar, selection);
        }

      gtk_stack_set_visible_child_name (GTK_STACK (actionbar->stack), "main");
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
nautilus_action_bar_finalize (GObject *object)
{
  NautilusActionBar *self = NAUTILUS_ACTION_BAR (object);

  if (self->update_status_timeout_id > 0)
    {
      g_source_remove (self->update_status_timeout_id);
      self->update_status_timeout_id = 0;
    }

  //g_signal_handlers_disconnect_by_func (nautilus_clipboard_monitor_get (), update_paste_button, self);
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
nautilus_action_bar_class_init (NautilusActionBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nautilus_action_bar_finalize;
  object_class->get_property = nautilus_action_bar_get_property;
  object_class->set_property = nautilus_action_bar_set_property;

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

  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, loading_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, paste_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, primary_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, secondary_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusActionBar, stack);

  gtk_widget_class_bind_template_callback (widget_class, open_preview_cb);

  gtk_widget_class_set_css_name (widget_class, "actionbar");
}

static void
nautilus_action_bar_init (NautilusActionBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#if 0
  update_paste_button (self);

  g_signal_connect_swapped (nautilus_clipboard_monitor_get (), "clipboard-changed",
                            G_CALLBACK (update_paste_button), self);
#endif
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
