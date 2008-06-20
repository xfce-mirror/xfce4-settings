/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>

#include <glib.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>
#include "appearance-dialog.glade.h"

typedef enum {
    THEME_TYPE_ICONS,
    THEME_TYPE_GTK,
} ThemeType;

typedef struct {
    GtkWidget *slave;
    XfconfChannel *channel;
} PropertyPair;

gboolean version = FALSE;

static GOptionEntry entries[] =
{
    {    "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    { NULL }
};

void
cb_icon_theme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel)
{
    GtkTreeModel *model = NULL;
    GList *list = gtk_tree_selection_get_selected_rows (selection, &model);
    GtkTreeIter iter;
    GValue value = {0,};

    /* valid failure */
    if ( g_list_length (list) == 0)
        return;

    /* everything else is invalid */
    g_return_if_fail (g_list_length (list) == 1);

    gtk_tree_model_get_iter (model, &iter, list->data);
    gtk_tree_model_get_value (model, &iter, 0, &value);
    
    xfconf_channel_set_property (channel, "/Net/IconThemeName", &value);

    g_value_unset (&value);

    g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (list);

}

void
cb_ui_theme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel)
{
    GtkTreeModel *model = NULL;
    GList *list = gtk_tree_selection_get_selected_rows (selection, &model);
    GtkTreeIter iter;
    GValue value = {0,};

    /* valid failure */
    if ( g_list_length (list) == 0)
        return;

    /* everything else is invalid */
    g_return_if_fail (g_list_length (list) == 1);

    gtk_tree_model_get_iter (model, &iter, list->data);
    gtk_tree_model_get_value (model, &iter, 0, &value);
    
    xfconf_channel_set_property (channel, "/Net/ThemeName", &value);

    g_value_unset (&value);

    g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (list);

}

void
cb_toolbar_style_combo_changed (GtkComboBox *combo, XfconfChannel *channel)
{
    switch (gtk_combo_box_get_active(combo))
    {
        case 0:
            xfconf_channel_set_string (channel, "/Gtk/ToolbarStyle", "icons");
            break;
        case 1:
            xfconf_channel_set_string (channel, "/Gtk/ToolbarStyle", "text");
            break;
        case 2:
            xfconf_channel_set_string (channel, "/Gtk/ToolbarStyle", "both");
            break;
        case 3:
            xfconf_channel_set_string (channel, "/Gtk/ToolbarStyle", "both-horiz");
            break;
    }
}

void
cb_antialias_check_button_toggled (GtkToggleButton *toggle, XfconfChannel *channel)
{
    gtk_toggle_button_set_inconsistent(toggle, FALSE);
    if (gtk_toggle_button_get_active(toggle))
    {
        xfconf_channel_set_int (channel, "/Xft/Antialias", 1);
    }
    else
    {
        xfconf_channel_set_int (channel, "/Xft/Antialias", 0);
    }
}

void
cb_hinting_style_combo_changed (GtkComboBox *combo, XfconfChannel *channel)
{
    switch (gtk_combo_box_get_active(combo))
    {
        case 0:
            xfconf_channel_set_string (channel, "/Xft/HintStyle", "hintnone");
            break;
        case 1:
            xfconf_channel_set_string (channel, "/Xft/HintStyle", "hintslight");
            break;
        case 2:
            xfconf_channel_set_string (channel, "/Xft/HintStyle", "hintmedium");
            break;
        case 3:
            xfconf_channel_set_string (channel, "/Xft/HintStyle", "hintfull");
            break;
    }
}

void
cb_rgba_style_combo_changed (GtkComboBox *combo, XfconfChannel *channel)
{
    switch (gtk_combo_box_get_active(combo))
    {
        case 0:
            xfconf_channel_set_string (channel, "/Xft/RGBA", "none");
            break;
        case 1:
            xfconf_channel_set_string (channel, "/Xft/RGBA", "rgb");
            break;
        case 2:
            xfconf_channel_set_string (channel, "/Xft/RGBA", "bgr");
            break;
        case 3:
            xfconf_channel_set_string (channel, "/Xft/RGBA", "vrgb");
            break;
        case 4:
            xfconf_channel_set_string (channel, "/Xft/RGBA", "vbgr");
            break;
    }
}

