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
#include <gnome.h>
#include <nautilus-druid.h>
#include <nautilus-druid-page-eazel.h>
#include "installer.h"

/* pixmaps */
#include "bootstrap-background.xpm"
#include "/h/robey/info.xpm"
#include "/h/robey/bong.xpm"
#include "/h/robey/rpm.xpm"

#define RGB_BLACK	0x000000
#define RGB_RED		0xFF0000
#define FONT_TITLE	_("-adobe-helvetica-bold-r-normal-*-14-*-*-*-p-*-*-*,*-r-*")

typedef struct _packageinfo PackageInfo;
typedef struct _packagecustomizer PackageCustomizer;

/* private */
typedef enum {
        INSTALL_GROUP = 1,
        UPGRADE_GROUP,
        DOWNGRADE_GROUP,
} PackageGroup;

/* item in package list */
struct _packageinfo {
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
};

struct _packagecustomizer {
        GList *packages;	/* GList<PackageInfo *> */
        GList *package_tree;	/* original package tree */
        GtkWidget *druid_page;
        GtkWidget *hbox_install;
        GtkWidget *hbox_upgrade;
        GtkWidget *hbox_downgrade;
        GtkWidget *vbox;
        int installs;
        int upgrades;
        int downgrades;
        int largest_desc_width;
};



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
        
/* build up a list of the packagedata's that reference this one */
static GList *
find_package_parents_int (PackageData *package, PackageData *top, GList *packlist, GList *sofar)
{
        GList *iter;
        PackageData *subpack;

        for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
                subpack = (PackageData *)(iter->data);
                if (subpack == package) {
                        if (top != NULL) {
                                sofar = g_list_prepend (sofar, top);
                        }
                } else {
                        sofar = find_package_parents_int (package, subpack, subpack->soft_depends, sofar);
                        sofar = find_package_parents_int (package, subpack, subpack->hard_depends, sofar);
                }
        }
        return sofar;
}

static GList *
find_package_parents (PackageData *package, GList *packlist, GList *sofar)
{
        return find_package_parents_int (package, NULL, packlist, sofar);
}

static int
package_customizer_compare (PackageInfo *info, PackageData *package)
{
        return (info->package == package) ? 0 : 1;
}

static PackageInfo *
package_customizer_find_package (PackageCustomizer *table, PackageData *package)
{
        GList *item;

        item = g_list_find_custom (table->packages, package, (GCompareFunc)package_customizer_compare);
        if (item != NULL) {
                return (PackageInfo *)(item->data);
        } else {
                return NULL;
        }
}

static GList *
get_errant_children_list (GList *bad, PackageInfo *info, GList *list)
{
        GList *iter;
        PackageInfo *sub_info;

        for (iter = g_list_first (list); iter != NULL; iter = g_list_next (iter)) {
                sub_info = package_customizer_find_package (info->table, (PackageData *)(iter->data));
                if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sub_info->checkbox))) {
                        /* unchecked dependency: you are an errant child! */
                        bad = g_list_prepend (bad, sub_info);
                }
                bad = get_errant_children_list (bad, info, sub_info->package->soft_depends);
                bad = get_errant_children_list (bad, info, sub_info->package->hard_depends);
        }
        return bad;
}

/* return a list of PackageInfo's for packages needed by this package, but unchecked */
/* in english: if this package has a bong next to it, return the unchecked packages that caused that bong. */
static GList *
get_errant_children (PackageInfo *info)
{
        GList *bad = NULL;

        bad = get_errant_children_list (bad, info, info->package->soft_depends);
        bad = get_errant_children_list (bad, info, info->package->hard_depends);
        return bad;
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
        dialog = gnome_dialog_new (title, GNOME_STOCK_BUTTON_OK, NULL);
        toplevel = gtk_widget_get_toplevel (info->table->druid_page);
        if (GTK_IS_WINDOW (toplevel)) {
                gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (toplevel));
        }
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        g_free (title);

        icon = create_gtk_pixmap (info->table->druid_page, rpm_xpm);
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
                bong = create_gtk_pixmap (info->table->druid_page, bong_xpm);
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
package_customizer_recompute_bongs (PackageCustomizer *table)
{
        GList *iter, *iter2;
        GList *parents, *sub_parents;
        PackageInfo *info, *info2;

        /* reset bongs */
        for (iter = g_list_first (table->packages); iter != NULL; iter = g_list_next (iter)) {
                info = (PackageInfo *)(iter->data);
                info->show_bong = FALSE;
                gtk_label_set_color (info->desc, RGB_BLACK);
        }

        /* find unchecked boxes, trace them up and flip bongs on for the parents */
        for (iter = g_list_first (table->packages); iter != NULL; iter = g_list_next (iter)) {
                info = (PackageInfo *)(iter->data);
                if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info->checkbox))) {
                        parents = find_package_parents (info->package, table->package_tree, NULL);
                        while (parents != NULL) {
                                sub_parents = NULL;
                                for (iter2 = g_list_first (parents); iter2 != NULL; iter2 = g_list_next (iter2)) {
                                        info2 = package_customizer_find_package (table, (PackageData *)(iter2->data));
                                        g_assert (info2 != NULL);
                                        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info2->checkbox))) {
                                                info2->show_bong = TRUE;
                                                gtk_label_set_color (info->desc, RGB_RED);
                                        }
                                        sub_parents = find_package_parents (info2->package, table->package_tree, sub_parents);
                                }
                                g_list_free (parents);
                                parents = sub_parents;
                        }
                }
        }

        /* now show the bong icons on packages with show_bong set */
        for (iter = g_list_first (table->packages); iter != NULL; iter = g_list_next (iter)) {
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
        package_customizer_recompute_bongs (info->table);
}

