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
#include <liboaf/liboaf.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-debug.h>
/*Include the GConf main header. */
#include <gconf/gconf.h>

typedef struct {
  NautilusView *nautilus_view;
  GtkWidget *htmlw;
  char *base_url, *base_target_url;

  int prevsel;

  HTMethod method;
  char *post_data;
} BrowserInfo;

typedef struct {
  GtkHTMLStream *sh;
  BrowserInfo *bi;
  char *url;
  HTStream *stream;
} VFSHandle;

static void do_vfs_load(VFSHandle *handle);

static char *
canonicalize_url (const char *in_url, const char *base_url)
{
  char *ctmp, *ctmp2, *retval, *removebegin, *removeend, *curpos;
  gboolean trailing_slash = FALSE;

  g_return_val_if_fail(in_url, NULL);

  if(base_url && base_url[strlen(base_url)-1] == '/')
    trailing_slash = TRUE;

  ctmp = strstr(in_url, "://");
  if(ctmp)
    {
      retval = g_strdup(in_url);
      goto out;
    }
  else if(*in_url == '/')
    {
      int inc = 0;

      if(trailing_slash)
	inc++;

      ctmp = base_url?strstr(base_url, "://"):NULL;
      if(!ctmp)
	{
	  retval = g_strconcat("file://", in_url, NULL);
	  goto out;
	}

      ctmp2 = strchr(ctmp + 3, '/');

      retval = g_strconcat(base_url, in_url+inc, NULL);
      goto out;
    }

  /* XXX TODO - We should really do processing of .. and . in URLs */

  ctmp = base_url?strstr(base_url, "://"):NULL;
  if(!ctmp)
    {
      char *cwd;

      if(in_url)
	{
	  ctmp = strchr(in_url, ':');
	  if(ctmp) /* OK, it's some funky URL scheme without any /'s */
	    return g_strdup(in_url);
	}

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
  if(!ctmp)
    return retval;

  ctmp += 3;
  ctmp = strchr(ctmp, '/');
  if(!ctmp) {
    ctmp = retval;
    retval = g_strconcat(retval, trailing_slash?"":"/", NULL);
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
  nautilus_view_report_load_complete(bi->nautilus_view);
}

struct _HTStream {
  const HTStreamClass *	isa;
  BrowserInfo *bi;
  GtkHTMLStream *handle;
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
  if(me->handle)
    gtk_html_end(GTK_HTML(me->bi->htmlw), me->handle, GTK_HTML_STREAM_OK);
  g_free(me);

  return HT_OK;
}

static int netin_stream_abort (HTStream * me, HTList * e)
{
  if(me->handle)
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
netin_stream_new (BrowserInfo *bi, GtkHTMLStream *handle)
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
  VFSHandle *vfsh;

  d = HTRequest_context(request);
  g_return_val_if_fail(d, HT_OK);
  vfsh = d;

  if (status < 0)
    {
      g_print("Load couldn't be completed successfully (%p)\n", request);
      vfsh->stream->handle = NULL;
      vfsh->stream = NULL;
      do_vfs_load(vfsh);
    }
  else
    {
      g_free(vfsh->url);
      g_free(vfsh);
    }

  HTRequest_setContext(request, NULL);
  g_idle_add(do_request_delete, request);

  return HT_OK;
}

static int
browser_do_post(HTRequest *request, HTStream *stream)
{
  VFSHandle *vfsh = HTRequest_context(request);
  int status;

  g_assert(vfsh);

  status = (*stream->isa->put_block)(stream, vfsh->bi->post_data, strlen(vfsh->bi->post_data));

  g_message("browser_do_post got status %d", status);

  switch(status)
    {
    case HT_LOADED:
    case HT_OK:
      g_free(vfsh->bi->post_data); vfsh->bi->post_data = NULL;
      (*stream->isa->flush)(stream);
    default:
      return status;
      break;
    }
}

static char vfs_read_buf[40960];

static void
browser_vfs_read_callback (GnomeVFSAsyncHandle *h, GnomeVFSResult res, gpointer buffer,
			   GnomeVFSFileSize bytes_requested,
			   GnomeVFSFileSize bytes_read,
			   gpointer data)
{
  VFSHandle *vfsh = data;
  
  g_message("browser_vfs_read_callback: %ld/%ld bytes", (long) bytes_read, (long) bytes_requested);
  if(bytes_read != 0)
    {
      gtk_html_write(GTK_HTML(vfsh->bi->htmlw), vfsh->sh, buffer, bytes_read);
    }

  if(bytes_read == 0 || res != GNOME_VFS_OK)
    {
      gtk_html_end(GTK_HTML(vfsh->bi->htmlw), vfsh->sh, GTK_HTML_STREAM_OK);
      gnome_vfs_async_close(h, (GnomeVFSAsyncCloseCallback)gtk_true, NULL);
      g_free(vfsh);
      return;
    }

  gnome_vfs_async_read(h, vfs_read_buf, sizeof(vfs_read_buf), browser_vfs_read_callback, data);
}

static void
browser_vfs_callback(GnomeVFSAsyncHandle *h, GnomeVFSResult res, gpointer data)
{
  VFSHandle *vfsh = data;

  g_message("browser_vfs_callback, res was %s", gnome_vfs_result_to_string(res));

  if(res != GNOME_VFS_OK)
    {
      nautilus_view_report_load_failed(vfsh->bi->nautilus_view);
      gtk_html_end(GTK_HTML(vfsh->bi->htmlw), vfsh->sh, GTK_HTML_STREAM_ERROR);
      g_free(vfsh);
    }
  else
    {
      gnome_vfs_async_read(h, vfs_read_buf, sizeof(vfs_read_buf), browser_vfs_read_callback, vfsh);
    }
}

static void
do_vfs_load(VFSHandle *vfsh)
{
  GnomeVFSAsyncHandle *ah;

  g_warning("Falling back to gnome-vfs for %s", vfsh->url);

  gnome_vfs_async_open(&ah, vfsh->url, GNOME_VFS_OPEN_READ, browser_vfs_callback, vfsh);
}

static void
browser_url_requested(GtkWidget *htmlw, const char *url, GtkHTMLStream *handle, BrowserInfo *bi)
{
  char *real_url;
  HTRequest *request;
  HTStream *writer;
  HTAnchor *anchor;
  VFSHandle *vfsh;

  real_url = canonicalize_url(url, bi->base_url);

  vfsh = g_new0(VFSHandle, 1);
  vfsh->sh = handle;
  vfsh->bi = bi;
  vfsh->url = real_url;

  anchor = HTAnchor_findAddress(real_url);
  if(!anchor)
    {
      do_vfs_load(vfsh);
      return;
    }

  request = HTRequest_new();
  HTRequest_setContext(request, vfsh);
  writer = netin_stream_new(bi, handle);
  vfsh->stream = writer;
  HTRequest_setOutputFormat(request, WWW_SOURCE);
  HTRequest_setOutputStream(request, writer);
  if(bi->method == METHOD_POST)
    HTRequest_setPostCallback(request, browser_do_post);
  HTRequest_setAnchor(request, anchor);
  HTRequest_setMethod(request, bi->method);
  bi->method = METHOD_GET;

  if(HTLoad(request, NO) == NO)
    {
      g_warning("Load failed");
      do_vfs_load(vfsh);
      writer->handle = NULL;
      HTRequest_delete(request);
    }
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
  GtkHTMLStream *stream;

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

  stream = gtk_html_begin(GTK_HTML(bi->htmlw));

  browser_url_requested(bi->htmlw, url, stream, bi);

  nautilus_view_report_load_underway(bi->nautilus_view);
}

static void
browser_goto_url(GtkWidget *htmlw, const char *url, BrowserInfo *bi)
{
  char *real_url;

  real_url = canonicalize_url(url, bi->base_target_url);

  g_return_if_fail(real_url);

  nautilus_view_report_location_change(bi->nautilus_view, real_url);
  browser_goto_url_real(htmlw, real_url, bi);
  g_free(real_url);
}

static void
browser_select_url(GtkWidget *htmlw, const char *url, BrowserInfo *bi)
{
  GList *list;
  GList simple_list;
  char *real_url;

  list = NULL;
  real_url = NULL;
  if(url && !bi->prevsel)
    {
      real_url = canonicalize_url(url, bi->base_target_url);
      simple_list.data = real_url;
      simple_list.next = NULL;
      simple_list.prev = NULL;
      list = &simple_list;
    }

  nautilus_view_report_selection_change(bi->nautilus_view, list);
  if (real_url != NULL)
    nautilus_view_report_status(bi->nautilus_view, real_url);
  g_free(real_url);
  bi->prevsel = url?1:0;
}

static void
browser_title_changed(GtkWidget *htmlw, const char *new_title, BrowserInfo *bi)
{
  nautilus_view_set_title(bi->nautilus_view, new_title);
}

static void
browser_submit(GtkWidget *htmlw, const char *method, const char *url, const char *encoding, BrowserInfo *bi)
{
  g_free(bi->post_data); bi->post_data = NULL;

  if(!g_strcasecmp(method, "POST"))
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
browser_load_location(NautilusView *nautilus_view, 
		      const char *location,
		      BrowserInfo *bi)
{
  browser_goto_url_real(NULL, location, bi);
}

static int object_count = 0;

static void
browser_do_destroy(GtkObject *obj)
{
  object_count--;
  if(object_count <= 0)
    gtk_main_quit();
}

static BonoboObject *
make_obj(BonoboGenericFactory *Factory, const char *goad_id, void *closure)
{
  BrowserInfo *bi;
  GtkWidget *wtmp;

  if(strcmp(goad_id, "OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b"))
    return NULL;

  bi = g_new0(BrowserInfo, 1);

  bi->htmlw = gtk_html_new();
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "link_clicked", browser_goto_url, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base", browser_set_base, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "load_done", browser_url_load_done, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base", browser_set_base, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "set_base_target", browser_set_base_target, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "url_requested", browser_url_requested, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "on_url", browser_select_url, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "submit", browser_submit, bi);
  gtk_signal_connect(GTK_OBJECT(bi->htmlw), "title_changed", browser_title_changed, bi);

  wtmp = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(wtmp), bi->htmlw);
  gtk_widget_show(bi->htmlw);
  gtk_widget_show(wtmp);

  bi->nautilus_view = nautilus_view_new (wtmp);

  gtk_signal_connect(GTK_OBJECT(bi->nautilus_view), "load_location",
		     browser_load_location, bi);
  object_count++;
  gtk_signal_connect(GTK_OBJECT(bi->nautilus_view), "destroy",
		     browser_do_destroy, NULL);

  return BONOBO_OBJECT (bi->nautilus_view);
}

int main(int argc, char *argv[])
{
  BonoboGenericFactory *factory;
  CORBA_ORB orb;
  GConfError *error;

  if (g_getenv("NAUTILUS_DEBUG") != NULL)
    nautilus_make_warnings_and_criticals_stop_in_debugger
      (G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", 
       "Nautilus-HTML", "gtkhtml", NULL);


  /* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);
#endif
	
  gnome_init_with_popt_table("ntl-web-browser", _VERSION, 
			     argc, argv,
			     oaf_popt_options, 0, NULL); 
  
  orb = oaf_init (argc, argv);
  /* Init the GConf library.*/
  gconf_init (argc, argv, &error);
  gnome_vfs_init();
  gdk_rgb_init();
  glibwww_init("ntl-web-browser", _VERSION);
  HTNet_addAfter(request_terminator, NULL, NULL, HT_ALL, HT_FILTER_LAST);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  factory = bonobo_generic_factory_new_multi("OAFIID:ntl_web_browser_factory:e553fd3e-101d-445d-ae53-a3a59e77fcc9", 
					     make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}
