/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
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
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif /* !HAVE_XCURSOR */

#ifdef HAVE_HAL
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <hal/libhal.h>
#endif /* !HAVE_HAL */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "mouse-dialog_glade.h"



/* settings */
#ifdef HAVE_XCURSOR
#define PREVIEW_ROWS    (3)
#define PREVIEW_COLUMNS (6)
#define PREVIEW_SIZE    (24)
#define PREVIEW_SPACING (2)
#endif /* !HAVE_XCURSOR */


/* global setting channels */
XfconfChannel *xsettings_channel;
XfconfChannel *xdevices_channel;

/* lock counter to avoid signals during updates */
static gint locked = 0;

/* the display for this window */
static GdkDisplay *display;

/* device update id */
static guint timeout_id = 0;

/* option entries */
static gboolean opt_version = FALSE;

static GOptionEntry option_entries[] =
{
    { "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

#ifdef HAVE_XCURSOR
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

enum
{
    COLUMN_THEME_PIXBUF,
    COLUMN_THEME_PATH,
    COLUMN_THEME_NAME,
    COLUMN_THEME_DISPLAY_NAME,
    COLUMN_THEME_COMMENT,
    N_THEME_COLUMNS
};
#endif /* !HAVE_XCURSOR */

enum
{
    COLUMN_DEVICE_ICON,
    COLUMN_DEVICE_NAME,
    COLUMN_DEVICE_DISPLAY_NAME,
    COLUMN_DEVICE_XID,
    COLUMN_DEVICE_NBUTTONS,
    N_DEVICE_COLUMNS
};


#ifdef HAVE_XCURSOR
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
                                     GtkImage    *image)
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



static void
mouse_settings_themes_selection_changed (GtkTreeSelection *selection,
                                         GladeXML       *gxml)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gchar        *path, *name;
    GtkWidget      *image;

    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get theme information from model */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_PATH, &path,
                            COLUMN_THEME_NAME, &name, -1);

        /* update the preview widget */
        image = glade_xml_get_widget (gxml, "mouse-theme-preview");
        mouse_settings_themes_preview_image (path, GTK_IMAGE (image));

        /* write configuration (not during a lock) */
        if (locked == 0)
            xfconf_channel_set_string (xsettings_channel, "/Gtk/CursorThemeName", name);

        /* cleanup */
        g_free (path);
        g_free (name);
    }
}



static gint
mouse_settings_themes_sort_func (GtkTreeModel *model,
                                 GtkTreeIter  *a,
                                 GtkTreeIter  *b,
                                 gpointer      user_data)
{
    gchar *name_a, *name_b;
    gint   retval;

    /* get the names from the model */
    gtk_tree_model_get (model, a, COLUMN_THEME_DISPLAY_NAME, &name_a, -1);
    gtk_tree_model_get (model, b, COLUMN_THEME_DISPLAY_NAME, &name_b, -1);

    /* make sure the names are not null */
    if (G_UNLIKELY (name_a == NULL))
        name_a = g_strdup ("");
    if (G_UNLIKELY (name_b == NULL))
        name_b = g_strdup ("");

    /* sort the names but keep Default on top */
    if (g_utf8_collate (name_a, _("Default")) == 0)
        retval = -1;
    else if (g_utf8_collate (name_b, _("Default")) == 0)
        retval = 1;
    else
        retval = g_utf8_collate (name_a, name_b);

    /* cleanup */
    g_free (name_a);
    g_free (name_b);

    return retval;
}



