/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 *  ohhhhh life sucks.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Robey Pointer <robey@eazel.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <nautilus-druid.h>
#include <nautilus-druid-page-eazel.h>
#include "installer.h"
#include "package-tree.h"

#include <libtrilobite/trilobite-i18n.h>

#include <string.h>

/* pixmaps */
#include "bootstrap-background.xpm"
#if 0	/* LATER */
#include "/h/robey/info.xpm"
#include "/h/robey/bong.xpm"
#include "/h/robey/rpm.xpm"
#else
char *info_xpm[] = { "" };
char *bong_xpm[] = { "" };
char *rpm_xpm[] = { "" };
#endif

#define RGB_BLACK	0x000000
#define RGB_RED		0xFF0000

static const char untranslated_font_title[] = N_("-adobe-helvetica-bold-r-normal-*-14-*-*-*-p-*-*-*,*-r-*");
#define FONT_TITLE	_(untranslated_font_title)


typedef enum {
        INSTALL_GROUP = 1,
        UPGRADE_GROUP,
        DOWNGRADE_GROUP,
} PackageGroup;

/* item in package list */
typedef struct {
        PackageData *package;
        PackageCustomizer *table;
        GtkWidget *checkbox;
        GtkWidget *info_button;
        GtkWidget *bong;
        GtkWidget *no_bong;
        GtkWidget *desc;
        gboolean show_bong;
        PackageGroup group;
        char *version;		/* upgraded or downgraded from */
} PackageInfo;

struct _PackageCustomizerPrivate {
        GList *packages;	/* GList<PackageInfo *> */
        GList *package_tree;	/* original package tree */
        GtkWidget *hbox_install;
        GtkWidget *hbox_upgrade;
        GtkWidget *hbox_downgrade;
        GtkWidget *vbox;
        int installs;
        int upgrades;
        int downgrades;
        int largest_desc_width;
	gboolean have_focus;
	int focus_hbox;		/* hbox & row combine to give y coords */
	int focus_row;
	int focus_col;		/* 0: checkbox, 1: info button */
};


static GtkWidget *category_hbox_new (void);


/**********   GTK object crap   **********/

/* This is the parent class pointer */
static GtkObjectClass *package_customizer_parent_class;

static void
package_customizer_finalize (GtkObject *object)
{
        PackageCustomizerPrivate *private;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_PACKAGE_CUSTOMIZER (object));

        private = PACKAGE_CUSTOMIZER (object)->private;

        g_list_free (private->packages);

        /* hbox's are owned by the top vbox, so that's all that needs to be dropped */
        gtk_widget_unref (private->vbox);

        g_free (private);
        PACKAGE_CUSTOMIZER (object)->private = NULL;

        if (GTK_OBJECT_CLASS (package_customizer_parent_class)->finalize) {
                GTK_OBJECT_CLASS (package_customizer_parent_class)->finalize (object);
        }
        log_debug ("<= package customizer finalize");
}

void
package_customizer_unref (GtkObject *object)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_PACKAGE_CUSTOMIZER (object));
        gtk_object_unref (object);
}

static void
package_customizer_class_initialize (PackageCustomizerClass *klass)
{
        GtkObjectClass *object_class;

        object_class = (GtkObjectClass *)klass;

        package_customizer_parent_class = gtk_type_class (gtk_object_get_type ());

        object_class->finalize = package_customizer_finalize;
}

static void
package_customizer_initialize (PackageCustomizer *object)
{
        g_assert (object != NULL);
        g_assert (IS_PACKAGE_CUSTOMIZER (object));

        log_debug ("=> package customizer create");

        object->private = g_new0 (PackageCustomizerPrivate, 1);

        /* setup widgets */
        object->private->hbox_install = category_hbox_new ();
        object->private->hbox_upgrade = category_hbox_new ();
        object->private->hbox_downgrade = category_hbox_new ();
        object->private->vbox = gtk_vbox_new (FALSE, 0);
}

PackageCustomizer *
package_customizer_new (void)
{
        PackageCustomizer *object;

        object = PACKAGE_CUSTOMIZER (gtk_object_new (TYPE_PACKAGE_CUSTOMIZER, NULL));
        gtk_object_ref (GTK_OBJECT (object));
        gtk_object_sink (GTK_OBJECT (object));

        return object;
}

