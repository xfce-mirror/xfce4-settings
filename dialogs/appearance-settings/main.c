/* $Id$ */
/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "appearance-dialog_glade.h"
#include "images.h"

/* Increase this number if new gtk settings have been added */
#define INITIALIZE_UINT (1)

enum
{
    COLUMN_THEME_NAME,
    COLUMN_THEME_DISPLAY_NAME,
    COLUMN_THEME_COMMENT,
    N_THEME_COLUMNS
};

enum
{
    COLUMN_RGBA_PIXBUF,
    COLUMN_RGBA_NAME,
    N_RGBA_COLUMNS
};

/* String arrays with the settings in combo boxes */
static const gchar* toolbar_styles_array[] =
{
    "icons", "text", "both", "both-horiz"
};

static const gchar* xft_hint_styles_array[] =
{
    "hintnone", "hintslight", "hintmedium", "hintfull"
};

static const gchar* xft_rgba_array[] =
{
    "none", "rgb", "bgr", "vrgb", "vbgr"
};

/* Option entries */
static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* Global xfconf channel */
static XfconfChannel *xsettings_channel;

static void
cb_theme_tree_selection_changed (GtkTreeSelection *selection,
                                 const gchar      *property)
{
    GtkTreeModel *model;
    gboolean      has_selection;
    gchar        *name;
    GtkTreeIter   iter;

    /* Get the selected list iter */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* Get the theme name */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &name, -1);

        /* Store the new theme */
        xfconf_channel_set_string (xsettings_channel, property, name);

        /* Cleanup */
        g_free (name);
    }
}

static void
cb_icon_theme_tree_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new icon theme */
    cb_theme_tree_selection_changed (selection, "/Net/IconThemeName");
}

static void
cb_ui_theme_tree_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new UI theme */
    cb_theme_tree_selection_changed (selection, "/Net/ThemeName");
}

static void
cb_toolbar_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (toolbar_styles_array));

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Gtk/ToolbarStyle", toolbar_styles_array[active]);
}

static void
cb_antialias_check_button_toggled (GtkToggleButton *toggle)
{
    gint active;

    /* Don't allow an inconsistent button anymore */
    gtk_toggle_button_set_inconsistent (toggle, FALSE);

    /* Get active */
    active = gtk_toggle_button_get_active (toggle) ? 1 : 0;

    /* Save setting */
    xfconf_channel_set_int (xsettings_channel, "/Xft/Antialias", active);
}

static void
cb_hinting_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_hint_styles_array));

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Xft/HintStyle", xft_hint_styles_array[active]);
}

static void
cb_rgba_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_rgba_array));

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Xft/RGBA", xft_rgba_array[active]);
}

static void
cb_custom_dpi_spin_value_changed (GtkSpinButton   *custom_dpi_spin,
                                  GtkToggleButton *custom_dpi_toggle)
{
    gint dpi;

    if (gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        dpi = gtk_spin_button_get_value (custom_dpi_spin);
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);
    }
}

static void
cb_custom_dpi_check_button_toggled (GtkToggleButton *custom_dpi_toggle,
                                    GtkSpinButton   *custom_dpi_spin)
{
    if (gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), TRUE);
        cb_custom_dpi_spin_value_changed (custom_dpi_spin, custom_dpi_toggle);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), FALSE);
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", -1);
    }
}

#ifdef ENABLE_SOUND_SETTINGS
static void
cb_enable_event_sounds_check_button_toggled (GtkToggleButton *toggle, GtkWidget *button)
{
    gboolean active;

    active = gtk_toggle_button_get_active (toggle);
    gtk_widget_set_sensitive (button, active);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
}
#endif

