#include <WWWCore.h>
#include <WWWStream.h>
#include <WWWTrans.h>
#include <WWWHTTP.h>
#include <WWWMIME.h>
#include <WWWFTP.h>
#include <WWWFile.h>
#include <WWWGophe.h>
#include <WWWZip.h>

/* clean up the cpp namespace -- libwww is particularly dirty */
#undef PACKAGE
#undef VERSION
#undef _

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glibwww.h"
#include <gnome.h>
#include <libnautilus/libnautilus.h>
#include <gtkhtml/gtkhtml.h>

extern GtkHTMLStreamHandle gtk_html_stream_ref(GtkHTMLStreamHandle handle);
extern void gtk_html_stream_unref(GtkHTMLStreamHandle handle);

typedef struct {
  NautilusViewClient *vc;
  GtkWidget *htmlw;
  char *base_url, *base_target_url;
} BrowserInfo;

static char *
canonicalize_url (const char *in_url, const char *base_url)
{
	char *ctmp, *ctmp2, *retval, *removebegin, *removeend, *curpos;

	g_return_val_if_fail(in_url, NULL);

	ctmp = strstr(in_url, "://");
	if(ctmp)
	{
		retval = g_strdup(in_url);
		goto out;
	}
	else if(*in_url == '/')
	{
		ctmp = base_url?strstr(base_url, "://"):NULL;
		if(!ctmp)
		{
			retval = g_strconcat("file://", in_url, NULL);
			goto out;
		}

		ctmp2 = strchr(ctmp + 3, '/');

		retval = g_strconcat(base_url, in_url, NULL);
		goto out;
	}

	/* XXX TODO - We should really do processing of .. and . in URLs */

	ctmp = base_url?strstr(base_url, "://"):NULL;
	if(!ctmp)
	{
		char *cwd;

		cwd = g_get_current_dir();
		ctmp = g_strconcat("file://", cwd, "/", in_url, NULL);
		g_free(cwd);

		retval = ctmp;
		goto out;
	}

	retval = g_strconcat(base_url, "/", in_url, NULL);

 out:
	/* Now fix up the /. and /.. pieces */

	ctmp = strstr(retval, "://");
	g_assert(ctmp);
	ctmp += 3;
	ctmp = strchr(ctmp, '/');
	if(!ctmp) {
		ctmp = retval;
		retval = g_strconcat(retval, "/", NULL);
		g_free(ctmp);
		return retval;
	}

	removebegin = removeend = NULL;
	do {
		if(removebegin && removeend)
		{
			memmove(removebegin, removeend, strlen(removeend) + 1);
			removebegin = removeend = NULL;
		}
		curpos = ctmp;

	redo:
		ctmp2 = strstr(curpos, "/.");
		if(!ctmp2)
			break;

		if(*(ctmp2 + 2) == '.') /* We have to skip over stuff like /...blahblah or /.foo */
		{
			if(*(ctmp2 + 3) != '/'
			   && *(ctmp2 + 3) != '\0')
			{
				curpos = ctmp2 + 3;
				goto redo;
			}
		}
		else if(*(ctmp2 + 2) != '/' && *(ctmp2 + 2) != '\0')
		{
			curpos = ctmp2 + 2;
			goto redo;
		}

		switch(*(ctmp2+2))
		{
		case '/':
		case '\0':
			removebegin = ctmp2;
			removeend = ctmp2 + 2;
			break;
		case '.':
			removeend = ctmp2 + 3;
			ctmp2--;
			while((ctmp2 >= ctmp) && *ctmp2 != '/')
				ctmp2--;
			if(*ctmp2 == '/')
				removebegin = ctmp2;
			break;
		}

	} while(removebegin);

	return retval;
}

static void
browser_url_load_done(GtkWidget *htmlw, BrowserInfo *bi)
{
  Nautilus_StatusRequestInfo sri;

  gtk_html_calc_scrollbars(GTK_HTML(bi->htmlw));

  memset(&sri, 0, sizeof(sri));
  sri.status_string = _("Load done.");
  nautilus_view_client_request_status_change(bi->vc, &sri);
}

struct _HTStream {
  const HTStreamClass *	isa;
  BrowserInfo *bi;
  GtkHTMLStreamHandle handle;
};

static int netin_stream_write (HTStream * me, const char * s, int l)
{
  gtk_html_write(GTK_HTML(me->bi->htmlw), me->handle, s, l);

  return HT_OK;
}

static int netin_stream_put_character (HTStream * me, char c)
{
  return netin_stream_write(me, &c, 1);
}

static int netin_stream_put_string (HTStream * me, const char * s)
{
  return netin_stream_write(me, s, strlen(s));
}

static int netin_stream_flush (HTStream * me)
{
  return HT_OK;
}

static int netin_stream_free (HTStream * me)
{
  g_return_val_if_fail(me->handle, HT_ERROR);

  g_message("netin_stream_free");
  gtk_html_end(GTK_HTML(me->bi->htmlw), me->handle, GTK_HTML_STREAM_OK);
  me->handle = NULL;
  g_free(me);

  return HT_OK;
}

static int netin_stream_abort (HTStream * me, HTList * e)
{
  g_message("netin_stream_abort");
  gtk_html_end(GTK_HTML(me->bi->htmlw), me->handle, GTK_HTML_STREAM_ERROR);
  g_free(me);

  return HT_OK;
}

