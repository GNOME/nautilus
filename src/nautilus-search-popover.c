/* nautilus-search-popover.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "nautilus-enum-types.h"
#include "nautilus-search-popover.h"

#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>

struct _NautilusSearchPopover
{
  GtkPopover          parent;

  GtkWidget          *around_revealer;
  GtkWidget          *around_stack;
  GtkWidget          *calendar;
  GtkWidget          *clear_date_button;
  GtkWidget          *dates_listbox;
  GtkWidget          *date_entry;
  GtkWidget          *date_stack;
  GtkWidget          *recursive_switch;
  GtkWidget          *select_date_button;
  GtkWidget          *select_date_button_label;
  GtkWidget          *type_label;
  GtkWidget          *type_listbox;
  GtkWidget          *type_stack;

  GFile              *location;
  NautilusQuery      *query;
  GBinding           *recursive_binding;
};

const gchar*         get_text_for_day                            (gint                   days);

static void          emit_date_changes_for_day                   (NautilusSearchPopover *popover,
                                                                  gint                   days);

static void          show_date_selection_widgets                 (NautilusSearchPopover *popover,
                                                                  gboolean               visible);

static void          show_other_types_dialog                     (NautilusSearchPopover *popover);

static void          update_date_label                           (NautilusSearchPopover *popover,
                                                                  guint                  days);

G_DEFINE_TYPE (NautilusSearchPopover, nautilus_search_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_LOCATION,
  PROP_QUERY,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct {
  char *name;
  char *mimetypes[20];
} mimetype_groups[] = {
  {
    N_("Anything"),
    { NULL }
  },
  {
    N_("Files"),
    { "application/octet-stream",
      "text/plain",
      NULL
    }
  },
  {
    N_("Folders"),
    { "inode/directory",
      NULL
    }
  },
  { N_("Documents"),
    { "application/rtf",
      "application/msword",
      "application/vnd.sun.xml.writer",
      "application/vnd.sun.xml.writer.global",
      "application/vnd.sun.xml.writer.template",
      "application/vnd.oasis.opendocument.text",
      "application/vnd.oasis.opendocument.text-template",
      "application/x-abiword",
      "application/x-applix-word",
      "application/x-mswrite",
      "application/docbook+xml",
      "application/x-kword",
      "application/x-kword-crypt",
      "application/x-lyx",
      NULL
    }
  },
  { N_("Illustration"),
    { "application/illustrator",
      "application/vnd.corel-draw",
      "application/vnd.stardivision.draw",
      "application/vnd.oasis.opendocument.graphics",
      "application/x-dia-diagram",
      "application/x-karbon",
      "application/x-killustrator",
      "application/x-kivio",
      "application/x-kontour",
      "application/x-wpg",
      NULL
    }
  },
  { N_("Music"),
    { "application/ogg",
      "audio/x-vorbis+ogg",
      "audio/ac3",
      "audio/basic",
      "audio/midi",
      "audio/x-flac",
      "audio/mp4",
      "audio/mpeg",
      "audio/x-mpeg",
      "audio/x-ms-asx",
      "audio/x-pn-realaudio",
      NULL
    }
  },
  { N_("PDF / Postscript"),
    { "application/pdf",
      "application/postscript",
      "application/x-dvi",
      "image/x-eps",
      NULL
    }
  },
  { N_("Picture"),
    { "application/vnd.oasis.opendocument.image",
      "application/x-krita",
      "image/bmp",
      "image/cgm",
      "image/gif",
      "image/jpeg",
      "image/jpeg2000",
      "image/png",
      "image/svg+xml",
      "image/tiff",
      "image/x-compressed-xcf",
      "image/x-pcx",
      "image/x-photo-cd",
      "image/x-psd",
      "image/x-tga",
      "image/x-xcf",
      NULL
    }
  },
  { N_("Presentation"),
    { "application/vnd.ms-powerpoint",
      "application/vnd.sun.xml.impress",
      "application/vnd.oasis.opendocument.presentation",
      "application/x-magicpoint",
      "application/x-kpresenter",
      NULL
    }
  },
  { N_("Spreadsheet"),
    { "application/vnd.lotus-1-2-3",
      "application/vnd.ms-excel",
      "application/vnd.stardivision.calc",
      "application/vnd.sun.xml.calc",
      "application/vnd.oasis.opendocument.spreadsheet",
      "application/x-applix-spreadsheet",
      "application/x-gnumeric",
      "application/x-kspread",
      "application/x-kspread-crypt",
      "application/x-quattropro",
      "application/x-sc",
      "application/x-siag",
      NULL
    }
  },
  { N_("Text File"),
    { "text/plain",
      NULL
    }
  },
  { N_("Video"),
    { "video/mp4",
      "video/3gpp",
      "video/mpeg",
      "video/quicktime",
      "video/vivo",
      "video/x-avi",
      "video/x-mng",
      "video/x-ms-asf",
      "video/x-ms-wmv",
      "video/x-msvideo",
      "video/x-nsv",
      "video/x-real-video",
      NULL
    }
  },
  { NULL,
    {
      NULL
    }
  }
};


/* Callbacks */

