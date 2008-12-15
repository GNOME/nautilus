/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-preferences-glade.c - Some functions to connect a Glade-file to gconf keys.

   Copyright (C) 2002 Jan Arne Petersen

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
*/

#include <glib.h>
#include <gtk/gtk.h>

#include "eel-preferences.h"

#define EEL_PREFERENCES_BUILDER_DATA_KEY "eel_preferences_builder_data_key"
#define EEL_PREFERENCES_BUILDER_DATA_VALUE "eel_preferences_builder_data_value"
#define EEL_PREFERENCES_BUILDER_DATA_MAP "eel_preferences_builder_data_map"
#define EEL_PREFERENCES_BUILDER_DATA_WIDGETS "eel_preferences_builder_data_widgets"

/* helper */

static void
eel_preferences_builder_combo_box_update (GtkComboBox *combo_box,
					gpointer value,
					GCallback change_callback)
{
	GHashTable *map;
	int active;
	gpointer key;

	map = (GHashTable *) g_object_get_data (G_OBJECT (combo_box),
						EEL_PREFERENCES_BUILDER_DATA_MAP);
	active = GPOINTER_TO_INT (g_hash_table_lookup (map, value));

	if (active == -1) {
		return;
	}

	key = g_object_get_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_KEY);

	g_signal_handlers_block_by_func (combo_box, change_callback, key);
	gtk_combo_box_set_active (combo_box, active);
	g_signal_handlers_unblock_by_func (combo_box, change_callback, key);
}

static void
eel_preference_glade_never_sensitive (GtkWidget *widget, GtkStateType state)
{
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
}

static void
eel_preferences_builder_set_never_sensitive (GtkWidget *widget)
{
	gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
	g_signal_connect (G_OBJECT (widget), "state_changed",
			  G_CALLBACK (eel_preference_glade_never_sensitive),
			  NULL);
}

/* bool preference */

static void
eel_preferences_builder_bool_toggled (GtkToggleButton *toggle_button,
				    char *key)
{
	eel_preferences_set_boolean (key, gtk_toggle_button_get_active (toggle_button));
}

static void
eel_preferences_builder_bool_update (GtkToggleButton *toggle_button)
{
	gboolean value;
	gpointer key;

	key = g_object_get_data (G_OBJECT (toggle_button), EEL_PREFERENCES_BUILDER_DATA_KEY);

	value = eel_preferences_get_boolean (key);
	g_signal_handlers_block_by_func (toggle_button, eel_preferences_builder_bool_toggled, key);
	gtk_toggle_button_set_active (toggle_button, value);
	g_signal_handlers_unblock_by_func (toggle_button, eel_preferences_builder_bool_toggled, key);
}

void
eel_preferences_builder_connect_bool (GtkBuilder *builder,
				      const char *component,
				      const char *key)
{
	GtkToggleButton *toggle_button;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (component != NULL);
	g_return_if_fail (key != NULL);

	toggle_button = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, component));
	g_object_set_data_full (G_OBJECT (toggle_button), EEL_PREFERENCES_BUILDER_DATA_KEY,
				g_strdup (key), (GDestroyNotify) g_free);

	eel_preferences_add_callback_while_alive (key,
				      		  (EelPreferencesCallback) eel_preferences_builder_bool_update,
				      		  toggle_button, G_OBJECT (toggle_button));

	if (!eel_preferences_key_is_writable (key)) {
		eel_preferences_builder_set_never_sensitive (GTK_WIDGET (toggle_button));
	}
	
	g_signal_connect (G_OBJECT (toggle_button), "toggled",
			  G_CALLBACK (eel_preferences_builder_bool_toggled),
			  g_object_get_data (G_OBJECT (toggle_button),
				  		       EEL_PREFERENCES_BUILDER_DATA_KEY));

	eel_preferences_builder_bool_update (toggle_button);
}

void
eel_preferences_builder_connect_bool_slave (GtkBuilder *builder,
					    const char *component,
					    const char *key)
{
	GtkToggleButton *toggle_button;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (component != NULL);
	g_return_if_fail (key != NULL);

	toggle_button = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, component));

	if (!eel_preferences_key_is_writable (key)) {
		eel_preferences_builder_set_never_sensitive (GTK_WIDGET (toggle_button));
	}
	
	g_signal_connect_data (G_OBJECT (toggle_button), "toggled",
			       G_CALLBACK (eel_preferences_builder_bool_toggled),
			       g_strdup (key), (GClosureNotify) g_free, 0);
}