GtkType
package_customizer_get_type (void)
{
        static GtkType my_type = 0;

        if (my_type == 0) {
                static const GtkTypeInfo my_info = {
                        "PackageCustomizer",
                        sizeof (PackageCustomizer),
                        sizeof (PackageCustomizerClass),
                        (GtkClassInitFunc) package_customizer_class_initialize,
                        (GtkObjectInitFunc) package_customizer_initialize,
                        NULL,
                        NULL,
                        (GtkClassInitFunc) NULL,
                };

                my_type = gtk_type_unique (gtk_object_get_type (), &my_info);
        }

        return my_type;
}


/**********   helper functions   **********/

/* figure out what group this package belongs to -- if version is non-NULL, the previous version is filled in */
static PackageGroup
find_package_group (PackageData *package, char **version)
{
        GList *iter;
        PackageData *mod_package;

        for (iter = g_list_first (package->modifies); iter != NULL; iter = g_list_next (iter)) {
                mod_package = (PackageData *)(iter->data);
                if (strcmp (mod_package->name, package->name) == 0) {
                        /* modifies itself!  upgrade or downgrade... */
                        if (mod_package->modify_status == PACKAGE_MOD_UPGRADED) {
                                if (version != NULL) {
                                        *version = g_strdup (mod_package->version);
                                }
                                return UPGRADE_GROUP;
                        } else if (mod_package->modify_status == PACKAGE_MOD_DOWNGRADED) {
                                if (version != NULL) {
                                        *version = g_strdup (mod_package->version);
                                }
                                return DOWNGRADE_GROUP;
                        }
                }
        }

        return INSTALL_GROUP;
}

/* build up a list of the PackageDatas that reference this one */
static GList *
find_package_parents_int (PackageData *package, PackageData *top, PackageData *subpack, GList *sofar)
{
	PackageDependency *dep;
	GList *iter;

	if (subpack == package) {
		/* be careful -- it's really a directed graph now, not a tree */
		if ((top != NULL) && (g_list_find (sofar, top) == NULL)) {
			sofar = g_list_prepend (sofar, top);
		}
	} else {
		for (iter = g_list_first (subpack->depends); iter != NULL; iter = g_list_next (iter)) {
			dep = (PackageDependency *)(iter->data);
			sofar = find_package_parents_int (package, subpack, dep->package, sofar);
		}
	}

	return sofar;
}

static GList *
find_package_parents (PackageData *package, GList *packlist, GList *sofar)
{
	PackageData *subpack;
	GList *iter;

	for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
		subpack = PACKAGEDATA (iter->data);
		sofar = find_package_parents_int (package, NULL, subpack, sofar);
	}

#if 0
	printf ("parents of %s:\n", packagedata_get_readable_name (package));
	for (iter = g_list_first (sofar); iter != NULL; iter = g_list_next (iter)) {
		printf ("\t%s\n", packagedata_get_readable_name (PACKAGEDATA (iter->data)));
	}
#endif
	return sofar;
}

static int
package_info_compare (PackageInfo *info, PackageData *package)
{
        return (info->package == package) ? 0 : 1;
}

static PackageInfo *
package_customizer_find_package (PackageCustomizer *table, PackageData *package)
{
        GList *item;

        item = g_list_find_custom (table->private->packages,
                                   package, (GCompareFunc)package_info_compare);
        if (item != NULL) {
                return (PackageInfo *)(item->data);
        } else {
                return NULL;
        }
}

static GList *
get_errant_children_int (GList *bad, PackageInfo *info, PackageData *subpack, GList **path)
{
	PackageDependency *dep;
	PackageInfo *sub_info;
	GList *iter;

	if (subpack != NULL) {
		if (g_list_find (*path, subpack) != NULL) {
			/* recursing... */
			return bad;
		}

		sub_info = package_customizer_find_package (info->table, subpack);
		if ((! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sub_info->checkbox))) &&
		    (g_list_find (bad, sub_info) == NULL)) {
			/* unchecked dependency: you are an errant child! */
			bad = g_list_prepend (bad, sub_info);
		}
	} else {
		subpack = info->package;
	}

	*path = g_list_prepend (*path, subpack);
        for (iter = g_list_first (subpack->depends); iter != NULL; iter = g_list_next (iter)) {
		dep = (PackageDependency *)(iter->data);
		bad = get_errant_children_int (bad, info, dep->package, path);
	}
	*path = g_list_remove (*path, subpack);

	return bad;
}

