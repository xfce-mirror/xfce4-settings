/*
 *  Copyright (c) 2008-2011 Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2008      Jannis Pohlmann <jannis@xfce.org>
 *  Copyright (c) 2026      Brian Tarricone <brian@tarricone.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include "xfce-device-manager.h"
#include "xfce-device.h"

#ifdef ENABLE_X11
#include "xfce-device-x11.h"
#endif

#if defined(ENABLE_X11) && defined(HAVE_XRANDR)
#include "common/xfce-randr.h"
#endif

#ifdef ENABLE_WAYLAND
#include "common/xfce-wlr-output-manager.h"
#endif

#include <cairo-gobject.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif /* !HAVE_XCURSOR */

#ifdef HAVE_MATH_H
#include <math.h>
#endif

/* settings */
#ifdef HAVE_XCURSOR
#define PREVIEW_ROWS (3)
#define PREVIEW_COLUMNS (6)
#define PREVIEW_SIZE (24)
#define PREVIEW_SPACING (2)
#endif /* !HAVE_XCURSOR */


/* global setting channels */
static XfconfChannel *xsettings_channel;
static XfconfChannel *pointers_channel;

/* the input devices backing the device combobox */
static XfceDeviceManager *device_manager = NULL;

/* the device whose "changed" signal is currently being followed, and the
 * reference and handler that keep it alive */
static XfceDevice *selected_device = NULL;
static gulong selected_device_changed_id = 0;

/* the selected device by name, which unlike the device itself survives the
 * device being unplugged and plugged back in */
static gchar *selected_device_name = NULL;

/* lock counter to avoid signals during updates */
static gint locked = 0;

#ifdef ENABLE_X11
/* device update id */
static guint timeout_id = 0;
#endif

/* option entries */
static gint opt_socket_id = 0;
static gchar *opt_device_name = NULL;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] = {
    { "device", 'd', 0, G_OPTION_ARG_STRING, &opt_device_name, N_ ("Active device in the dialog"), N_ ("DEVICE NAME") },
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_ ("Settings manager socket"), N_ ("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Version information"), NULL },
    { NULL }
};

#ifdef HAVE_XCURSOR
/* icon names for the preview widget */
/* clang-format off */
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
/* clang-format on */

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
    COLUMN_DEVICE_NAME,
    COLUMN_DEVICE_OBJECT,
    N_DEVICE_COLUMNS
};

/* The scroll-method combobox rows, in the order the Glade file lists them. */
enum
{
    SCROLL_MODE_DISABLED,
    SCROLL_MODE_EDGE,
    SCROLL_MODE_TWO_FINGER,
    SCROLL_MODE_CIRCULAR,
};

/* The click-method combobox rows, in the order the Glade file lists them. */
enum
{
    CLICK_METHOD_NONE,
    CLICK_METHOD_BUTTON_AREAS,
    CLICK_METHOD_CLICKFINGER,
};

static void
mouse_settings_device_selection_changed (GtkBuilder *builder);
static void
mouse_settings_device_populate_store (GtkBuilder *builder,
                                      gboolean create_store);


static gchar *
mouse_settings_format_value_px (GtkScale *scale,
                                gdouble value)
{
    /* pixel value for some of the scales in the dialog */
    return g_strdup_printf (_("%g px"), value);
}



static gchar *
mouse_settings_format_value_ms (GtkScale *scale,
                                gdouble value)
{
    /* miliseconds value for some of the scales in the dialog */
    return g_strdup_printf (_("%g ms"), value);
}



static gchar *
mouse_settings_format_value_s (GtkScale *scale,
                               gdouble value)
{
    /* seconds value for some of the scales in the dialog */
    return g_strdup_printf (_("%.1f s"), value);
}



#ifdef HAVE_XCURSOR
static cairo_surface_t *
mouse_settings_themes_pixbuf_from_filename (const gchar *filename,
                                            guint size,
                                            gint scale_factor)
{
    XcursorImage *image;
    GdkPixbuf *scaled, *pixbuf = NULL;
    gsize bsize;
    guchar *buffer, *p, tmp;
    gdouble wratio, hratio;
    gint dest_width, dest_height;
    guint full_size = size * scale_factor;

    /* load the image */
    image = XcursorFilenameLoadImage (filename, full_size);
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
                                           (GdkPixbufDestroyNotify) (void (*) (void)) g_free, NULL);

        /* don't leak when creating the pixbuf failed */
        if (G_UNLIKELY (pixbuf == NULL))
            g_free (buffer);

        /* scale pixbuf if needed */
        if (pixbuf && (image->height > full_size || image->width > full_size))
        {
            /* calculate the ratio */
            wratio = (gdouble) image->width / (gdouble) full_size;
            hratio = (gdouble) image->height / (gdouble) full_size;

            /* init */
            dest_width = dest_height = full_size;

            /* set dest size */
            if (hratio > wratio)
                dest_width = rint (image->width / hratio);
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

    if (G_LIKELY (pixbuf != NULL))
    {
        cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, NULL);
        g_object_unref (pixbuf);
        return surface;
    }
    else
    {
        return NULL;
    }
}



static cairo_surface_t *
mouse_settings_themes_preview_icon (const gchar *path,
                                    gint scale_factor)
{
    cairo_surface_t *surface = NULL;
    gchar *filename;

    /* we only try the normal cursor, it is (most likely) always there */
    filename = g_build_filename (path, "left_ptr", NULL);

    /* try to load the preview */
    surface = mouse_settings_themes_pixbuf_from_filename (filename, PREVIEW_SIZE, scale_factor);

    /* cleanup */
    g_free (filename);

    return surface;
}



static void
mouse_settings_themes_preview_image (const gchar *path,
                                     GtkImage *image)
{
    cairo_surface_t *preview;
    cairo_t *cr;
    guint i, position;
    gint scale_factor;

    /* create an empty preview image */
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (image));
    preview = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                          ((PREVIEW_SIZE + PREVIEW_SPACING) * PREVIEW_COLUMNS - PREVIEW_SPACING) * scale_factor,
                                          ((PREVIEW_SIZE + PREVIEW_SPACING) * PREVIEW_ROWS - PREVIEW_SPACING) * scale_factor);
    cairo_surface_set_device_scale (preview, scale_factor, scale_factor);
    cr = cairo_create (preview);

    for (i = 0, position = 0; i < G_N_ELEMENTS (preview_names); i++)
    {
        /* create cursor filename and try to load the pixbuf */
        gchar *filename = g_build_filename (path, preview_names[i], NULL);
        cairo_surface_t *surface = mouse_settings_themes_pixbuf_from_filename (filename, PREVIEW_SIZE, scale_factor);

        g_free (filename);

        if (G_LIKELY (surface))
        {
            gint dest_x, dest_y;

            cairo_save (cr);

            /* calculate the icon position */
            dest_x = (position % PREVIEW_COLUMNS) * (PREVIEW_SIZE + PREVIEW_SPACING);
            dest_y = (position / PREVIEW_COLUMNS) * (PREVIEW_SIZE + PREVIEW_SPACING);
            cairo_translate (cr, dest_x, dest_y);

            cairo_set_source_surface (cr, surface, 0, 0);
            cairo_paint (cr);

            cairo_restore (cr);
            cairo_surface_destroy (surface);

            /* break if we've added enough icons */
            if (++position >= PREVIEW_ROWS * PREVIEW_COLUMNS)
                break;
        }
    }

    cairo_destroy (cr);

    gtk_image_set_from_surface (image, preview);
    cairo_surface_destroy (preview);
}



