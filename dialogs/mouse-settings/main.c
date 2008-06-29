/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2008 Mike Massonnet <mmassonnet@xfce.org>
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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xcursor/Xcursor.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "mouse-dialog_glade.h"


/* settings */
#define PREVIEW_ROWS    (3)
#define PREVIEW_COLUMNS (6)
#define PREVIEW_SIZE    (24)
#define PREVIEW_SPACING (2)



/* treeview columns */
enum
{
    COLUMN_THEME_PIXBUF,
    COLUMN_THEME_PATH,
    COLUMN_THEME_NAME,
    COLUMN_THEME_REAL_NAME,
    COLUMN_THEME_COMMENT,
    N_THEME_COLUMNS
};



/* icon names for the preview widget */
static const gchar *preview_names[] = {
    "left_ptr",            "left_ptr_watch",    "watch",             "hand2",
    "question_arrow",      "sb_h_double_arrow", "sb_v_double_arrow", "bottom_left_corner",
    "bottom_right_corner", "fleur",             "pirate",            "cross",
    "X_cursor",            "right_ptr",         "right_side",        "right_tee",
    "sb_right_arrow",      "sb_right_tee",      "base_arrow_down",   "base_arrow_up",
    "bottom_side",         "bottom_tee",        "center_ptr",        "circle",
    "dot",                 "dot_box_mask",      "dot_box_mask",      "double_arrow",
    "draped_box",          "left_side",         "left_tee",          "ll_angle",
    "top_side",            "top_tee"
};



/* option entries */
static gboolean opt_version = FALSE;

static GOptionEntry option_entries[] =
{
    { "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* global xfconf channel */
static XfconfChannel *xsettings_channel;



static GdkPixbuf *
mouse_settings_themes_pixbuf_from_filename (const gchar *filename,
                                            guint        size)
{
    XcursorImage *image;
    GdkPixbuf    *scaled, *pixbuf = NULL;
    gsize         bsize;
    guchar       *buffer, *p, tmp;
    gdouble       wratio, hratio;
    gint          dest_width, dest_height;

    /* load the image */
    image = XcursorFilenameLoadImage (filename, size);
    if (G_LIKELY (image))
    {
        /* buffer size */
        bsize = image->width * image->height * 4;

        /* allocate buffer */
        buffer = g_malloc (bsize);

        /* copy pixel data to buffer */
        memcpy (buffer, image->pixels, bsize);

        /* swap bits */
        for (p = buffer; p < buffer + bsize; p += 4)
        {
            tmp = p[0];
            p[0] = p[2];
            p[2] = tmp;
        }

        /* create pixbuf */
        pixbuf = gdk_pixbuf_new_from_data (buffer, GDK_COLORSPACE_RGB, TRUE,
                                           8, image->width, image->height,
                                           4 * image->width,
                                           (GdkPixbufDestroyNotify) g_free, NULL);

        /* don't leak when creating the pixbuf failed */
        if (G_UNLIKELY (pixbuf == NULL))
            g_free (buffer);

        /* scale pixbuf if needed */
        if (pixbuf && (image->height > size || image->width > size))
        {
            /* calculate the ratio */
            wratio = (gdouble) image->width / (gdouble) size;
            hratio = (gdouble) image->height / (gdouble) size;

            /* init */
            dest_width = dest_height = size;

            /* set dest size */
            if (hratio > wratio)
                dest_width  = rint (image->width / hratio);
            else
                dest_height = rint (image->height / wratio);

            /* scale pixbuf */
            scaled = gdk_pixbuf_scale_simple (pixbuf, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);

            /* release and set scaled pixbuf */
            g_object_unref (G_OBJECT (pixbuf));
            pixbuf = scaled;
         }

        /* cleanup */
        XcursorImageDestroy (image);
    }

    return pixbuf;
}



static GdkPixbuf *
mouse_settings_themes_preview_icon (const gchar *path)
{
    GdkPixbuf *pixbuf = NULL;
    gchar     *filename;

    /* we only try the normal cursor, it is (most likely) always there */
    filename = g_build_filename (path, "left_ptr", NULL);

    /* try to load the pixbuf */
    pixbuf = mouse_settings_themes_pixbuf_from_filename (filename, PREVIEW_SIZE);

    /* cleanup */
    g_free (filename);

    return pixbuf;
}



static void
mouse_settings_themes_preview_image (const gchar *path,
                                     GtkWidget   *image)
{
    GdkPixbuf *pixbuf;
    GdkPixbuf *preview;
    guint      i, position;
    gchar     *filename;
    gint       dest_x, dest_y;

    /* create an empty preview image */
    preview = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                              (PREVIEW_SIZE + PREVIEW_SPACING) * PREVIEW_COLUMNS - PREVIEW_SPACING,
                              (PREVIEW_SIZE + PREVIEW_SPACING) * PREVIEW_ROWS - PREVIEW_SPACING);

    if (G_LIKELY (preview))
    {
        /* make the pixbuf transparent */
        gdk_pixbuf_fill (preview, 0x00000000);

        for (i = 0, position = 0; i < G_N_ELEMENTS (preview_names); i++)
        {
            /* create cursor filename and try to load the pixbuf */
            filename = g_build_filename (path, preview_names[i], NULL);
            pixbuf = mouse_settings_themes_pixbuf_from_filename (filename, PREVIEW_SIZE);
            g_free (filename);

            if (G_LIKELY (pixbuf))
            {
                /* calculate the icon position */
                dest_x = (position % PREVIEW_COLUMNS) * (PREVIEW_SIZE + PREVIEW_SPACING);
                dest_y = (position / PREVIEW_COLUMNS) * (PREVIEW_SIZE + PREVIEW_SPACING);

                /* render it in the preview */
                gdk_pixbuf_scale (pixbuf, preview, dest_x, dest_y,
                                  gdk_pixbuf_get_width (pixbuf),
                                  gdk_pixbuf_get_height (pixbuf),
                                  dest_x, dest_y,
                                  1.00, 1.00, GDK_INTERP_BILINEAR);


                /* release the pixbuf */
                g_object_unref (G_OBJECT (pixbuf));

                /* break if we've added enough icons */
                if (++position >= PREVIEW_ROWS * PREVIEW_COLUMNS)
                    break;
            }
        }

        /* set the image */
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), preview);

        /* release the pixbuf */
        g_object_unref (G_OBJECT (preview));
    }
    else
    {
        /* clear the image */
        gtk_image_clear (GTK_IMAGE (image));
    }
}



