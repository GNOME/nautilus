/*
 * gnome-thumbnail.c: Utilities for handling thumbnails
 *
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include <pthread.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-init.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "gnome-thumbnail.h"

#ifdef HAVE_LIBJPEG
GdkPixbuf * _gnome_thumbnail_load_scaled_jpeg (const char *uri,
					       int         target_width,
					       int         target_height);
#endif

#define SECONDS_BETWEEN_STATS 10

struct ThumbMD5Context {
	guint32 buf[4];
	guint32 bits[2];
	unsigned char in[64];
};

static void thumb_md5 (const char *string, unsigned char digest[16]);

struct _GnomeThumbnailFactoryPrivate {
  char *application;
  GnomeThumbnailSize size;

  GHashTable *existing_thumbs;
  time_t read_existing_mtime;
  long last_existing_time;
  
  GHashTable *failed_thumbs;
  time_t read_failed_mtime;
  long last_failed_time;

  pthread_mutex_t lock;

  GHashTable *scripts_hash;
};

struct ThumbnailInfo {
  time_t mtime;
  char *uri;
};

GNOME_CLASS_BOILERPLATE (GnomeThumbnailFactory,
			 gnome_thumbnail_factory,
			 GObject, G_TYPE_OBJECT);

static void gnome_thumbnail_factory_instance_init (GnomeThumbnailFactory      *factory);
static void gnome_thumbnail_factory_class_init    (GnomeThumbnailFactoryClass *class);


static void
thumbnail_info_free (gpointer data)
{
  struct ThumbnailInfo *info = data;

  if (info)
    {
      g_free (info->uri);
      g_free (info);
    }
}

static void
gnome_thumbnail_factory_finalize (GObject *object)
{
  GnomeThumbnailFactory *factory;
  GnomeThumbnailFactoryPrivate *priv;
  
  factory = GNOME_THUMBNAIL_FACTORY (object);

  priv = factory->priv;
  
  g_free (priv->application);
  priv->application = NULL;
  
  if (priv->existing_thumbs)
    {
      g_hash_table_destroy (priv->existing_thumbs);
      priv->existing_thumbs = NULL;
    }
  
  if (priv->failed_thumbs)
    {
      g_hash_table_destroy (priv->failed_thumbs);
      priv->failed_thumbs = NULL;
    }

  if (priv->scripts_hash)
    {
      g_hash_table_destroy (priv->scripts_hash);
      priv->scripts_hash = NULL;
    }
  
  g_free (priv);
  factory->priv = NULL;
  
  if (G_OBJECT_CLASS (parent_class)->finalize)
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static guint
md5_hash (gconstpointer key)
{
  const char *digest = key;

  return *(guint *)digest;
}

static gboolean
md5_equal (gconstpointer  a,
	   gconstpointer  b)
{
  const char *digest_a = a;
  const char *digest_b = b;
  int i;

  for (i = 0; i < 16; i++)
    {
      if (digest_a[i] != digest_b[i])
	return FALSE;
    }

  return TRUE;
}

static void
read_scripts_file (GnomeThumbnailFactory *factory, const char *file)
{
  FILE *f;
  char buf[1024];
  char *p;
  gchar **mime_types;
  int i;
    
  f = fopen (file, "r");

  if (f)
    {
      while (fgets (buf, 1024, f) != NULL)
	{
	  if (buf[0] == '#')
	    continue;

	  p = strchr (buf, ':');

	  if (p == NULL)
	    continue;

	  *p++ = 0;
	  while (g_ascii_isspace (*p))
	    p++;
	  
	  mime_types = g_strsplit (buf, ",", 0);

	  for (i = 0; mime_types[i] != NULL; i++)
	    g_hash_table_insert (factory->priv->scripts_hash,
				 mime_types[i], g_strdup (p));

	  /* The mimetype strings are owned by the hash table now */
	  g_free (mime_types);
	}
      fclose (f);
    }
}

static void
read_scripts (GnomeThumbnailFactory *factory)
{
  char *file;
  
  read_scripts_file (factory, SYSCONFDIR "/gnome/thumbnailrc");

  file = g_build_filename (g_get_home_dir (),
			   GNOME_DOT_GNOME,
			   "thumbnailrc",
			   NULL);
  read_scripts_file (factory, file);
  g_free (file);
}