static void
mouse_settings_themes_populate_store (GladeXML *gxml)
{
    const gchar        *path;
    gchar             **basedirs;
    gint                i;
    gchar              *homedir;
    GDir               *dir;
    const gchar        *theme;
    gchar              *filename;
    gchar              *index_file;
    XfceRc             *rc;
    const gchar        *name;
    const gchar        *comment;
    GtkTreeIter         iter;
    gint                position = 0;
    GdkPixbuf          *pixbuf;
    gchar              *active_theme;
    GtkTreePath        *active_path = NULL;
    GtkListStore       *store;
    GtkCellRenderer    *renderer;
    GtkTreeViewColumn  *column;
    GtkWidget          *treeview;
    GtkTreeSelection   *selection;

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

    /* create the store */
    store = gtk_list_store_new (N_THEME_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* insert default */
    gtk_list_store_insert_with_values (store, &iter, position++,
                                       COLUMN_THEME_NAME, "default",
                                       COLUMN_THEME_DISPLAY_NAME, _("Default"), -1);

    /* store the default path, so we always select a theme */
    active_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);

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
                                                           COLUMN_THEME_DISPLAY_NAME, theme,
                                                           COLUMN_THEME_PATH, filename, -1);

                        /* check if this is the active theme, set the path */
                        if (strcmp (active_theme, theme) == 0)
                        {
                            gtk_tree_path_free (active_path);
                            active_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
                        }

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
                                                        COLUMN_THEME_DISPLAY_NAME, name,
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

    /* set the treeview store */
    treeview = glade_xml_get_widget (gxml, "mouse-theme-treeview");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
#if GTK_CHECK_VERSION (2, 12, 0)
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_THEME_COMMENT);
#endif

    /* setup the columns */
    renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "pixbuf", COLUMN_THEME_PIXBUF, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    /* setup selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (mouse_settings_themes_selection_changed), gxml);

    /* select the active theme in the treeview */
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), active_path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), active_path, NULL, FALSE, 0.5, 0.0);
    gtk_tree_path_free (active_path);

    /* sort the store */
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), COLUMN_THEME_DISPLAY_NAME, mouse_settings_themes_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);

    /* release the store */
    g_object_unref (G_OBJECT (store));
}
#endif /* !HAVE_XCURSOR */



static void
mouse_settings_device_selection_changed (GtkTreeSelection *selection,
                                         GladeXML         *gxml)
{
    gint               nbuttons;
    Display           *xdisplay;
    XDevice           *device;
    XFeedbackState    *states;
    gint               nstates;
    XPtrFeedbackState *state;
    gint               i;
    guchar            *buttonmap;
    gint               id_1 = 0, id_3 = 0;
    gint               id_4 = 0, id_5 = 0;
    gdouble            acceleration = -1.00;
    gint               threshold = -1;
    GtkWidget         *widget;
    GtkTreeModel      *model;
    GtkTreeIter        iter;
    gboolean           has_selection;
    XID                xid;

    /* lock the dialog */
    locked++;

    /* get the selected item */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get device id and number of buttons */
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_XID, &xid,
                            COLUMN_DEVICE_NBUTTONS, &nbuttons, -1);

        /* get the x display */
        xdisplay = gdk_x11_display_get_xdisplay (display);

        /* open the device */
        device = XOpenDevice (xdisplay, xid);

        if (G_LIKELY (device))
        {
            /* allocate button map */
            buttonmap = g_new0 (guchar, nbuttons);

            /* get the button mapping */
            XGetDeviceButtonMapping (xdisplay, device, buttonmap, nbuttons);

            /* figure out the position of the first and second/third button in the map */
            for (i = 0; i < nbuttons; i++)
            {
                if (buttonmap[i] == 1)
                    id_1 = i;
                else if (buttonmap[i] == (nbuttons < 3 ? 2 : 3))
                    id_3 = i;
                else if (buttonmap[i] == 4)
                    id_4 = i;
                else if (buttonmap[i] == 5)
                    id_5 = i;
            }

            /* cleanup */
            g_free (buttonmap);

            /* get the feedback states for this device */
            states = XGetFeedbackControl (xdisplay, device, &nstates);

            /* intial values */
            acceleration = threshold = -1;

            /* get the pointer feedback class */
            for (i = 0; i < nstates; i++)
            {
                if (states->class == PtrFeedbackClass)
                {
                    /* get the state */
                    state = (XPtrFeedbackState *) states;

                    /* set values */
                    acceleration = (gdouble) state->accelNum / (gdouble) state->accelDenom;
                    threshold = state->threshold;

                    /* done */
                    break;
                }

                /* advance the offset */
                states = (XFeedbackState *) ((gchar *) states + states->length);
            }

            /* close the device */
            XCloseDevice (xdisplay, device);
        }
    }

    /* update button order */
    widget = glade_xml_get_widget (gxml, id_1 > id_3 ? "mouse-left-handed" : "mouse-right-handed");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

    widget = glade_xml_get_widget (gxml, "mouse-reverse-scrolling");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !!(id_5 < id_4));
    gtk_widget_set_sensitive (widget, nbuttons >= 5);

    /* update acceleration scale */
    widget = glade_xml_get_widget (gxml, "mouse-acceleration-scale");
    gtk_range_set_value (GTK_RANGE (widget), acceleration);
    gtk_widget_set_sensitive (GTK_WIDGET (widget), acceleration != -1);

    /* update threshold scale */
    widget = glade_xml_get_widget (gxml, "mouse-threshold-scale");
    gtk_range_set_value (GTK_RANGE (widget), threshold);
    gtk_widget_set_sensitive (GTK_WIDGET (widget), threshold != -1);

    /* unlock */
    locked--;
}



