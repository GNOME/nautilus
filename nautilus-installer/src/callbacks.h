#include <gnome.h>


void
druid_cancel                           (GnomeDruid      *gnomedruid,
                                        gpointer         user_data);

void
druid_finish                           (GnomeDruidPage  *gnomedruidpage,
                                        gpointer         arg1,
                                        gpointer         user_data);

void
begin_install                          (GtkButton       *button,
                                        gpointer         user_data);

void
prep_install                           (GnomeDruidPage  *gnomedruidpage,
                                        gpointer         arg1,
                                        gpointer         user_data);

void set_white_stuff (GtkWidget *w);

void set_images  (GtkWidget *window);

