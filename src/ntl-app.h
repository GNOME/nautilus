#ifndef NTL_APP_H
#define NTL_APP_H 1

#include "ntl-window.h"

void nautilus_app_exiting(void);
void nautilus_app_init(const char *initial_url);
NautilusWindow *nautilus_app_create_window(void);

#endif
