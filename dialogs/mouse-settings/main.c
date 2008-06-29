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

#include <X11/Xlib.h>
#include <X11/Xcursor/Xcursor.h>

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
#include "mouse-dialog_glade.h"

static XfconfChannel *xsettings_channel;

/* XDG? */
static gchar *cursor_dirs[][2] = {
    { "%s/.icons/",                     "HOME" },
    { "%s/.themes/",                    "HOME" },
    { "/usr/share/cursors/xorg-x11/",   NULL },
    { "/usr/share/cursors/xfree/",      NULL },
    { "/usr/X11R6/lib/X11/icons/",      NULL },
    { "/usr/Xorg/lib/X11/icons/",       NULL },
    { "/usr/share/icons",               NULL },
    { NULL, NULL }
};

/* List from kde kcontrol */
const static gchar *preview_filenames[] = {
    "left_ptr",             "left_ptr_watch",       "watch",                "hand2",
    "question_arrow",       "sb_h_double_arrow",    "sb_v_double_arrow",    "bottom_left_corner",
    "bottom_right_corner",  "fleur",                "pirate",               "cross",
    "X_cursor",             "right_ptr",            "right_side",           "right_tee",
    "sb_right_arrow",       "sb_right_tee",         "base_arrow_down",      "base_arrow_up",
    "bottom_side",          "bottom_tee",           "center_ptr",           "circle",
    "dot",                  "dot_box_mask",         "dot_box_mask",         "double_arrow",
    "draped_box",           "left_side",            "left_tee",             "ll_angle",
    "top_side",             "top_tee"
};
#define NUM_PER_COL         (3)
#define NUM_PREVIEW         (6)
#define PREVIEW_SIZE        (24)

enum {
    TLIST_THEME_NAME,
    TLIST_THEME_PATH,
    TLIST_NUM_COLUMNS
};

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



static void
cursor_plugin_pixbuf_destroy_notify_cb (guchar *pixels, gpointer data)
{
    g_free(pixels);
}


GdkPixbuf *
cursor_image_get_pixbuf (XcursorImage *cursor)
{
    GdkPixbuf *pixbuf = NULL;
    guchar *data, *p;
    gsize dlen = cursor->width * cursor->height * sizeof(XcursorPixel);
    guint i;

    data = g_malloc(dlen);
    for (i = 0, p = (guchar *) cursor->pixels; i < dlen; i += 4, p += 4) {
        data[i] = p[2];
        data[i+1] = p[1];
        data[i+2] = p[0];
        data[i+3] = p[3];
    }

    pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE, 8,
                                      cursor->width, cursor->height,
                                      cursor->width * sizeof( XcursorPixel ),
                                      cursor_plugin_pixbuf_destroy_notify_cb,
                                      NULL);
    if (pixbuf == NULL) {
        g_free(data);
        return NULL;
    }

    if (cursor->width != PREVIEW_SIZE || cursor->height != PREVIEW_SIZE) {
        gfloat f;
        GdkPixbuf *tmp;
        guint w, h;

        f = (gfloat) cursor->width / (gfloat) cursor->height;
        if (f >= 1.0f) {
            w = (gfloat) PREVIEW_SIZE / f;
            h = PREVIEW_SIZE;
        }
        else {
            w = PREVIEW_SIZE;
            h = (gfloat) PREVIEW_SIZE * f;
        }
        tmp = gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_BILINEAR);

        g_return_val_if_fail(tmp != NULL, pixbuf);

        g_object_unref(pixbuf);
        pixbuf = tmp;
    }

    return pixbuf;
}

