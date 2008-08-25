/*
 *  xfce4-settings-manager
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <exo/exo.h>

#include "xfce-settings-manager-dialog.h"

#define SETTINGS_CATEGORY  "X-XfceSettingsDialog"

struct _XfceSettingsManagerDialog
{
    XfceTitledDialog parent;

    GtkListStore *ls;
};

typedef struct _XfceSettingsManagerDialogClass
{
    XfceTitledDialogClass parent;
} XfceSettingsManagerDialogClass;

enum
{
    COL_NAME = 0,
    COL_ICON_NAME,
    COL_COMMENT,
    COL_EXEC,
    COL_SNOTIFY,
    N_COLS
};

static void xfce_settings_manager_dialog_class_init(XfceSettingsManagerDialogClass *klass);
static void xfce_settings_manager_dialog_init(XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_finalize(GObject *obj);

static void xfce_settings_manager_dialog_create_liststore(XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_item_activated(GtkIconView *iconview,
                                                        GtkTreePath *path,
                                                        gpointer user_data);
#if GTK_CHECK_VERSION(2, 12, 0)
static gboolean xfce_settings_manager_dialog_query_tooltip(GtkWidget *widget,
                                                           gint x,
                                                           gint y,
                                                           gboolean keyboard_tip,
                                                           GtkTooltip *tooltip,
                                                           gpointer data);
#endif


G_DEFINE_TYPE(XfceSettingsManagerDialog, xfce_settings_manager_dialog, XFCE_TYPE_TITLED_DIALOG)


static void
xfce_settings_manager_dialog_class_init(XfceSettingsManagerDialogClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->finalize = xfce_settings_manager_dialog_finalize;
}

static void
xfce_settings_manager_dialog_init(XfceSettingsManagerDialog *dialog)
{
    GtkWidget *sw, *iconview;
    GtkCellRenderer *render;

    xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(dialog),
                                    _("Customize your Xfce desktop"));
    gtk_window_set_title(GTK_WINDOW(dialog), _("Xfce Settings Manager"));
    gtk_window_set_icon_name(GTK_WINDOW(dialog), "preferences-desktop");
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(sw), 6);
    gtk_widget_show(sw);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), sw, TRUE, TRUE, 0);

    xfce_settings_manager_dialog_create_liststore(dialog);
    iconview = exo_icon_view_new_with_model(GTK_TREE_MODEL(dialog->ls));
    exo_icon_view_set_orientation(EXO_ICON_VIEW(iconview),
                                  GTK_ORIENTATION_HORIZONTAL);
    exo_icon_view_set_layout_mode(EXO_ICON_VIEW(iconview),
                                  EXO_ICON_VIEW_LAYOUT_ROWS);
    exo_icon_view_set_single_click(EXO_ICON_VIEW(iconview), TRUE);
    exo_icon_view_set_reorderable(EXO_ICON_VIEW(iconview), FALSE);
    exo_icon_view_set_selection_mode(EXO_ICON_VIEW(iconview),
                                     GTK_SELECTION_NONE);
    gtk_widget_show(iconview);
    gtk_container_add(GTK_CONTAINER(sw), iconview);
    g_signal_connect(G_OBJECT(iconview), "item-activated",
                     G_CALLBACK(xfce_settings_manager_dialog_item_activated),
                     dialog);
#if GTK_CHECK_VERSION(2, 12, 0)
    g_object_set(G_OBJECT(iconview), "has-tooltip", TRUE, NULL);
    g_signal_connect(G_OBJECT(iconview), "query-tooltip",
                     G_CALLBACK(xfce_settings_manager_dialog_query_tooltip),
                     NULL);
#endif

    render = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(iconview), render, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(iconview), render,
                                  "icon-name", COL_ICON_NAME);
    g_object_set(G_OBJECT(render), "stock-size", GTK_ICON_SIZE_DIALOG, NULL);

    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_end(GTK_CELL_LAYOUT(iconview), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(iconview), render,
                                  "text", COL_NAME);
    
    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE,
                          GTK_RESPONSE_ACCEPT);
}

static void
xfce_settings_manager_dialog_finalize(GObject *obj)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG(obj);

    g_object_unref(G_OBJECT(dialog->ls));

    G_OBJECT_CLASS(xfce_settings_manager_dialog_parent_class)->finalize(obj);
}



static gint
xfce_settings_manager_dialog_sort_icons(GtkTreeModel *model,
                                        GtkTreeIter *a,
                                        GtkTreeIter *b,
                                        gpointer user_data)
{
    gchar *namea = NULL, *nameb = NULL;
    gint ret;

    gtk_tree_model_get(model, a, COL_NAME, &namea, -1);
    gtk_tree_model_get(model, b, COL_NAME, &nameb, -1);

    if(!namea && !nameb)
        ret = 0;
    else if(!namea)
        ret = -1;
    else if(!nameb)
        ret = 1;
    else
        ret = g_utf8_collate(namea, nameb);

    g_free(namea);
    g_free(nameb);

    return ret;
}

static void
xfce_settings_manager_dialog_create_liststore(XfceSettingsManagerDialog *dialog)
{
    gchar **dirs, buf[PATH_MAX];
    gint i, icon_size;

    dialog->ls = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_BOOLEAN);
    
    dirs = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "applications/");
    if(!dirs)
        return;

    gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &icon_size, &icon_size);

    for(i = 0; dirs[i]; ++i) {
        GDir *d = g_dir_open(dirs[i], 0, 0);
        const gchar *file;

        if(!d)
            continue;

        while((file = g_dir_read_name(d))) {
            XfceRc *rcfile;
            const gchar *name, *exec, *value;
            GtkTreeIter iter;

            if(!g_str_has_suffix(file, ".desktop"))
                continue;

            g_snprintf(buf, sizeof(buf), "%s/%s", dirs[i], file);
            rcfile = xfce_rc_simple_open(buf, TRUE);
            if(!rcfile)
                continue;

            if(!xfce_rc_has_group(rcfile, "Desktop Entry")) {
                xfce_rc_close(rcfile);
                continue;
            }
            xfce_rc_set_group(rcfile, "Desktop Entry");

            value = xfce_rc_read_entry(rcfile, "Categories", NULL);
            if(!value) {
                xfce_rc_close(rcfile);
                continue;
            }

            if(!strncmp(value, SETTINGS_CATEGORY ";",
                        strlen(SETTINGS_CATEGORY ";"))
                || !strstr(value, ";" SETTINGS_CATEGORY ";"))
            {
                xfce_rc_close(rcfile);
                continue;
            }

            if(xfce_rc_read_bool_entry(rcfile, "Hidden", FALSE)) {
                xfce_rc_close(rcfile);
                continue;
            }

            value = xfce_rc_read_entry(rcfile, "TryExec", NULL);
            if(value) {
                gchar *prog = g_find_program_in_path(value);

                if(!prog || access(prog, R_OK|X_OK)) {
                    g_free(prog);
                    xfce_rc_close(rcfile);
                    continue;
                }
                g_free(prog);
            }

            if(!(name = xfce_rc_read_entry(rcfile, "X-XfceSettingsName", NULL))) {
                if(!(name = xfce_rc_read_entry(rcfile, "GenericName", NULL))) {
                    if(!(name = xfce_rc_read_entry(rcfile, "Name", NULL))) {
                        xfce_rc_close(rcfile);
                        continue;
                    }
                }
            }

            exec = xfce_rc_read_entry(rcfile, "Exec", NULL);
            if(!exec) {
                xfce_rc_close(rcfile);
                continue;
            }

            gtk_list_store_append(dialog->ls, &iter);
            gtk_list_store_set(dialog->ls, &iter,
                               COL_NAME, name,
                               COL_ICON_NAME, xfce_rc_read_entry(rcfile, "Icon", GTK_STOCK_MISSING_IMAGE),
                               COL_COMMENT, xfce_rc_read_entry(rcfile, "Comment", NULL),
                               COL_EXEC, exec,
                               COL_SNOTIFY, xfce_rc_read_bool_entry(rcfile, "StartupNotify", FALSE),
                               -1);

            xfce_rc_close(rcfile);
        }

        g_dir_close(d);
    }

    g_strfreev(dirs);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(dialog->ls), COL_NAME,
                                    xfce_settings_manager_dialog_sort_icons,
                                    dialog, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->ls),
                                         COL_NAME, GTK_SORT_ASCENDING);
}

static void
xfce_settings_manager_dialog_item_activated(GtkIconView *iconview,
                                            GtkTreePath *path,
                                            gpointer user_data)
{
    XfceSettingsManagerDialog *dialog = user_data;
    GtkTreeIter iter;
    gchar *exec = NULL;
    gboolean snotify = FALSE;
    GError *error = NULL;

    if(!gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->ls), &iter, path))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(dialog->ls), &iter,
                       COL_EXEC, &exec,
                       COL_SNOTIFY, &snotify,
                       -1);

    if(!xfce_exec_on_screen(gtk_widget_get_screen(GTK_WIDGET(iconview)),
                            exec, FALSE, snotify, &error))
    {
        gchar *primary = g_strdup_printf(_("Unable to start \"%s\""), exec);
        xfce_message_dialog(GTK_WINDOW(dialog), _("Xfce Settings Manager"),
                            GTK_STOCK_DIALOG_ERROR, primary, error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        g_free(primary);
        g_error_free(error);
    }

    g_free(exec);
}

#if GTK_CHECK_VERSION(2, 12, 0)
static gboolean
xfce_settings_manager_dialog_query_tooltip(GtkWidget *widget,
                                           gint x,
                                           gint y,
                                           gboolean keyboard_tip,
                                           GtkTooltip *tooltip,
                                           gpointer data)
{
    GtkTreePath *path = NULL;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *comment = NULL;

    path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
    if(!path)
        return FALSE;

    model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
    if(!gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_path_free(path);
        return FALSE;
    }
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter, COL_COMMENT, &comment, -1);
    if(!comment || !*comment) {
        g_free(comment);
        return FALSE;
    }

    gtk_tooltip_set_text(tooltip, comment);
    g_free(comment);

    return TRUE;
}
#endif


GtkWidget *
xfce_settings_manager_dialog_new()
{
    return g_object_new(XFCE_TYPE_SETTINGS_MANAGER_DIALOG, NULL);
}
