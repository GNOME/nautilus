#ifndef NAUTILUS_URI_MAP_H
#define NAUTILUS_URI_MAP_H 1

#include "ntl-types.h"

NautilusNavigationInfo *nautilus_navinfo_new(NautilusNavigationInfo *navinfo, NautilusLocationReference uri,
					     NautilusLocationReference referring_uri,
					     NautilusLocationReference actual_referring_uri,
					     const char *referring_content_type);
void nautilus_navinfo_free(NautilusNavigationInfo *navinfo);

#endif