void
cb_custom_dpi_check_button_toggled (GtkToggleButton *toggle, PropertyPair *pair)
{
    if (gtk_toggle_button_get_active(toggle))
    {
        xfconf_channel_set_int (pair->channel, "/Xft/DPI", 96*1024);
        gtk_widget_set_sensitive(pair->slave, TRUE);
    }
    else
    {
        xfconf_channel_set_int (pair->channel, "/Xft/DPI", -1);
        gtk_widget_set_sensitive(pair->slave, FALSE);
    }
}

void
cb_custom_dpi_spin_value_changed (GtkSpinButton *spin, XfconfChannel *channel)
{
    xfconf_channel_set_int (channel, "/Xft/DPI", (gint)(gtk_spin_button_get_value(spin)*1024));
}

GList *
read_themes_from_dir (const gchar *dir_name, ThemeType type)
{
    GList *theme_list = NULL;
    const gchar *theme_name = NULL;
    gchar *theme_index;
    GDir *dir = g_dir_open (dir_name, 0, NULL);

    if (dir)
    {
        theme_name = g_dir_read_name (dir);

        while (theme_name)
        {
            switch (type)
            {
                case THEME_TYPE_ICONS:
                    theme_index = g_build_filename (dir_name, theme_name, "index.theme", NULL);

                    if (g_file_test (theme_index, G_FILE_TEST_EXISTS))
                    {
                        g_free (theme_index);
                        theme_index = g_build_filename (dir_name, theme_name, "icon-theme.cache", NULL);

                        /* check for the icon-theme cache,
                         * this does not exist for cursor-themes so we can filter those out...
                         */
                        if (g_file_test (theme_index, G_FILE_TEST_EXISTS))
                        {
                            /* unfortunately, need to strdup here because the 
                             * resources allocated to the dir get released once 
                             * the dir is closed at the end of this function
                             */ 
                            theme_list = g_list_append (theme_list, g_strdup(theme_name));
                        }
                    }

                    g_free (theme_index);
                    break;
                case THEME_TYPE_GTK:
                    theme_index = g_build_filename (dir_name, theme_name, "gtk-2.0", "gtkrc", NULL);

                    if (g_file_test (theme_index, G_FILE_TEST_EXISTS))
                    {
                        /* unfortunately, need to strdup here because the 
                         * resources allocated to the dir get released once 
                         * the dir is closed at the end of this function
                         */ 
                        theme_list = g_list_append (theme_list, g_strdup(theme_name));
                    }

                    g_free (theme_index);
                    break;
            }

            theme_name = g_dir_read_name (dir);
        }

        g_dir_close (dir);
    }

    return theme_list;
}

/**
 * TODO: Fix icon-theme-spec compliance
 */