static void
mouse_settings_themes_selection_changed (GtkTreeSelection *selection,
                                         GtkBuilder *builder)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean has_selection;
    gchar *path, *name;
    GObject *image;

    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get theme information from model */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_PATH, &path,
                            COLUMN_THEME_NAME, &name, -1);

        /* update the preview widget */
        image = gtk_builder_get_object (builder, "theme-preview");
        mouse_settings_themes_preview_image (path, GTK_IMAGE (image));

        /* write configuration (not during a lock) */
        if (locked == 0)
        {
            xfconf_channel_set_string (xsettings_channel, "/Gtk/CursorThemeName", name);
        }

        /* cleanup */
        g_free (path);
        g_free (name);
    }
}



static gint
mouse_settings_themes_sort_func (GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data)
{
    gchar *name_a, *name_b;
    gint retval;

    /* get the names from the model */
    gtk_tree_model_get (model, a, COLUMN_THEME_DISPLAY_NAME, &name_a, -1);
    gtk_tree_model_get (model, b, COLUMN_THEME_DISPLAY_NAME, &name_b, -1);

    /* make sure the names are not null */
    if (G_UNLIKELY (name_a == NULL))
        name_a = g_strdup ("");
    if (G_UNLIKELY (name_b == NULL))
        name_b = g_strdup ("");

    /* sort the names but keep Default on top */
    if (g_utf8_collate (name_a, _( "Default")) == 0)
        retval = -1;
    else if (g_utf8_collate (name_b, _( "Default")) == 0)
        retval = 1;
    else
        retval = g_utf8_collate (name_a, name_b);

    /* cleanup */
    g_free (name_a);
    g_free (name_b);

    return retval;
}



static void
mouse_settings_themes_populate_store (GtkBuilder *builder)
{
    const gchar *path;
    gchar **basedirs;
    gint i;
    gchar *homedir;
    GDir *dir;
    const gchar *theme;
    gchar *filename;
    gchar *index_file;
    XfceRc *rc;
    const gchar *name;
    const gchar *comment;
    GtkTreeIter iter;
    gint position = 0;
    gchar *active_theme;
    GtkTreePath *active_path = NULL;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GObject *treeview;
    GtkTreeSelection *selection;
    gchar *comment_escaped;
    gint scale_factor;

    /* get the cursor paths */
    path = XcursorLibraryPath ();

    /* split the paths */
    basedirs = g_strsplit (path, ":", -1);

    /* get the active theme */
    active_theme = xfconf_channel_get_string (xsettings_channel, "/Gtk/CursorThemeName", "default");

    treeview = gtk_builder_get_object (builder, "theme-treeview");
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (treeview));

    /* create the store */
    store = gtk_list_store_new (N_THEME_COLUMNS, CAIRO_GOBJECT_TYPE_SURFACE, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* insert default */
    gtk_list_store_insert_with_values (store, &iter, position++,
                                       COLUMN_THEME_NAME, "default",
                                       COLUMN_THEME_DISPLAY_NAME, _( "Default"), -1);

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
                        cairo_surface_t *surface = mouse_settings_themes_preview_icon (filename, scale_factor);

                        /* insert in the store */
                        gtk_list_store_insert_with_values (store, &iter, position++,
                                                           COLUMN_THEME_PIXBUF, surface,
                                                           COLUMN_THEME_NAME, theme,
                                                           COLUMN_THEME_DISPLAY_NAME, theme,
                                                           COLUMN_THEME_PATH, filename, -1);

                        if (G_LIKELY (surface != NULL))
                        {
                            cairo_surface_destroy (surface);
                        }

                        /* check if this is the active theme, set the path */
                        if (strcmp (active_theme, theme) == 0)
                        {
                            gtk_tree_path_free (active_path);
                            active_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
                        }

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

                                    /* escape the comment */
                                    comment_escaped = comment ? g_markup_escape_text (comment, -1) : NULL;

                                    /* update store */
                                    gtk_list_store_set (store, &iter,
                                                        COLUMN_THEME_DISPLAY_NAME, name,
                                                        COLUMN_THEME_COMMENT, comment_escaped, -1);

                                    /* cleanup */
                                    g_free (comment_escaped);
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
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_THEME_COMMENT);

    /* setup the columns */
    renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "surface", COLUMN_THEME_PIXBUF, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    /* setup selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (mouse_settings_themes_selection_changed), builder);

    /* select the active theme in the treeview */
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), active_path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), active_path, NULL, TRUE, 0.5, 0.0);
    gtk_tree_path_free (active_path);

    /* sort the store */
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), COLUMN_THEME_DISPLAY_NAME, mouse_settings_themes_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);

    /* release the store */
    g_object_unref (G_OBJECT (store));
}
#endif /* !HAVE_XCURSOR */



// Returns a new reference, and is only for finding out what the combobox has
// moved to; everything else reads `selected_device`, which holds a reference to
// that same device until the selection changes again.
static XfceDevice *
mouse_settings_device_dup_selected (GtkBuilder *builder)
{
    GObject *combobox = gtk_builder_get_object (builder, "device-combobox");
    XfceDevice *device = NULL;
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter))
    {
        GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
        gtk_tree_model_get (model, &iter, COLUMN_DEVICE_OBJECT, &device, -1);
    }

    return device;
}



// The touchpad tab collects the tap, scroll-method, click-method and
// disable-while-typing settings, so it is relevant whenever a device offers any
// of the ones only touchpads have.
static gboolean
mouse_settings_device_is_touchpad (XfceDevice *device)
{
    /* on-button-down scrolling is offered by ordinary mice, so only the methods
     * a touchpad has count towards being one */
    XfceDeviceScrollMethod scroll = xfce_device_get_scroll_method_supported (device);

    return xfce_device_get_tap_available (device)
           || xfce_device_get_dwt_available (device)
           || xfce_device_get_click_method_available (device)
           || (scroll
               & (XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER
                  | XFCE_DEVICE_SCROLL_METHOD_EDGE
                  | XFCE_DEVICE_SCROLL_METHOD_CIRCULAR))
                  != 0;
}