static void
calendar_day_selected (GtkCalendar           *calendar,
                       NautilusSearchPopover *popover)
{
  GDateTime *now;
  GDateTime *dt;
  guint year, month, day;

  now = g_date_time_new_now_local ();

  gtk_calendar_get_date (calendar,
                         &year,
                         &month,
                         &day);

  dt = g_date_time_new_local (year, month + 1, day, 0, 0, 0);

  if (g_date_time_compare (dt, now) < 1) {
    guint days;

    days = g_date_time_difference (now, dt) / G_TIME_SPAN_DAY;

    if (days > 0) {
      update_date_label (popover, days);
      emit_date_changes_for_day (popover, days);
    }
  }

  g_date_time_unref (now);
  g_date_time_unref (dt);
}

static void
setup_date (NautilusSearchPopover *popover,
            NautilusQuery         *query)
{
  GDateTime *dt;
  GDateTime *now;

  now = g_date_time_new_now_local ();
  dt = nautilus_query_get_date (query);

  /* Update date */
  if (dt && g_date_time_compare (dt, now) < 1) {
    guint days;

    days = g_date_time_difference (now, dt) / G_TIME_SPAN_DAY;

    if (days > 0) {
      g_signal_handlers_block_by_func (popover->calendar, calendar_day_selected, popover);

      gtk_calendar_select_month (GTK_CALENDAR (popover->calendar),
                                 g_date_time_get_month (dt) - 1,
                                 g_date_time_get_year (dt));

      gtk_calendar_select_day (GTK_CALENDAR (popover->calendar),
                               g_date_time_get_day_of_month (dt));

      update_date_label (popover, days);

      g_signal_handlers_unblock_by_func (popover->calendar, calendar_day_selected, popover);
    }
  }

  g_clear_pointer (&now, g_date_time_unref);
}

static void
query_date_changed (GObject               *object,
                    GParamSpec            *pspec,
                    NautilusSearchPopover *popover)
{
  setup_date (popover, NAUTILUS_QUERY (object));
}

static void
clear_date_button_clicked (GtkButton             *button,
                           NautilusSearchPopover *popover)
{
  gtk_label_set_label (GTK_LABEL (popover->select_date_button_label), _("Select Dates..."));
  gtk_widget_hide (popover->clear_date_button);
  emit_date_changes_for_day (popover, 0);
}