/* return a list of PackageInfo's for packages needed by this package, but unchecked */
/* in english: if this package has a bong next to it, return the unchecked packages that caused that bong. */
static GList *
get_errant_children (PackageInfo *info)
{
	GList *path;

	path = NULL;
	return get_errant_children_int (NULL, info, NULL, &path);
}

/* display info about a package */
static void
popup_package_dialog (PackageInfo *info)
{
        GtkWidget *dialog;
        GtkWidget *toplevel;
        GtkWidget *icon;
        GtkWidget *label;
        char *title, *text;
        GList *errant_children;

        title = g_strdup_printf (_("Package info: %s"), info->package->name);
        dialog = gnome_dialog_new (title, "Button_Ok", NULL);
        toplevel = gtk_widget_get_toplevel (info->table->private->vbox);
        if (GTK_IS_WINDOW (toplevel)) {
                gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (toplevel));
        }
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        g_free (title);

        icon = create_gtk_pixmap (info->table->private->vbox, rpm_xpm);
        gtk_widget_show (icon);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), icon, FALSE, FALSE, 0);

        label = gtk_label_new (info->package->name);
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, FALSE, FALSE, 0);

        text = g_strdup_printf (_("version %s"), info->package->version);
        label = gtk_label_new (text);
        g_free (text);
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, FALSE, FALSE, 0);

        if (info->group == UPGRADE_GROUP) {
                text = g_strdup_printf (_("upgrades from version %s"), info->version);
        } else if (info->group == DOWNGRADE_GROUP) {
                text = g_strdup_printf (_("downgrades from version %s"), info->version);
        } else {
                text = NULL;
        }
        if (text != NULL) {
                label = gtk_label_new (text);
                g_free (text);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, FALSE, FALSE, 0);
        }

        if (info->package->summary != NULL) {
                label = gtk_label_new (info->package->summary);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, FALSE, FALSE, 0);
        }

        errant_children = get_errant_children (info);
        if (errant_children != NULL) {
                GtkWidget *hbox;
                GtkWidget *bong;
                GString *str;
                GList *iter;
                PackageInfo *sub_info;
                char *pack_name;

                str = g_string_new ("");
                for (iter = g_list_first (errant_children); iter != NULL; iter = g_list_next (iter)) {
                        sub_info = (PackageInfo *)(iter->data);
                        pack_name = packagedata_get_readable_name (sub_info->package);
                        g_string_sprintfa (str, "%s%s", (iter == g_list_first (errant_children)) ? "" : ", ",
                                           pack_name);
                        g_free (pack_name);
                }
                hbox = gtk_hbox_new (FALSE, 0);
                bong = create_gtk_pixmap (info->table->private->vbox, bong_xpm);
                gtk_widget_show (bong);
                gtk_box_pack_start (GTK_BOX (hbox), bong, FALSE, FALSE, 0);
                gtk_box_add_padding (hbox, 5, 0);

                text = g_strdup_printf (_("This package won't install correctly without the following packages:\n%s"),
                                        str->str);
                label = gtk_label_new (text);
                gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
                gtk_widget_show (label);
                g_free (text);
                g_string_free (str, TRUE);
                gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

                gtk_widget_show (hbox);
                gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
        }

        gnome_dialog_run_and_close (GNOME_DIALOG (dialog));	/* don't care about what the user clicked */
}

static void
package_info_click (GtkButton *button, PackageInfo *info)
{
        popup_package_dialog (info);
}

/* figure out which packages should have bongs, and display them */
static void
package_customizer_recompute_bongs (PackageCustomizerPrivate *private)
{
        GList *iter, *iter2;
        GList *parents, *sub_parents;
        PackageInfo *info, *info2;

        /* reset bongs */
        for (iter = g_list_first (private->packages); iter != NULL; iter = g_list_next (iter)) {
                info = (PackageInfo *)(iter->data);
                info->show_bong = FALSE;
                gtk_label_set_color (info->desc, RGB_BLACK);
        }

        /* find unchecked boxes, trace them up and flip bongs on for the parents */
        for (iter = g_list_first (private->packages); iter != NULL; iter = g_list_next (iter)) {
                info = (PackageInfo *)(iter->data);
                if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info->checkbox))) {
                        parents = find_package_parents (info->package, private->package_tree, NULL);
                        while (parents != NULL) {
                                sub_parents = NULL;
                                for (iter2 = g_list_first (parents); iter2 != NULL; iter2 = g_list_next (iter2)) {
                                        info2 = package_customizer_find_package (info->table, (PackageData *)(iter2->data));
                                        g_assert (info2 != NULL);
                                        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info2->checkbox))) {
                                                info2->show_bong = TRUE;
                                                gtk_label_set_color (info->desc, RGB_RED);
                                        }
                                        sub_parents = find_package_parents (info2->package, private->package_tree, sub_parents);
                                }
                                g_list_free (parents);
                                parents = sub_parents;
                        }
                }
        }

        /* now show the bong icons on packages with show_bong set */
        for (iter = g_list_first (private->packages); iter != NULL; iter = g_list_next (iter)) {
                info = (PackageInfo *)(iter->data);
                if (info->show_bong) {
                        gtk_widget_show (info->bong);
                        gtk_widget_hide (info->no_bong);
                } else {
                        gtk_widget_hide (info->bong);
                        gtk_widget_show (info->no_bong);
                }
        }
}

