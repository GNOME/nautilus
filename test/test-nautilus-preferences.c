
#include <config.h>

#include <libnautilus-extensions/nautilus-radio-button-group.h>
#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libnautilus-extensions/nautilus-password-dialog.h>
#include <libnautilus-extensions/nautilus-preferences-group.h>
#include <libnautilus-extensions/nautilus-preferences-item.h>
#include <libnautilus-extensions/nautilus-preferences.h>

#include <gtk/gtk.h>

static void test_preferences_group               (void);
static void test_preferences_item                (void);
static void register_global_preferences          (void);
GtkWidget * create_enum_item                     (const char *preference_name);
GtkWidget * create_bool_item                     (const char *preference_name);

enum
{
	FRUIT_APPLE,
	FRUIT_ORANGE,
	FRUIT_BANNANA
};

static const char FRUIT_PREFERENCE[] = "/a/very/fruity/path";

int
main (int argc, char * argv[])
{
	gnome_init ("foo", "bar", argc, argv);

	register_global_preferences ();

	test_preferences_group ();
	test_preferences_item ();
		
	gtk_main ();

	return 0;
}

static void
test_preferences_item (void)
{
	GtkWidget * window;
	GtkWidget * item;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	item = create_enum_item (FRUIT_PREFERENCE);

	gtk_container_add (GTK_CONTAINER (window), item);

	gtk_widget_show (item);

	gtk_widget_show (window);
}

static void
test_preferences_group (void)
{
	GtkWidget * window;
	GtkWidget * group;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	group = nautilus_preferences_group_new ("A group");
	
	nautilus_preferences_group_add_item (NAUTILUS_PREFERENCES_GROUP (group),
					     FRUIT_PREFERENCE,
					     NAUTILUS_PREFERENCE_ITEM_ENUM);
	
	gtk_container_add (GTK_CONTAINER (window), group);

	gtk_widget_show (group);

	gtk_widget_show (window);
}

GtkWidget *
create_enum_item (const char *preference_name)
{
 	return nautilus_preferences_item_new (preference_name, NAUTILUS_PREFERENCE_ITEM_ENUM);
}

// GtkWidget *
// create_bool_item (const char *preference_name)
// {
// 	return nautilus_preferences_item_new (global_preferences,
// 					      preference_name,
// 					      NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
// }

static void
register_global_preferences (void)
{
	gconstpointer default_values[3] = { (gconstpointer)FRUIT_ORANGE, (gconstpointer)FRUIT_ORANGE, (gconstpointer)FRUIT_ORANGE };

	nautilus_preference_set_info_by_name (FRUIT_PREFERENCE,
					      "Fruits",
					      NAUTILUS_PREFERENCE_ENUM,
					      default_values,
					      3);
	
	nautilus_preference_enum_add_entry_by_name (FRUIT_PREFERENCE,
						    "apple",
						    "Apple",
						    FRUIT_APPLE);

	nautilus_preference_enum_add_entry_by_name (FRUIT_PREFERENCE,
					     "orange",
					     "Orange",
					     FRUIT_ORANGE);

	nautilus_preference_enum_add_entry_by_name (FRUIT_PREFERENCE,
					     "bannana",
					     "Bannana",
					     FRUIT_BANNANA);

	nautilus_preferences_set_enum (FRUIT_PREFERENCE,
				       FRUIT_BANNANA);
}