static void
date_entry_activate (GtkEntry              *entry,
                     NautilusSearchPopover *popover)
{
  if (gtk_entry_get_text_length (entry) > 0) {
    GDateTime *now;
    GDateTime *dt;
    guint days;
    GDate *date;

    date = g_date_new ();
    g_date_set_parse (date, gtk_entry_get_text (entry));

    /* Invalid date silently does nothing */
    if (!g_date_valid (date)) {
      g_date_free (date);
      return;
    }

    now = g_date_time_new_now_local ();
    dt = g_date_time_new_local (g_date_get_year (date),
                                g_date_get_month (date),
                                g_date_get_day (date),
                                0,
                                0,
                                0);

    /* Future dates also silently fails */
    if (g_date_time_compare (dt, now) == 1)
      goto out;

    days = g_date_time_difference (now, dt) / G_TIME_SPAN_DAY;

    if (days > 0) {
      update_date_label (popover, days);
      show_date_selection_widgets (popover, FALSE);
      emit_date_changes_for_day (popover, days);
    }

out:
    g_date_time_unref (now);
    g_date_time_unref (dt);
    g_date_free (date);
  }
}

static void
dates_listbox_row_activated (GtkListBox            *listbox,
                             GtkListBoxRow         *row,
                             NautilusSearchPopover *popover)
{
  gint days;

  days = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "days"));

  update_date_label (popover, days);
  show_date_selection_widgets (popover, FALSE);
  emit_date_changes_for_day (popover, days);
}

static void
listbox_header_func (GtkListBoxRow         *row,
                     GtkListBoxRow         *before,
                     NautilusSearchPopover *popover)
{
  gboolean show_separator;

  show_separator = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "show-separator"));

  if (show_separator) {
    GtkWidget *separator;

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show (separator);

    gtk_list_box_row_set_header (row, separator);
  }
}

static void
select_date_button_clicked (GtkButton             *button,
                            NautilusSearchPopover *popover)
{

  /* Hide the type selection widgets when date selection
   * widgets are shown.
   */
  gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");

  show_date_selection_widgets (popover, TRUE);
}

static void
select_type_button_clicked (GtkButton             *button,
                            NautilusSearchPopover *popover)
{
  gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-list");

  /* Hide the date selection widgets when the type selection
   * listbox is shown.
   */
  show_date_selection_widgets (popover, FALSE);
}

static void
toggle_calendar_icon_clicked (GtkEntry              *entry,
                              GtkEntryIconPosition   position,
                              GdkEvent              *event,
                              NautilusSearchPopover *popover)
{
  const gchar *current_visible_child;
  const gchar *child;
  const gchar *icon_name;
  const gchar *tooltip;

  current_visible_child = gtk_stack_get_visible_child_name (GTK_STACK (popover->around_stack));

  if (g_strcmp0 (current_visible_child, "date-list") == 0) {
    child = "date-calendar";
    icon_name = "view-list-symbolic";
    tooltip = _("Show a list to select the date");
  } else {
    child = "date-list";
    icon_name = "x-office-calendar-symbolic";
    tooltip = _("Show a calendar to select the date");
  }

  gtk_stack_set_visible_child_name (GTK_STACK (popover->around_stack), child);
  gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, icon_name);
  gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, tooltip);
}

static void
types_listbox_row_activated (GtkListBox            *listbox,
                             GtkListBoxRow         *row,
                             NautilusSearchPopover *popover)
{
  gint group;

  group = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "mimetype-group"));

  /* The -1 group stands for the "Other Types" group, for which
   * we should show the mimetype dialog.
   */
  if (group == -1) {
    show_other_types_dialog (popover);
  }
  else {
    GList *mimetypes;
    gint i;

    mimetypes = NULL;

    /* Setup the new mimetypes set */
    for (i = 0; mimetype_groups[group].mimetypes[i]; i++) {
      mimetypes = g_list_append (mimetypes, mimetype_groups[group].mimetypes[i]);
    }

    gtk_label_set_label (GTK_LABEL (popover->type_label), gettext (mimetype_groups[group].name));

    g_signal_emit (popover, signals[CHANGED], 0, NAUTILUS_SEARCH_FILTER_TYPE, mimetypes);

    g_list_free (mimetypes);
  }

  gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");
}