static void
package_toggled (GtkToggleButton *button, PackageInfo *info)
{
        package_customizer_recompute_bongs (info->table->private);
}

static void package_customizer_fill (PackageData *package, PackageCustomizer *table);

static void
package_customizer_fill_dep (PackageDependency *dep, PackageCustomizer *table)
{
	package_customizer_fill (dep->package, table);
}

static void
package_customizer_fill (PackageData *package, PackageCustomizer *table)
{
        PackageCustomizerPrivate *private;
        PackageInfo *info;
        GtkWidget *info_pixmap;
        GtkWidget *hbox;
        GtkWidget *vbox;
        GtkWidget *hbox_group;
        char *desc;
        char *pack_name;
        int width, height;
        int desc_width;

        g_assert (table != NULL);
        g_assert (IS_PACKAGE_CUSTOMIZER (table));
        private = table->private;

        if (package_customizer_find_package (table, package) != NULL) {
		/* recursing */
                return;
        }
        info = g_new0 (PackageInfo, 1);
        info->package = package;
        info->table = table;
        info->group = find_package_group (package, &info->version);

        info->checkbox = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->checkbox), TRUE);
        gtk_signal_connect (GTK_OBJECT (info->checkbox), "toggled", GTK_SIGNAL_FUNC (package_toggled), info);
        gtk_widget_show (info->checkbox);

        info->info_button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (info->info_button), GTK_RELIEF_NONE);
        info_pixmap = create_gtk_pixmap (table->private->vbox, info_xpm);
        gtk_widget_show (info_pixmap);
        gtk_container_add (GTK_CONTAINER (info->info_button), info_pixmap);
        gtk_signal_connect (GTK_OBJECT (info->info_button), "clicked", GTK_SIGNAL_FUNC (package_info_click), info);
        gtk_widget_show (info->info_button);

        info->bong = create_gtk_pixmap (table->private->vbox, bong_xpm);
        gtk_widget_hide (info->bong);
        info->no_bong = gtk_label_new ("");
        gtk_widget_show (info->no_bong);
        get_pixmap_width_height (info_xpm, &width, &height);
        gtk_widget_set_usize (info->no_bong, width, height);
        vbox = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), info->bong, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), info->no_bong, FALSE, FALSE, 0);
        gtk_widget_show (vbox);

        pack_name = packagedata_get_readable_name (package);
        switch (info->group) {
        case INSTALL_GROUP:
                desc = g_strdup (pack_name);
                hbox_group = private->hbox_install;
                private->installs++;
                break;
        case UPGRADE_GROUP:
                desc = g_strdup_printf (_("%s (from v%s)"), pack_name, info->version);
                hbox_group = private->hbox_upgrade;
                private->upgrades++;
                break;
        case DOWNGRADE_GROUP:
                desc = g_strdup_printf (_("%s (from v%s)"), pack_name, info->version);
                hbox_group = private->hbox_downgrade;
                private->downgrades++;
                break;
        default:
                g_assert_not_reached ();
                desc = g_strdup ("fucked.");
                hbox_group = NULL;
        }
        g_free (pack_name);

        info->desc = gtk_label_new (desc);
        gtk_label_set_color (info->desc, RGB_BLACK);
        gtk_label_set_justify (GTK_LABEL (info->desc), GTK_JUSTIFY_LEFT);
        desc_width = gdk_string_width (info->desc->style->font, desc);
        if (desc_width > private->largest_desc_width) {
                private->largest_desc_width = desc_width;
        }
        g_free (desc);
        gtk_widget_show (info->desc);

        private->packages = g_list_prepend (private->packages, info);

        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), info->desc, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), TRUE, TRUE, 0);
        gtk_widget_show (hbox);

        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 0)), vbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 1)), hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 2)), info->checkbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 3)), info->info_button, FALSE, FALSE, 0);

        g_list_foreach (package->depends, (GFunc)package_customizer_fill_dep, table);
}