/* string enum (ComboBox) preference */

static void
eel_preferences_builder_string_enum_combo_box_changed (GtkComboBox *combo_box,
						     char *key)
{
	int active;
	char **values;
	int i;

	active = gtk_combo_box_get_active  (combo_box);
	values = g_object_get_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_VALUE);

	i = 0;
	while (i < active && values[i] != NULL) {
		i++;
	}

	if (values[i] == NULL) {
		return;
	}

	eel_preferences_set (key, values[i]);
}

static void
eel_preferences_builder_string_enum_combo_box_update (GtkComboBox *combo_box)
{
	char *value;

	value = eel_preferences_get (g_object_get_data (G_OBJECT (combo_box),
							EEL_PREFERENCES_BUILDER_DATA_KEY));

	eel_preferences_builder_combo_box_update (combo_box, value,
						  G_CALLBACK (eel_preferences_builder_string_enum_combo_box_changed));

	g_free (value);
}

void
eel_preferences_builder_connect_string_enum_combo_box (GtkBuilder *builder,
						       const char *component,
						       const char *key,
						       const char **values)
{
	GtkWidget *combo_box;
	GHashTable *map;
	int i;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (component != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (values != NULL);
	
	combo_box = GTK_WIDGET (gtk_builder_get_object (builder, component));

	map = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);

	for (i = 0; values[i] != NULL; i++) {
		g_hash_table_insert (map, g_strdup (values[i]), GINT_TO_POINTER (i));
	}

	g_object_set_data_full (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_MAP, map,
				(GDestroyNotify) g_hash_table_destroy);
	g_object_set_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_VALUE, values);
	g_object_set_data_full (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_KEY,
				g_strdup (key), (GDestroyNotify) g_free);

	eel_preferences_add_callback_while_alive (key,
				 		  (EelPreferencesCallback) eel_preferences_builder_string_enum_combo_box_update,
						  combo_box, G_OBJECT (combo_box));

	if (!eel_preferences_key_is_writable (key)) {
		eel_preferences_builder_set_never_sensitive (GTK_WIDGET (combo_box));
	}

	g_signal_connect (G_OBJECT (combo_box), "changed",
			  G_CALLBACK (eel_preferences_builder_string_enum_combo_box_changed),
			  g_object_get_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_KEY));

	eel_preferences_builder_string_enum_combo_box_update (GTK_COMBO_BOX (combo_box));
}

void
eel_preferences_builder_connect_string_enum_combo_box_slave (GtkBuilder *builder,
							     const char *component,
							     const char *key)
{
	GtkWidget *combo_box;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (component != NULL);
	g_return_if_fail (key != NULL);
	
	combo_box = GTK_WIDGET (gtk_builder_get_object (builder, component));

	g_assert (g_object_get_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_MAP) != NULL);

	if (!eel_preferences_key_is_writable (key)) {
		eel_preferences_builder_set_never_sensitive (GTK_WIDGET (combo_box));
	}

	g_signal_connect_data (G_OBJECT (combo_box), "changed",
			       G_CALLBACK (eel_preferences_builder_string_enum_combo_box_changed),
			       g_strdup (key), (GClosureNotify) g_free, 0);
}


/* int enum preference */

static void
eel_preferences_builder_uint_enum_changed (GtkComboBox *combo_box,
					   char *key)
{
	int active;
	GSList *value_list;
	int i;

	active = gtk_combo_box_get_active (combo_box);
	value_list = (GSList *) g_object_get_data (G_OBJECT (combo_box),
						   EEL_PREFERENCES_BUILDER_DATA_VALUE);

	i = 0;
	while (i < active && value_list->next != NULL) {
		i++;
		value_list = value_list->next;
	}

	eel_preferences_set_uint (key, GPOINTER_TO_UINT (value_list->data));
}

static void
eel_preferences_builder_uint_enum_update (GtkComboBox *combo_box)
{
	guint value;

	value = eel_preferences_get_uint (g_object_get_data (G_OBJECT (combo_box),
							     EEL_PREFERENCES_BUILDER_DATA_KEY));

	eel_preferences_builder_combo_box_update (combo_box, GUINT_TO_POINTER (value),
						  G_CALLBACK (eel_preferences_builder_uint_enum_changed));
}