/* Auxiliary methods */

static GtkWidget*
create_row_for_label (const gchar *text,
                      gboolean     show_separator)
{
  GtkWidget *row;
  GtkWidget *label;

  row = gtk_list_box_row_new ();

  g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", text,
                        "hexpand", TRUE,
                        "xalign", 0.0,
                        "margin-start", 6,
                        NULL);

  gtk_container_add (GTK_CONTAINER (row), label);
  gtk_widget_show_all (row);

  return row;
}

static void
emit_date_changes_for_day (NautilusSearchPopover *popover,
                           gint                   days)
{
  GDateTime *dt;

  dt = NULL;

  if (days > 0) {
    GDateTime *now;

    now = g_date_time_new_now_local ();
    dt = g_date_time_add_days (now, -days);

    g_date_time_unref (now);
  }

  g_signal_emit (popover, signals[CHANGED], 0, NAUTILUS_SEARCH_FILTER_DATE, dt);

  g_clear_pointer (&dt, g_date_time_unref);
}

static void
fill_fuzzy_dates_listbox (NautilusSearchPopover *popover)
{
  GDateTime *maximum_dt, *now;
  GtkWidget *row;
  gint days, max_days;

  days = 0;

  maximum_dt = g_date_time_new_from_unix_local (0);
  now = g_date_time_new_now_local ();
  max_days = (g_date_time_get_year (now) - g_date_time_get_year (maximum_dt)) * 365;

  /* This is a tricky loop. The main intention here is that each
   * timeslice (day, week, month) have 2 or 3 entries. Years,
   * however, are exceptions and should show many entries.
   */
  while (days < max_days) {
    gchar *label;
    gint n, step;

    if (days == 0) {
      n = 0;
      step = 1;
    } else if (days < 7) {
      /* days */
      n = days;
      step = 2;
    } else if (days < 30) {
      /* weeks */
      n = days / 7;
      step = 7;
    } else if (days < 365) {
      /* months */
      n = days / 30;
      step = 84;
    } else if (days < 1825) {
      /* years */
      n = days / 365;
      step = 365;
    } else {
      /* after the first 5 years, jump at a 5-year pace */
      n = days / 365;
      step = 1825;
    }

    label = g_strdup_printf (get_text_for_day (days), n);

    row = create_row_for_label (label, n == 1);
    g_object_set_data (G_OBJECT (row), "days", GINT_TO_POINTER (days));

    gtk_container_add (GTK_CONTAINER (popover->dates_listbox), row);

    g_free (label);

    days += step;
  }

  g_date_time_unref (maximum_dt);
  g_date_time_unref (now);
}

static void
fill_types_listbox (NautilusSearchPopover *popover)
{
  GtkWidget *row;
  int i;

  /* Mimetypes */
  for (i = 0; i < G_N_ELEMENTS (mimetype_groups); i++) {

    /* On the third row, which is right below "Folders", there should be an
     * separator to logically group the types.
     */
    row = create_row_for_label (gettext (mimetype_groups[i].name), i == 3);
    g_object_set_data (G_OBJECT (row), "mimetype-group", GINT_TO_POINTER (i));

    gtk_container_add (GTK_CONTAINER (popover->type_listbox), row);
  }

  /* Other types */
  row = create_row_for_label (_("Other Type…"), TRUE);
  g_object_set_data (G_OBJECT (row), "mimetype-group", GINT_TO_POINTER (-1));
  gtk_container_add (GTK_CONTAINER (popover->type_listbox), row);
}