static gboolean
mouse_settings_device_is_touchscreen (XfceDevice *device)
{
    return (xfce_device_get_capabilities (device) & XFCE_DEVICE_CAPABILITIES_TOUCH) != 0;
}



// The syndaemon and core-X pointer feedback settings belong to the legacy X
// input drivers; libinput devices and every Wayland device configure the same
// things through the shared settings instead.
static gboolean
mouse_settings_device_is_legacy_x11 (XfceDevice *device)
{
#ifdef ENABLE_X11
    return XFCE_IS_DEVICE_X11 (device)
           && !xfce_device_x11_is_libinput (XFCE_DEVICE_X11 (device));
#else
    return FALSE;
#endif
}



static void
mouse_settings_touchscreen_append_monitors (GtkComboBoxText *combobox)
{
#if defined(ENABLE_X11) && defined(HAVE_XRANDR)
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        XfceRandr *randr = xfce_randr_new (gdk_display_get_default (), NULL);
        if (randr != NULL)
        {
            for (guint i = 0; i < randr->noutput; i++)
            {
                gchar *display_name = g_strdup_printf ("%s (%s)", randr->friendly_name[i],
                                                       xfce_randr_get_output_info_name (randr, i));
                gtk_combo_box_text_append (combobox, xfce_randr_get_edid (randr, i), display_name);
                g_free (display_name);
            }
            xfce_randr_free (randr);
        }
    }
#endif

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
        XfceWlrOutputManager *manager = xfce_wlr_output_manager_new (NULL, NULL);
        GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (manager);
        for (guint i = 0; outputs != NULL && i < outputs->len; i++)
        {
            XfceWlrOutput *output = g_ptr_array_index (outputs, i);
            gchar *display_name = g_strdup_printf ("%s (%s)",
                                                   output->description != NULL ? output->description : output->name,
                                                   output->name);
            gtk_combo_box_text_append (combobox, output->edid, display_name);
            g_free (display_name);
        }
        g_object_unref (manager);
    }
#endif
}



static void
mouse_settings_touchscreen_populate_monitors (GtkBuilder *builder)
{
    GtkComboBoxText *combobox = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "touchscreen-assigned-monitor"));

    locked++;

    /* Clear old options */
    gtk_combo_box_text_remove_all (combobox);

    /* No assignment option */
    gtk_combo_box_text_append (combobox, NULL, _("None"));

    /* Add options for currently connected monitors */
    mouse_settings_touchscreen_append_monitors (combobox);

    gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);

    /* Retrieve saved setting if available */
    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        gchar *stored_edid = xfce_device_get_assigned_monitor (device);
        if (stored_edid != NULL)
        {
            gtk_combo_box_set_active_id (GTK_COMBO_BOX (combobox), stored_edid);
            g_free (stored_edid);
        }
    }

    locked--;
}



static void
mouse_settings_touchscreen_assigned_monitor_changed (GtkComboBox *combobox,
                                                     GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device == NULL)
    {
        g_warning ("No device selected");
        return;
    }

    xfce_device_set_assigned_monitor (device, gtk_combo_box_get_active_id (combobox));
}



static void
mouse_settings_touchscreen_rotation_changed (GtkComboBox *combobox,
                                             GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device == NULL)
    {
        g_warning ("No device selected");
        return;
    }

    XfceDeviceRotation rotation;
    /* 0==None, 1==Left, 2==Inverted, 3==Right */
    switch (gtk_combo_box_get_active (combobox))
    {
        case 1:
            rotation = XFCE_DEVICE_ROTATION_90;
            break;
        case 2:
            rotation = XFCE_DEVICE_ROTATION_180;
            break;
        case 3:
            rotation = XFCE_DEVICE_ROTATION_270;
            break;
        default:
            rotation = XFCE_DEVICE_ROTATION_NONE;
            break;
    }

    xfce_device_set_touchscreen_rotation (device, rotation);
}



static void
mouse_settings_touchscreen_reflection_changed (GtkComboBox *combobox,
                                               GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device == NULL)
    {
        g_warning ("No device selected");
        return;
    }

    XfceDeviceReflection reflection;
    /* 0==None, 1==Horizontal, 2==Vertical, 3==Both */
    switch (gtk_combo_box_get_active (combobox))
    {
        case 1:
            reflection = XFCE_DEVICE_REFLECTION_X;
            break;
        case 2:
            reflection = XFCE_DEVICE_REFLECTION_Y;
            break;
        case 3:
            reflection = XFCE_DEVICE_REFLECTION_XY;
            break;
        default:
            reflection = XFCE_DEVICE_REFLECTION_NONE;
            break;
    }

    xfce_device_set_touchscreen_reflection (device, reflection);
}



static void
mouse_settings_touchscreen_populate (GtkBuilder *builder,
                                     XfceDevice *device)
{
    mouse_settings_touchscreen_populate_monitors (builder);

    gint rotation_option_id;
    switch (xfce_device_get_touchscreen_rotation (device))
    {
        case XFCE_DEVICE_ROTATION_90:
            rotation_option_id = 1;
            break;
        case XFCE_DEVICE_ROTATION_180:
            rotation_option_id = 2;
            break;
        case XFCE_DEVICE_ROTATION_270:
            rotation_option_id = 3;
            break;
        default:
            rotation_option_id = 0;
            break;
    }

    GObject *object = gtk_builder_get_object (builder, "touchscreen-rotation");
    gtk_combo_box_set_active (GTK_COMBO_BOX (object), rotation_option_id);

    gint reflection_option_id;
    switch (xfce_device_get_touchscreen_reflection (device))
    {
        case XFCE_DEVICE_REFLECTION_X:
            reflection_option_id = 1;
            break;
        case XFCE_DEVICE_REFLECTION_Y:
            reflection_option_id = 2;
            break;
        case XFCE_DEVICE_REFLECTION_XY:
            reflection_option_id = 3;
            break;
        default:
            reflection_option_id = 0;
            break;
    }

    object = gtk_builder_get_object (builder, "touchscreen-reflection");
    gtk_combo_box_set_active (GTK_COMBO_BOX (object), reflection_option_id);
}



static void
mouse_settings_device_enabled_changed (GtkSwitch *widget,
                                       GParamSpec *pspec,
                                       GtkBuilder *builder)
{
    gboolean enabled = gtk_switch_get_active (widget);

    GObject *object = gtk_builder_get_object (builder, "device-notebook");
    gtk_widget_set_sensitive (GTK_WIDGET (object), enabled);

    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_enabled (device, enabled);
    }
}



static void
mouse_settings_device_acceleration_changed (GtkRange *range,
                                            GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_acceleration (device, gtk_range_get_value (range));
    }
}