void
eel_preferences_builder_connect_uint_enum (GtkBuilder  *builder,
					   const char  *component,
					   const char  *key,
					   const guint *values,
					   int          num_values)
{
	GHashTable *map;
	int i;
	guint value;
	GtkComboBox *combo_box;
	GSList *value_list;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (component != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (values != NULL);
	
	combo_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, component));

	map = g_hash_table_new (g_direct_hash, g_direct_equal);
	value_list = NULL;

	for (i = 0; i < num_values; i++) {
		value = values[i];
		value_list = g_slist_append (value_list, GUINT_TO_POINTER (value));
		g_hash_table_insert (map, GUINT_TO_POINTER (value), GINT_TO_POINTER (i));
	}

	g_object_set_data_full (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_MAP, map,
				(GDestroyNotify) g_hash_table_destroy);
	g_object_set_data_full (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_VALUE, value_list,
				(GDestroyNotify) g_slist_free);
	g_object_set_data_full (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_KEY,
				g_strdup (key), (GDestroyNotify) g_free);

	if (!eel_preferences_key_is_writable (key)) {
		eel_preferences_builder_set_never_sensitive (GTK_WIDGET (combo_box));
	}

	g_signal_connect (G_OBJECT (combo_box), "changed",
			  G_CALLBACK (eel_preferences_builder_uint_enum_changed),
			  g_object_get_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_KEY));

	eel_preferences_add_callback_while_alive (key,
				 		  (EelPreferencesCallback) eel_preferences_builder_uint_enum_update,
						  combo_box, G_OBJECT (combo_box));

	eel_preferences_builder_uint_enum_update (combo_box);
}


/* String Enum (RadioButton) preference */

static void
eel_preferences_builder_string_enum_radio_button_toggled (GtkToggleButton *toggle_button,
							char *key)
{
	if (gtk_toggle_button_get_active (toggle_button) == FALSE) {
		return;
	}

	eel_preferences_set (key,
			     g_object_get_data (G_OBJECT (toggle_button),
				     		EEL_PREFERENCES_BUILDER_DATA_VALUE));
}

static void
eel_preferences_builder_string_enum_radio_button_update (GtkWidget *widget)
{
	gpointer key;
	char *value;
	GHashTable *map;
	gpointer object;

	key = g_object_get_data (G_OBJECT (widget), EEL_PREFERENCES_BUILDER_DATA_KEY);
	value = eel_preferences_get (key);
	map = g_object_get_data (G_OBJECT (widget), EEL_PREFERENCES_BUILDER_DATA_MAP);
	object = g_hash_table_lookup (map, value);
	g_free (value);
	if (object == NULL) {
		return;
	}

	g_signal_handlers_block_by_func (widget,
					 eel_preferences_builder_string_enum_radio_button_toggled,
					 key);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), TRUE);
	g_signal_handlers_unblock_by_func (widget,
					   eel_preferences_builder_string_enum_radio_button_toggled,
					   key);
}

void
eel_preferences_builder_connect_string_enum_radio_button (GtkBuilder *builder,
							  const char **components,
							  const char *key,
							  const char **values)
{
	GHashTable *map;
	int i;
	GtkWidget *widget;
	gboolean writable;

	g_return_if_fail (builder != NULL);
	g_return_if_fail (components != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (values != NULL);

	map = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);

	writable = eel_preferences_key_is_writable (key);

	widget = NULL;
	for (i = 0; components[i] != NULL && values[i] != NULL; i++) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, components[i]));
		g_hash_table_insert (map, g_strdup (values[i]), widget);
		if (i == 0) {
			g_object_set_data_full (G_OBJECT (widget),
						EEL_PREFERENCES_BUILDER_DATA_MAP, map,
					        (GDestroyNotify) g_hash_table_destroy);
		} else {
			g_object_set_data (G_OBJECT (widget),
					   EEL_PREFERENCES_BUILDER_DATA_MAP, map);
		}
		g_object_set_data_full (G_OBJECT (widget),
					EEL_PREFERENCES_BUILDER_DATA_VALUE, g_strdup (values[i]),
					(GDestroyNotify) g_free);
		g_object_set_data_full (G_OBJECT (widget),
					EEL_PREFERENCES_BUILDER_DATA_KEY, g_strdup (key),
					(GDestroyNotify) g_free);

		if (!writable) {
			eel_preferences_builder_set_never_sensitive (widget);
		}

		g_signal_connect (G_OBJECT (widget), "toggled",
				  G_CALLBACK (eel_preferences_builder_string_enum_radio_button_toggled),
				  g_object_get_data (G_OBJECT (widget),
					  	     EEL_PREFERENCES_BUILDER_DATA_KEY));
	}

	eel_preferences_add_callback_while_alive (key,
						  (EelPreferencesCallback) eel_preferences_builder_string_enum_radio_button_update,
						  widget, G_OBJECT (widget));

	eel_preferences_builder_string_enum_radio_button_update (widget);
}


