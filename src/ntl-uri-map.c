#include "ntl-uri-map.h"
#include <libgnorba/gnorba.h>

NautilusNavigationInfo *
nautilus_navinfo_new(NautilusNavigationInfo *navinfo,
		     Nautilus_NavigationRequestInfo *nri,
		     NautilusLocationReference referring_uri,
		     NautilusLocationReference actual_referring_uri,
		     const char *referring_content_type,
		     GtkWidget *requesting_view)
{
  memset(navinfo, 0, sizeof(*navinfo));

  navinfo->navinfo.requested_uri = nri->requested_uri;
  navinfo->navinfo.referring_uri = referring_uri;
  navinfo->navinfo.actual_referring_uri = actual_referring_uri;
  navinfo->navinfo.referring_content_type = (char *)referring_content_type;

  navinfo->requesting_view = requesting_view;

  /* XXX turn the provided information into some activateable IID's */

  return NULL;
}

void
nautilus_navinfo_free(NautilusNavigationInfo *navinfo)
{
}
