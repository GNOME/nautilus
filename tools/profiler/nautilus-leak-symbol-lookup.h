#ifndef NAUTILUS_LEAK_SYMBOL_LOOKUP__
#define NAUTILUS_LEAK_SYMBOL_LOOKUP__

#include <string>

void nautilus_leak_print_symbol_address (const char *app_path, void *address);
void get_function_at_address (const char *app_path, void *address, string &result);

#endif