static void
mouse_settings_device_save (GladeXML *gxml)
{
    GtkWidget        *treeview;
    GtkTreeSelection *selection;
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    gboolean          has_selection;
    gchar            *name;
    GtkWidget        *widget;
    gchar            *property_name;
    gboolean          righthanded;
    gint              threshold;
    gdouble           acceleration;
    gboolean          reverse_scrolling;

    g_return_if_fail (GLADE_IS_XML (gxml));

    /* leave when locked */
    if (locked > 0)
        return;

    /* get the treeview */
    treeview = glade_xml_get_widget (gxml, "mouse-devices-treeview");

    /* get the selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get device id and number of buttons */
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_NAME, &name, -1);

        if (G_LIKELY (name))
        {
            /* store the button order */
            widget = glade_xml_get_widget (gxml, "mouse-right-handed");
            property_name = g_strdup_printf ("/Pointers/%s/RightHanded", name);
            righthanded = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
            if (xfconf_channel_get_bool (xdevices_channel, property_name, TRUE) != righthanded)
                xfconf_channel_set_bool (xdevices_channel, property_name, righthanded);
            g_free (property_name);

            /* store reverse scrolling */
            widget = glade_xml_get_widget (gxml, "mouse-reverse-scrolling");
            property_name = g_strdup_printf ("/Pointers/%s/ReverseScrolling", name);
            reverse_scrolling = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
            if (xfconf_channel_get_bool (xdevices_channel, property_name, TRUE) != reverse_scrolling)
                xfconf_channel_set_bool (xdevices_channel, property_name, reverse_scrolling);
            g_free (property_name);

            /* store the threshold */
            widget = glade_xml_get_widget (gxml, "mouse-threshold-scale");
            property_name = g_strdup_printf ("/Pointers/%s/Threshold", name);
            threshold = gtk_range_get_value (GTK_RANGE (widget));
            if (xfconf_channel_get_int (xdevices_channel, property_name, -1) != threshold)
                xfconf_channel_set_int (xdevices_channel, property_name, threshold);
            g_free (property_name);

            /* store the acceleration */
            widget = glade_xml_get_widget (gxml, "mouse-acceleration-scale");
            property_name = g_strdup_printf ("/Pointers/%s/Acceleration", name);
            acceleration = gtk_range_get_value (GTK_RANGE (widget));
            if (xfconf_channel_get_double (xdevices_channel, property_name, -1) != acceleration)
                xfconf_channel_set_double (xdevices_channel, property_name, acceleration);
            g_free (property_name);

            /* cleanup */
            g_free (name);
        }
    }
}



static gchar *
mouse_settings_device_xfconf_name (const gchar *name)
{
    GString     *string;
    const gchar *p;

    /* NOTE: this function exists in both the dialog and
     *       helper code and they have to identical! */

    /* allocate a string */
    string = g_string_sized_new (strlen (name));

    /* create a name with only valid chars */
    for (p = name; *p != '\0'; p++)
    {
        if ((*p >= 'A' && *p <= 'Z')
            || (*p >= 'a' && *p <= 'z')
            || (*p >= '0' && *p <= '9')
            || *p == '_' || *p == '-')
          string = g_string_append_c (string, *p);
        else if (*p == ' ')
            string = g_string_append_c (string, '_');
    }

    /* return the new string */
    return g_string_free (string, FALSE);
}