static void
mouse_settings_device_left_handed_toggled (GtkToggleButton *button,
                                           GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_left_handed (device, gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_device_reverse_scrolling_toggled (GtkToggleButton *button,
                                                 GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_natural_scroll (device, gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_device_accel_profile_toggled (GtkToggleButton *button,
                                             GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_accel_profile (device, gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_device_tap_to_click_toggled (GtkToggleButton *button,
                                            GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_tap (device, gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_device_dwt_toggled (GtkToggleButton *button,
                                   GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL)
    {
        xfce_device_set_dwt (device, gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_device_click_method_changed (GtkComboBox *combobox,
                                            GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device == NULL)
        return;

    XfceDeviceClickMethod method;
    switch (gtk_combo_box_get_active (combobox))
    {
        case CLICK_METHOD_BUTTON_AREAS:
            method = XFCE_DEVICE_CLICK_METHOD_BUTTON_AREAS;
            break;
        case CLICK_METHOD_CLICKFINGER:
            method = XFCE_DEVICE_CLICK_METHOD_CLICKFINGER;
            break;
        default:
            method = XFCE_DEVICE_CLICK_METHOD_NONE;
            break;
    }

    xfce_device_set_click_method (device, method);
}



/* The horizontal-scroll toggle only means something once a scroll method is
 * picked. */
static void
mouse_settings_scroll_horiz_sensitive (GtkBuilder *builder)
{
    GObject *object = gtk_builder_get_object (builder, "synaptics-scroll");
    gboolean sensitive = gtk_widget_get_sensitive (GTK_WIDGET (object))
                         && gtk_combo_box_get_active (GTK_COMBO_BOX (object)) > SCROLL_MODE_DISABLED;

    object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
    gtk_widget_set_sensitive (GTK_WIDGET (object), sensitive);
}



static void
mouse_settings_device_scroll_method_changed (GtkComboBox *combobox,
                                             GtkBuilder *builder)
{
    if (locked > 0)
        return;

    mouse_settings_scroll_horiz_sensitive (builder);

    XfceDevice *device = selected_device;
    if (device == NULL)
        return;

    XfceDeviceScrollMethod method;
    switch (gtk_combo_box_get_active (combobox))
    {
        case SCROLL_MODE_EDGE:
            method = XFCE_DEVICE_SCROLL_METHOD_EDGE;
            break;
        case SCROLL_MODE_TWO_FINGER:
            method = XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER;
            break;
        case SCROLL_MODE_CIRCULAR:
            method = XFCE_DEVICE_SCROLL_METHOD_CIRCULAR;
            break;
        default:
            method = XFCE_DEVICE_SCROLL_METHOD_NO_SCROLL;
            break;
    }

    xfce_device_set_scroll_method (device, method);
}



#ifdef ENABLE_X11
static void
mouse_settings_device_threshold_changed (GtkRange *range,
                                         GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL && XFCE_IS_DEVICE_X11 (device))
    {
        xfce_device_x11_set_threshold (XFCE_DEVICE_X11 (device), gtk_range_get_value (range));
    }
}



static void
mouse_settings_device_hires_scrolling_toggled (GtkToggleButton *button,
                                               GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL && XFCE_IS_DEVICE_X11 (device))
    {
        xfce_device_x11_set_hires_scrolling (XFCE_DEVICE_X11 (device), gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_device_scroll_horiz_toggled (GtkToggleButton *button,
                                            GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL && XFCE_IS_DEVICE_X11 (device))
    {
        xfce_device_x11_set_synaptics_scroll_horizontal (XFCE_DEVICE_X11 (device),
                                                         gtk_toggle_button_get_active (button));
    }
}



static void
mouse_settings_wacom_set_mode (GtkComboBox *combobox,
                               GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    GtkTreeIter iter;
    if (device != NULL && XFCE_IS_DEVICE_X11 (device)
        && gtk_combo_box_get_active_iter (combobox, &iter))
    {
        GtkTreeModel *model = gtk_combo_box_get_model (combobox);
        gchar *mode = NULL;
        gtk_tree_model_get (model, &iter, 0, &mode, -1);
        xfce_device_x11_set_wacom_mode (XFCE_DEVICE_X11 (device), mode);
        g_free (mode);
    }
}



#endif /* ENABLE_X11 */



static void
mouse_settings_device_rotation_changed (GtkComboBox *combobox,
                                        GtkBuilder *builder)
{
    if (locked > 0)
        return;

    XfceDevice *device = selected_device;
    if (device == NULL)
        return;

    XfceDeviceRotation rotation;
    /* the rows read none, half, clockwise, counter-clockwise */
    switch (gtk_combo_box_get_active (combobox))
    {
        case 1:
            rotation = XFCE_DEVICE_ROTATION_180;
            break;
        case 2:
            rotation = XFCE_DEVICE_ROTATION_90;
            break;
        case 3:
            rotation = XFCE_DEVICE_ROTATION_270;
            break;
        default:
            rotation = XFCE_DEVICE_ROTATION_NONE;
            break;
    }

    xfce_device_set_tablet_rotation (device, rotation);
}



static void
mouse_settings_device_populate_scroll_method (GtkBuilder *builder,
                                              XfceDevice *device)
{
    XfceDeviceScrollMethod supported = xfce_device_get_scroll_method_supported (device);
    GObject *object = gtk_builder_get_object (builder, "synaptics-scroll-store");
    GtkTreeIter iter;

    if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, SCROLL_MODE_EDGE))
    {
        gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1,
                            (supported & XFCE_DEVICE_SCROLL_METHOD_EDGE) != 0, -1);
    }

    if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, SCROLL_MODE_TWO_FINGER))
    {
        gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1,
                            (supported & XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER) != 0, -1);
    }

    if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, SCROLL_MODE_CIRCULAR))
    {
        gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1,
                            (supported & XFCE_DEVICE_SCROLL_METHOD_CIRCULAR) != 0, -1);
    }

    gint scroll_mode;
    switch (xfce_device_get_scroll_method (device))
    {
        case XFCE_DEVICE_SCROLL_METHOD_EDGE:
            scroll_mode = SCROLL_MODE_EDGE;
            break;
        case XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER:
            scroll_mode = SCROLL_MODE_TWO_FINGER;
            break;
        case XFCE_DEVICE_SCROLL_METHOD_CIRCULAR:
            scroll_mode = SCROLL_MODE_CIRCULAR;
            break;
        default:
            scroll_mode = SCROLL_MODE_DISABLED;
            break;
    }

    object = gtk_builder_get_object (builder, "synaptics-scroll");
    gtk_combo_box_set_active (GTK_COMBO_BOX (object), scroll_mode);
}