void
check_icon_themes (GtkListStore *list_store, GtkTreeView *tree_view, XfconfChannel *channel)
{
    gchar *dir_name;
    gchar *active_theme_name = xfconf_channel_get_string (channel, "/Net/IconThemeName", "hicolor");
    const gchar * const *xdg_system_data_dirs = g_get_system_data_dirs();
    GList *user_theme_list = NULL;
    GList *xdg_user_theme_list = NULL;
    GList *xdg_system_theme_list = NULL;
    GList *theme_list = NULL;
    GList *list_iter = NULL;
    GList *temp_iter = NULL;
    GtkTreeIter iter;
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);

    dir_name = g_build_filename (g_get_home_dir (), ".icons", NULL);
    user_theme_list = read_themes_from_dir (dir_name, THEME_TYPE_GTK);
    g_free (dir_name);

    dir_name = g_build_filename (g_get_user_data_dir(), "icons",  NULL);
    xdg_user_theme_list = read_themes_from_dir (dir_name, THEME_TYPE_ICONS);
    g_free (dir_name);

    while (*xdg_system_data_dirs)
    {
        dir_name = g_build_filename (*xdg_system_data_dirs, "icons", NULL);
        xdg_system_theme_list = g_list_concat (xdg_system_theme_list, read_themes_from_dir (dir_name, THEME_TYPE_ICONS));
        g_free (dir_name);

        xdg_system_data_dirs++;
    }

    /* Legacy ~/.icons */
    list_iter = user_theme_list;
    while (user_theme_list && list_iter != NULL)
    {
        temp_iter = g_list_find_custom (theme_list, list_iter->data, (GCompareFunc)strcmp);
        if (temp_iter == NULL)
        {
            user_theme_list = g_list_remove_link (user_theme_list, list_iter);
            theme_list = g_list_concat (theme_list, list_iter);

            list_iter = user_theme_list;
        }
        else
            list_iter = g_list_next (list_iter);
    }

    /* XDG_DATA_HOME */
    for (list_iter = xdg_user_theme_list; list_iter != NULL; list_iter = g_list_next (list_iter))
    {
        temp_iter = g_list_find_custom (theme_list, list_iter->data, (GCompareFunc)strcmp);
        if (temp_iter == NULL)
        {
            xdg_user_theme_list = g_list_remove_link (xdg_system_theme_list, list_iter);
            theme_list = g_list_concat (theme_list, list_iter);

            list_iter = xdg_user_theme_list;
        }
        else
            list_iter = g_list_next (list_iter);
    }

    /* XDG_DATA_DIRS */
    list_iter = xdg_system_theme_list;
    while (xdg_system_theme_list && list_iter != NULL)
    {
        temp_iter = g_list_find_custom (theme_list, list_iter->data, (GCompareFunc)strcmp);
        if (temp_iter == NULL)
        {
            xdg_system_theme_list = g_list_remove_link (xdg_system_theme_list, list_iter);
            theme_list = g_list_concat (theme_list, list_iter);

            list_iter = xdg_system_theme_list;
        }
        else
            list_iter = g_list_next (list_iter);
    }

    /* Add all unique themes to the liststore */
    for (list_iter = theme_list; list_iter != NULL; list_iter = g_list_next (list_iter))
    {
        gtk_list_store_insert (list_store, &iter, 0);
        gtk_list_store_set (list_store, &iter, 0, list_iter->data, -1);

        if (strcmp (list_iter->data, active_theme_name) == 0)
        {
            GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
            gtk_tree_selection_select_path (selection, path);
        }
    }

    /* cleanup */
    if (xdg_system_theme_list)
    {
        g_list_foreach (xdg_system_theme_list, (GFunc)g_free, NULL);
        g_list_free (xdg_system_theme_list);   
    }
    if (xdg_user_theme_list)
    {
        g_list_foreach (xdg_user_theme_list, (GFunc)g_free, NULL);
        g_list_free (xdg_user_theme_list);   
    }
    if (user_theme_list)
    {
        g_list_foreach (user_theme_list, (GFunc)g_free, NULL);
        g_list_free (user_theme_list);   
    }


}

void
check_ui_themes (GtkListStore *list_store, GtkTreeView *tree_view, XfconfChannel *channel)
{
    gchar *dir_name;
    gchar *active_theme_name = xfconf_channel_get_string (channel, "/Net/ThemeName", "Default");
    const gchar * const *xdg_system_data_dirs = g_get_system_data_dirs();
    GList *user_theme_list = NULL;
    GList *xdg_user_theme_list = NULL;
    GList *xdg_system_theme_list = NULL;
    GList *theme_list = NULL;
    GList *list_iter = NULL;
    GList *temp_iter = NULL;
    GtkTreeIter iter;
    GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);

    dir_name = g_build_filename (g_get_home_dir (), ".themes", NULL);
    user_theme_list = read_themes_from_dir (dir_name, THEME_TYPE_GTK);
    g_free (dir_name);

    dir_name = g_build_filename (g_get_user_data_dir(), "themes",  NULL);
    xdg_user_theme_list = read_themes_from_dir (dir_name, THEME_TYPE_GTK);
    g_free (dir_name);

    while (*xdg_system_data_dirs)
    {
        dir_name = g_build_filename (*xdg_system_data_dirs, "themes", NULL);
        xdg_system_theme_list = g_list_concat (xdg_system_theme_list, read_themes_from_dir (dir_name, THEME_TYPE_GTK));
        g_free (dir_name);

        xdg_system_data_dirs++;
    }

    list_iter = user_theme_list;
    while (user_theme_list && list_iter != NULL)
    {
        temp_iter = g_list_find_custom (theme_list, list_iter->data, (GCompareFunc)strcmp);
        if (temp_iter == NULL)
        {
            user_theme_list = g_list_remove_link (user_theme_list, list_iter);
            theme_list = g_list_concat (theme_list, list_iter);

            list_iter = user_theme_list;
        }
        else
            list_iter = g_list_next (list_iter);
    }