GdkPixbuf *
generate_preview_image (GtkWidget *widget, const gchar *theme_path)
{
    guint i, num_loaded;
    GdkPixbuf *preview_pix = NULL;
    GdkPixmap *pmap;
    GtkStyle *style;

    if(!GTK_WIDGET_REALIZED(widget))
        gtk_widget_realize(widget);

    pmap = gdk_pixmap_new(GDK_DRAWABLE(widget->window),
                          NUM_PREVIEW*PREVIEW_SIZE, (NUM_PREVIEW/NUM_PER_COL)*PREVIEW_SIZE, -1);
    style = gtk_widget_get_style(widget);

    gdk_draw_rectangle(GDK_DRAWABLE(pmap), style->bg_gc[GTK_STATE_NORMAL], TRUE,
                       0, 0,
                       NUM_PREVIEW * PREVIEW_SIZE, PREVIEW_SIZE);

    for (i = 0, num_loaded = 0; i < G_N_ELEMENTS(preview_filenames) && num_loaded < NUM_PREVIEW; i++) {
        XcursorImage *cursor;
        gchar *fn = g_build_filename(theme_path, preview_filenames[i], NULL);

        cursor = XcursorFilenameLoadImage(fn, PREVIEW_SIZE);

        if (cursor) {
            GdkPixbuf *pb = cursor_image_get_pixbuf(cursor);
            if (pb) {
                gdk_draw_pixbuf(GDK_DRAWABLE(pmap),
                                style->bg_gc[GTK_STATE_NORMAL], pb,
                                0, 0,
                                num_loaded*PREVIEW_SIZE, 0,
                                -1, -1,
                                GDK_RGB_DITHER_NONE, 0, 0);
                g_object_unref(pb);
                num_loaded++;
            }
            else {
                g_warning("pb == NULL");
            }
            XcursorImageDestroy(cursor);
        }
    }

    if (num_loaded > 0) {
        preview_pix = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(pmap),
                                                   NULL, 0, 0, 0, 0,
                                                   NUM_PREVIEW*PREVIEW_SIZE,
                                                   PREVIEW_SIZE);
    }

    g_object_unref(G_OBJECT(pmap));

    return preview_pix;
}



void
cb_cursor_theme_treeselection_changed (GtkTreeSelection *selection, GladeXML *gxml)
{
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    GList *list = gtk_tree_selection_get_selected_rows(selection, &model);
    GValue path = { 0, }, name = { 0, };

    /* valid failure */
    if (g_list_length(list) == 0)
        return;

    /* everything else is invalid */
    g_return_if_fail(g_list_length(list) == 1);

    gtk_tree_model_get_iter(model, &iter, list->data);
    gtk_tree_model_get_value(model, &iter, TLIST_THEME_NAME, &name);
    gtk_tree_model_get_value(model, &iter, TLIST_THEME_PATH, &path);

    xfconf_channel_set_property(xsettings_channel, "/Gtk/CursorThemeName", &name);

    g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(list);

    GtkWidget *preview_image = glade_xml_get_widget(gxml, "cursor_preview_image");
    GdkPixbuf *pb = generate_preview_image(preview_image, g_value_get_string(&path));
    gtk_image_set_from_pixbuf(GTK_IMAGE(preview_image), pb);
    if (NULL != pb)
      g_object_unref(pb);

    g_value_unset(&path);
    g_value_unset(&name);
}



static gint
tree_view_cmp_alpha (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
    gchar *a_s = NULL, *b_s = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, TLIST_THEME_NAME, &a_s, -1);
    gtk_tree_model_get(model, b, TLIST_THEME_NAME, &b_s, -1);

    if(!a_s)
        ret = -1;
    else if(!b_s)
        ret = 1;
    else
        ret = g_ascii_strcasecmp(a_s, b_s);

    g_free(a_s);
    g_free(b_s);

    return ret;
}

static void
check_cursor_themes (GtkListStore *list_store, GtkTreeView *tree_view)
{
    GDir *dir = NULL;
    guint i;
    GtkTreeIter iter;
    GtkTreePath *path;
    GHashTable *themes;
    const gchar *theme;
    gchar *active_theme_name = xfconf_channel_get_string(xsettings_channel, "/Gtk/CursorThemeName", "default");

    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, "default", -1);

    path = gtk_tree_path_new_first();
    gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
    gtk_tree_path_free(path);

    themes = g_hash_table_new_full(g_str_hash, g_str_equal,
                                   (GDestroyNotify)g_free, NULL);

    for (i = 0; cursor_dirs[i][0]; i++) {
        gchar *curdir = cursor_dirs[i][0];

        if (cursor_dirs[i][1]) {
            curdir = g_strdup_printf(cursor_dirs[i][0], g_getenv(cursor_dirs[i][1]));
        }

        if ((dir = g_dir_open( curdir, 0, NULL))) {
            for (theme = g_dir_read_name(dir); theme != NULL; theme = g_dir_read_name(dir)) {
                gchar *full_path = g_build_filename(curdir, theme, "cursors", NULL);

                if (g_file_test(full_path, G_FILE_TEST_IS_DIR)
                    && !g_hash_table_lookup(themes, theme))
                {
                    gtk_list_store_append(list_store, &iter);
                    gtk_list_store_set(list_store, &iter,
                                       TLIST_THEME_NAME, theme,
                                       TLIST_THEME_PATH, full_path,
                                       -1);
                    g_hash_table_insert(themes, g_strdup(theme), GINT_TO_POINTER(1));

                    if (!strcmp(active_theme_name, theme)) {
                        path = gtk_tree_model_get_path(GTK_TREE_MODEL(list_store), &iter);
                        gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
                        gtk_tree_view_scroll_to_cell(tree_view, path, NULL, FALSE, 0.5, 0.0);
                        gtk_tree_path_free(path);
                    }
                }
                g_free(full_path);
            }
            g_dir_close(dir);
        }

        if (cursor_dirs[i][1]) {
            g_free(curdir);
        }
    }

    g_hash_table_destroy(themes);
}