const gchar*
get_text_for_day (gint days)
{
  if (days == 0) {
      return _("Any time");
    } else if (days < 7) {
      /* days */
      return ngettext ("%d day ago", "%d days ago", days);
    } else if (days < 30) {
      /* weeks */
      return ngettext ("Last week", "%d weeks ago", days / 7);
    } else if (days < 365) {
      /* months */
      return ngettext ("Last month", "%d months ago", days / 30);
    } else {
      /* years */
      return ngettext ("Last year", "%d years ago", days / 365);
    }
}

static void
show_date_selection_widgets (NautilusSearchPopover *popover,
                             gboolean               visible)
{
  gtk_stack_set_visible_child_name (GTK_STACK (popover->date_stack), visible ? "date-entry" : "date-button");
  gtk_stack_set_visible_child_name (GTK_STACK (popover->around_stack), "date-list");
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (popover->date_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "x-office-calendar-symbolic");

  gtk_widget_set_visible (popover->around_revealer, visible);

  gtk_revealer_set_reveal_child (GTK_REVEALER (popover->around_revealer), visible);

  /* Only update the date button when we're not editing a query.
   * Otherwise, when we select a date and try to select a mimetype,
   * the label is unwantedly reset.
   */
  if (!popover->query)
    update_date_label (popover, 0);
}

static void
show_other_types_dialog (NautilusSearchPopover *popover)
{
  GList *mime_infos, *l;
  GtkWidget *dialog;
  GtkWidget *scrolled, *treeview;
  GtkListStore *store;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkWidget *toplevel;
  GtkTreeSelection *selection;

  mime_infos = g_content_types_get_registered ();

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  for (l = mime_infos; l != NULL; l = l->next) {
    GtkTreeIter iter;
    char *mime_type = l->data;
    char *description;

    description = g_content_type_get_description (mime_type);
    if (description == NULL) {
      description = g_strdup (mime_type);
    }

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
            0, description,
            1, mime_type,
            -1);

    g_free (mime_type);
    g_free (description);
  }
  g_list_free (mime_infos);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (popover));
  dialog = gtk_dialog_new_with_buttons (_("Select type"),
                GTK_WINDOW (toplevel),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                _("_Cancel"), GTK_RESPONSE_CANCEL,
                _("Select"), GTK_RESPONSE_OK,
                NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 600);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_widget_show (scrolled);
  gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 0);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), scrolled, TRUE, TRUE, 0);

  treeview = gtk_tree_view_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 0, GTK_SORT_ASCENDING);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);


  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name",
                                                     renderer,
                                                     "text",
                                                     0,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

  gtk_widget_show (treeview);
  gtk_container_add (GTK_CONTAINER (scrolled), treeview);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    GtkTreeIter iter;
    GList *mimetypes;
    char *mimetype, *description;

    gtk_tree_selection_get_selected (selection, NULL, &iter);
    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
            0, &description,
            1, &mimetype,
            -1);

    mimetypes = g_list_append (NULL, mimetype);

    gtk_label_set_label (GTK_LABEL (popover->type_label), description);

    g_signal_emit (popover, signals[CHANGED], 0, NAUTILUS_SEARCH_FILTER_TYPE, mimetypes);

    gtk_stack_set_visible_child_name (GTK_STACK (popover->type_stack), "type-button");
  }

  gtk_widget_destroy (dialog);
}

static void
update_date_label (NautilusSearchPopover *popover,
                   guint                  days)
{
  if (days > 0) {
    GDateTime *now;
    GDateTime *dt;
    gchar *formatted_date;
    gchar *label;
    guint n;

    if (days < 7) {
      n = days;
    } else if (days < 30) {
      n = days / 7;
    } else if (days < 365) {
      n = days / 30;
    } else {
      n = days / 365;
    }

    label = g_strdup_printf (get_text_for_day (days), n);

    now = g_date_time_new_now_local ();
    dt = g_date_time_add_days (now, -days);
    formatted_date = g_date_time_format (dt, "%x");

    gtk_entry_set_text (GTK_ENTRY (popover->date_entry), formatted_date);

    gtk_widget_show (popover->clear_date_button);
    gtk_label_set_label (GTK_LABEL (popover->select_date_button_label), label);

    g_date_time_unref (now);
    g_date_time_unref (dt);
    g_free (formatted_date);
    g_free (label);
  } else {
    gtk_label_set_label (GTK_LABEL (popover->select_date_button_label), _("Select Dates..."));
    gtk_widget_hide (popover->clear_date_button);
  }
}