static GtkTreePath *
mouse_settings_themes_populate_store (GtkListStore *store)
{
    const gchar  *path;
    gchar       **basedirs;
    gint          i;
    gchar        *homedir;
    GDir         *dir;
    const gchar  *theme;
    gchar        *filename;
    gchar        *index_file;
    XfceRc       *rc;
    const gchar  *name;
    const gchar  *comment;
    GtkTreeIter   iter;
    gint          position = 0;
    GdkPixbuf    *pixbuf;
    gchar        *active_theme;
    GtkTreePath  *active_path = NULL;

    /* get the cursor paths */
#if XCURSOR_LIB_MAJOR == 1 && XCURSOR_LIB_MINOR < 1
    path = "~/.icons:/usr/share/icons:/usr/share/pixmaps:/usr/X11R6/lib/X11/icons";
#else
    path = XcursorLibraryPath ();
#endif

    /* split the paths */
    basedirs = g_strsplit (path, ":", -1);

    /* get the active theme */
    active_theme = xfconf_channel_get_string (xsettings_channel, "/Gtk/CursorThemeName", "default");

    if (G_LIKELY (basedirs))
    {
        /* walk the base directories */
        for (i = 0; basedirs[i] != NULL; i++)
        {
            /* init */
            homedir = NULL;

            /* parse the homedir if needed */
            if (strstr (basedirs[i], "~/") != NULL)
                path = homedir = g_strconcat (g_get_home_dir (), basedirs[i] + 1, NULL);
            else
                path = basedirs[i];

            /* open directory */
            dir = g_dir_open (path, 0, NULL);
            if (G_LIKELY (dir))
            {
                for (;;)
                {
                    /* get the directory name */
                    theme = g_dir_read_name (dir);
                    if (G_UNLIKELY (theme == NULL))
                        break;

                    /* build the full cursor path */
                    filename = g_build_filename (path, theme, "cursors", NULL);

                    /* check if it looks like a cursor theme */
                    if (g_file_test (filename, G_FILE_TEST_IS_DIR))
                    {
                        /* try to load a pixbuf */
                        pixbuf = mouse_settings_themes_preview_icon (filename);

                        /* insert in the store */
                        gtk_list_store_insert_with_values (store, &iter, position++,
                                                           COLUMN_THEME_PIXBUF, pixbuf,
                                                           COLUMN_THEME_NAME, theme,
                                                           COLUMN_THEME_REAL_NAME, theme,
                                                           COLUMN_THEME_PATH, filename, -1);

                        /* check if this is the active theme, set the path */
                        if (strcmp (active_theme, theme) == 0 && active_path == NULL)
                            active_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);

                        /* release pixbuf */
                        if (G_LIKELY (pixbuf))
                            g_object_unref (G_OBJECT (pixbuf));

                        /* check for a index.theme file for additional information */
                        index_file = g_build_filename (path, theme, "index.theme", NULL);
                        if (g_file_test (index_file, G_FILE_TEST_IS_REGULAR))
                        {
                            /* open theme desktop file */
                            rc = xfce_rc_simple_open (index_file, TRUE);
                            if (G_LIKELY (rc))
                            {
                                /* check for the theme group */
                                if (xfce_rc_has_group (rc, "Icon Theme"))
                                {
                                    /* set group */
                                    xfce_rc_set_group (rc, "Icon Theme");

                                    /* read values */
                                    name = xfce_rc_read_entry (rc, "Name", theme);
                                    comment = xfce_rc_read_entry (rc, "Comment", NULL);

                                    /* update store */
                                    gtk_list_store_set (store, &iter,
                                                        COLUMN_THEME_REAL_NAME, name,
                                                        COLUMN_THEME_COMMENT, comment, -1);
                                }

                                /* close rc file */
                                xfce_rc_close (rc);
                            }
                        }

                        /* cleanup */
                        g_free (index_file);
                    }

                    /* cleanup */
                    g_free (filename);
                }

                /* close directory */
                g_dir_close (dir);
            }

            /* cleanup */
            g_free (homedir);
        }

        /* cleanup */
        g_strfreev (basedirs);
    }

    /* cleanup */
    g_free (active_theme);

    return active_path;
}