static void
mouse_settings_device_populate_click_method (GtkBuilder *builder,
                                             XfceDevice *device)
{
    XfceDeviceClickMethod supported = xfce_device_get_click_method_supported (device);
    GObject *object = gtk_builder_get_object (builder, "libinput-click-methods-store");
    GtkTreeIter iter;

    if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, CLICK_METHOD_BUTTON_AREAS))
    {
        gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1,
                            (supported & XFCE_DEVICE_CLICK_METHOD_BUTTON_AREAS) != 0, -1);
    }

    if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (object), &iter, NULL, CLICK_METHOD_CLICKFINGER))
    {
        gtk_list_store_set (GTK_LIST_STORE (object), &iter, 1,
                            (supported & XFCE_DEVICE_CLICK_METHOD_CLICKFINGER) != 0, -1);
    }

    gint click_method;
    switch (xfce_device_get_click_method (device))
    {
        case XFCE_DEVICE_CLICK_METHOD_BUTTON_AREAS:
            click_method = CLICK_METHOD_BUTTON_AREAS;
            break;
        case XFCE_DEVICE_CLICK_METHOD_CLICKFINGER:
            click_method = CLICK_METHOD_CLICKFINGER;
            break;
        default:
            click_method = CLICK_METHOD_NONE;
            break;
    }

    object = gtk_builder_get_object (builder, "libinput-click-method-box");
    gtk_combo_box_set_active (GTK_COMBO_BOX (object), click_method);
}



static void
mouse_settings_device_populate_touchpad (GtkBuilder *builder,
                                         XfceDevice *device)
{
    gboolean is_legacy = mouse_settings_device_is_legacy_x11 (device);
    GObject *object;

    object = gtk_builder_get_object (builder, "synaptics-tap-to-click");
    gtk_widget_set_sensitive (GTK_WIDGET (object), xfce_device_get_tap_available (device));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), xfce_device_get_tap (device));

    mouse_settings_device_populate_scroll_method (builder, device);

    object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
    mouse_settings_scroll_horiz_sensitive (builder);
#ifdef ENABLE_X11
    if (XFCE_IS_DEVICE_X11 (device))
    {
        XfceDeviceX11 *x11_device = XFCE_DEVICE_X11 (device);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
                                      xfce_device_x11_get_synaptics_scroll_horizontal (x11_device));
        gtk_widget_set_visible (GTK_WIDGET (object),
                                xfce_device_x11_get_synaptics_scroll_horizontal_available (x11_device));
    }
    else
#endif
    {
        gtk_widget_set_visible (GTK_WIDGET (object), FALSE);
    }

    /* the syndaemon-driven settings only apply to the legacy synaptics driver */
    object = gtk_builder_get_object (builder, "synaptics-disable-while-type");
    gtk_widget_set_visible (GTK_WIDGET (object), is_legacy);

    object = gtk_builder_get_object (builder, "synaptics-disable-duration-box");
    gtk_widget_set_visible (GTK_WIDGET (object), is_legacy);

    gboolean dwt_available = xfce_device_get_dwt_available (device);
    object = gtk_builder_get_object (builder, "libinput-disable-while-type");
    if (dwt_available)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), xfce_device_get_dwt (device));
    }
    gtk_widget_set_visible (GTK_WIDGET (object), dwt_available);

    gboolean click_method_available = xfce_device_get_click_method_available (device);
    object = gtk_builder_get_object (builder, "libinput-click-method-box");
    if (click_method_available)
    {
        mouse_settings_device_populate_click_method (builder, device);
    }
    gtk_widget_set_visible (GTK_WIDGET (object), click_method_available);

    object = gtk_builder_get_object (builder, "libinput-click-method-label");
    gtk_widget_set_visible (GTK_WIDGET (object), click_method_available);
}



static void
mouse_settings_device_populate_rotation (GtkBuilder *builder,
                                         XfceDevice *device)
{
    gboolean available = device != NULL && xfce_device_get_tablet_rotation_available (device);

    GObject *object = gtk_builder_get_object (builder, "wacom-rotation-label");
    gtk_widget_set_visible (GTK_WIDGET (object), available);

    object = gtk_builder_get_object (builder, "wacom-rotation");
    gtk_widget_set_visible (GTK_WIDGET (object), available);

    /* the rows read none, half, clockwise, counter-clockwise */
    gint row = 0;
    if (available)
    {
        switch (xfce_device_get_tablet_rotation (device))
        {
            case XFCE_DEVICE_ROTATION_180:
                row = 1;
                break;
            case XFCE_DEVICE_ROTATION_90:
                row = 2;
                break;
            case XFCE_DEVICE_ROTATION_270:
                row = 3;
                break;
            default:
                row = 0;
                break;
        }
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (object), row);
}



static void
mouse_settings_device_populate_wacom_mode (GtkBuilder *builder,
                                           XfceDevice *device)
{
    GObject *label = gtk_builder_get_object (builder, "wacom-mode-label");
    GObject *combo = gtk_builder_get_object (builder, "wacom-mode");

#ifdef ENABLE_X11
    if (XFCE_IS_DEVICE_X11 (device))
    {
        gint mode = xfce_device_x11_get_wacom_mode (XFCE_DEVICE_X11 (device));
        gtk_widget_set_visible (GTK_WIDGET (label), mode != -1);
        gtk_widget_set_visible (GTK_WIDGET (combo), mode != -1);
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), mode == -1 ? 1 : mode);
    }
    else
#endif
    {
        gtk_widget_set_visible (GTK_WIDGET (label), FALSE);
        gtk_widget_set_visible (GTK_WIDGET (combo), FALSE);
    }
}



static void
mouse_settings_device_selection_changed (GtkBuilder *builder)
{
    XfceDevice *device = selected_device;
    GObject *object;

    /* lock the dialog */
    locked++;

    gboolean is_legacy = device != NULL && mouse_settings_device_is_legacy_x11 (device);
    gboolean is_touchpad = device != NULL && mouse_settings_device_is_touchpad (device);
    gboolean is_touchscreen = device != NULL && mouse_settings_device_is_touchscreen (device);

    /* update button order */
    gboolean left_handed = device != NULL && xfce_device_get_left_handed (device);
    object = gtk_builder_get_object (builder, left_handed ? "device-left-handed" : "device-right-handed");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), TRUE);

    /* the scroll settings only make sense with a scroll wheel */
    gboolean scroll_wheel_available = device != NULL && xfce_device_get_natural_scroll_available (device);

    object = gtk_builder_get_object (builder, "device-reverse-scrolling");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
                                  device != NULL && xfce_device_get_natural_scroll (device));
    gtk_widget_set_sensitive (GTK_WIDGET (object), scroll_wheel_available);

    object = gtk_builder_get_object (builder, "libinput-hires-scrolling");