GtkWidget *
mouse_settings_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget *dialog;
    GtkListStore *list_store;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
    GtkTreeSelection *cursor_selection;

    GtkWidget *mouse_button_right_handed = glade_xml_get_widget(gxml, "button_right_handed");
    GtkWidget *mouse_motion_acceleration = (GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget(gxml, "mouse_motion_acceleration")));
    GtkWidget *mouse_motion_threshold = (GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget(gxml, "mouse_motion_threshold")));
    GtkWidget *mouse_dnd_threshold = (GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget(gxml, "mouse_dnd_threshold")));
    GtkWidget *mouse_double_click_speed = (GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget(gxml, "mouse_double_click_speed")));

    GtkWidget *spin_cursor_size = glade_xml_get_widget(gxml, "spin_cursor_size");
    GtkWidget *treeview_cursor_theme = glade_xml_get_widget(gxml, "treeview_cursor_theme");

    GtkWidget *preview_image = glade_xml_get_widget(gxml, "cursor_preview_image");
    gtk_widget_set_size_request(preview_image, NUM_PREVIEW * PREVIEW_SIZE, PREVIEW_SIZE);

    /* cursor tree view */
    list_store = gtk_list_store_new(TLIST_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_cursor_theme), GTK_TREE_MODEL(list_store));
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview_cursor_theme), TLIST_THEME_NAME, _("Cursor theme"), renderer, "text", 0, NULL);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(list_store), TLIST_THEME_NAME,
                                    tree_view_cmp_alpha, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list_store),
                                         TLIST_THEME_NAME, GTK_SORT_ASCENDING);

    check_cursor_themes(list_store, GTK_TREE_VIEW(treeview_cursor_theme));

    cursor_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_cursor_theme));
    gtk_tree_selection_set_mode(cursor_selection, GTK_SELECTION_SINGLE);

    if (gtk_tree_selection_get_selected(cursor_selection, &model, &iter)) {
        gchar *path = NULL;
        gtk_tree_model_get(model, &iter, TLIST_THEME_PATH, &path, -1);
        if (NULL != path) {
            GdkPixbuf *pb = generate_preview_image(preview_image, path);
            if (pb) {
                gtk_image_set_from_pixbuf(GTK_IMAGE(preview_image), pb);
                g_object_unref(pb);
            }
            g_free(path);
        }
    }

    /* xfconf */
    xfconf_g_property_bind(xsettings_channel,
                           "/Mouse/RightHanded",
                           G_TYPE_INT,
                           G_OBJECT(mouse_button_right_handed), "active");
    xfconf_g_property_bind(xsettings_channel,
                           "/Mouse/Acceleration",
                           G_TYPE_INT,
                           G_OBJECT(mouse_motion_acceleration), "value");
    xfconf_g_property_bind(xsettings_channel,
                           "/Mouse/Threshold",
                           G_TYPE_INT,
                           G_OBJECT(mouse_motion_threshold), "value");
    xfconf_g_property_bind(xsettings_channel,
                           "/Net/DndDragThreshold",
                           G_TYPE_INT,
                           G_OBJECT(mouse_dnd_threshold), "value");
    xfconf_g_property_bind(xsettings_channel,
                           "/Net/DoubleClickTime",
                           G_TYPE_INT,
                           G_OBJECT(mouse_double_click_speed), "value");

    xfconf_g_property_bind(xsettings_channel,
                           "/Gtk/CursorThemeSize",
                           G_TYPE_INT,
                           G_OBJECT(spin_cursor_size), "value");

    g_signal_connect(G_OBJECT(cursor_selection), "changed", G_CALLBACK(cb_cursor_theme_treeselection_changed), gxml);

    dialog = glade_xml_get_widget(gxml, "mouse-settings-dialog");
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
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

    xsettings_channel = xfconf_channel_new("xsettings");

    gxml = glade_xml_new_from_buffer (mouse_dialog_glade,
                                      mouse_dialog_glade_length,
                                      NULL, NULL);

    dialog = mouse_settings_dialog_new_from_xml (gxml);

    gtk_dialog_run(GTK_DIALOG(dialog));

    xfconf_shutdown();

    return 0;
}