static void
gnome_thumbnail_factory_instance_init (GnomeThumbnailFactory *factory)
{
  factory->priv = g_new0 (GnomeThumbnailFactoryPrivate, 1);

  factory->priv->size = GNOME_THUMBNAIL_SIZE_NORMAL;
  factory->priv->application = g_strdup ("gnome-thumbnail-factory");
  
  factory->priv->existing_thumbs = g_hash_table_new_full (md5_hash,
							  md5_equal,
							  g_free, thumbnail_info_free);
  factory->priv->failed_thumbs = g_hash_table_new_full (md5_hash,
							md5_equal,
							g_free, NULL);
  
  factory->priv->scripts_hash = g_hash_table_new_full (g_str_hash,
						       g_str_equal,
						       g_free, g_free);


  read_scripts (factory);
  
  pthread_mutex_init (&factory->priv->lock, NULL);
}

static void
gnome_thumbnail_factory_class_init (GnomeThumbnailFactoryClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);
	
  gobject_class->finalize = gnome_thumbnail_factory_finalize;
}

GnomeThumbnailFactory *
gnome_thumbnail_factory_new (GnomeThumbnailSize size)
{
  GnomeThumbnailFactory *factory;
  
  factory = g_object_new (GNOME_TYPE_THUMBNAIL_FACTORY, NULL);
  
  factory->priv->size = size;
  
  return factory;
}

static char *
thumb_digest_to_ascii (unsigned char digest[16])
{
  static char hex_digits[] = "0123456789abcdef";
  unsigned char *res;
  int i;
  
  res = g_malloc (33);
  
  for (i = 0; i < 16; i++) {
    res[2*i] = hex_digits[digest[i] >> 4];
    res[2*i+1] = hex_digits[digest[i] & 0xf];
  }
  
  res[32] = 0;
  
  return res;
}

static void
thumb_digest_from_ascii (unsigned char *ascii, unsigned char digest[16])
{
  int i;

  for (i = 0; i < 16; i++)
    {
      digest[i] =
	g_ascii_xdigit_value (ascii[2*i]) << 4 |
	g_ascii_xdigit_value (ascii[2*i + 1]);
    }
}

static gboolean
remove_all (void)
{
  return TRUE;
}

static void
read_md5_dir (const char *path, GHashTable *hash_table)
{
  DIR *dir;
  struct dirent *dirent;
  char *digest;

  /* Remove all current thumbs */
  g_hash_table_foreach_remove (hash_table,
			       (GHRFunc) remove_all,
			       NULL);
  
  dir = opendir (path);

  if (dir)
    {
      while ((dirent = readdir (dir)) != NULL)
	{
	  if (strlen (dirent->d_name) == 36 &&
	      strcmp (dirent->d_name + 32, ".png") == 0)
	    {
	      digest = g_malloc (16);
	      thumb_digest_from_ascii (dirent->d_name, digest);
	      g_hash_table_insert (hash_table, digest, NULL);
	    }
	}
      closedir (dir);
    }
}

