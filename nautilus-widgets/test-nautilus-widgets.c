
#include <nautilus-widgets/nautilus-radio-button-group.h>
#include <nautilus-widgets/nautilus-caption-table.h>
#include <nautilus-widgets/nautilus-preferences-group.h>
#include <nautilus-widgets/nautilus-preferences-item.h>
#include <nautilus-widgets/nautilus-preferences.h>

#include <gtk/gtk.h>
#include <stdio.h>

static void test_radio_group                     (void);
static void test_caption_table                   (void);
static void test_preferences_group               (void);
static void test_preferences_item                (void);
static void test_radio_changed_callback            (GtkWidget  *button_group,
						  gpointer    user_data);
static void test_caption_table_activate_callback (GtkWidget  *button_group,
						  gint        active_index,
						  gpointer    user_data);
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
	gtk_init (&argc, &argv);

	nautilus_preferences_initialize (argc, argv);

	register_global_preferences ();

	test_radio_group ();
	test_caption_table ();
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
			    GTK_SIGNAL_FUNC (test_radio_changed_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), buttons);

	gtk_widget_show (buttons);

	gtk_widget_show (window);
}

static void
test_caption_table (void)
{
	GtkWidget * window;
	GtkWidget * table;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	table = nautilus_caption_table_new (4);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     0,
					     "Something",
					     "Text",
					     TRUE,
					     FALSE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     1,
					     "ReadOnly",
					     "Cant Change Me",
					     TRUE,
					     TRUE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     2,
					     "Password",
					     "sekret",
					     FALSE,
					     FALSE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (table),
					     3,
					     "This is a very long label",
					     "Text",
					     TRUE,
					     FALSE);

	gtk_signal_connect (GTK_OBJECT (table),
			    "activate",
			    GTK_SIGNAL_FUNC (test_caption_table_activate_callback),
			    (gpointer) NULL);

	gtk_container_add (GTK_CONTAINER (window), table);

	gtk_widget_show (table);

	gtk_widget_show (window);
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

static void
test_radio_changed_callback (GtkWidget *buttons, gpointer user_data)
{
	gint i;

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));

	g_print ("test_radio_changed_callback (%d)\n", i);
}

static void
test_caption_table_activate_callback (GtkWidget  *button_group,
				      gint        active_index,
				      gpointer    user_data)
{
	g_print ("test_caption_table_activate_callback (active_index=%d)\n", active_index);
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
	g_assert (nautilus_preferences_is_initialized ());

	nautilus_preferences_set_info (FRUIT_PREFERENCE,
				       "Fruits",
				       NAUTILUS_PREFERENCE_ENUM,
				       (gconstpointer) FRUIT_ORANGE);

	nautilus_preferences_enum_add_entry (FRUIT_PREFERENCE,
					     "apple",
					     "Apple",
					     FRUIT_APPLE);

	nautilus_preferences_enum_add_entry (FRUIT_PREFERENCE,
					     "orange",
					     "Orange",
					     FRUIT_ORANGE);

	nautilus_preferences_enum_add_entry (FRUIT_PREFERENCE,
					     "bannana",
					     "Bannana",
					     FRUIT_BANNANA);

	nautilus_preferences_set_enum (FRUIT_PREFERENCE,
				       FRUIT_BANNANA);
}