#if 0
    /** Gtk does not adhere to the xdg_basedir_spec yet */
    for (list_iter = xdg_user_theme_list; list_iter != NULL; list_iter = g_list_next (list_iter))
    {
    }
#endif

    list_iter = xdg_system_theme_list;
    while (xdg_system_theme_list && list_iter != NULL)
    {
        temp_iter = g_list_find_custom (theme_list, list_iter->data, (GCompareFunc)strcmp);
        if (temp_iter == NULL)
        {
            xdg_system_theme_list = g_list_remove_link (xdg_system_theme_list, list_iter);
            theme_list = g_list_concat (theme_list, list_iter);

            list_iter = xdg_system_theme_list;
        }
        else
            list_iter = g_list_next (list_iter);
    }

    /* Add all unique themes to the liststore */
    for (list_iter = theme_list; list_iter != NULL; list_iter = g_list_next (list_iter))
    {
        gtk_list_store_insert (list_store, &iter, 0);
        gtk_list_store_set (list_store, &iter, 0, list_iter->data, -1);

        if (strcmp (list_iter->data, active_theme_name) == 0)
        {
            GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
            gtk_tree_selection_select_path (selection, path);
        }
    }

    /* cleanup */
    if (xdg_system_theme_list)
    {
        g_list_foreach (xdg_system_theme_list, (GFunc)g_free, NULL);
        g_list_free (xdg_system_theme_list);   
    }
    if (xdg_user_theme_list)
    {
        g_list_foreach (xdg_user_theme_list, (GFunc)g_free, NULL);
        g_list_free (xdg_user_theme_list);   
    }
    if (user_theme_list)
    {
        g_list_foreach (user_theme_list, (GFunc)g_free, NULL);
        g_list_free (user_theme_list);   
    }


}