static void
gnome_thumbnail_factory_ensure_uptodate (GnomeThumbnailFactory *factory)
{
  char *path;
  struct timeval tv;
  struct stat statbuf;
  GnomeThumbnailFactoryPrivate *priv = factory->priv;

  if (priv->last_existing_time != 0)
    {
      gettimeofday (&tv, NULL);

      if (tv.tv_sec >= priv->last_existing_time &&
	  tv.tv_sec < priv->last_existing_time + SECONDS_BETWEEN_STATS)
	return;
    }

  path = g_build_filename (g_get_home_dir (),
			   ".thumbnails",
			   (priv->size == GNOME_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
			   NULL);
  
  if (stat(path, &statbuf) != 0)
    {
      g_free (path);
      return;
    }
  
  if (statbuf.st_mtime == priv->read_existing_mtime)
    {
      g_free (path);
      return;
    }

  priv->read_existing_mtime = statbuf.st_mtime;
  priv->last_existing_time = tv.tv_sec;

  read_md5_dir (path, priv->existing_thumbs);

  g_free (path);
}

static struct ThumbnailInfo *
load_thumbnail_info (const char *path)
{
  struct ThumbnailInfo *info;
  GdkPixbuf *pixbuf;
  const char *thumb_uri, *thumb_mtime_str;

  pixbuf = gdk_pixbuf_new_from_file (path, NULL);

  if (pixbuf == NULL)
    return NULL;
  
  info = g_new0 (struct ThumbnailInfo, 1);
  
  thumb_uri = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::URI");
  info->uri = g_strdup (thumb_uri);
  
  thumb_mtime_str = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::MTime");
  info->mtime = atol (thumb_mtime_str);

  g_object_unref (pixbuf);
  
  return info;
}

static void
gnome_thumbnail_factory_ensure_failed_uptodate (GnomeThumbnailFactory *factory)
{
  char *path;
  struct timeval tv;
  struct stat statbuf;
  GnomeThumbnailFactoryPrivate *priv = factory->priv;

  if (priv->last_failed_time != 0)
    {
      gettimeofday (&tv, NULL);

      if (tv.tv_sec >= priv->last_failed_time &&
	  tv.tv_sec < priv->last_failed_time + SECONDS_BETWEEN_STATS)
	return;
    }

  path = g_build_filename (g_get_home_dir (),
			   ".thumbnails/fail/",
			   factory->priv->application,
			   NULL);
  
  if (stat(path, &statbuf) != 0)
    {
      g_free (path);
      return;
    }
  
  if (statbuf.st_mtime == priv->read_failed_mtime)
    {
      g_free (path);
      return;
    }

  priv->read_failed_mtime = statbuf.st_mtime;
  priv->last_failed_time = tv.tv_sec;

  read_md5_dir (path, priv->failed_thumbs);

  g_free (path);
}

char *
gnome_thumbnail_factory_lookup (GnomeThumbnailFactory *factory,
				const char            *uri,
				time_t                 mtime)
{
  GnomeThumbnailFactoryPrivate *priv = factory->priv;
  unsigned char digest[16];
  char *path, *md5, *file;
  gpointer value;
  struct ThumbnailInfo *info;

  pthread_mutex_lock (&priv->lock);

  gnome_thumbnail_factory_ensure_uptodate (factory);

  thumb_md5 (uri, digest);

  if (g_hash_table_lookup_extended (priv->existing_thumbs,
				    digest, NULL, &value))
    {
      md5 = thumb_digest_to_ascii (digest);
      file = g_strconcat (md5, ".png", NULL);
      g_free (md5);
  
      path = g_build_filename (g_get_home_dir (),
			       ".thumbnails",
			       (priv->size == GNOME_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
			       file,
			       NULL);
      g_free (file);

      if (value == NULL)
	{
	  info = load_thumbnail_info (path);
	  if (info)
	    {
	      unsigned char *key;
	      key = g_malloc (16);
	      memcpy (key, digest, 16);
	      g_hash_table_insert (priv->existing_thumbs, key, info);
	    }
	}
      else
	info = value;

      if (info &&
	  info->mtime == mtime &&
	  strcmp (info->uri, uri) == 0)
	{
	  pthread_mutex_unlock (&priv->lock);
	  return path;
	}
      
      g_free (path);
    }

  pthread_mutex_unlock (&priv->lock);
  
  return NULL;
}

gboolean
gnome_thumbnail_factory_has_valid_failed_thumbnail (GnomeThumbnailFactory *factory,
						    const char            *uri,
						    time_t                 mtime)
{
  GnomeThumbnailFactoryPrivate *priv = factory->priv;
  unsigned char digest[16];
  char *path, *file, *md5;
  GdkPixbuf *pixbuf;
  gboolean res;

  res = FALSE;

  pthread_mutex_lock (&priv->lock);

  gnome_thumbnail_factory_ensure_failed_uptodate (factory);
  
  thumb_md5 (uri, digest);
  
  if (g_hash_table_lookup_extended (factory->priv->failed_thumbs,
				    digest, NULL, NULL))
    {
        md5 = thumb_digest_to_ascii (digest);
	file = g_strconcat (md5, ".png", NULL);
	g_free (md5);

	path = g_build_filename (g_get_home_dir (),
				 ".thumbnails/fail",
				 factory->priv->application,
				 file,
				 NULL);
	g_free (file);

	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	if (pixbuf)
	  {
	    res = gnome_thumbnail_is_valid (pixbuf, uri, mtime);
	    g_object_unref (pixbuf);
	  }
    }

  pthread_mutex_unlock (&priv->lock);
  
  return res;
}

static gboolean
mimetype_supported_by_gdk_pixbuf (const char *mime_type)
{
	guint i;
	static GHashTable *formats = NULL;
	static const char *types [] = {
	  "image/x-bmp", "image/x-ico", "image/jpeg", "image/gif",
	  "image/png", "image/pnm", "image/ras", "image/tga",
	  "image/tiff", "image/wbmp", "image/x-xbitmap",
	  "image/x-xpixmap"
	};

	if (!formats) {
		formats = g_hash_table_new (g_str_hash, g_str_equal);

		for (i = 0; i < G_N_ELEMENTS (types); i++)
			g_hash_table_insert (formats,
					     (gpointer) types [i],
					     GUINT_TO_POINTER (1));	
	}

	if (g_hash_table_lookup (formats, mime_type))
		return TRUE;

	return FALSE;
}


gboolean
gnome_thumbnail_factory_can_thumbnail (GnomeThumbnailFactory *factory,
				       const char            *uri,
				       const char            *mime_type,
				       time_t                 mtime)
{
  /* Don't thumbnail thumbnails */
  if (uri &&
      strncmp (uri, "file:/", 6) == 0 &&
      strstr (uri, "/.thumbnails/") != NULL)
    return FALSE;
  
  /* TODO: Replace with generic system */
  if (mime_type != NULL &&
      (mimetype_supported_by_gdk_pixbuf (mime_type) ||
       g_hash_table_lookup (factory->priv->scripts_hash, mime_type)))
    {
      return !gnome_thumbnail_factory_has_valid_failed_thumbnail (factory,
								  uri,
								  mtime);
    }
  
  return FALSE;
}

static char *
expand_thumbnailing_script (const char *script,
			    const char *inuri,
			    const char *outfile)
{
  GString *str;
  const char *p, *last;
  char *localfile, *quoted;
  gboolean got_in;

  str = g_string_new (NULL);
  
  got_in = FALSE;
  last = script;
  while ((p = strchr (last, '%')) != NULL)
    {
      g_string_append_len (str, last, p - last);
      p++;

      switch (*p) {
      case 'u':
	quoted = g_shell_quote (inuri);
	g_string_append (str, quoted);
	g_free (quoted);
	got_in = TRUE;
	p++;
	break;
      case 'i':
	localfile = gnome_vfs_get_local_path_from_uri (inuri);
	if (localfile)
	  {
	    quoted = g_shell_quote (localfile);
	    g_string_append (str, quoted);
	    got_in = TRUE;
	    g_free (quoted);
	    g_free (localfile);
	  }
	p++;
	break;
      case 'o':
	quoted = g_shell_quote (outfile);
	g_string_append (str, quoted);
	g_free (quoted);
	p++;
	break;
      case 's':
	g_string_append (str, "128");
	p++;
	break;
      case '%':
	g_string_append_c (str, '%');
	p++;
	break;
      case 0:
      default:
	break;
      }
      last = p;
    }
  g_string_append (str, last);

  if (got_in)
    return g_string_free (str, FALSE);

  g_string_free (str, TRUE);
  return NULL;
}


GdkPixbuf *
gnome_thumbnail_factory_generate_thumbnail (GnomeThumbnailFactory *factory,
					    const char            *uri,
					    const char            *mime_type)
{
  GdkPixbuf *pixbuf, *scaled;
  char *script, *expanded_script;
  int width, height, size;
  double scale;
  int exit_status;
  char tmpname[50];

  /* Doesn't access any volatile fields in factory, so it's threadsafe */
  
  size = 128;
  if (factory->priv->size == GNOME_THUMBNAIL_SIZE_LARGE)
    size = 256;

  pixbuf = NULL;
  
  script = g_hash_table_lookup (factory->priv->scripts_hash, mime_type);
  if (script)
    {
      int fd;

      strcpy (tmpname, "/tmp/.gnome_thumbnail.XXXXXX");

      fd = mkstemp(tmpname);

      if (fd)
	{
	  close (fd);

	  expanded_script = expand_thumbnailing_script (script, uri, tmpname);
	  if (expanded_script != NULL &&
	      g_spawn_command_line_sync (expanded_script,
					 NULL, NULL, &exit_status, NULL) &&
	      exit_status == 0)
	    {
	      pixbuf = gdk_pixbuf_new_from_file (tmpname, NULL);
	      g_free (expanded_script);
	    }
	  
	  unlink(tmpname);
	}
    }

  /* Fall back to gdk-pixbuf */
  if (pixbuf == NULL)
    {
#ifdef HAVE_LIBJPEG
      if (strcmp (mime_type, "image/jpeg") == 0)
	pixbuf = _gnome_thumbnail_load_scaled_jpeg (uri, size, size);
      else
#endif
	pixbuf = gnome_thumbnail_load_pixbuf (uri);
    }
      
  if (pixbuf == NULL)
    return NULL;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (width > size || height > size)
    {
      scale = (double)size / MAX (width, height);

      scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf,
						  floor (width * scale + 0.5),
						  floor (height * scale + 0.5));

      g_object_unref (pixbuf);
      pixbuf = scaled;
    }
  
  return pixbuf;
}

static gboolean
make_thumbnail_dirs (GnomeThumbnailFactory *factory)
{
  char *thumbnail_dir;
  char *image_dir;
  gboolean res;

  res = FALSE;

  thumbnail_dir = g_build_filename (g_get_home_dir (),
				    ".thumbnails",
				    NULL);
  if (!g_file_test (thumbnail_dir, G_FILE_TEST_IS_DIR))
    {
      mkdir (thumbnail_dir, 0700);
      res = TRUE;
    }

  image_dir = g_build_filename (thumbnail_dir,
				(factory->priv->size == GNOME_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
				NULL);
  if (!g_file_test (image_dir, G_FILE_TEST_IS_DIR))
    {
      mkdir (image_dir, 0700);
      res = TRUE;
    }

  g_free (thumbnail_dir);
  g_free (image_dir);
  
  return res;
}

static gboolean
make_thumbnail_fail_dirs (GnomeThumbnailFactory *factory)
{
  char *thumbnail_dir;
  char *fail_dir;
  char *app_dir;
  gboolean res;

  res = FALSE;

  thumbnail_dir = g_build_filename (g_get_home_dir (),
				    ".thumbnails",
				    NULL);
  if (!g_file_test (thumbnail_dir, G_FILE_TEST_IS_DIR))
    {
      mkdir (thumbnail_dir, 0700);
      res = TRUE;
    }

  fail_dir = g_build_filename (thumbnail_dir,
			       "fail",
			       NULL);
  if (!g_file_test (fail_dir, G_FILE_TEST_IS_DIR))
    {
      mkdir (fail_dir, 0700);
      res = TRUE;
    }

  app_dir = g_build_filename (fail_dir,
			      factory->priv->application,
			      NULL);
  if (!g_file_test (app_dir, G_FILE_TEST_IS_DIR))
    {
      mkdir (app_dir, 0700);
      res = TRUE;
    }

  g_free (thumbnail_dir);
  g_free (fail_dir);
  g_free (app_dir);
  
  return res;
}


void
gnome_thumbnail_factory_save_thumbnail (GnomeThumbnailFactory *factory,
					GdkPixbuf             *thumbnail,
					const char            *uri,
					time_t                 original_mtime)
{
  GnomeThumbnailFactoryPrivate *priv = factory->priv;
  unsigned char *digest;
  char *path, *md5, *file, *dir;
  char *tmp_path;
  int tmp_fd;
  char mtime_str[21];
  gboolean saved_ok;
  struct stat statbuf;
  struct ThumbnailInfo *info;
  
  pthread_mutex_lock (&priv->lock);
  
  gnome_thumbnail_factory_ensure_uptodate (factory);

  pthread_mutex_unlock (&priv->lock);
  
  digest = g_malloc (16);
  thumb_md5 (uri, digest);

  md5 = thumb_digest_to_ascii (digest);
  file = g_strconcat (md5, ".png", NULL);
  g_free (md5);
  
  dir = g_build_filename (g_get_home_dir (),
			  ".thumbnails",
			  (priv->size == GNOME_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
			  NULL);
  
  path = g_build_filename (dir,
			   file,
			   NULL);
  g_free (file);

  tmp_path = g_strconcat (path, ".XXXXXX", NULL);

  tmp_fd = mkstemp (tmp_path);
  if (tmp_fd == -1 &&
      make_thumbnail_dirs (factory))
    {
      g_free (tmp_path);
      tmp_path = g_strconcat (path, ".XXXXXX", NULL);
      tmp_fd = mkstemp (tmp_path);
    }

  if (tmp_fd == -1)
    {
      gnome_thumbnail_factory_create_failed_thumbnail (factory, uri, original_mtime);
      g_free (dir);
      g_free (tmp_path);
      g_free (path);
      g_free (digest);
      return;
    }
  close (tmp_fd);
  
  g_snprintf (mtime_str, 21, "%lu",  original_mtime);
  saved_ok  = gdk_pixbuf_save (thumbnail,
			       tmp_path,
			       "png", NULL, 
			       "tEXt::Thumb::URI", uri,
			       "tEXt::Thumb::MTime", mtime_str,
			       "tEXt::Software", "GNOME::ThumbnailFactory",
			       NULL);
  if (saved_ok)
    {
      chmod (tmp_path, 0600);
      rename(tmp_path, path);

      info = g_new (struct ThumbnailInfo, 1);
      info->mtime = original_mtime;
      info->uri = g_strdup (uri);
      
      pthread_mutex_lock (&priv->lock);
      
      g_hash_table_insert (factory->priv->existing_thumbs, digest, info);
      /* Make sure we don't re-read the directory. We should be uptodate
       * with all previous changes du to the ensure_uptodate above.
       * There is still a small window here where we might miss exisiting
       * thumbnails, but that shouldn't matter. (we would just redo them or
       * catch them later).
       */
      if (stat(dir, &statbuf) == 0)
	factory->priv->read_existing_mtime = statbuf.st_mtime;
      
      pthread_mutex_unlock (&priv->lock);
    }
  else
    {
      g_free (digest);
      gnome_thumbnail_factory_create_failed_thumbnail (factory, uri, original_mtime);
    }

  g_free (dir);
  g_free (path);
  g_free (tmp_path);
}

void
gnome_thumbnail_factory_create_failed_thumbnail (GnomeThumbnailFactory *factory,
						 const char            *uri,
						 time_t                 mtime)
{
  GnomeThumbnailFactoryPrivate *priv = factory->priv;
  unsigned char *digest;
  char *path, *md5, *file, *dir;
  char *tmp_path;
  int tmp_fd;
  char mtime_str[21];
  gboolean saved_ok;
  struct stat statbuf;
  GdkPixbuf *pixbuf;
  
  pthread_mutex_lock (&priv->lock);
  
  gnome_thumbnail_factory_ensure_failed_uptodate (factory);

  pthread_mutex_unlock (&priv->lock);

  digest = g_malloc (16);
  thumb_md5 (uri, digest);

  md5 = thumb_digest_to_ascii (digest);
  file = g_strconcat (md5, ".png", NULL);
  g_free (md5);
  
  dir = g_build_filename (g_get_home_dir (),
			  ".thumbnails/fail",
			  factory->priv->application,
			  NULL);
  
  path = g_build_filename (dir,
			   file,
			   NULL);
  g_free (file);

  tmp_path = g_strconcat (path, ".XXXXXX", NULL);

  tmp_fd = mkstemp (tmp_path);
  if (tmp_fd == -1 &&
      make_thumbnail_fail_dirs (factory))
    {
      g_free (tmp_path);
      tmp_path = g_strconcat (path, ".XXXXXX", NULL);
      tmp_fd = mkstemp (tmp_path);
    }

  if (tmp_fd == -1)
    {
      g_free (dir);
      g_free (tmp_path);
      g_free (path);
      g_free (digest);
      return;
    }
  close (tmp_fd);
  
  g_snprintf (mtime_str, 21, "%lu",  mtime);
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
  saved_ok  = gdk_pixbuf_save (pixbuf,
			       tmp_path,
			       "png", NULL, 
			       "tEXt::Thumb::URI", uri,
			       "tEXt::Thumb::MTime", mtime_str,
			       "tEXt::Software", "GNOME::ThumbnailFactory",
			       NULL);
  g_object_unref (pixbuf);
  if (saved_ok)
    {
      chmod (tmp_path, 0600);
      rename(tmp_path, path);

      pthread_mutex_lock (&priv->lock);
      
      g_hash_table_insert (factory->priv->failed_thumbs, digest, NULL);
      /* Make sure we don't re-read the directory. We should be uptodate
       * with all previous changes du to the ensure_uptodate above.
       * There is still a small window here where we might miss exisiting
       * thumbnails, but that shouldn't matter. (we would just redo them or
       * catch them later).
       */
      if (stat(dir, &statbuf) == 0)
	factory->priv->read_failed_mtime = statbuf.st_mtime;
      
      pthread_mutex_unlock (&priv->lock);
    }
  else
    g_free (digest);

  g_free (dir);
  g_free (path);
  g_free (tmp_path);
}

char *
gnome_thumbnail_md5 (const char *uri)
{
  unsigned char digest[16];

  thumb_md5 (uri, digest);
  return thumb_digest_to_ascii (digest);
}

char *
gnome_thumbnail_path_for_uri (const char         *uri,
			      GnomeThumbnailSize  size)
{
  char *md5;
  char *file;
  char *path;

  md5 = gnome_thumbnail_md5 (uri);
  file = g_strconcat (md5, ".png", NULL);
  g_free (md5);
  
  path = g_build_filename (g_get_home_dir (),
			   ".thumbnails",
			   (size == GNOME_THUMBNAIL_SIZE_NORMAL)?"normal":"large",
			   file,
			   NULL);
    
  g_free (file);

  return path;
}

gboolean
gnome_thumbnail_has_uri (GdkPixbuf          *pixbuf,
			 const char         *uri)
{
  const char *thumb_uri;
  
  thumb_uri = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::URI");
  return strcmp (uri, thumb_uri) == 0;
}

gboolean
gnome_thumbnail_is_valid (GdkPixbuf          *pixbuf,
			  const char         *uri,
			  time_t              mtime)
{
  const char *thumb_uri, *thumb_mtime_str;
  time_t thumb_mtime;
  
  thumb_uri = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::URI");
  if (strcmp (uri, thumb_uri) != 0)
    return FALSE;
  
  thumb_mtime_str = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::MTime");
  thumb_mtime = atol (thumb_mtime_str);
  if (mtime != thumb_mtime)
    return FALSE;
  
  return TRUE;
}


/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * ThumbMD5Context structure, pass it to thumb_md5_init, call
 * thumb_md5_update as needed on buffers full of bytes, and then call
 * thumb_md5_final, which will fill a supplied 32-byte array with the
 * digest in ascii form. 
 *
 */

static void thumb_md5_init      (struct ThumbMD5Context *context);
static void thumb_md5_update    (struct ThumbMD5Context *context,
				 unsigned char const    *buf,
				 unsigned                len);
static void thumb_md5_final     (unsigned char           digest[16],
				 struct ThumbMD5Context *context);
static void thumb_md5_transform (guint32                 buf[4],
				 guint32 const           in[16]);


static void
thumb_md5 (const char *string, unsigned char digest[16])
{
  struct ThumbMD5Context md5_context;
  
  thumb_md5_init (&md5_context);
  thumb_md5_update (&md5_context, string, strlen (string));
  thumb_md5_final (digest, &md5_context);
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define byteReverse(buf, len)	/* Nothing */
#else

/*
 * Note: this code is harmless on little-endian machines.
 */
static void
byteReverse(unsigned char *buf, unsigned longs)
{
    guint32 t;
    do {
	t = (guint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(guint32 *) buf = t;
	buf += 4;
    } while (--longs);
}

#endif

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void 
thumb_md5_init (struct ThumbMD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void 
thumb_md5_update (struct ThumbMD5Context *ctx,
		  unsigned char const *buf,
		  unsigned len)
{
    guint32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((guint32) len << 3)) < t)
	ctx->bits[1]++;		/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */

    if (t) {
	unsigned char *p = (unsigned char *) ctx->in + t;

	t = 64 - t;
	if (len < t) {
	    memcpy (p, buf, len);
	    return;
	}
	memcpy (p, buf, t);
	byteReverse (ctx->in, 16);
	thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);
	buf += t;
	len -= t;
    }

    /* Process data in 64-byte chunks */

    while (len >= 64) {
	memcpy (ctx->in, buf, 64);
	byteReverse (ctx->in, 16);
	thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);
	buf += 64;
	len -= 64;
    }

    /* Handle any remaining bytes of data. */

    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void 
thumb_md5_final (unsigned char digest[16], struct ThumbMD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
	/* Two lots of padding:  Pad the first block to 64 bytes */
	memset (p, 0, count);
	byteReverse (ctx->in, 16);
	thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);

	/* Now fill the next block with 56 bytes */
	memset(ctx->in, 0, 56);
    } else {
	/* Pad block to 56 bytes */
	memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((guint32 *) ctx->in)[14] = ctx->bits[0];
    ((guint32 *) ctx->in)[15] = ctx->bits[1];

    thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);
    byteReverse ((unsigned char *) ctx->buf, 4);
    memcpy (digest, ctx->buf, 16);
    memset (ctx, 0, sizeof(ctx));	/* In case it's sensitive */
}


/* The four core functions - F1 is optimized somewhat */

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1 (z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define thumb_md5_step(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  ThumbMD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void 
thumb_md5_transform (guint32 buf[4], guint32 const in[16])
{
    register guint32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    thumb_md5_step(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    thumb_md5_step(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    thumb_md5_step(F1, c, d, a, b, in[2] + 0x242070db, 17);
    thumb_md5_step(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    thumb_md5_step(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    thumb_md5_step(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    thumb_md5_step(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    thumb_md5_step(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    thumb_md5_step(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    thumb_md5_step(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    thumb_md5_step(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    thumb_md5_step(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    thumb_md5_step(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    thumb_md5_step(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    thumb_md5_step(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    thumb_md5_step(F1, b, c, d, a, in[15] + 0x49b40821, 22);
		
    thumb_md5_step(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    thumb_md5_step(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    thumb_md5_step(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    thumb_md5_step(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    thumb_md5_step(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    thumb_md5_step(F2, d, a, b, c, in[10] + 0x02441453, 9);
    thumb_md5_step(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    thumb_md5_step(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    thumb_md5_step(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    thumb_md5_step(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    thumb_md5_step(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    thumb_md5_step(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    thumb_md5_step(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    thumb_md5_step(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    thumb_md5_step(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    thumb_md5_step(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);
		
    thumb_md5_step(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    thumb_md5_step(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    thumb_md5_step(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    thumb_md5_step(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    thumb_md5_step(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    thumb_md5_step(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    thumb_md5_step(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    thumb_md5_step(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    thumb_md5_step(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    thumb_md5_step(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    thumb_md5_step(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    thumb_md5_step(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    thumb_md5_step(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    thumb_md5_step(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    thumb_md5_step(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    thumb_md5_step(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);
		
    thumb_md5_step(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    thumb_md5_step(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    thumb_md5_step(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    thumb_md5_step(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    thumb_md5_step(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    thumb_md5_step(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    thumb_md5_step(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    thumb_md5_step(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    thumb_md5_step(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    thumb_md5_step(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    thumb_md5_step(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    thumb_md5_step(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    thumb_md5_step(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    thumb_md5_step(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    thumb_md5_step(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    thumb_md5_step(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