static void
mouse_settings_device_populate_store (GladeXML *gxml,
                                      gboolean  create_store)
{
    Display           *xdisplay;
    XDeviceInfo       *device_list, *device_info;
    gchar             *display_name, *usb;
    gshort             num_buttons;
    gint               ndevices;
    gint               i, m;
    XAnyClassPtr       ptr;
    GtkTreeIter        iter;
    GtkListStore      *store;
    GtkWidget         *treeview;
    GtkTreePath       *path;
    GtkTreeViewColumn *column;
    GtkCellRenderer   *renderer;
    GtkTreeSelection  *selection;
    gchar             *device_name;

    /* lock */
    locked++;

    /* get the treeview */
    treeview = glade_xml_get_widget (gxml, "mouse-devices-treeview");

    /* create or get the store */
    if (G_LIKELY (create_store))
    {
        store = gtk_list_store_new (N_DEVICE_COLUMNS, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
    }
    else
    {
        store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
        gtk_list_store_clear (store);
    }

    /* get the x display */
    xdisplay = gdk_x11_display_get_xdisplay (display);

    /* get all the registered devices */
    device_list = XListInputDevices (xdisplay, &ndevices);

    for (i = 0; i < ndevices; i++)
    {
        /* get the device */
        device_info = &device_list[i];

        /* filter out the pointer devices */
        if (device_info->use == IsXExtensionPointer)
        {
            /* get the device name */
            display_name = g_strdup (device_info->name);

            /* get rid of usb crap in the name */
            if ((usb = strstr (display_name, "-usb")) != NULL)
                *usb = '\0';

            /* get the device classes */
            ptr = device_info->inputclassinfo;

            /* walk all the classes */
            for (m = 0, num_buttons = 0; m < device_info->num_classes; m++)
            {
                /* find the button class */
                if (ptr->class == ButtonClass)
                {
                    /* get the number of buttons */
                    num_buttons = ((XButtonInfoPtr) ptr)->num_buttons;

                    /* done */
                    break;
                }

                /* advance the offset */
                ptr = (XAnyClassPtr) ((gchar *) ptr + ptr->length);
            }

            /* insert the device if it has buttons */
            if (G_LIKELY (num_buttons > 0))
            {
                /* create a valid xfconf device name */
                device_name = mouse_settings_device_xfconf_name (device_info->name);

                /* insert in the store */
                gtk_list_store_insert_with_values (store, &iter, i,
                                                   COLUMN_DEVICE_ICON, "input-mouse",
                                                   COLUMN_DEVICE_NAME, device_name,
                                                   COLUMN_DEVICE_DISPLAY_NAME, display_name,
                                                   COLUMN_DEVICE_XID, device_info->id,
                                                   COLUMN_DEVICE_NBUTTONS, num_buttons, -1);

                /* cleanup */
                g_free (device_name);
            }

            /* cleanup */
            g_free (display_name);
        }
    }

    /* cleanup */
    XFreeDeviceList (device_list);

    /* get the selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (G_LIKELY (create_store))
    {
        /* set the treeview model */
        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COLUMN_DEVICE_XID, GTK_SORT_ASCENDING);
        g_object_unref (G_OBJECT (store));

        /* icon renderer */
        renderer = gtk_cell_renderer_pixbuf_new ();
        column = gtk_tree_view_column_new_with_attributes ("", renderer, "icon-name", COLUMN_DEVICE_ICON, NULL);
        g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DND, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        /* text renderer */
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", COLUMN_DEVICE_DISPLAY_NAME, NULL);
        g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        /* setup tree selection */
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (mouse_settings_device_selection_changed), gxml);
    }

    /* select the first mouse in the tree */
    path = gtk_tree_path_new_first ();
    gtk_tree_selection_select_path (selection, path);
    gtk_tree_path_free (path);

    /* unlock */
    locked--;
}



static gboolean
mouse_settings_device_update_sliders (gpointer user_data)
{
    GladeXML  *gxml = GLADE_XML (user_data);
    GtkWidget *treeview;
    GtkWidget *button;

    GDK_THREADS_ENTER ();

    /* get the treeview */
    treeview = glade_xml_get_widget (gxml, "mouse-devices-treeview");

    /* update */
    mouse_settings_device_selection_changed (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), gxml);

    /* make the button sensitive again */
    button = glade_xml_get_widget (gxml, "mouse-reset");
    gtk_widget_set_sensitive (button, TRUE);

    GDK_THREADS_LEAVE ();

    return FALSE;
}



