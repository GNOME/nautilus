#include "ntl-uri-map.h"
#include <libgnorba/gnorba.h>

NautilusNavigationInfo *
nautilus_navinfo_new(NautilusNavigationInfo *navinfo, NautilusLocationReference uri,
		     NautilusLocationReference referring_uri,
		     NautilusLocationReference actual_referring_uri,
		     const char *referring_content_type)
{
  navinfo->requested_uri = uri;
  navinfo->referring_uri = referring_uri;
  navinfo->actual_referring_uri = actual_referring_uri;
  navinfo->referring_content_type = (char *)referring_content_type;

  /* XXX turn the provided information into some activateable IID's */

  return NULL;
}

void
nautilus_navinfo_free(NautilusNavigationInfo *navinfo)
{
}