/* Overrides */

static void
nautilus_search_popover_closed (GtkPopover *popover)
{
  NautilusSearchPopover *self = NAUTILUS_SEARCH_POPOVER (popover);
  GDateTime *now;

  /* Always switch back to the initial states */
  gtk_stack_set_visible_child_name (GTK_STACK (self->type_stack), "type-button");
  show_date_selection_widgets (self, FALSE);

  /* If we're closing an ongoing query, the popover must not
   * clear the current settings.
   */
  if (self->query)
    return;

  now = g_date_time_new_now_local ();

  /* Reselect today at the calendar */
  g_signal_handlers_block_by_func (self->calendar, calendar_day_selected, self);

  gtk_calendar_select_month (GTK_CALENDAR (self->calendar),
                             g_date_time_get_month (now) - 1,
                             g_date_time_get_year (now));

  gtk_calendar_select_day (GTK_CALENDAR (self->calendar),
                           g_date_time_get_day_of_month (now));

  g_signal_handlers_unblock_by_func (self->calendar, calendar_day_selected, self);
}

static void
nautilus_search_popover_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  NautilusSearchPopover *self;

  self = NAUTILUS_SEARCH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      g_value_set_object (value, self->location);
      break;

    case PROP_QUERY:
      g_value_set_object (value, self->query);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_search_popover_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
        NautilusSearchPopover  *self;

        self = NAUTILUS_SEARCH_POPOVER (object);

        switch (prop_id)
        {
        case PROP_LOCATION:
                nautilus_search_popover_set_location (self, g_value_get_object (value));
                break;

        case PROP_QUERY:
                nautilus_search_popover_set_query (self, g_value_get_object (value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}


static void
nautilus_search_popover_class_init (NautilusSearchPopoverClass *klass)
{
  GtkPopoverClass *popover_class = GTK_POPOVER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = nautilus_search_popover_get_property;
  object_class->set_property = nautilus_search_popover_set_property;

  popover_class->closed = nautilus_search_popover_closed;

  signals[CHANGED] = g_signal_new ("changed",
                                   NAUTILUS_TYPE_SEARCH_POPOVER,
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL,
                                   NULL,
                                   g_cclosure_marshal_generic,
                                   G_TYPE_NONE,
                                   2,
                                   NAUTILUS_TYPE_SEARCH_FILTER,
                                   G_TYPE_POINTER);

  /**
   * NautilusSearchPopover::location:
   *
   * The current location of the search.
   */
  g_object_class_install_property (object_class,
                                   PROP_LOCATION,
                                   g_param_spec_object ("location",
                                                        "Location of the popover",
                                                        "The current location of the search",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));

  /**
   * NautilusSearchPopover::query:
   *
   * The current #NautilusQuery being edited.
   */
  g_object_class_install_property (object_class,
                                   PROP_QUERY,
                                   g_param_spec_object ("query",
                                                        "Query of the popover",
                                                        "The current query being edited",
                                                        NAUTILUS_TYPE_QUERY,
                                                        G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-search-popover.ui");

  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, around_revealer);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, around_stack);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, clear_date_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, calendar);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, dates_listbox);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, date_entry);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, date_stack);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, recursive_switch);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, select_date_button);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, select_date_button_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_label);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_listbox);
  gtk_widget_class_bind_template_child (widget_class, NautilusSearchPopover, type_stack);

  gtk_widget_class_bind_template_callback (widget_class, calendar_day_selected);
  gtk_widget_class_bind_template_callback (widget_class, clear_date_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, date_entry_activate);
  gtk_widget_class_bind_template_callback (widget_class, dates_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, select_date_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, select_type_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, toggle_calendar_icon_clicked);
  gtk_widget_class_bind_template_callback (widget_class, types_listbox_row_activated);
}