static void
appearance_settings_load_icon_themes (GtkListStore *list_store,
                                      GtkTreeView  *tree_view)
{
    GDir         *dir;
    GtkTreePath  *tree_path;
    GtkTreeIter   iter;
    XfceRc       *index_file;
    const gchar  *file;
    gchar       **icon_theme_dirs;
    gchar        *index_filename;
    const gchar  *theme_name;
    const gchar  *theme_comment;
    gchar        *comment_escaped;
    gchar        *active_theme_name;
    gint          i;
    GSList       *check_list = NULL;

    /* Determine current theme */
    active_theme_name = xfconf_channel_get_string (xsettings_channel, "/Net/IconThemeName", "hicolor");

    /* Determine directories to look in for icon themes */
    xfce_resource_push_path (XFCE_RESOURCE_ICONS, DATADIR G_DIR_SEPARATOR_S "icons");
    icon_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_ICONS);
    xfce_resource_pop_path (XFCE_RESOURCE_ICONS);

    /* Iterate over all base directories */
    for (i = 0; icon_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (icon_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build filename for the index.theme of the current icon theme directory */
            index_filename = g_build_filename (icon_theme_dirs[i], file, "index.theme", NULL);

            /* Try to open the theme index file */
            index_file = xfce_rc_simple_open (index_filename, TRUE);

            if (index_file != NULL
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Set the icon theme group */
                xfce_rc_set_group (index_file, "Icon Theme");

                /* Check if the icon theme is valid and visible to the user */
                if (G_LIKELY (xfce_rc_has_entry (index_file, "Directories")
                              && !xfce_rc_read_bool_entry (index_file, "Hidden", FALSE)))
                {
                    /* Insert the theme in the check list */
                    check_list = g_slist_prepend (check_list, g_strdup (file));

                    /* Get translated icon theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the comment, since tooltips are markup, not text */
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;

                    /* Append icon theme to the list store */
                    gtk_list_store_append (list_store, &iter);
                    gtk_list_store_set (list_store, &iter,
                                        COLUMN_THEME_NAME, file,
                                        COLUMN_THEME_DISPLAY_NAME, theme_name,
                                        COLUMN_THEME_COMMENT, comment_escaped, -1);

                    /* Cleanup */
                    g_free (comment_escaped);

                    /* Check if this is the active theme, if so, select it */
                    if (G_UNLIKELY (g_utf8_collate (theme_name, active_theme_name) == 0))
                    {
                        tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                        gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                        gtk_tree_path_free (tree_path);
                    }
                }
            }

            /* Close theme index file */
            if (G_LIKELY (index_file))
                xfce_rc_close (index_file);

            /* Free theme index filename */
            g_free (index_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (icon_theme_dirs);

    /* Free the check list */
    if (G_LIKELY (check_list))
    {
        g_slist_foreach (check_list, (GFunc) g_free, NULL);
        g_slist_free (check_list);
    }
}

static void
appearance_settings_load_ui_themes (GtkListStore *list_store,
                                    GtkTreeView  *tree_view)
{
    GDir         *dir;
    GtkTreePath  *tree_path;
    GtkTreeIter   iter;
    XfceRc       *index_file;
    const gchar  *file;
    gchar       **ui_theme_dirs;
    gchar        *index_filename;
    const gchar  *theme_name;
    const gchar  *theme_comment;
    gchar        *active_theme_name;
    gchar        *gtkrc_filename;
    gchar        *comment_escaped;
    gint          i;
    GSList       *check_list = NULL;

    /* Determine current theme */
    active_theme_name = xfconf_channel_get_string (xsettings_channel, "/Net/ThemeName", "Default");

    /* Determine directories to look in for ui themes */
    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    ui_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    /* Iterate over all base directories */
    for (i = 0; ui_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (ui_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build the theme style filename */
            gtkrc_filename = g_build_filename (ui_theme_dirs[i], file, "gtk-2.0", "gtkrc", NULL);

            /* Check if the gtkrc file exists and the theme is not already in the list */
            if (g_file_test (gtkrc_filename, G_FILE_TEST_EXISTS)
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Insert the theme in the check list */
                check_list = g_slist_prepend (check_list, g_strdup (file));

                /* Build filename for the index.theme of the current ui theme directory */
                index_filename = g_build_filename (ui_theme_dirs[i], file, "index.theme", NULL);

                /* Try to open the theme index file */
                index_file = xfce_rc_simple_open (index_filename, TRUE);

                if (G_LIKELY (index_file != NULL))
                {
                    /* Get translated ui theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the comment because tooltips are markup, not text */
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;

                    /* Close theme index file */
                    xfce_rc_close (index_file);
                }
                else
                {
                    /* Set defaults */
                    theme_name = file;
                    comment_escaped = NULL;
                }

                /* Append ui theme to the list store */
                gtk_list_store_append (list_store, &iter);
                gtk_list_store_set (list_store, &iter,
                                    COLUMN_THEME_NAME, file,
                                    COLUMN_THEME_DISPLAY_NAME, theme_name,
                                    COLUMN_THEME_COMMENT, comment_escaped, -1);

                /* Cleanup */
                g_free (comment_escaped);

                /* Check if this is the active theme, if so, select it */
                if (G_UNLIKELY (g_utf8_collate (theme_name, active_theme_name) == 0))
                {
                    tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                    gtk_tree_path_free (tree_path);
                }

                /* Free theme index filename */
                g_free (index_filename);
            }

            /* Free gtkrc filename */
            g_free (gtkrc_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (ui_theme_dirs);

    /* Free the check list */
    if (G_LIKELY (check_list))
    {
        g_slist_foreach (check_list, (GFunc) g_free, NULL);
        g_slist_free (check_list);
    }
}

static void
appearance_settings_from_gtk_settings (void)
{
    GtkSettings     *gtk_settings;
#ifdef GDK_WINDOWING_X11
    gint             gtk_xft_hinting;
    gint             gtk_xft_antialias;
    gint             gtk_xft_dpi;
    gchar           *gtk_xft_rgba = NULL;
    gchar           *gtk_xft_hintstyle = NULL;
#endif
    gboolean         gtk_can_change_accels;
    gboolean         gtk_button_images;
    gchar           *gtk_font_name = NULL;
    gchar           *gtk_icon_theme_name = NULL;
    gchar           *gtk_theme_name = NULL;

    /* read the default gtk settings */
    gtk_settings = gtk_settings_get_default ();

    if (G_LIKELY (gtk_settings))
    {
        /* read settings from gtk */
        g_object_get (G_OBJECT (gtk_settings),
#ifdef GDK_WINDOWING_X11
                      "gtk-xft-antialias", &gtk_xft_antialias,
                      "gtk-xft-hinting", &gtk_xft_hinting,
                      "gtk-xft-hintstyle", &gtk_xft_hintstyle,
                      "gtk-xft-rgba", &gtk_xft_rgba,
                      "gtk-xft-dpi", &gtk_xft_dpi,
#endif
                      "gtk-can-change-accels", &gtk_can_change_accels,
                      "gtk-button-images", &gtk_button_images,
                      "gtk-font-name", &gtk_font_name,
                      "gtk-icon-theme-name", &gtk_icon_theme_name,
                      "gtk-theme-name", &gtk_theme_name,
                      NULL);

#ifdef GDK_WINDOWING_X11
        /* save default xft settings */
        xfconf_channel_set_int (xsettings_channel, "/Xft/Hinting", gtk_xft_hinting);
        xfconf_channel_set_int (xsettings_channel, "/Xft/Antialias", gtk_xft_antialias);
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", gtk_xft_dpi);

        if (G_LIKELY (gtk_xft_rgba))
            xfconf_channel_set_string (xsettings_channel, "/Xft/RGBA", gtk_xft_rgba);

        if (G_LIKELY (gtk_xft_hintstyle))
            xfconf_channel_set_string (xsettings_channel, "/Xft/HintStyle", gtk_xft_hintstyle);
#endif

        /* save the default gtk settings */
        xfconf_channel_set_bool (xsettings_channel, "/Gtk/CanChangeAccels", gtk_can_change_accels);
        xfconf_channel_set_bool (xsettings_channel, "/Gtk/ButtonImages", gtk_button_images);

        if (G_LIKELY (gtk_font_name))
            xfconf_channel_set_string (xsettings_channel, "/Gtk/FontName", gtk_font_name);

        if (G_LIKELY (gtk_icon_theme_name))
            xfconf_channel_set_string (xsettings_channel, "/Net/IconThemeName", gtk_icon_theme_name);

        if (G_LIKELY (gtk_theme_name))
            xfconf_channel_set_string (xsettings_channel, "/Net/ThemeName", gtk_theme_name);

        /* cleanup */
        g_free (gtk_font_name);
        g_free (gtk_icon_theme_name);
        g_free (gtk_theme_name);
#ifdef GDK_WINDOWING_X11
        g_free (gtk_xft_rgba);
        g_free (gtk_xft_hintstyle);
#endif
    }
}

static void
appearance_settings_dialog_configure_widgets (GladeXML *gxml)
{
    GtkListStore     *list_store;
    GtkCellRenderer  *renderer;
    GtkTreeSelection *icon_selection, *ui_selection;
    gchar            *string;
    guint             i;
    GdkPixbuf        *pixbuf;

    /* check if we need to restore settings from GtkSettings */
    if (xfconf_channel_get_uint (xsettings_channel, "/Initialized", 0) < INITIALIZE_UINT)
    {
      /* read the gtk settings */
      appearance_settings_from_gtk_settings ();

      /* store the number */
      xfconf_channel_set_uint (xsettings_channel, "/Initialized", INITIALIZE_UINT);
    }

    /* Icon themes list */
    GtkWidget *icon_theme_treeview = glade_xml_get_widget (gxml, "icon_theme_treeview");

    list_store = gtk_list_store_new (N_THEME_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (icon_theme_treeview), GTK_TREE_MODEL (list_store));
#if GTK_CHECK_VERSION (2, 12, 0)
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (icon_theme_treeview), COLUMN_THEME_COMMENT);
#endif

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (icon_theme_treeview), 0, "", renderer, "text", COLUMN_THEME_DISPLAY_NAME, NULL);

    appearance_settings_load_icon_themes (list_store, GTK_TREE_VIEW (icon_theme_treeview));

    g_object_unref (G_OBJECT (list_store));

    icon_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (icon_theme_treeview));
    gtk_tree_selection_set_mode (icon_selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (icon_selection), "changed", G_CALLBACK (cb_icon_theme_tree_selection_changed), NULL);

    /* Gtk (UI) themes */
    GtkWidget *ui_theme_treeview = glade_xml_get_widget (gxml, "gtk_theme_treeview");

    list_store = gtk_list_store_new (N_THEME_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (ui_theme_treeview), GTK_TREE_MODEL (list_store));
#if GTK_CHECK_VERSION (2, 12, 0)
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (ui_theme_treeview), COLUMN_THEME_COMMENT);
#endif

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (ui_theme_treeview), 0, "", renderer, "text", COLUMN_THEME_DISPLAY_NAME, NULL);

    appearance_settings_load_ui_themes (list_store, GTK_TREE_VIEW (ui_theme_treeview));

    g_object_unref (G_OBJECT (list_store));

    ui_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ui_theme_treeview));
    gtk_tree_selection_set_mode (ui_selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (ui_selection), "changed", G_CALLBACK (cb_ui_theme_tree_selection_changed), NULL);

    /* Subpixel (rgba) hinting Combo */
    GtkWidget *rgba_combo_box = glade_xml_get_widget (gxml, "xft_rgba_combo_box");

    list_store = gtk_list_store_new (N_RGBA_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING);

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_none_xpm);
    gtk_list_store_insert_with_values (list_store, NULL, 0, COLUMN_RGBA_PIXBUF, pixbuf, COLUMN_RGBA_NAME, _("None"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_rgb_xpm);
    gtk_list_store_insert_with_values (list_store, NULL, 1, COLUMN_RGBA_PIXBUF, pixbuf, COLUMN_RGBA_NAME, _("RGB"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_bgr_xpm);
    gtk_list_store_insert_with_values (list_store, NULL, 2, COLUMN_RGBA_PIXBUF, pixbuf, COLUMN_RGBA_NAME, _("BGR"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_vrgb_xpm);
    gtk_list_store_insert_with_values (list_store, NULL, 3, COLUMN_RGBA_PIXBUF, pixbuf, COLUMN_RGBA_NAME, _("Vertical RGB"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    pixbuf = gdk_pixbuf_new_from_xpm_data (rgba_image_vbgr_xpm);
    gtk_list_store_insert_with_values (list_store, NULL, 4, COLUMN_RGBA_PIXBUF, pixbuf, COLUMN_RGBA_NAME, _("Vertical BGR"), -1);
    g_object_unref (G_OBJECT (pixbuf));

    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (rgba_combo_box));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (rgba_combo_box), renderer, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (rgba_combo_box), renderer, "pixbuf", COLUMN_RGBA_PIXBUF);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (rgba_combo_box), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (rgba_combo_box), renderer, "text", COLUMN_RGBA_NAME);

    gtk_combo_box_set_model (GTK_COMBO_BOX (rgba_combo_box), GTK_TREE_MODEL (list_store));
    g_object_unref (G_OBJECT (list_store));

    string = xfconf_channel_get_string (xsettings_channel, "/Xft/RGBA", xft_rgba_array[0]);
    for (i = 0; i < G_N_ELEMENTS (xft_rgba_array); i++)
        if (strcmp (string, xft_rgba_array[i]) == 0)
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (rgba_combo_box), i);
            break;
        }
    g_free (string);

    g_signal_connect (G_OBJECT (rgba_combo_box), "changed", G_CALLBACK (cb_rgba_style_combo_changed), NULL);

    /* Enable editable menu accelerators */
    GtkWidget *caneditaccels_check_button = glade_xml_get_widget (gxml, "gtk_caneditaccels_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/CanChangeAccels", G_TYPE_BOOLEAN,
                            G_OBJECT (caneditaccels_check_button), "active");

    /* Show menu images */
    GtkWidget *menu_images_check_button = glade_xml_get_widget (gxml, "gtk_menu_images_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/MenuImages", G_TYPE_BOOLEAN,
                            G_OBJECT (menu_images_check_button), "active");

    /* Show button images */
    GtkWidget *button_images_check_button = glade_xml_get_widget (gxml, "gtk_button_images_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/ButtonImages", G_TYPE_BOOLEAN,
                            G_OBJECT (button_images_check_button), "active");

    /* Font name */
    GtkWidget *fontname_button = glade_xml_get_widget (gxml, "gtk_fontname_button");
    xfconf_g_property_bind (xsettings_channel,  "/Gtk/FontName", G_TYPE_STRING,
                            G_OBJECT (fontname_button), "font-name");

    /* Toolbar style */
    GtkWidget *toolbar_style_combo = glade_xml_get_widget (gxml, "gtk_toolbar_style_combo_box");

    list_store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_list_store_insert_with_values (list_store, NULL, 0, 0, _("Icons"), -1);
    gtk_list_store_insert_with_values (list_store, NULL, 1, 0, _("Text"), -1);
    gtk_list_store_insert_with_values (list_store, NULL, 2, 0, _("Both"), -1);
    gtk_list_store_insert_with_values (list_store, NULL, 3, 0, _("Both Horizontal"), -1);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (toolbar_style_combo));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (toolbar_style_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (toolbar_style_combo), renderer, "text", 0);
    gtk_combo_box_set_model (GTK_COMBO_BOX (toolbar_style_combo), GTK_TREE_MODEL (list_store));
    g_object_unref (G_OBJECT (list_store));

    string = xfconf_channel_get_string (xsettings_channel, "/Gtk/ToolbarStyle", toolbar_styles_array[2]);
    for (i = 0; i < G_N_ELEMENTS (toolbar_styles_array); i++)
        if (strcmp (string, toolbar_styles_array[i]) == 0)
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (toolbar_style_combo), i);
            break;
        }
    g_free (string);

    g_signal_connect (G_OBJECT (toolbar_style_combo), "changed", G_CALLBACK(cb_toolbar_style_combo_changed), NULL);

    /* Hinting style */
    GtkWidget *hinting_style_combo = glade_xml_get_widget (gxml, "xft_hinting_style_combo_box");

    list_store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_list_store_insert_with_values (list_store, NULL, 0, 0, _("None"), -1);
    gtk_list_store_insert_with_values (list_store, NULL, 1, 0, _("Slight"), -1);
    gtk_list_store_insert_with_values (list_store, NULL, 2, 0, _("Medium"), -1);
    gtk_list_store_insert_with_values (list_store, NULL, 3, 0, _("Full"), -1);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (hinting_style_combo));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (hinting_style_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (hinting_style_combo), renderer, "text", 0);
    gtk_combo_box_set_model (GTK_COMBO_BOX (hinting_style_combo), GTK_TREE_MODEL (list_store));
    g_object_unref (G_OBJECT (list_store));

    string = xfconf_channel_get_string (xsettings_channel, "/Xft/HintStyle", xft_hint_styles_array[0]);
    for (i = 0; i < G_N_ELEMENTS (xft_hint_styles_array); i++)
        if (strcmp (string, xft_hint_styles_array[i]) == 0)
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (hinting_style_combo), i);
            break;
        }
    g_free (string);

    g_signal_connect (G_OBJECT (hinting_style_combo), "changed", G_CALLBACK (cb_hinting_style_combo_changed), NULL);

    /* Hinting */
    GtkWidget *antialias_check_button = glade_xml_get_widget (gxml, "xft_antialias_check_button");
    gint antialias = xfconf_channel_get_int (xsettings_channel, "/Xft/Antialias", -1);
    gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (antialias_check_button), antialias == -1);

    switch (antialias)
    {
        case 1:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (antialias_check_button), TRUE);
            break;

        case 0:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (antialias_check_button), FALSE);
            break;

        default: /* -1 */
            gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (antialias_check_button), TRUE);
            break;
    }

    g_signal_connect (G_OBJECT (antialias_check_button), "toggled", G_CALLBACK (cb_antialias_check_button_toggled), NULL);

    /* DPI */
    GtkWidget *custom_dpi_check = glade_xml_get_widget (gxml, "xft_custom_dpi_check_button");
    GtkWidget *custom_dpi_spin = glade_xml_get_widget (gxml, "xft_custom_dpi_spin_button");
    gint dpi = xfconf_channel_get_int (xsettings_channel, "/Xft/DPI", -1);

    if (dpi == -1)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (custom_dpi_check), FALSE);
        gtk_widget_set_sensitive (custom_dpi_spin, FALSE);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (custom_dpi_spin), 96);
    }
    else
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (custom_dpi_check), TRUE);
        gtk_widget_set_sensitive (custom_dpi_spin, TRUE);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (custom_dpi_spin), (gdouble) dpi);
    }

    g_signal_connect (G_OBJECT (custom_dpi_check), "toggled", G_CALLBACK (cb_custom_dpi_check_button_toggled), custom_dpi_spin);
    g_signal_connect (G_OBJECT (custom_dpi_spin), "value-changed", G_CALLBACK (cb_custom_dpi_spin_value_changed), custom_dpi_check);