#ifdef ENABLE_X11
    if (device != NULL && XFCE_IS_DEVICE_X11 (device)
        && xfce_device_x11_get_hires_scrolling_available (XFCE_DEVICE_X11 (device)))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
                                      xfce_device_x11_get_hires_scrolling (XFCE_DEVICE_X11 (device)));
        gtk_widget_set_sensitive (GTK_WIDGET (object), scroll_wheel_available);
        gtk_widget_set_visible (GTK_WIDGET (object), TRUE);
    }
    else
#endif
    {
        gtk_widget_set_visible (GTK_WIDGET (object), FALSE);
    }

    gboolean accel_profile_available = device != NULL && xfce_device_get_accel_profile_available (device);
    object = gtk_builder_get_object (builder, "libinput-accel-profile");
    if (accel_profile_available)
    {
        XfceDeviceAccelProfile supported = xfce_device_get_accel_profile_supported (device);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
                                      xfce_device_get_accel_profile (device) == XFCE_DEVICE_ACCEL_PROFILE_ADAPTIVE);
        gtk_widget_set_sensitive (GTK_WIDGET (object), (supported & XFCE_DEVICE_ACCEL_PROFILE_ADAPTIVE) != 0);
    }
    gtk_widget_set_visible (GTK_WIDGET (object), accel_profile_available);

    /* update acceleration scale */
    gboolean acceleration_available = device != NULL && xfce_device_get_acceleration_available (device);
    object = gtk_builder_get_object (builder, "device-acceleration-scale");
    gtk_range_set_value (GTK_RANGE (object), acceleration_available ? xfce_device_get_acceleration (device) : -1.0);
    gtk_widget_set_visible (GTK_WIDGET (object), acceleration_available);
    object = gtk_builder_get_object (builder, "device-acceleration-label");
    gtk_widget_set_visible (GTK_WIDGET (object), acceleration_available);

    /* update threshold scale */
    gboolean threshold_available = FALSE;
    gint threshold = -1;
#ifdef ENABLE_X11
    if (device != NULL && XFCE_IS_DEVICE_X11 (device))
    {
        threshold_available = xfce_device_x11_get_threshold_available (XFCE_DEVICE_X11 (device));
        threshold = xfce_device_x11_get_threshold (XFCE_DEVICE_X11 (device));
    }
#endif
    object = gtk_builder_get_object (builder, "device-threshold-scale");
    gtk_range_set_value (GTK_RANGE (object), threshold);
    gtk_widget_set_visible (GTK_WIDGET (object), threshold_available);
    object = gtk_builder_get_object (builder, "device-threshold-label");
    gtk_widget_set_visible (GTK_WIDGET (object), threshold_available);

    gboolean enabled = device != NULL && xfce_device_get_enabled (device);
    object = gtk_builder_get_object (builder, "device-enabled");
    gtk_widget_set_sensitive (GTK_WIDGET (object),
                              device != NULL && xfce_device_get_send_events_available (device));
    gtk_switch_set_active (GTK_SWITCH (object), enabled);

    object = gtk_builder_get_object (builder, "device-notebook");
    gtk_widget_set_sensitive (GTK_WIDGET (object), enabled);

    /* the core-X pointer feedback is what the reset button restores */
    object = gtk_builder_get_object (builder, "device-reset-feedback");
    gtk_widget_set_visible (GTK_WIDGET (object), is_legacy);

    // Devices that have none of these leave the frame holding nothing but its
    // title.
    object = gtk_builder_get_object (builder, "device-pointer-speed-frame");
    gtk_widget_set_visible (GTK_WIDGET (object),
                            acceleration_available || threshold_available || accel_profile_available || is_legacy);

    /* touchpad options */
    object = gtk_builder_get_object (builder, "synaptics-tab");
    gtk_widget_set_visible (GTK_WIDGET (object), is_touchpad);

    if (is_touchpad)
    {
        mouse_settings_device_populate_touchpad (builder, device);
    }

    /* tablet options */
    gboolean is_tablet = device != NULL
                         && (xfce_device_get_capabilities (device) & XFCE_DEVICE_CAPABILITIES_TABLET_TOOL) != 0;

    /* the tracking mode is the legacy wacom driver's alone; the rotation is
     * shared, applied through that driver on X11 and a transformation matrix on
     * Wayland */
    mouse_settings_device_populate_wacom_mode (builder, device);
    mouse_settings_device_populate_rotation (builder, device);

    object = gtk_builder_get_object (builder, "wacom-tab");
    gtk_widget_set_visible (GTK_WIDGET (object), is_tablet);

    /* If this is a touchscreen:                                          */
    /* 1. Hide "Buttons and feedback" as none of the settings there apply */
    object = gtk_builder_get_object (builder, "device-box");
    gtk_widget_set_visible (GTK_WIDGET (object), !is_touchscreen);
    /* 2. Show touchscreen tab                                            */
    object = gtk_builder_get_object (builder, "touchscreen-tab");
    gtk_widget_set_visible (GTK_WIDGET (object), is_touchscreen);

    if (is_touchscreen)
    {
        mouse_settings_touchscreen_populate (builder, device);
    }

    /* unlock */
    locked--;
}



static void
mouse_settings_device_combobox_changed (GtkBuilder *builder)
{
    XfceDevice *device = mouse_settings_device_dup_selected (builder);

    if (device != selected_device)
    {
        if (selected_device != NULL)
        {
            g_signal_handler_disconnect (selected_device, selected_device_changed_id);
            g_object_unref (selected_device);
            selected_device_changed_id = 0;
        }

        /* takes over the reference from the lookup above */
        selected_device = device;

        if (selected_device != NULL)
        {
            // Re-read the device, so that settings changed since it was last
            // looked at show their current state. Refreshing before connecting
            // saves a repopulate, since the tail of this function does one.
            xfce_device_refresh (selected_device);

            selected_device_changed_id =
                g_signal_connect_swapped (G_OBJECT (selected_device), "changed",
                                          G_CALLBACK (mouse_settings_device_selection_changed), builder);

            g_set_str (&selected_device_name, xfce_device_get_name (selected_device));
        }
    }
    else
    {
        g_clear_object (&device);
    }

    mouse_settings_device_selection_changed (builder);
}