static void
nautilus_search_popover_init (NautilusSearchPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Fuzzy dates listbox */
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->dates_listbox),
                                (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                self, NULL);

  fill_fuzzy_dates_listbox (self);

  /* Types listbox */
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->type_listbox),
                                (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                self, NULL);

  fill_types_listbox (self);
}

GtkWidget*
nautilus_search_popover_new (void)
{
  return g_object_new (NAUTILUS_TYPE_SEARCH_POPOVER, NULL);
}

/**
 * nautilus_search_popover_get_location:
 *
 * Retrieves the current directory as a #GFile.
 *
 * Returns: (transfer none): a #GFile.
 */
GFile*
nautilus_search_popover_get_location (NautilusSearchPopover *popover)
{
  g_return_val_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover), NULL);

  return popover->location;
}

/**
 * nautilus_search_popover_set_location:
 *
 * Sets the current location that the search is being
 * performed on.
 *
 * Returns:
 */
void
nautilus_search_popover_set_location (NautilusSearchPopover *popover,
                                      GFile                 *location)
{
  g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

  if (g_set_object (&popover->location, location))
    {
      if (!popover->query && location)
        {
          NautilusFile *file;
          gboolean active;

          file = nautilus_file_get (location);

          if (!nautilus_file_is_local (file))
            {
              active = g_settings_get_boolean (nautilus_preferences,
                                               "enable-remote-recursive-search");
            }
          else
            {
              active = g_settings_get_boolean (nautilus_preferences,
                                               "enable-recursive-search");
            }

          gtk_switch_set_active (GTK_SWITCH (popover->recursive_switch), active);
        }

      g_object_notify (G_OBJECT (popover), "location");
    }
}

/**
 * nautilus_search_popover_get_query:
 * @popover: a #NautilusSearchPopover
 *
 * Gets the current query for @popover.
 *
 * Returns: (transfer none): the current #NautilusQuery from @popover.
 */
NautilusQuery*
nautilus_search_popover_get_query (NautilusSearchPopover *popover)
{
  g_return_val_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover), NULL);

  return popover->query;
}

/**
 * nautilus_search_popover_set_query:
 * @popover: a #NautilusSearchPopover
 * @query (nullable): a #NautilusQuery
 *
 * Sets the current query for @popover.
 *
 * Returns:
 */
void
nautilus_search_popover_set_query (NautilusSearchPopover *popover,
                                   NautilusQuery         *query)
{
  NautilusQuery *previous_query;

  g_return_if_fail (NAUTILUS_IS_SEARCH_POPOVER (popover));

  previous_query = popover->query;

  if (popover->query != query) {
      /* Disconnect signals and bindings from the old query */
      if (previous_query) {
        g_signal_handlers_disconnect_by_func (query, query_date_changed, popover);
        g_clear_pointer (&popover->recursive_binding, g_binding_unbind);
      }

      g_set_object (&popover->query, query);

      if (query) {
        /* Date */
        setup_date (popover, query);

        g_signal_connect (query,
                          "notify::date",
                          G_CALLBACK (query_date_changed),
                          popover);
        /* Recursive */
        gtk_switch_set_active (GTK_SWITCH (popover->recursive_switch),
                               nautilus_query_get_recursive (query));

        popover->recursive_binding = g_object_bind_property (query,
                                                             "recursive",
                                                             popover->recursive_switch,
                                                             "active",
                                                             G_BINDING_BIDIRECTIONAL);
      } else {
        update_date_label (popover, 0);
        gtk_label_set_label (GTK_LABEL (popover->type_label), _("Anything"));
      }
  }
}