#ifdef ENABLE_SOUND_SETTINGS
    /* Sounds */
    GtkWidget *event_sounds_frame = glade_xml_get_widget (gxml, "event_sounds_frame");
    GtkWidget *enable_event_sounds_check_button = glade_xml_get_widget (gxml, "enable_event_sounds_check_button");
    GtkWidget *enable_input_feedback_sounds_button  = glade_xml_get_widget (gxml, "enable_input_feedback_sounds_button");

    gtk_widget_show (event_sounds_frame);
    g_signal_connect (G_OBJECT (enable_event_sounds_check_button),
                      "toggled",
                      G_CALLBACK (cb_enable_event_sounds_check_button_toggled),
                      enable_input_feedback_sounds_button);
    xfconf_g_property_bind (xsettings_channel, "/Net/EnableEventSounds", G_TYPE_BOOLEAN,
                            G_OBJECT (enable_event_sounds_check_button), "active");
    xfconf_g_property_bind (xsettings_channel, "/Net/EnableInputFeedbackSounds", G_TYPE_BOOLEAN,
                            G_OBJECT (enable_input_feedback_sounds_button), "active");
    gtk_widget_set_sensitive (GTK_BUTTON (enable_input_feedback_sounds_button),
                              gtk_toggle_button_get_active (enable_event_sounds_check_button));