static void
mouse_settings_device_list_changed_timeout_destroyed (gpointer user_data)
{
    /* reset the timeout id */
    timeout_id = 0;
}



#ifdef HAVE_HAL
static gboolean
mouse_settings_device_list_changed_timeout (gpointer user_data)
{
    GladeXML *gxml = GLADE_XML (user_data);

    GDK_THREADS_ENTER ();

    /* update the list */
    mouse_settings_device_populate_store (gxml, FALSE);

    GDK_THREADS_LEAVE ();

    return FALSE;
}



static void
mouse_settings_device_list_changed (LibHalContext *context,
                                    const gchar   *udi)
{
    GladeXML *gxml;

    /* queue a new timeout if none is set */
    if (timeout_id == 0)
    {
        /* get the user data */
        gxml = libhal_ctx_get_user_data (context);

        /* update the dialog in 1 second */
        timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 1000, mouse_settings_device_list_changed_timeout,
                                         gxml, mouse_settings_device_list_changed_timeout_destroyed);
    }
}
#endif  /* !HAVE_HAL */


static void
mouse_settings_device_reset (GtkWidget *button,
                             GladeXML  *gxml)
{
    GtkWidget        *treeview;
    GtkTreeSelection *selection;
    gchar            *name, *property_name;
    gboolean          has_selection;
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    /* leave when locked */
    if (locked > 0)
        return;

    /* get the treeview */
    treeview = glade_xml_get_widget (gxml, "mouse-devices-treeview");

    /* get the selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get device id and number of buttons */
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_NAME, &name, -1);

        if (G_LIKELY (name && timeout_id == 0))
        {
            /* make the button insensitive */
            gtk_widget_set_sensitive (button, FALSE);

            /* set the threshold to -1 */
            property_name = g_strdup_printf ("/Pointers/%s/Threshold", name);
            xfconf_channel_set_int (xdevices_channel, property_name, -1);
            g_free (property_name);

            /* set the acceleration to -1 */
            property_name = g_strdup_printf ("/Pointers/%s/Acceleration", name);
            xfconf_channel_set_double (xdevices_channel, property_name, -1.00);
            g_free (property_name);

            /* update the sliders in 500ms */
            timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 500, mouse_settings_device_update_sliders,
                                             gxml, mouse_settings_device_list_changed_timeout_destroyed);


        }

        /* cleanup */
        g_free (name);
    }
}



gint
main(gint argc, gchar **argv)
{
    GtkWidget      *dialog;
    GladeXML       *gxml;
    GError         *error = NULL;
    GtkAdjustment  *adjustment;
    GtkWidget      *widget;
#ifdef HAVE_HAL
    DBusConnection *connection;
    LibHalContext  *context = NULL;
    DBusError       derror;
#endif /* !HAVE_HAL */

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
    if (G_UNLIKELY (!xfconf_init (&error)))
    {
        /* print error and leave */
        g_critical ("Failed to connect to Xfconf daemon: %s", error->message);
        g_error_free (error);
        
        return EXIT_FAILURE;
    }

    /* open the xsettings and xdevices channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    xdevices_channel = xfconf_channel_new ("xdevices");

    if (G_LIKELY (xdevices_channel && xsettings_channel))
    {
        /* load the glade xml file */
        gxml = glade_xml_new_from_buffer (mouse_dialog_glade, mouse_dialog_glade_length, NULL, NULL);
        if (G_LIKELY (gxml))
        {
            /* lock */
            locked++;

            /* set the working display for this instance */
            display = gdk_display_get_default ();

            /* populate the devices treeview */
            mouse_settings_device_populate_store (gxml, TRUE);

            /* connect signals */
            widget = glade_xml_get_widget (gxml, "mouse-acceleration-scale");
            g_signal_connect_swapped (G_OBJECT (widget), "value-changed", G_CALLBACK (mouse_settings_device_save), gxml);

            widget = glade_xml_get_widget (gxml, "mouse-threshold-scale");
            g_signal_connect_swapped (G_OBJECT (widget), "value-changed", G_CALLBACK (mouse_settings_device_save), gxml);

            widget = glade_xml_get_widget (gxml, "mouse-left-handed");
            g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (mouse_settings_device_save), gxml);

            widget = glade_xml_get_widget (gxml, "mouse-right-handed");
            g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (mouse_settings_device_save), gxml);

            widget = glade_xml_get_widget (gxml, "mouse-reverse-scrolling");
            g_signal_connect_swapped (G_OBJECT (widget), "toggled", G_CALLBACK (mouse_settings_device_save), gxml);

            widget = glade_xml_get_widget (gxml, "mouse-reset");
            g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (mouse_settings_device_reset), gxml);