static void
mouse_settings_themes_selection_changed (GtkTreeSelection *selection,
                                         GladeXML         *gxml)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gchar        *path, *name;

    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get theme information from model */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_PATH, &path,
                            COLUMN_THEME_NAME, &name, -1);

        /* update the preview widget */
        mouse_settings_themes_preview_image (path, glade_xml_get_widget (gxml, "cursor_preview_image"));

        /* write configuration */
        xfconf_channel_set_string (xsettings_channel, "/Gtk/CursorThemeName", name);

        /* cleanup */
        g_free (path);
        g_free (name);
    }
}



static GtkWidget *
mouse_settings_dialog_new_from_xml (GladeXML *gxml)
{
    GtkListStore      *store;
    GtkWidget         *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer   *renderer;
    GtkTreeSelection  *selection;
    GtkWidget         *widget;
    GtkTreePath       *path;
    GtkWidget         *dialog;
    GtkAdjustment     *adjustment;

    /* setup the icon theme treeview */
    store = gtk_list_store_new (N_THEME_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* add all the themes to the tree */
    path = mouse_settings_themes_populate_store (store);

    treeview = glade_xml_get_widget (gxml, "treeview_cursor_theme");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
#if GTK_CHECK_VERSION (2, 12, 0)
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_THEME_COMMENT);
#endif

    g_object_unref (G_OBJECT (store));

    renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "pixbuf", COLUMN_THEME_PIXBUF, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", COLUMN_THEME_REAL_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (mouse_settings_themes_selection_changed), gxml);

    /* select the active item */
    if (G_LIKELY (path != NULL))
    {
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path, NULL, FALSE, 0.5, 0.0);
        gtk_tree_path_free (path);
    }

    /* sort the tree, after setting the active item */
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COLUMN_THEME_REAL_NAME, GTK_SORT_ASCENDING);

    /* connect xfconf properties */
    widget = glade_xml_get_widget (gxml, "button_right_handed");
    xfconf_g_property_bind(xsettings_channel, "/Mouse/RightHanded", G_TYPE_INT, G_OBJECT (widget), "active");

    adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse_motion_acceleration")));
    xfconf_g_property_bind(xsettings_channel, "/Mouse/Acceleration", G_TYPE_INT, G_OBJECT (adjustment), "value");

    adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse_motion_threshold")));
    xfconf_g_property_bind(xsettings_channel, "/Mouse/Threshold", G_TYPE_INT, G_OBJECT (adjustment), "value");

    adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse_dnd_threshold")));
    xfconf_g_property_bind(xsettings_channel, "/Net/DndDragThreshold", G_TYPE_INT, G_OBJECT (adjustment), "value");

    adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse_double_click_speed")));
    xfconf_g_property_bind(xsettings_channel, "/Net/DoubleClickTime", G_TYPE_INT, G_OBJECT (adjustment), "value");

    widget = glade_xml_get_widget (gxml, "spin_cursor_size");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/CursorThemeSize", G_TYPE_INT, G_OBJECT (widget), "value");

    /* show all the dialog widgets */
    dialog = glade_xml_get_widget (gxml, "mouse-settings-dialog");
    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    return dialog;
}



gint
main(gint argc, gchar **argv)
{
    GtkWidget *dialog;
    GladeXML  *gxml;
    GError    *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error == NULL))
        {
            g_critical (_("Failed to open display"));
        }
        else
        {
            /* show error message */
            g_critical (error->message);

            /* cleanup */
            g_error_free (error);
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", PACKAGE_NAME, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2008");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    xfconf_init (NULL);

    /* open the xsettings channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    if (G_LIKELY (xsettings_channel))
    {
        /* load the dialog glade xml */
        gxml = glade_xml_new_from_buffer (mouse_dialog_glade, mouse_dialog_glade_length, NULL, NULL);
        if (G_LIKELY (gxml))
        {
            /* build the dialog */
            dialog = mouse_settings_dialog_new_from_xml (gxml);

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

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

