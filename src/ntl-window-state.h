#ifndef NTL_WINDOW_STATE_H
#define NTL_WINDOW_STATE_H 1

#include "ntl-window.h"

void nautilus_window_save_state(NautilusWindow *window, const char *config_path);
void nautilus_window_load_state(NautilusWindow *window, const char *config_path);
void nautilus_window_set_initial_state(NautilusWindow *window);

#endif