static void
package_customizer_fill (PackageData *package, PackageCustomizer *table)
{
        PackageInfo *info;
        GtkWidget *info_pixmap;
        GtkWidget *hbox;
        GtkWidget *vbox;
        GtkWidget *hbox_group;
        char *desc;
        char *pack_name;
        int width, height;
        int desc_width;

        if (package_customizer_find_package (table, package) != NULL) {
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
        info_pixmap = create_gtk_pixmap (table->druid_page, info_xpm);
        gtk_widget_show (info_pixmap);
        gtk_container_add (GTK_CONTAINER (info->info_button), info_pixmap);
        gtk_signal_connect (GTK_OBJECT (info->info_button), "clicked", GTK_SIGNAL_FUNC (package_info_click), info);
        gtk_widget_show (info->info_button);

        info->bong = create_gtk_pixmap (table->druid_page, bong_xpm);
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
                hbox_group = table->hbox_install;
                table->installs++;
                break;
        case UPGRADE_GROUP:
                desc = g_strdup_printf (_("%s (from v%s)"), pack_name, info->version);
                hbox_group = table->hbox_upgrade;
                table->upgrades++;
                break;
        case DOWNGRADE_GROUP:
                desc = g_strdup_printf (_("%s (from v%s)"), pack_name, info->version);
                hbox_group = table->hbox_downgrade;
                table->downgrades++;
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
        if (desc_width > table->largest_desc_width) {
                table->largest_desc_width = desc_width;
        }
        g_free (desc);
        gtk_widget_show (info->desc);

        table->packages = g_list_prepend (table->packages, info);

        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), info->desc, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (""), TRUE, TRUE, 0);
        gtk_widget_show (hbox);

        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 0)), vbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 1)), hbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 2)), info->checkbox, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (gtk_box_nth (hbox_group, 3)), info->info_button, FALSE, FALSE, 0);

        g_list_foreach (package->soft_depends, (GFunc)package_customizer_fill, table);
        g_list_foreach (package->hard_depends, (GFunc)package_customizer_fill, table);
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
        gtk_widget_set_usize (top_label, table->largest_desc_width, -2);
}

static PackageCustomizer *
package_customizer_new (GtkWidget *page, GList *packages)
{
        PackageCustomizer *table = g_new0 (PackageCustomizer, 1);
        GtkWidget *label;
        GList *iter;

        table->hbox_install = category_hbox_new ();
        table->hbox_upgrade = category_hbox_new ();
        table->hbox_downgrade = category_hbox_new ();
        table->druid_page = page;
        table->packages = NULL;
        table->package_tree = packages;

        for (iter = g_list_first (packages); iter != NULL; iter = g_list_next (iter)) {
                package_customizer_fill ((PackageData *)(iter->data), table);
        }

        table->vbox = gtk_vbox_new (FALSE, 0);
        if (table->installs > 0) {
                label = gtk_label_new_with_font (_("Packages to install"), FONT_TITLE);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (table->vbox), gtk_label_as_hbox (label), FALSE, FALSE, 5);
                normalize_labels (table, table->hbox_install);
                gtk_box_pack_start (GTK_BOX (table->vbox), table->hbox_install, FALSE, FALSE, 0);
        }
        if (table->upgrades > 0) {
                label = gtk_label_new_with_font (_("Packages to upgrade"), FONT_TITLE);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (table->vbox), gtk_label_as_hbox (label), FALSE, FALSE, 5);
                normalize_labels (table, table->hbox_upgrade);
                gtk_box_pack_start (GTK_BOX (table->vbox), table->hbox_upgrade, FALSE, FALSE, 0);
        }
        if (table->downgrades > 0) {
                label = gtk_label_new_with_font (_("Packages to downgrade"), FONT_TITLE);
                gtk_widget_show (label);
                gtk_box_pack_start (GTK_BOX (table->vbox), gtk_label_as_hbox (label), FALSE, FALSE, 5);
                normalize_labels (table, table->hbox_downgrade);
                gtk_box_pack_start (GTK_BOX (table->vbox), table->hbox_downgrade, FALSE, FALSE, 0);
        }
        gtk_widget_show (table->vbox);

        return table;
}

void
jump_to_package_tree_page (EazelInstaller *installer, GList *packages)
{
        PackageCustomizer *table;
        GtkWidget *page;
        GtkWidget *pane;
        GtkWidget *hbox;

        page = nautilus_druid_page_eazel_new_with_vals (NAUTILUS_DRUID_PAGE_EAZEL_OTHER,
                                                        NULL, NULL, NULL, NULL,
                                                        create_pixmap (GTK_WIDGET (installer->window),
                                                                       bootstrap_background));

        table = package_customizer_new (page, packages);

        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_add_padding (hbox, 10, 0);
        gtk_box_pack_start (GTK_BOX (hbox), table->vbox, FALSE, FALSE, 0);
        gtk_widget_show (hbox);

        pane = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pane), hbox);
        gtk_widget_show (pane);

	nautilus_druid_page_eazel_put_widget (NAUTILUS_DRUID_PAGE_EAZEL (page), pane);

        gtk_widget_show (page);
	gnome_druid_append_page (installer->druid, GNOME_DRUID_PAGE (page));
	gnome_druid_set_page (installer->druid, GNOME_DRUID_PAGE (page));
}