#ifdef HAVE_XCURSOR
            /* populate the themes treeview */
            mouse_settings_themes_populate_store (gxml);

            /* connect the cursor size in the cursor tab */
            widget = glade_xml_get_widget (gxml, "mouse-cursor-size");
            xfconf_g_property_bind (xsettings_channel, "/Gtk/CursorThemeSize", G_TYPE_INT, G_OBJECT (widget), "value");
#else
            /* hide the themes tab */
            widget = glade_xml_get_widget (gxml, "mouse-themes-hbox");
            gtk_widget_hide (widget);
#endif /* !HAVE_XCURSOR */

            /* connect sliders in the gtk tab */
            adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse-dnd-threshold")));
            xfconf_g_property_bind (xsettings_channel, "/Net/DndDragThreshold", G_TYPE_INT, G_OBJECT (adjustment), "value");

            adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse-double-click-time")));
            xfconf_g_property_bind (xsettings_channel, "/Net/DoubleClickTime", G_TYPE_INT, G_OBJECT (adjustment), "value");

            adjustment = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "mouse-double-click-distance")));
            xfconf_g_property_bind (xsettings_channel, "/Net/DoubleClickDistance", G_TYPE_INT, G_OBJECT (adjustment), "value");

#ifdef HAVE_HAL
            /* initialize the dbus error variable */
            dbus_error_init (&derror);

            /* connect to the dbus system bus */
            connection = dbus_bus_get (DBUS_BUS_SYSTEM, &derror);
            if (G_LIKELY (connection))
            {
                /* connect dbus to the main loop */
                dbus_connection_setup_with_g_main (connection, NULL);

                /* create hal context */
                context = libhal_ctx_new ();
                if (G_LIKELY (context))
                {
                    /* set user data for the callbacks */
                    libhal_ctx_set_user_data (context, gxml);

                    /* set the dbus connection */
                    if (G_LIKELY (libhal_ctx_set_dbus_connection (context, connection)))
                    {
                        /* connect to hal */
                        if (G_LIKELY (libhal_ctx_init (context, &derror)))
                        {
                            /* add callbacks for device changes */
                            libhal_ctx_set_device_added (context, mouse_settings_device_list_changed);
                            libhal_ctx_set_device_removed (context, mouse_settings_device_list_changed);
                        }
                        else
                        {
                           /* print warning */
                           g_warning ("Failed to connect to hald: %s", derror.message);

                           /* cleanup */
                           LIBHAL_FREE_DBUS_ERROR (&derror);
                        }
                    }
                }
            }
            else
            {
                /* print warning */
                g_warning ("Failed to connect to DBus: %s", derror.message);

                /* cleanup */
                LIBHAL_FREE_DBUS_ERROR (&derror);
            }
#endif /* !HAVE_HAL */

            /* gtk the dialog */
            dialog = glade_xml_get_widget (gxml, "mouse-dialog");

            /* unlock */
            locked--;

            /* show the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

#ifdef HAVE_HAL
            /* close the hal connection */
            if (G_LIKELY (context))
            {
                libhal_ctx_shutdown (context, NULL);
                libhal_ctx_free (context);
            }

            /* close the dbus connection */
            if (G_LIKELY (connection))
                dbus_connection_unref (connection);

            /* stop any running sources */
            if (G_UNLIKELY (timeout_id != 0))
                g_source_remove (timeout_id);
#endif /* !HAVE_HAL */   

            /* destroy the dialog */
            gtk_widget_destroy (GTK_WIDGET (dialog));

            /* release the glade xml */
            g_object_unref (G_OBJECT (gxml));
        }

        /* release the channels */
        g_object_unref (G_OBJECT (xsettings_channel));
        g_object_unref (G_OBJECT (xdevices_channel));
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