GtkWidget *
appearance_settings_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget *dialog;
    PropertyPair *pair = NULL;
    GtkTreeIter iter;
    GtkListStore *list_store;
    GtkCellRenderer *renderer;
    GtkTreeSelection *icon_selection, *ui_selection;

    XfconfChannel *xsettings_channel = xfconf_channel_new("xsettings");

    GtkWidget *can_edit_accels = glade_xml_get_widget (gxml, "gtk_caneditaccels_check_button");
    GtkWidget *menu_images = glade_xml_get_widget (gxml, "gtk_menu_images_check_button");
    GtkWidget *button_images = glade_xml_get_widget (gxml, "gtk_button_images_check_button");
    GtkWidget *fontname_button = glade_xml_get_widget (gxml, "gtk_fontname_button");
    GtkWidget *toolbar_style_combo = glade_xml_get_widget (gxml, "gtk_toolbar_style_combo_box");

    GtkWidget *antialias_check_button = glade_xml_get_widget (gxml, "xft_antialias_check_button");
    GtkWidget *hinting_style_combo = glade_xml_get_widget (gxml, "xft_hinting_style_combo_box");
    GtkWidget *rgba_style_combo = glade_xml_get_widget (gxml, "xft_rgba_combo_box");
    GtkWidget *custom_dpi_check = glade_xml_get_widget (gxml, "xft_custom_dpi_check_button");
    GtkWidget *custom_dpi_spin = glade_xml_get_widget (gxml, "xft_custom_dpi_spin_button");
    GtkWidget *icon_theme_treeview = glade_xml_get_widget (gxml, "icon_theme_treeview");
    GtkWidget *ui_theme_treeview = glade_xml_get_widget (gxml, "gtk_theme_treeview");

    /* Fill the theme-icons */
    /* Check icon-themes */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (icon_theme_treeview), GTK_TREE_MODEL (list_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (icon_theme_treeview), 0, _("Icon theme name"), renderer, "text", 0, NULL);

    icon_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (icon_theme_treeview));
    gtk_tree_selection_set_mode (icon_selection, GTK_SELECTION_SINGLE);

    check_icon_themes (list_store, GTK_TREE_VIEW (icon_theme_treeview), xsettings_channel);

    /* Check gtk-themes */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (ui_theme_treeview), GTK_TREE_MODEL (list_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui_theme_treeview), 0, _("Theme name"), renderer, "text", 0, NULL);
    
    ui_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui_theme_treeview));
    gtk_tree_selection_set_mode (ui_selection, GTK_SELECTION_SINGLE);

    check_ui_themes (list_store, GTK_TREE_VIEW (ui_theme_treeview), xsettings_channel);

    /* Fill the combo-boxes */
    /* ToolbarStyle combo */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Icons"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Text"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Both"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Both Horizontal"), -1);
    
    /* Should not need to clear the cell layout, doing it anyways. just to be sure */
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (toolbar_style_combo));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (toolbar_style_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (toolbar_style_combo), renderer, "text", 0);

    gtk_combo_box_set_model (GTK_COMBO_BOX (toolbar_style_combo), GTK_TREE_MODEL(list_store));

    /* Hinting Combo */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("No Hinting"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Slight Hinting"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Medium Hinting"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Full Hinting"), -1);

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (hinting_style_combo));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (hinting_style_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (hinting_style_combo), renderer, "text", 0);

    gtk_combo_box_set_model (GTK_COMBO_BOX (hinting_style_combo), GTK_TREE_MODEL(list_store));

    /* Subpixel (rgba)  hinting Combo */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("none"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("rgb"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("bgr"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("vrgb"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("vbgr"), -1);

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (rgba_style_combo));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (rgba_style_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (rgba_style_combo), renderer, "text", 0);

    gtk_combo_box_set_model (GTK_COMBO_BOX (rgba_style_combo), GTK_TREE_MODEL(list_store));

    /* Bind easy properties */
    xfconf_g_property_bind (xsettings_channel, 
                            "/Gtk/CanChangeAccels",
                            G_TYPE_BOOLEAN,
                            (GObject *)can_edit_accels, "active");
    xfconf_g_property_bind (xsettings_channel, 
                            "/Gtk/MenuImages",
                            G_TYPE_BOOLEAN,
                            (GObject *)menu_images, "active");
    xfconf_g_property_bind (xsettings_channel, 
                            "/Gtk/ButtonImages",
                            G_TYPE_BOOLEAN,
                            (GObject *)button_images, "active");

    xfconf_g_property_bind (xsettings_channel, 
                            "/Gtk/FontName",
                            G_TYPE_STRING,
                            (GObject *)fontname_button, "font-name"); 
    /* Less easy properties */
    {
        gchar *toolbar_style_string = xfconf_channel_get_string (xsettings_channel, "/Gtk/ToolbarStyle", "Both");
        if (!strcmp(toolbar_style_string, "icons"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(toolbar_style_combo), 0);
        if (!strcmp(toolbar_style_string, "text"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(toolbar_style_combo), 1);
        if (!strcmp(toolbar_style_string, "both"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(toolbar_style_combo), 2);
        if (!strcmp(toolbar_style_string, "both-horiz"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(toolbar_style_combo), 3);
        g_free (toolbar_style_string);
    }
    {
        gchar *hinting_style_string = xfconf_channel_get_string (xsettings_channel, "/Xft/HintStyle", "hintnone");
        if (!strcmp(hinting_style_string, "hintnone"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(hinting_style_combo), 0);
        if (!strcmp(hinting_style_string, "hintslight"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(hinting_style_combo), 1);
        if (!strcmp(hinting_style_string, "hintmedium"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(hinting_style_combo), 2);
        if (!strcmp(hinting_style_string, "hintfull"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(hinting_style_combo), 3);
        g_free (hinting_style_string);
    }
    {
        gint hinting = xfconf_channel_get_int (xsettings_channel, "/Xft/Hinting", 1);
        switch (hinting)
        {
            case -1:
            case 1:
                gtk_combo_box_set_active (GTK_COMBO_BOX(hinting_style_combo), 1);
                hinting = 1;
                break;
            case 0:
                gtk_combo_box_set_active (GTK_COMBO_BOX(hinting_style_combo), 0);
        }

    }
    {
        gint antialias = xfconf_channel_get_int (xsettings_channel, "/Xft/Antialias", -1);
        switch (antialias)
        {
            case 1:
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(antialias_check_button), TRUE);
                break;
            case 0:
                gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(antialias_check_button), FALSE);
            case -1:
                gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON(antialias_check_button), TRUE);
                break;
        }

    }
    {
        gchar *rgba_style_string = xfconf_channel_get_string (xsettings_channel, "/Xft/RGBA", "none");
        if (!strcmp(rgba_style_string, "none"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(rgba_style_combo), 0);
        if (!strcmp(rgba_style_string, "rgb"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(rgba_style_combo), 1);
        if (!strcmp(rgba_style_string, "bgr"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(rgba_style_combo), 2);
        if (!strcmp(rgba_style_string, "vrgb"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(rgba_style_combo), 3);
        if (!strcmp(rgba_style_string, "vbgr"))
            gtk_combo_box_set_active (GTK_COMBO_BOX(rgba_style_combo), 4);
        g_free (rgba_style_string);
    }
    {
        gint dpi = xfconf_channel_get_int (xsettings_channel, "/Xft/DPI", -1);
        if (dpi == -1)
        {
            gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(custom_dpi_check), FALSE);
            gtk_widget_set_sensitive (custom_dpi_spin, FALSE);
        }
        else
        {
            gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(custom_dpi_check), TRUE);
            gtk_widget_set_sensitive (custom_dpi_spin, TRUE);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON(custom_dpi_spin), (gdouble)dpi/1024);
        }
    }

    g_signal_connect (G_OBJECT(ui_selection), "changed", G_CALLBACK (cb_ui_theme_treeselection_changed), xsettings_channel);
    g_signal_connect (G_OBJECT(icon_selection), "changed", G_CALLBACK (cb_icon_theme_treeselection_changed), xsettings_channel);

    g_signal_connect (G_OBJECT(toolbar_style_combo), "changed", G_CALLBACK(cb_toolbar_style_combo_changed), xsettings_channel);
    g_signal_connect (G_OBJECT(hinting_style_combo), "changed", G_CALLBACK(cb_hinting_style_combo_changed), xsettings_channel);
    g_signal_connect (G_OBJECT(rgba_style_combo), "changed", G_CALLBACK(cb_rgba_style_combo_changed), xsettings_channel);
    g_signal_connect (G_OBJECT(antialias_check_button), "toggled", G_CALLBACK(cb_antialias_check_button_toggled), xsettings_channel);

    pair = g_new0(PropertyPair, 1);
    pair->channel = xsettings_channel;
    pair->slave = custom_dpi_spin;
    g_signal_connect (G_OBJECT(custom_dpi_check), "toggled", G_CALLBACK(cb_custom_dpi_check_button_toggled), pair);
    g_signal_connect (G_OBJECT(custom_dpi_spin), "value-changed", G_CALLBACK(cb_custom_dpi_spin_value_changed), xsettings_channel);
    
    dialog = glade_xml_get_widget (gxml, "appearance-settings-dialog");

    return dialog;
}

int
main(int argc, char **argv)
{
    GtkWidget *dialog = NULL;
    GladeXML *gxml;
    GError *cli_error = NULL;

    #ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    if(!gtk_init_with_args(&argc, &argv, _("."), entries, PACKAGE, &cli_error))
    {
        if (cli_error != NULL)
        {
            g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), PACKAGE, cli_error->message, PACKAGE_NAME);
            g_error_free (cli_error);
            return 1;
        }
    }

    if(version)
    {
        g_print("%s\n", PACKAGE_STRING);
        return 0;
    }

    xfconf_init(NULL);

    gxml = glade_xml_new_from_buffer (appearance_dialog_glade,
                                      appearance_dialog_glade_length,
                                      NULL, NULL);

    dialog = appearance_settings_dialog_new_from_xml (gxml);

    gtk_dialog_run(GTK_DIALOG(dialog));

    
    xfconf_shutdown();

    return 0;
}
