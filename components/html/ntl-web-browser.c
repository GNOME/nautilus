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
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <gnome.h>
#include <libnautilus/libnautilus.h>
#include <gtkhtml/gtkhtml.h>

extern GtkHTMLStreamHandle gtk_html_stream_ref(GtkHTMLStreamHandle handle);
extern void gtk_html_stream_unref(GtkHTMLStreamHandle handle);

typedef struct {
  NautilusViewFrame *view_frame;
  GtkWidget *htmlw;
  char *base_url, *base_target_url;

  int prevsel;

  HTMethod method;
  char *post_data;
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
  Nautilus_ProgressRequestInfo pri;

  gtk_html_calc_scrollbars(GTK_HTML(bi->htmlw));

  memset(&pri, 0, sizeof(pri));

  pri.type = Nautilus_PROGRESS_DONE_OK;
  pri.amount = 100.0;
  nautilus_view_frame_request_progress_change(bi->view_frame, &pri);
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
request_terminator (HTRequest * request, HTResponse * response, void * param, int status)
{
  gpointer d;

  if (status != HT_LOADED) 
    g_print("Load couldn't be completed successfully (%p)\n", request);

  d = HTRequest_context(request);
  g_return_val_if_fail(d, HT_OK);

  HTRequest_setContext(request, NULL);
  g_idle_add(do_request_delete, request);

  return HT_OK;
}

static int
browser_do_post(HTRequest *request, HTStream *stream)
{
  BrowserInfo *bi = HTRequest_context(request);
  int status;

  g_assert(bi);

  status = (*stream->isa->put_block)(stream, bi->post_data, strlen(bi->post_data));

  g_message("browser_do_post got status %d", status);

  switch(status)
    {
    case HT_LOADED:
    case HT_OK:
      g_free(bi->post_data); bi->post_data = NULL;
      (*stream->isa->flush)(stream);
    default:
      return status;
      break;
    }
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
  HTRequest_setContext(request, bi);
  HTRequest_setOutputFormat(request, WWW_SOURCE);
  HTRequest_setOutputStream(request, writer);
  if(bi->method == METHOD_POST)
    HTRequest_setPostCallback(request, browser_do_post);
  HTRequest_setAnchor(request, HTAnchor_findAddress(real_url));
  HTRequest_setMethod(request, bi->method);
  bi->method = METHOD_GET;

  if(HTLoad(request, NO) == NO)
    {
      HTRequest_delete(request);
      /* I think deleting the request will end the stream, too */
      /* gtk_html_end(GTK_HTML(bi->htmlw), handle, GTK_HTML_STREAM_ERROR); */
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
  Nautilus_ProgressRequestInfo pri;

  pri.type = Nautilus_PROGRESS_UNDERWAY;
  pri.amount = 0.0;

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
  nautilus_view_frame_request_progress_change(bi->view_frame, &pri);
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
  nautilus_view_frame_request_location_change(bi->view_frame, &nri);
  browser_goto_url_real(htmlw, real_url, bi);
  g_free(real_url);
}

static void
browser_select_url(GtkWidget *htmlw, const char *url, BrowserInfo *bi)
{
  Nautilus_SelectionRequestInfo si;
  char *real_url = NULL;

  memset(&si, 0, sizeof(si));
  if(url && !bi->prevsel)
    {
      si.selected_uris._length = 1;
      real_url = canonicalize_url(url, bi->base_target_url);
      si.selected_uris._buffer = &real_url;
    }
  else if(!url && bi->prevsel)
    {
      si.selected_uris._length = 0;
    }

  nautilus_view_frame_request_selection_change(bi->view_frame, &si);
  g_free(real_url);
  bi->prevsel = url?1:0;
}

static void
browser_submit(GtkWidget *htmlw, const char *method, const char *url, const char *encoding, BrowserInfo *bi)
{
  g_free(bi->post_data); bi->post_data = NULL;

  if(!strcasecmp(method, "POST"))
    {
      char **pieces = g_strsplit(encoding, "&", -1);

      if(pieces)
	{
	  char *ctmp;
	  ctmp = g_strjoinv("\r\n", pieces);
	  bi->post_data = g_strconcat(ctmp, "\r\n", NULL);
	  g_free(ctmp);
	  g_strfreev(pieces);
	  bi->method = METHOD_POST;
	}

      browser_goto_url(htmlw, url, bi);
    }
  else
    {
      char tmp_url[4096];

      g_snprintf(tmp_url, sizeof(tmp_url), "%s?%s", url, encoding);
      browser_goto_url(htmlw, tmp_url, bi);
    }
}

static void
browser_notify_location_change(NautilusViewFrame *view_frame, Nautilus_NavigationInfo *ni, BrowserInfo *bi)
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
  bi->view_frame = NAUTILUS_VIEW_FRAME(gtk_widget_new(nautilus_content_view_frame_get_type(), NULL));
  gtk_signal_connect(GTK_OBJECT(bi->view_frame), "notify_location_change", browser_notify_location_change,
		     bi);
  gtk_signal_connect(GTK_OBJECT(bi->view_frame), "destroy", browser_do_destroy, NULL);
  object_count++;

  bi->htmlw = gtk_html_new();
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "link_clicked", browser_goto_url, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base", browser_set_base, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "load_done", browser_url_load_done, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base", browser_set_base, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base_target", browser_set_base_target, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "url_requested", browser_url_requested, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "on_url", browser_select_url, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "submit", browser_submit, bi);

  wtmp = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(bi->view_frame), wtmp);

  gtk_container_add(GTK_CONTAINER(wtmp), bi->htmlw);
  gtk_widget_show(bi->htmlw);
  gtk_widget_show(wtmp);
  gtk_widget_show(GTK_WIDGET(bi->view_frame));

  return nautilus_view_frame_get_gnome_object(bi->view_frame);
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
