
#ifndef FAKE_STOCK_H
#define FAKE_STOCK_H

#include <gtk/gtkwidget.h>

#define GNOME_PAD          8
#define GNOME_PAD_SMALL    4
#define GNOME_PAD_BIG      12

#define GNOME_STOCK_BUTTON_OK     "Button_Ok"
#define GNOME_STOCK_BUTTON_CANCEL "Button_Cancel"
#define GNOME_STOCK_BUTTON_YES    "Button_Yes"
#define GNOME_STOCK_BUTTON_NO     "Button_No"

extern char * gnome_question_xpm[];
extern char * stock_right_arrow_xpm[];
extern char * stock_left_arrow_xpm[];
extern char * stock_button_cancel_xpm[];
extern char * stock_button_apply_xpm[];
extern char * stock_button_no_xpm[];
extern char * stock_button_yes_xpm[];
extern char * stock_button_ok_xpm[];


GtkWidget *fake_stock_pixmap_new_from_xpm_data (char **data);

GtkWidget *fake_stock_pixmap_button (GtkWidget *pixmap, const char *text);

GtkWidget *fake_stock_or_ordinary_button (const char *button_name);

#endif /* FAKE_STOCK_H */