#endif
}

gint
main(gint argc, gchar **argv)
{
    GtkWidget *dialog;
    GtkWidget *plug;
    GtkWidget *plug_child;
    GladeXML  *gxml;
    GError    *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2008");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    if (G_LIKELY (xsettings_channel))
    {
        /* load the dialog glade xml */
        gxml = glade_xml_new_from_buffer (appearance_dialog_glade, appearance_dialog_glade_length, NULL, NULL);
        if (G_LIKELY (gxml))
        {
            appearance_settings_dialog_configure_widgets (gxml);

            if (G_UNLIKELY (opt_socket_id == 0))
            {
                /* build the dialog */
                dialog = glade_xml_get_widget (gxml, "appearance-settings-dialog");

                /* run the dialog */
                gtk_dialog_run (GTK_DIALOG (dialog));

                /* destroy the dialog */
                gtk_widget_destroy (dialog);
            }
            else
            {
                /* Create plug widget */
                plug = gtk_plug_new (opt_socket_id);
                g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
                gtk_widget_show (plug);

                /* Stop startup notification */
                gdk_notify_startup_complete ();

                /* Get plug child widget */
                plug_child = glade_xml_get_widget (gxml, "plug-child");
                gtk_widget_reparent (plug_child, plug);
                gtk_widget_show (plug_child);

                /* Enter main loop */
                gtk_main ();
            }

            /* release the glade xml */
            g_object_unref (G_OBJECT (gxml));
        }

        /* release the channel */
        g_object_unref (G_OBJECT (xsettings_channel));
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