static GtkWidget *
category_hbox_new (void)
{
        GtkWidget *hbox;
        GtkWidget *vbox_bong, *vbox_desc, *vbox_checkbox, *vbox_info;

        hbox = gtk_hbox_new (FALSE, 0);
        vbox_bong = gtk_vbox_new (TRUE, 0);
        vbox_desc = gtk_vbox_new (TRUE, 0);
        vbox_checkbox = gtk_vbox_new (TRUE, 0);
        vbox_info = gtk_vbox_new (TRUE, 0);
        gtk_widget_show (vbox_bong);
        gtk_widget_show (vbox_desc);
        gtk_widget_show (vbox_checkbox);
        gtk_widget_show (vbox_info);
        gtk_box_pack_start (GTK_BOX (hbox), vbox_bong, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), vbox_desc, FALSE, FALSE, 10);
        gtk_box_pack_start (GTK_BOX (hbox), vbox_checkbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), vbox_info, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), FALSE, FALSE, 0);
        gtk_widget_show (hbox);

        return hbox;
}

static void
normalize_labels (PackageCustomizer *table, GtkWidget *hbox_group)
{
        GtkWidget *vbox_desc;
        GtkWidget *top_label;

        vbox_desc = gtk_box_nth (hbox_group, 1);
        top_label = gtk_box_nth (vbox_desc, 0);
        /* -2 : magic gtk number meaning "don't change" */
        gtk_widget_set_usize (top_label, table->private->largest_desc_width, -2);
}

void
package_customizer_set_package_list (PackageCustomizer *table, GList *package_tree)
{
        PackageCustomizerPrivate *private;
        GtkWidget *label;
        GList *iter;

        g_return_if_fail (table != NULL);
        g_return_if_fail (IS_PACKAGE_CUSTOMIZER (table));
        private = table->private;

        g_list_free (private->packages);
        private->packages = NULL;
        private->package_tree = package_tree;

        for (iter = g_list_first (package_tree); iter != NULL; iter = g_list_next (iter)) {
                package_customizer_fill ((PackageData *)(iter->data), table);
        }

        if (private->installs > 0) {
                label = gtk_label_new_with_font (_("Packages to install"), FONT_TITLE);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (private->vbox), gtk_label_as_hbox (label), FALSE, FALSE, 5);
                normalize_labels (table, private->hbox_install);
		gtk_box_pack_start (GTK_BOX (private->vbox), private->hbox_install, FALSE, FALSE, 0);
        }
        if (private->upgrades > 0) {
                label = gtk_label_new_with_font (_("Packages to upgrade"), FONT_TITLE);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (private->vbox), gtk_label_as_hbox (label), FALSE, FALSE, 5);
                normalize_labels (table, private->hbox_upgrade);
		gtk_box_pack_start (GTK_BOX (private->vbox), private->hbox_upgrade, FALSE, FALSE, 0);
        }
        if (private->downgrades > 0) {
                label = gtk_label_new_with_font (_("Packages to downgrade"), FONT_TITLE);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (private->vbox), gtk_label_as_hbox (label), FALSE, FALSE, 5);
                normalize_labels (table, private->hbox_downgrade);
		gtk_box_pack_start (GTK_BOX (private->vbox), private->hbox_downgrade, FALSE, FALSE, 0);
        }

        gtk_widget_show (private->vbox);
}

GtkWidget *
package_customizer_get_widget (PackageCustomizer *table)
{
        g_return_val_if_fail (table != NULL, NULL);
        g_return_val_if_fail (IS_PACKAGE_CUSTOMIZER (table), NULL);

        return table->private->vbox;
}

static GtkWidget *
table_hbox_nth (PackageCustomizer *table, int n)
{
	switch (n) {
	case 0:
		return table->private->hbox_install;
	case 1:
		return table->private->hbox_upgrade;
	case 2:
		return table->private->hbox_downgrade;
	default:
		return NULL;
	}
}

