/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-labeled-image.h - A labeled image.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

/* NautilusLabeledImage is a container widget.  It can only contain internal 
 * widgets.  These internal widgets are can be a NautilusLabel and/or a 
 * NautilusImage.  These internal widgets are created as needed.  That means
 * that NautilusLabeledImage can always be used for "free" instead of a 
 * NautilusLabel or NautilusImage.  The only overhead is that of the GtkObject
 * machinery.
 *
 * The position of the label with respect to the image is controlled by the
 * 'label_positon' attribute.
 *
 * By default the internal image and label widgets are sized to their natural
 * preferred geometry.  You can use the 'fill' attribute of LabeledImage
 * to have the internal widgets fill as much of the LabeledImage allocation
 * as is available.  This is useful if you install a tile_pixbuf and want it
 * to cover the whole widget, and not just the areas occupied by the internal
 * widgets.
 *
 * LabeledImage also has x_padding/y_padding and x_alignment/y_alignment 
 * attributes that behave exaclty as those in the GtkMisc class.
 *
 * Note that the alignments are ignored if the fill attribute is TRUE.
 */

#ifndef NAUTILUS_LABELED_IMAGE_H
#define NAUTILUS_LABELED_IMAGE_H

#include <gtk/gtkcontainer.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-image.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_LABELED_IMAGE            (nautilus_labeled_image_get_type ())
#define NAUTILUS_LABELED_IMAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LABELED_IMAGE, NautilusLabeledImage))
#define NAUTILUS_LABELED_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LABELED_IMAGE, NautilusLabeledImageClass))
#define NAUTILUS_IS_LABELED_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LABELED_IMAGE))
#define NAUTILUS_IS_LABELED_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LABELED_IMAGE))

typedef struct _NautilusLabeledImage	      NautilusLabeledImage;
typedef struct _NautilusLabeledImageClass     NautilusLabeledImageClass;
typedef struct _NautilusLabeledImageDetails   NautilusLabeledImageDetails;

struct _NautilusLabeledImage
{
	/* Superclass */
	GtkContainer container;

	/* Private things */
	NautilusLabeledImageDetails *details;
};

struct _NautilusLabeledImageClass
{
	GtkContainerClass parent_class;
};

/* Public GtkLabeledImage methods */
GtkType         nautilus_labeled_image_get_type                         (void);
GtkWidget *     nautilus_labeled_image_new                              (const char                   *text,
									 GdkPixbuf                    *pixbuf);
GtkWidget *     nautilus_labeled_image_new_from_file_name               (const char                   *text,
									 const char                   *pixbuf_file_name);
void            nautilus_labeled_image_set_label_position               (NautilusLabeledImage         *labeled_image,
									 GtkPositionType               label_position);