static const HTStreamClass netin_stream_class =
{		
    "netin_stream",
    netin_stream_flush,
    netin_stream_free,
    netin_stream_abort,
    netin_stream_put_character,
    netin_stream_put_string,
    netin_stream_write
}; 

static HTStream *
netin_stream_new (BrowserInfo *bi, GtkHTMLStreamHandle handle)
{
  HTStream *retval;

  retval = g_new0(HTStream, 1);

  retval->isa = &netin_stream_class;
  retval->bi = bi;
  retval->handle = handle;

  return retval;
}

static gboolean
do_request_delete(gpointer req)
{
  HTRequest_delete(req);

  return FALSE;
}

static int
request_terminator (HTRequest * request, HTResponse * response, void * param, int status) {
  if (status != HT_LOADED) 
    g_print("Load couldn't be completed successfully (%p)\n", request);

  g_idle_add(do_request_delete, request);

  return HT_OK;
}


static void
browser_url_requested(GtkWidget *htmlw, const char *url, GtkHTMLStreamHandle handle, BrowserInfo *bi)
{
  char *real_url;
  HTRequest *request;
  HTStream *writer;

  real_url = canonicalize_url(url, bi->base_url);

  request = HTRequest_new();
  writer = netin_stream_new(bi, handle);
  HTRequest_setContext(request, writer);
  HTRequest_setOutputFormat(request, WWW_SOURCE);
  HTRequest_setOutputStream(request, writer);
  HTRequest_setAnchor(request, HTAnchor_findAddress(real_url));

  if(HTLoad(request, NO) == NO)
    {
      HTRequest_delete(request);
      gtk_html_end(GTK_HTML(bi->htmlw), handle, GTK_HTML_STREAM_ERROR);
    }

  g_free(real_url);
}

static void
browser_set_base(GtkWidget *htmlw, const char *base_url, BrowserInfo *bi)
{
  if(bi->base_url)
    g_free(bi->base_url);
  bi->base_url = g_strdup(base_url);
}

static void
browser_set_base_target(GtkWidget *htmlw, const char *base_target_url, BrowserInfo *bi)
{
  if(bi->base_target_url)
    g_free(bi->base_target_url);
  bi->base_target_url = g_strdup(base_target_url);
}

static void
browser_goto_url_real(GtkWidget *htmlw, const char *url, BrowserInfo *bi)
{
  HTNet_killAll();
  g_free(bi->base_url);
  g_free(bi->base_target_url);

  if(url[strlen(url) - 1] == '/')
    {
      bi->base_url = g_strdup(url);
      bi->base_target_url = g_strdup(url);
    }
  else
    {
      bi->base_url = g_dirname(url);
      bi->base_target_url = g_dirname(url);
    }

  gtk_html_begin(GTK_HTML(bi->htmlw), url);
  gtk_html_parse(GTK_HTML(bi->htmlw));
}

static void
browser_goto_url(GtkWidget *htmlw, const char *url, BrowserInfo *bi)
{
  Nautilus_NavigationRequestInfo nri;
  char *real_url;

  real_url = canonicalize_url(url, bi->base_target_url);

  g_return_if_fail(real_url);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = real_url;
  nautilus_view_client_request_location_change(bi->vc, &nri);
  browser_goto_url_real(htmlw, real_url, bi);
  g_free(real_url);
}

static void
browser_notify_location_change(NautilusViewClient *vc, Nautilus_NavigationInfo *ni, BrowserInfo *bi)
{
  if(ni->self_originated)
    return;

  browser_goto_url_real(NULL, ni->requested_uri, bi);
}

static int object_count = 0;

static void
browser_do_destroy(GtkObject *obj)
{
  object_count--;
  if(object_count <= 0)
    gtk_main_quit();
}

static GnomeObject *
make_obj(GnomeGenericFactory *Factory, const char *goad_id, void *closure)
{
  BrowserInfo *bi;
  GtkWidget *wtmp;

  if(strcmp(goad_id, "ntl_web_browser"))
    return NULL;

  bi = g_new0(BrowserInfo, 1);
  bi->vc = NAUTILUS_VIEW_CLIENT(gtk_widget_new(nautilus_content_view_client_get_type(), NULL));
  gtk_signal_connect(GTK_OBJECT(bi->vc), "notify_location_change", browser_notify_location_change,
		     bi);
  gtk_signal_connect(GTK_OBJECT(bi->vc), "destroy", browser_do_destroy, NULL);
  object_count++;

  bi->htmlw = gtk_html_new();
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "link_clicked", browser_goto_url, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base", browser_set_base, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "load_done", browser_url_load_done, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base", browser_set_base, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base_target", browser_set_base_target, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "url_requested", browser_url_requested, bi);

  wtmp = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(bi->vc), wtmp);

  gtk_container_add(GTK_CONTAINER(wtmp), bi->htmlw);
  gtk_widget_show(bi->htmlw);
  gtk_widget_show(wtmp);
  gtk_widget_show(GTK_WIDGET(bi->vc));

  return nautilus_view_client_get_gnome_object(bi->vc);
}

int main(int argc, char *argv[])
{
  GnomeGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table("ntl-web-browser", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  gdk_rgb_init();
  glibwww_init("ntl-web-browser", VERSION);
  HTNet_addAfter(request_terminator, NULL, NULL, HT_ALL, HT_FILTER_LAST);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  factory = gnome_generic_factory_new_multi("ntl_web_browser_factory", make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}
