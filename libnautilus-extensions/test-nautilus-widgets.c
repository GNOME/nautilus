
#include <nautilus-widgets/nautilus-radio-button-group.h>
#include <nautilus-widgets/nautilus-preferences-group.h>
#include <nautilus-widgets/nautilus-preferences-item.h>
#include <nautilus-widgets/nautilus-preferences.h>

#include <gtk/gtk.h>
#include <stdio.h>

static void test_radio_group (void);
static void test_preferences_group (void);
static void test_preferences_item (void);


static void test_radio_changed_signal (GtkWidget *button_group, gpointer user_data);

static GtkObject *
create_dummy_prefs (void);

GtkWidget *
create_enum_item (void);

GtkWidget *
create_bool_item (void);

static GtkObject * dummy_prefs = NULL;

int
main (int argc, char * argv[])
{
	gtk_init (&argc, &argv);

	dummy_prefs = create_dummy_prefs ();
	
	test_radio_group ();
	test_preferences_group ();
	test_preferences_item ();
		
	gtk_main ();

	return 0;
}

static void
test_radio_group (void)
{
	GtkWidget * window;
	GtkWidget * buttons;


	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	buttons = nautilus_radio_button_group_new ();

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (buttons), "Apples");
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (buttons), "Oranges");
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (buttons), "Strawberries");

	gtk_signal_connect (GTK_OBJECT (buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (test_radio_changed_signal),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), buttons);

	gtk_widget_show (buttons);

	gtk_widget_show (window);
}

static void
test_preferences_item (void)
{
	GtkWidget * window;
	GtkWidget * item;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	item = create_enum_item ();

	gtk_container_add (GTK_CONTAINER (window), item);

	gtk_widget_show (item);

	gtk_widget_show (window);
}

static void
test_preferences_group (void)
{
	GtkWidget * window;
	GtkWidget * group;

	GtkWidget * item;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	group = nautilus_preferences_group_new ("A group");

	item = create_enum_item ();

	nautilus_preferences_group_add (NAUTILUS_PREFERENCES_GROUP (group),
					item);

	gtk_widget_show (item);

	gtk_container_add (GTK_CONTAINER (window), group);

	gtk_widget_show (group);

	gtk_widget_show (window);
}

static void
test_radio_changed_signal (GtkWidget *buttons, gpointer user_data)
{
	gint i;

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));

	printf ("test_radio_changed_signal (%d)\n", i);
}

GtkWidget *
create_enum_item (void)
{
	GtkWidget * item;

	item = nautilus_preferences_item_new (dummy_prefs,
					      "user_level",
					      NAUTILUS_PREFERENCE_ENUM);
	
	return item;
}

GtkWidget *
create_bool_item (void)
{
	GtkWidget * item;

	item = nautilus_preferences_item_new (dummy_prefs,
					      "foo",
					      NAUTILUS_PREFERENCE_BOOLEAN);
	
	return item;
}


static const gchar * prefs_global_user_level_names[] =
{
	"novice",
	"intermediate",
	"hacker",
	"ettore"
};

static const gchar * prefs_global_user_level_descriptions[] =
{
	"Novice",
	"Intermediate",
	"Hacker",
	"Ettore"
};

static const gint prefs_global_user_level_values[] =
{
	0,
	1,
	2,
	3
};

static NautilusPreferencesEnumData prefs_global_user_level_data =
{
	prefs_global_user_level_names,
	prefs_global_user_level_descriptions,
	prefs_global_user_level_values,
	4
};

static NautilusPreferencesInfo prefs_global_static_pref_info[] =
{
	{
		"user_level",
		"User Level",
		GTK_TYPE_ENUM,
		FALSE,
		(gpointer) &prefs_global_user_level_data
	},
	{
		"foo",
		"Create new window for each new page",
		GTK_TYPE_BOOL,
		FALSE,
		NULL
	},
	{
		"bar",
		"Do not open more than one window with the same page",
		GTK_TYPE_BOOL,
		FALSE,
		NULL
	},
};

static GtkObject *
create_dummy_prefs (void)
{
	GtkObject * dummy_prefs;
	guint i;

	dummy_prefs = nautilus_preferences_new ("dummy");

	/* Register the static prefs */
	for (i = 0; i < 3; i++)
	{
		nautilus_preferences_register_from_info (NAUTILUS_PREFS (dummy_prefs),
						   &prefs_global_static_pref_info[i]);
	}


	nautilus_preferences_set_enum (NAUTILUS_PREFS (dummy_prefs),
				 "user_level",
				 2);

	return dummy_prefs;
}
