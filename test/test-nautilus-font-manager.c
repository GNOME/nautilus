
#include <config.h>

#include "test.h"

#include <libnautilus-extensions/nautilus-font-manager.h>

static void
font_iterator_callback (const char *foundry,
			const char *family,
			const char *weight,
			const char *slant,
			const char *set_width,
			const char *char_set_registry,
			const char *char_set_encoding,
			gpointer callback_data)
{

	g_print ("%s-%s-%s-%s-%s-%s-%s\n",
		 foundry,
		 family,
		 weight,
		 slant,
		 set_width,
		 char_set_registry,
		 char_set_encoding);
}

int 
main (int argc, char* argv[])
{
	test_init (&argc, &argv);

	nautilus_font_manager_for_each_font (font_iterator_callback, NULL);
	
	return test_quit (EXIT_SUCCESS);
}