GtkPositionType nautilus_labeled_image_get_label_position               (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_show_label                   (NautilusLabeledImage         *labeled_image,
									 gboolean                      show_label);
gboolean        nautilus_labeled_image_get_show_label                   (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_show_image                   (NautilusLabeledImage         *labeled_image,
									 gboolean                      show_image);
gboolean        nautilus_labeled_image_get_show_image                   (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_spacing                      (NautilusLabeledImage         *labeled_image,
									 guint                         spacing);
guint           nautilus_labeled_image_get_spacing                      (const NautilusLabeledImage   *labeled_image);
int             nautilus_labeled_image_get_x_padding                    (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_x_padding                    (NautilusLabeledImage         *labeled_image,
									 int                           x_padding);
int             nautilus_labeled_image_get_y_padding                    (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_y_padding                    (NautilusLabeledImage         *labeled_image,
									 int                           y_padding);
float           nautilus_labeled_image_get_x_alignment                  (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_x_alignment                  (NautilusLabeledImage         *labeled_image,
									 float                         x_alignment);
float           nautilus_labeled_image_get_y_alignment                  (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_y_alignment                  (NautilusLabeledImage         *labeled_image,
									 float                         y_alignment);
void            nautilus_labeled_image_set_fill                         (NautilusLabeledImage         *labeled_image,
									 gboolean                      fill);
gboolean        nautilus_labeled_image_get_fill                         (const NautilusLabeledImage   *labeled_image);

/* Functions for creating stock GtkButtons with a labeled image child */
GtkWidget *     nautilus_labeled_image_button_new                       (const char                   *text,
									 GdkPixbuf                    *pixbuf);
GtkWidget *     nautilus_labeled_image_button_new_from_file_name        (const char                   *text,
									 const char                   *pixbuf_file_name);
GtkWidget *     nautilus_labeled_image_toggle_button_new                (const char                   *text,
									 GdkPixbuf                    *pixbuf);
GtkWidget *     nautilus_labeled_image_toggle_button_new_from_file_name (const char                   *text,
									 const char                   *pixbuf_file_name);
GtkWidget *     nautilus_labeled_image_check_button_new                 (const char                   *text,
									 GdkPixbuf                    *pixbuf);
GtkWidget *     nautilus_labeled_image_check_button_new_from_file_name  (const char                   *text,
									 const char                   *pixbuf_file_name);

/* These are proxies for methods in NautilusImage and NautilusLabel */
void            nautilus_labeled_image_set_pixbuf                       (NautilusLabeledImage         *labeled_image,
									 GdkPixbuf                    *pixbuf);
void            nautilus_labeled_image_set_pixbuf_from_file_name        (NautilusLabeledImage         *labeled_image,
									 const char                   *pixbuf_file_name);
GdkPixbuf*      nautilus_labeled_image_get_pixbuf                       (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_text                         (NautilusLabeledImage         *labeled_image,
									 const char                   *text);
char*           nautilus_labeled_image_get_text                         (const NautilusLabeledImage   *labeled_image);
void            nautilus_labeled_image_set_tile_pixbuf                  (NautilusLabeledImage         *image,
									 GdkPixbuf                    *pixbuf);
void            nautilus_labeled_image_set_tile_pixbuf_from_file_name   (NautilusLabeledImage         *image,
									 const char                   *tile_file_name);
void            nautilus_labeled_image_make_bold                        (NautilusLabeledImage         *labeled_image);
void            nautilus_labeled_image_make_larger                      (NautilusLabeledImage         *labeled_image,
									 guint                         num_sizes);
void            nautilus_labeled_image_make_smaller                     (NautilusLabeledImage         *labeled_image,
									 guint                         num_sizes);
void            nautilus_labeled_image_set_tile_width                   (NautilusLabeledImage         *labeled_image,
									 int                           tile_width);
void            nautilus_labeled_image_set_tile_height                  (NautilusLabeledImage         *labeled_image,
									 int                           tile_height);
void            nautilus_labeled_image_set_background_mode              (NautilusLabeledImage         *labeled_image,
									 NautilusSmoothBackgroundMode  background_mode);
void            nautilus_labeled_image_set_solid_background_color       (NautilusLabeledImage         *labeled_image,
									 guint32                       solid_background_color);
void            nautilus_labeled_image_set_smooth_drop_shadow_offset    (NautilusLabeledImage         *labeled_image,
									 guint                         drop_shadow_offset);
void            nautilus_labeled_image_set_smooth_drop_shadow_color     (NautilusLabeledImage         *labeled_image,
									 guint32                       drop_shadow_color);
void            nautilus_labeled_image_set_text_color                   (NautilusLabeledImage         *labeled_image,
									 guint32                       text_color);
void            nautilus_labeled_image_set_label_never_smooth           (NautilusLabeledImage         *labeled_image,
									 gboolean                      never_smooth);
ArtIRect        nautilus_labeled_image_get_image_bounds                 (const NautilusLabeledImage   *labeled_image);
ArtIRect        nautilus_labeled_image_get_label_bounds                 (const NautilusLabeledImage   *labeled_image);

END_GNOME_DECLS

#endif /* NAUTILUS_LABELED_IMAGE_H */