static gboolean
focus_next (PackageCustomizer *table, int incr)
{
	GtkWidget *hbox, *vbox, *item;
	int col, row, box;

	col = table->private->focus_col;
	row = table->private->focus_row;
	box = table->private->focus_hbox;

	switch (incr) {
	case 1:
		col++;
		break;
	case 2:
		row++;
		break;
	case -1:
		col--;
		break;
	case -2:
		row--;
		break;
	}

	if (col > 1) {
		col = 0;
		row++;
	}
	if (col < 0) {
		col = 1;
		row--;
	}
	while (row < 0) {
		box--;
		if (box < 0) {
			box = 2;
		}
		hbox = table_hbox_nth (table, box);
		vbox = gtk_box_nth (hbox, 2);
		row = g_list_length (GTK_BOX (vbox)->children) - 1;
	}

	hbox = table_hbox_nth (table, box);
	if (hbox == NULL) {
		return FALSE;
	}

	while (gtk_box_nth (gtk_box_nth (hbox, 0), row) == NULL) {
		row = 0;
		box++;
		hbox = table_hbox_nth (table, box);
		if (hbox == NULL) {
			box = 0;
			hbox = table_hbox_nth (table, box);
		}
	}

	vbox = gtk_box_nth (hbox, 2 + col);
	item = gtk_box_nth (vbox, row);
	gtk_widget_grab_focus (item);

	table->private->focus_col = col;
	table->private->focus_row = row;
	table->private->focus_hbox = box;

	return TRUE;
}

static gboolean
handle_focus (GtkContainer *container, GtkDirectionType direction)
{
	PackageCustomizer *table;
	int incr = 0;

	table = PACKAGE_CUSTOMIZER (gtk_object_get_data (GTK_OBJECT (container), "table"));

	switch (direction) {
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_TAB_BACKWARD:
		if (table->private->have_focus) {
			/* tab always leaves the frame */
			table->private->have_focus = FALSE;
			return FALSE;
		} else {
			table->private->have_focus = focus_next (table, 0);
			return table->private->have_focus;
		}
	case GTK_DIR_UP:
		incr = -2;
		break;
	case GTK_DIR_DOWN:
		incr = 2;
		break;
	case GTK_DIR_LEFT:
		incr = -1;
		break;
	case GTK_DIR_RIGHT:
		incr = 1;
		break;
	}

	if (table->private->have_focus) {
		if (! focus_next (table, incr)) {
			focus_next (table, 0);
		}
		return TRUE;
	} else {
		table->private->focus_hbox = 0;
		table->private->focus_row = 0;
		table->private->focus_col = 0;
		table->private->have_focus = focus_next (table, 0);
		return table->private->have_focus;
	}
}

void
jump_to_package_tree_page (EazelInstaller *installer, GList *packages)
{
        PackageCustomizer *table;
        GtkWidget *page;
        GtkWidget *pane;
        GtkWidget *hbox;
        GtkWidget *table_widget;
	GtkWidget *viewport;

        page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER,
                                                        NULL, NULL, NULL, NULL,
                                                        create_pixmap (GTK_WIDGET (installer->window),
                                                                       bootstrap_background));

        table = package_customizer_new ();
        package_customizer_set_package_list (table, packages);
        table_widget = package_customizer_get_widget (table);

        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_add_padding (hbox, 10, 0);
        gtk_box_pack_start (GTK_BOX (hbox), table_widget, FALSE, FALSE, 0);
        gtk_widget_show (table_widget);
        gtk_widget_show (hbox);

        pane = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	viewport = gtk_viewport_new (NULL, NULL);
	/* bug in gtk viewport causes this not to work.  ramiro's nautilus viewport
	 * would probably fix this, if it ever becomes important.
	   gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	*/
	gtk_container_add (GTK_CONTAINER (pane), viewport);
	gtk_widget_show (viewport);
	gtk_container_add (GTK_CONTAINER (viewport), hbox);
	gtk_widget_show (hbox);
        gtk_widget_show (pane);
	/* gtk_window_set_focus (window, widget); */
	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (page), pane);

        gtk_widget_show (page);

	/* ----- wow, why isn't there a better way to do this? ----- */
	gtk_object_set_data (GTK_OBJECT (page), "table", table);
	GTK_CONTAINER_CLASS (GTK_OBJECT (page)->klass)->focus = handle_focus;

	gnome_druid_append_page (installer->druid, GNOME_DRUID_PAGE (page));
	gnome_druid_set_page (installer->druid, GNOME_DRUID_PAGE (page));
}