/* list enum preference */

static void
eel_preferences_builder_list_enum_changed (GtkComboBox *combo_box,
					 char *key)
{
	GSList *widgets;
	int active;
	char **values;
	int i;
	GPtrArray *v;

	widgets = g_object_get_data (G_OBJECT (combo_box), EEL_PREFERENCES_BUILDER_DATA_WIDGETS);

	v = g_ptr_array_new ();
	for (; widgets != NULL; widgets = widgets->next) {
		active = gtk_combo_box_get_active (GTK_COMBO_BOX (widgets->data));
		values = g_object_get_data (G_OBJECT (combo_box),
					    EEL_PREFERENCES_BUILDER_DATA_VALUE);

		i = 0;
		while (i < active && values[i] != NULL) {
			i++;
		}

		if (values[i] != NULL) {
			g_ptr_array_add (v, values[i]);
		}
	}
	g_ptr_array_add (v, NULL);

	eel_preferences_set_string_array (key, (char **) v->pdata);
	g_ptr_array_free (v, TRUE);
}

static void
eel_preferences_builder_list_enum_update (GtkWidget *widget)
{
	char **values;
	GSList *components;
	int i;

	values = eel_preferences_get_string_array (g_object_get_data (G_OBJECT (widget),
								      EEL_PREFERENCES_BUILDER_DATA_KEY));
	components = g_object_get_data (G_OBJECT (widget), EEL_PREFERENCES_BUILDER_DATA_WIDGETS);
	for (i = 0; values[i] != NULL && components != NULL; i++, components = components->next) {
		eel_preferences_builder_combo_box_update (GTK_COMBO_BOX (components->data), 
							values[i],
							G_CALLBACK (eel_preferences_builder_list_enum_changed));
	}

	g_strfreev (values);
}

void 
eel_preferences_builder_connect_list_enum (GtkBuilder *builder,
					   const char **components,
					   const char *key,
					   const char **values)
{
	GtkWidget *combo_box;
	GHashTable *map;
	int i;
	GSList *widgets;
	gboolean writable;

 	g_return_if_fail (builder != NULL);
	g_return_if_fail (components != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (values != NULL);
	
	map = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);

	for (i = 0; values[i] != NULL; i++) {
		g_hash_table_insert (map, g_strdup (values[i]), GINT_TO_POINTER (i));
	}

	writable = eel_preferences_key_is_writable (key);

	combo_box = NULL;
	widgets = NULL;
	for (i = 0; components[i] != NULL; i++) {
		combo_box = GTK_WIDGET (gtk_builder_get_object (builder, components[i]));
		widgets = g_slist_append (widgets, combo_box);
		if (i == 0) {
			g_object_set_data_full (G_OBJECT (combo_box),
						EEL_PREFERENCES_BUILDER_DATA_MAP, map,
						(GDestroyNotify) g_hash_table_destroy);
			g_object_set_data_full (G_OBJECT (combo_box),
						EEL_PREFERENCES_BUILDER_DATA_WIDGETS,
						widgets, (GDestroyNotify) g_slist_free);
		} else {
			g_object_set_data (G_OBJECT (combo_box),
					   EEL_PREFERENCES_BUILDER_DATA_MAP, map);
			g_object_set_data (G_OBJECT (combo_box),
					   EEL_PREFERENCES_BUILDER_DATA_WIDGETS, widgets);
		}
		g_object_set_data (G_OBJECT (combo_box),
				   EEL_PREFERENCES_BUILDER_DATA_VALUE, values);
		g_object_set_data_full (G_OBJECT (combo_box),
				        EEL_PREFERENCES_BUILDER_DATA_KEY, g_strdup (key),
					(GDestroyNotify) g_free);

		if (!writable) {
			eel_preferences_builder_set_never_sensitive (combo_box);
		}

		g_signal_connect (G_OBJECT (combo_box), "changed",
			  	  G_CALLBACK (eel_preferences_builder_list_enum_changed),
				  g_object_get_data (G_OBJECT (combo_box),
					  	     EEL_PREFERENCES_BUILDER_DATA_KEY));
	}

	eel_preferences_add_callback_while_alive (key,
						  (EelPreferencesCallback) eel_preferences_builder_list_enum_update,
						  combo_box, G_OBJECT (combo_box));
	
	eel_preferences_builder_list_enum_update (combo_box);
}