static void
mouse_settings_device_populate_store (GtkBuilder *builder,
                                      gboolean create_store)
{
    GObject *combobox = gtk_builder_get_object (builder, "device-combobox");
    GtkListStore *store;

    /* lock */
    locked++;

    /* create or get the store */
    if (G_LIKELY (create_store))
    {
        store = gtk_list_store_new (N_DEVICE_COLUMNS,
                                    G_TYPE_STRING /* COLUMN_DEVICE_NAME */,
                                    XFCE_TYPE_DEVICE /* COLUMN_DEVICE_OBJECT */);
        gtk_combo_box_set_model (GTK_COMBO_BOX (combobox), GTK_TREE_MODEL (store));
        g_object_unref (store);

        /* text renderer */
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", COLUMN_DEVICE_NAME, NULL);
    }
    else
    {
        store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox)));
        gtk_list_store_clear (store);
    }

    gboolean has_active_item = FALSE;
    gint position = 0;

    for (GList *lp = xfce_device_manager_list_devices (device_manager); lp != NULL; lp = lp->next)
    {
        XfceDevice *device = lp->data;
        GtkTreeIter iter;

        gtk_list_store_insert_with_values (store, &iter, position++,
                                           COLUMN_DEVICE_NAME, xfce_device_get_name (device),
                                           COLUMN_DEVICE_OBJECT, device,
                                           -1);

        /* check if we should select this device */
        if (opt_device_name != NULL
            && g_strcmp0 (opt_device_name, xfce_device_get_name (device)) == 0)
        {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            g_clear_pointer (&opt_device_name, g_free);
            has_active_item = TRUE;
        }
        else if (!has_active_item
                 && g_strcmp0 (selected_device_name, xfce_device_get_name (device)) == 0)
        {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            has_active_item = TRUE;
        }
    }

    if (!has_active_item)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
    }

    // Connected once the store is filled, so that populating it does not run the
    // handler for every intermediate selection; the caller does that once.
    if (G_LIKELY (create_store))
    {
        g_signal_connect_swapped (G_OBJECT (combobox), "changed",
                                  G_CALLBACK (mouse_settings_device_combobox_changed), builder);
    }

    /* unlock */
    locked--;
}



static void
mouse_settings_device_list_changed (XfceDeviceManager *manager,
                                    XfceDevice *device,
                                    GtkBuilder *builder)
{
    mouse_settings_device_populate_store (builder, FALSE);
}



#ifdef ENABLE_X11
static gboolean
mouse_settings_device_update_sliders (gpointer user_data)
{
    GtkBuilder *builder = GTK_BUILDER (user_data);

    /* pick up the acceleration and threshold the daemon has restored */
    if (selected_device != NULL)
    {
        // The single "changed" this emits repopulates the dialog already.
        xfce_device_refresh (selected_device);
    }
    else
    {
        mouse_settings_device_selection_changed (builder);
    }

    /* make the button sensitive again */
    GObject *button = gtk_builder_get_object (builder, "device-reset-feedback");
    gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);

    return FALSE;
}



static void
mouse_settings_device_list_changed_timeout_destroyed (gpointer user_data)
{
    /* reset the timeout id */
    timeout_id = 0;
}



static void
mouse_settings_device_reset (GtkWidget *button,
                             GtkBuilder *builder)
{
    /* leave when locked */
    if (locked > 0 || timeout_id != 0)
        return;

    XfceDevice *device = selected_device;
    if (device != NULL && XFCE_IS_DEVICE_X11 (device))
    {
        /* make the button insensitive */
        gtk_widget_set_sensitive (button, FALSE);

        xfce_device_x11_reset_feedback (XFCE_DEVICE_X11 (device));

        /* update the sliders in 500ms */
        timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 500, mouse_settings_device_update_sliders,
                                         builder, mouse_settings_device_list_changed_timeout_destroyed);
    }
}
#endif



static void
mouse_settings_dialog_response (GtkWidget *dialog,
                                gint response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "mouse",
                                            NULL, VERSION_SHORT);
    else
        gtk_main_quit ();
}



/* Keep the settings dialog out of the session. */
static void
mouse_settings_disable_session_management (void)
{
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        gdk_x11_set_sm_client_id ("FAKE ID");
    }
#endif
}



gint
main (gint argc,
      gchar **argv)
{
    GObject *dialog;
    GtkBuilder *builder;
    GError *error = NULL;
    GObject *object;
    gchar *syndaemon;
    GObject *synaptics_disable_while_type;
    GObject *synaptics_disable_duration_table;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
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
            g_critical ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION_FULL, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-" COPYRIGHT_YEAR);
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

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* open the xsettings and pointers channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    pointers_channel = xfconf_channel_new ("pointers");

    device_manager = xfce_device_manager_new (gdk_display_get_default (), pointers_channel, &error);
    if (device_manager == NULL)
    {
        xfce_message_dialog (NULL, _("Mouse and Touchpad"), "dialog-error", _("Unable to start"), error->message, _("Quit"), GTK_RESPONSE_ACCEPT, NULL);
        g_error_free (error);

        g_object_unref (G_OBJECT (xsettings_channel));
        g_object_unref (G_OBJECT (pointers_channel));
        xfconf_shutdown ();

        return EXIT_FAILURE;
    }

    /* load the Gtk+ user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_resource (builder, "/org/xfce/settings/mouse-dialog.glade", &error) != 0)
    {
        /* lock */
        locked++;

        /* populate the devices combobox */
        mouse_settings_device_populate_store (builder, TRUE);

        /* keep the combobox in sync with device hotplugging */
        g_signal_connect (G_OBJECT (device_manager), "device-added",
                          G_CALLBACK (mouse_settings_device_list_changed), builder);
        g_signal_connect (G_OBJECT (device_manager), "device-removed",
                          G_CALLBACK (mouse_settings_device_list_changed), builder);

        /* connect signals */
        object = gtk_builder_get_object (builder, "device-enabled");
        g_signal_connect (G_OBJECT (object), "notify::active",
                          G_CALLBACK (mouse_settings_device_enabled_changed), builder);

        object = gtk_builder_get_object (builder, "device-acceleration-scale");
        g_signal_connect (G_OBJECT (object), "value-changed",
                          G_CALLBACK (mouse_settings_device_acceleration_changed), builder);

        object = gtk_builder_get_object (builder, "device-threshold-scale");
        g_signal_connect (G_OBJECT (object), "format-value",
                          G_CALLBACK (mouse_settings_format_value_px), NULL);
#ifdef ENABLE_X11
        g_signal_connect (G_OBJECT (object), "value-changed",
                          G_CALLBACK (mouse_settings_device_threshold_changed), builder);
#endif

        object = gtk_builder_get_object (builder, "device-left-handed");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_left_handed_toggled), builder);

        object = gtk_builder_get_object (builder, "device-reverse-scrolling");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_reverse_scrolling_toggled), builder);

        object = gtk_builder_get_object (builder, "libinput-accel-profile");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_accel_profile_toggled), builder);

#ifdef ENABLE_X11
        object = gtk_builder_get_object (builder, "libinput-hires-scrolling");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_hires_scrolling_toggled), builder);

        object = gtk_builder_get_object (builder, "device-reset-feedback");
        g_signal_connect (G_OBJECT (object), "clicked",
                          G_CALLBACK (mouse_settings_device_reset), builder);
#endif

        synaptics_disable_while_type = gtk_builder_get_object (builder, "synaptics-disable-while-type");
        syndaemon = g_find_program_in_path ("syndaemon");
        gtk_widget_set_sensitive (GTK_WIDGET (synaptics_disable_while_type), syndaemon != NULL);
        g_free (syndaemon);
        xfconf_g_property_bind (pointers_channel, "/DisableTouchpadWhileTyping",
                                G_TYPE_BOOLEAN, G_OBJECT (synaptics_disable_while_type), "active");

        synaptics_disable_duration_table = gtk_builder_get_object (builder, "synaptics-disable-duration-box");

        g_object_bind_property (G_OBJECT (synaptics_disable_while_type), "active",
                                G_OBJECT (synaptics_disable_duration_table), "sensitive",
                                G_BINDING_SYNC_CREATE);

        object = gtk_builder_get_object (builder, "libinput-disable-while-type");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_dwt_toggled), builder);

        object = gtk_builder_get_object (builder, "libinput-click-method-box");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_device_click_method_changed), builder);

        object = gtk_builder_get_object (builder, "synaptics-disable-duration-scale");
        g_signal_connect (G_OBJECT (object), "format-value",
                          G_CALLBACK (mouse_settings_format_value_s), NULL);

        object = gtk_builder_get_object (builder, "synaptics-disable-duration");
        xfconf_g_property_bind (pointers_channel, "/DisableTouchpadDuration",
                                G_TYPE_DOUBLE, G_OBJECT (object), "value");

        object = gtk_builder_get_object (builder, "synaptics-tap-to-click");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_tap_to_click_toggled), builder);

        object = gtk_builder_get_object (builder, "synaptics-scroll");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_device_scroll_method_changed), builder);

#ifdef ENABLE_X11
        object = gtk_builder_get_object (builder, "synaptics-scroll-horiz");
        g_signal_connect (G_OBJECT (object), "toggled",
                          G_CALLBACK (mouse_settings_device_scroll_horiz_toggled), builder);

        object = gtk_builder_get_object (builder, "wacom-mode");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_wacom_set_mode), builder);
#endif

        object = gtk_builder_get_object (builder, "wacom-rotation");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_device_rotation_changed), builder);

        object = gtk_builder_get_object (builder, "touchscreen-rotation");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_touchscreen_rotation_changed), builder);

        object = gtk_builder_get_object (builder, "touchscreen-reflection");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_touchscreen_reflection_changed), builder);

        object = gtk_builder_get_object (builder, "touchscreen-assigned-monitor");
        g_signal_connect (G_OBJECT (object), "changed",
                          G_CALLBACK (mouse_settings_touchscreen_assigned_monitor_changed), builder);

#ifdef HAVE_XCURSOR
        /* populate the themes treeview */
        mouse_settings_themes_populate_store (builder);

        /* connect the cursor size in the cursor tab */
        object = gtk_builder_get_object (builder, "theme-cursor-size");
        xfconf_g_property_bind (xsettings_channel, "/Gtk/CursorThemeSize",
                                G_TYPE_INT, G_OBJECT (object), "value");
#else
        /* hide the themes tab */
        object = gtk_builder_get_object (builder, "themes-hbox");
        gtk_widget_hide (GTK_WIDGET (object));
#endif /* !HAVE_XCURSOR */

        /* connect sliders in the gtk tab */
        object = gtk_builder_get_object (builder, "dnd-threshold");
        xfconf_g_property_bind (xsettings_channel, "/Net/DndDragThreshold",
                                G_TYPE_INT, G_OBJECT (object), "value");

        object = gtk_builder_get_object (builder, "dnd-threshold-scale");
        g_signal_connect (G_OBJECT (object), "format-value",
                          G_CALLBACK (mouse_settings_format_value_px), NULL);

        object = gtk_builder_get_object (builder, "dclick-time");
        xfconf_g_property_bind (xsettings_channel, "/Net/DoubleClickTime",
                                G_TYPE_INT, G_OBJECT (object), "value");

        object = gtk_builder_get_object (builder, "dclick-time-scale");
        g_signal_connect (G_OBJECT (object), "format-value",
                          G_CALLBACK (mouse_settings_format_value_ms), NULL);

        object = gtk_builder_get_object (builder, "dclick-distance");
        xfconf_g_property_bind (xsettings_channel, "/Net/DoubleClickDistance",
                                G_TYPE_INT, G_OBJECT (object), "value");

        object = gtk_builder_get_object (builder, "dclick-distance-scale");
        g_signal_connect (G_OBJECT (object), "format-value",
                          G_CALLBACK (mouse_settings_format_value_px), NULL);

        object = gtk_builder_get_object (builder, "middle-button-paste");
        xfconf_g_property_bind (xsettings_channel, "/Gtk/EnablePrimaryPaste",
                                G_TYPE_BOOLEAN, G_OBJECT (object), "active");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
                                      xfconf_channel_get_bool (xsettings_channel, "/Gtk/EnablePrimaryPaste", TRUE));

        /* now that the widgets are wired up, fill them from the selected device */
        mouse_settings_device_combobox_changed (builder);

        mouse_settings_disable_session_management ();

#ifdef ENABLE_X11
        if (opt_socket_id != 0 && GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
            /* Create plug widget */
            GtkWidget *plug = gtk_plug_new (opt_socket_id);
            g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
            gtk_widget_show (plug);

            /* Stop startup notification */
            gdk_notify_startup_complete ();

            /* Get plug child widget */
            GObject *plug_child = gtk_builder_get_object (builder, "plug-child");
            xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));

            /* Unlock */
            locked--;

            /* Enter main loop */
            gtk_main ();
        }
        else
#endif
        {
            /* get the dialog */
            dialog = gtk_builder_get_object (builder, "mouse-dialog");

            /* unlock */
            locked--;

            g_signal_connect (dialog, "response",
                              G_CALLBACK (mouse_settings_dialog_response), NULL);
            gtk_window_present (GTK_WINDOW (dialog));

            gtk_main ();

            gtk_widget_destroy (GTK_WIDGET (dialog));
        }
    }
    else
    {
        g_critical ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    if (selected_device != NULL)
    {
        g_signal_handler_disconnect (selected_device, selected_device_changed_id);
        g_clear_object (&selected_device);
    }

    /* release the Gtk+ user-interface file */
    g_object_unref (G_OBJECT (builder));

    g_object_unref (G_OBJECT (device_manager));

    /* release the channels */
    g_object_unref (G_OBJECT (xsettings_channel));
    g_object_unref (G_OBJECT (pointers_channel));

    /* shutdown xfconf */
    xfconf_shutdown ();

    /* cleanup */
    g_free (opt_device_name);
    g_free (selected_device_name);

    return EXIT_SUCCESS;
}
