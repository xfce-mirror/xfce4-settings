/*
 *  xfce4-settings-manager
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *                     Jannis Pohlmann <jannis@xfce.org>
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

#include <signal.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <exo/exo.h>
#include <xfconf/xfconf.h>

#include "xfce-settings-manager-dialog.h"

#ifndef MIN
#define MIN(a, b)  ( (a) < (b) ? (a) : (b) )
#endif

#define WINDOW_MAX_WIDTH 800
#define WINDOW_MAX_HEIGHT 600

struct _XfceSettingsManagerDialog
{
    XfceTitledDialog parent;

    GtkListStore *ls;

    GtkWidget *content_frame;

    GtkWidget *scrollwin;

    GtkWidget *icon_view;
    GtkWidget *client_frame;
    GtkWidget *socket_viewport;
    GtkWidget *socket;

    GtkWidget *back_button;
    GtkWidget *help_button;

    const gchar *default_title;
    const gchar *default_subtitle;
    const gchar *default_icon;

    gchar       *help_file;

    GPid last_pid;
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
    COL_PLUGGABLE,
    COL_HELP_FILE,
    COL_DIALOG_NAME,
    N_COLS
};

static void xfce_settings_manager_dialog_class_init(XfceSettingsManagerDialogClass *klass);
static void xfce_settings_manager_dialog_init(XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_finalize(GObject *obj);

static void xfce_settings_manager_dialog_reset_view(XfceSettingsManagerDialog *dialog,
                                                    gboolean overview);
static void xfce_settings_manager_dialog_create_liststore(XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_item_activated(ExoIconView *iconview,
                                                        GtkTreePath *path,
                                                        gpointer user_data);
static void xfce_settings_manager_dialog_back_button_clicked(GtkWidget *button,
                                                             XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_help_button_clicked(GtkWidget *button,
                                                             XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_response(GtkDialog *dialog,
                                                  gint response);
static void xfce_settings_manager_dialog_plug_added(GtkSocket *socket,
                                                    XfceSettingsManagerDialog *dialog);
static gboolean xfce_settings_manager_dialog_plug_removed(GtkSocket *socket,
                                                          XfceSettingsManagerDialog *dialog);
static GtkWidget *xfce_settings_manager_dialog_recreate_socket(XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_compute_default_size (XfceSettingsManagerDialog *dialog,
                                                               gint *width,
                                                               gint *height);
static gboolean xfce_settings_manager_dialog_closed (GtkWidget *dialog,
                                                     GdkEvent *event);
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
    XfconfChannel *channel;
    GtkWidget *scrollwin;
    GtkCellRenderer *render;
    gint width, height;

    dialog->socket = NULL;
    dialog->last_pid = -1;

    dialog->default_title = _("Settings");
    dialog->default_subtitle = _("Customize your desktop");
    dialog->default_icon = "preferences-desktop";

    dialog->help_file = NULL;

    channel = xfconf_channel_get("xfce4-settings-manager");
    xfce_settings_manager_dialog_compute_default_size(dialog, &width, &height);
    width = xfconf_channel_get_int(channel, "/window-width", width);
    height = xfconf_channel_get_int(channel, "/window-height", height);
    gtk_window_set_default_size(GTK_WINDOW(dialog), width, height);

    g_signal_connect(dialog, "delete-event", G_CALLBACK(xfce_settings_manager_dialog_closed), NULL);

    xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(dialog),
                                    dialog->default_subtitle);
    gtk_window_set_title(GTK_WINDOW(dialog), dialog->default_title);
    gtk_window_set_icon_name(GTK_WINDOW(dialog), dialog->default_icon);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    dialog->content_frame = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(dialog->content_frame);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 
                      dialog->content_frame);

    dialog->scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(dialog->scrollwin), 
                                        GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dialog->scrollwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->scrollwin), 6);
    gtk_widget_show(dialog->scrollwin);
    gtk_container_add(GTK_CONTAINER(dialog->content_frame), dialog->scrollwin);

    xfce_settings_manager_dialog_create_liststore(dialog);
    dialog->icon_view = exo_icon_view_new_with_model(GTK_TREE_MODEL(dialog->ls));
    exo_icon_view_set_orientation(EXO_ICON_VIEW(dialog->icon_view),
                                  GTK_ORIENTATION_HORIZONTAL);
    exo_icon_view_set_layout_mode(EXO_ICON_VIEW(dialog->icon_view),
                                  EXO_ICON_VIEW_LAYOUT_ROWS);
    exo_icon_view_set_single_click(EXO_ICON_VIEW(dialog->icon_view), TRUE);
    exo_icon_view_set_reorderable(EXO_ICON_VIEW(dialog->icon_view), FALSE);
    exo_icon_view_set_selection_mode(EXO_ICON_VIEW(dialog->icon_view),
                                     GTK_SELECTION_SINGLE);
    gtk_widget_show(dialog->icon_view);
    gtk_container_add(GTK_CONTAINER(dialog->scrollwin), dialog->icon_view);
    g_signal_connect(G_OBJECT(dialog->icon_view), "item-activated",
                     G_CALLBACK(xfce_settings_manager_dialog_item_activated),
                     dialog);
#if GTK_CHECK_VERSION(2, 12, 0)
    g_object_set(G_OBJECT(dialog->icon_view), "has-tooltip", TRUE, NULL);
    g_signal_connect(G_OBJECT(dialog->icon_view), "query-tooltip",
                     G_CALLBACK(xfce_settings_manager_dialog_query_tooltip),
                     NULL);
#endif

    render = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dialog->icon_view), render, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(dialog->icon_view), render,
                                  "icon-name", COL_ICON_NAME);
    g_object_set(G_OBJECT(render), "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
    g_object_set(G_OBJECT(render), "follow-state", TRUE, NULL);

    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_end(GTK_CELL_LAYOUT(dialog->icon_view), render, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(dialog->icon_view), render,
                                  "text", COL_NAME);

    /* Create client frame to contain the socket scroll window */
    dialog->client_frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(dialog->client_frame), 
                              GTK_SHADOW_NONE);
    gtk_widget_hide(dialog->client_frame);
    gtk_container_add(GTK_CONTAINER(dialog->content_frame), dialog->client_frame);

    /* Create scroll window to contain the socket viewport */
    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwin), 
                                        GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show(scrollwin);
    gtk_container_add(GTK_CONTAINER(dialog->client_frame), scrollwin);

    /* Create socket viewport */
    dialog->socket_viewport = gtk_viewport_new(NULL, NULL);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(dialog->socket_viewport), 
                                 GTK_SHADOW_NONE);
    gtk_widget_show(dialog->socket_viewport);
    gtk_container_add(GTK_CONTAINER(scrollwin), dialog->socket_viewport);

    /* Create socket */
    dialog->socket = xfce_settings_manager_dialog_recreate_socket(dialog);

    /* Connect to response signal because maybe we need to kill the settings
     * dialog spawned last before closing the dialog */
    g_signal_connect(dialog, "response",
                     G_CALLBACK(xfce_settings_manager_dialog_response), NULL);

    /* Configure action area */
    gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_DIALOG(dialog)->action_area),
                              GTK_BUTTONBOX_EDGE);

    /* Create back button which takes the user back to the overview */
    dialog->back_button = gtk_button_new_with_mnemonic(_("_Overview"));
    gtk_button_set_image(GTK_BUTTON(dialog->back_button),
                         gtk_image_new_from_stock(GTK_STOCK_GO_BACK, 
                                                  GTK_ICON_SIZE_BUTTON));
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 
                      dialog->back_button);
    gtk_widget_set_sensitive(dialog->back_button, FALSE);
    gtk_widget_show(dialog->back_button);

    g_signal_connect(dialog->back_button, "clicked", 
                     G_CALLBACK(xfce_settings_manager_dialog_back_button_clicked),
                     dialog);

    /* Create help button */
    dialog->help_button = gtk_button_new_from_stock(GTK_STOCK_HELP);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 
                      dialog->help_button);
    gtk_widget_hide(dialog->help_button);

    g_signal_connect(dialog->help_button, "clicked",
                     G_CALLBACK(xfce_settings_manager_dialog_help_button_clicked),
                     dialog);

    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE,
                          GTK_RESPONSE_CLOSE);

    xfce_settings_manager_dialog_reset_view(dialog, TRUE);
}

static void
xfce_settings_manager_dialog_finalize(GObject *obj)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG(obj);
    
    g_free (dialog->help_file);
    g_object_unref(dialog->ls);

    G_OBJECT_CLASS(xfce_settings_manager_dialog_parent_class)->finalize(obj);
}

static void
xfce_settings_manager_dialog_reset_view(XfceSettingsManagerDialog *dialog,
                                        gboolean overview)
{
    if(overview) {
        /* Reset dialog title and icon */
        gtk_window_set_title(GTK_WINDOW(dialog), dialog->default_title);
        gtk_window_set_icon_name(GTK_WINDOW(dialog), dialog->default_icon);
        xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(dialog),
                                        dialog->default_subtitle);

        /* Hide the socket view and display the overview */
        gtk_widget_hide(dialog->client_frame);
        gtk_widget_show(dialog->scrollwin);

        /* Hide the back button in the overview */
        gtk_widget_set_sensitive(dialog->back_button, FALSE);
        
        /* Show the help button */
        gtk_widget_show(dialog->help_button);

        /* Use default help url */
        g_free(dialog->help_file);
        dialog->help_file = NULL;
    } else {
        /* Hide overview and (just to be sure) the socket view. The latter is
         * to made visible once a plug has been added to the socket */
        gtk_widget_hide(dialog->scrollwin);
        gtk_widget_hide(dialog->client_frame);

        /* Realize the socket (just to make sure embedding will succeed) */
        gtk_widget_realize(dialog->socket);

        /* Display the back button */
        gtk_widget_set_sensitive(dialog->back_button, TRUE);

        /* Hide the help button */
        gtk_widget_hide(dialog->help_button);
    }
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
                                    G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, 
                                    G_TYPE_STRING, G_TYPE_STRING);
    
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
            gchar **categories, *dialog_name;
            gboolean have_x_xfce = FALSE, have_desktop_settings = FALSE;
            gint j;
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

            categories = xfce_rc_read_list_entry(rcfile, "Categories", ";");
            if(!categories) {
                xfce_rc_close(rcfile);
                continue;
            }

            for(j = 0; categories[j]; ++j) {
                if(!strcmp(categories[j], "X-XFCE"))
                    have_x_xfce = TRUE;
                else if(!strcmp(categories[j], "DesktopSettings"))
                    have_desktop_settings = TRUE;
            }
            g_strfreev(categories);
            if(!have_x_xfce || !have_desktop_settings) {
                xfce_rc_close(rcfile);
                continue;
            }

            if(xfce_rc_read_bool_entry(rcfile, "Hidden", FALSE)
               || xfce_rc_read_bool_entry(rcfile,
                                          "X-XfceSettingsManagerHidden",
                                          FALSE))
            {
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

            dialog_name = g_strndup(file, g_strrstr(file, ".desktop") - file);

            gtk_list_store_append(dialog->ls, &iter);
            gtk_list_store_set(dialog->ls, &iter,
                               COL_NAME, name,
                               COL_ICON_NAME, xfce_rc_read_entry(rcfile, "Icon", GTK_STOCK_MISSING_IMAGE),
                               COL_COMMENT, xfce_rc_read_entry(rcfile, "Comment", NULL),
                               COL_EXEC, exec,
                               COL_SNOTIFY, xfce_rc_read_bool_entry(rcfile, "StartupNotify", FALSE),
                               COL_PLUGGABLE, xfce_rc_read_bool_entry(rcfile, "X-XfcePluggable", FALSE),
                               COL_HELP_FILE, xfce_rc_read_entry(rcfile, "X-XfceHelpFile", FALSE),
                               COL_DIALOG_NAME, dialog_name,
                               -1);
            
            g_free(dialog_name);

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
xfce_settings_manager_dialog_item_activated(ExoIconView *iconview,
                                            GtkTreePath *path,
                                            gpointer user_data)
{
    XfceSettingsManagerDialog *dialog = user_data;
    GtkTreeIter iter;
    gchar *exec = NULL, *name, *comment, *icon_name, *primary, *help_file, 
          *command;
    gboolean snotify = FALSE;
    gboolean pluggable = FALSE;
    GError *error = NULL;

    if(!gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->ls), &iter, path))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(dialog->ls), &iter,
                       COL_NAME, &name,
                       COL_COMMENT, &comment,
                       COL_EXEC, &exec,
                       COL_ICON_NAME, &icon_name,
                       COL_SNOTIFY, &snotify,
                       COL_PLUGGABLE, &pluggable,
                       COL_HELP_FILE, &help_file,
                       -1);

    /* Kill the previously spawned dialog (if there is any) */
    xfce_settings_manager_dialog_recreate_socket(dialog);

    if(pluggable) {
        /* Update dialog title and icon */
        gtk_window_set_title(GTK_WINDOW(dialog), name);
        gtk_window_set_icon_name(GTK_WINDOW(dialog), icon_name);
        xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(dialog), comment);

        /* Switch to the socket view (but don't display it yet) */
        xfce_settings_manager_dialog_reset_view(dialog, FALSE);

        /* If the dialog supports help, show the help button */
        if(help_file) {
            gtk_widget_show (dialog->help_button);

            /* Replace the current help url */
            g_free(dialog->help_file);
            dialog->help_file = g_strdup(help_file);
        } 

        /* Build the dialog command */
        command = g_strdup_printf("%s --socket-id=%d", exec,
                                  gtk_socket_get_id(GTK_SOCKET(dialog->socket)));

        /* Try to spawn the dialog */
        if(!xfce_exec_on_screen (gtk_widget_get_screen(GTK_WIDGET(iconview)), 
                                 command, FALSE, snotify, &error))
        {
            /* Spawning failed, go back to the overview */
            xfce_settings_manager_dialog_recreate_socket(dialog);
            xfce_settings_manager_dialog_reset_view(dialog, TRUE);

            /* Notify the user that there has been a problem */
            primary = g_strdup_printf(_("Unable to start \"%s\""), exec);
            xfce_message_dialog(GTK_WINDOW(dialog), _("Xfce Settings Manager"),
                                GTK_STOCK_DIALOG_ERROR, primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
        }

        g_free(command);
    } else {
        /* Switch to the main view (just to be sure) */
        xfce_settings_manager_dialog_reset_view(dialog, TRUE);

        /* Try to spawn the dialog */
        if (!xfce_exec_on_screen(gtk_widget_get_screen(GTK_WIDGET(iconview)),
                                 exec, FALSE, snotify, &error))
        {
            /* Notify the user that there has been a problem */
            primary = g_strdup_printf(_("Unable to start \"%s\""), exec);
            xfce_message_dialog(GTK_WINDOW(dialog), _("Xfce Settings Manager"),
                                GTK_STOCK_DIALOG_ERROR, primary, error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
            g_free(primary);
            g_error_free(error);
        }
    }

    g_free(exec);
    g_free(name);
    g_free(comment);
    g_free(icon_name);
    g_free(help_file);
}

static void
xfce_settings_manager_dialog_back_button_clicked(GtkWidget *button,
                                                 XfceSettingsManagerDialog *dialog)
{
    /* Kill the currently embedded dialog and go back to the overview */
    xfce_settings_manager_dialog_recreate_socket(dialog);
    xfce_settings_manager_dialog_reset_view(dialog, TRUE);
}

static void 
xfce_settings_manager_dialog_help_button_clicked(GtkWidget *button,
                                                 XfceSettingsManagerDialog *dialog)
{
    GError *error = NULL;
    gchar  *command;

    /* Open absolute filenames with exo-open and relative filenames with xfhelp4 */
    if(dialog->help_file) {
        if(g_path_is_absolute(dialog->help_file))
            command = g_strconcat("exo-open ", dialog->help_file, NULL);
        else
            command = g_strconcat("xfhelp4 ", dialog->help_file, NULL);
    } else {
        /* TODO: Maybe use xfce4-settings-manager.html or something similar here */
        command = g_strconcat("xfhelp4 ", "xfce4-settings.html", NULL);
    }

    /* Try to open the documentation */
    if(!gdk_spawn_command_line_on_screen(gtk_widget_get_screen(button), 
                                         command, &error))
    {
        xfce_err(_("Failed to open the documentation. Reason: %s"), 
                 error->message);
        g_error_free(error);
    }

    g_free(command);
}

static void
xfce_settings_manager_dialog_response(GtkDialog *dialog,
                                      gint response)
{
    XfceSettingsManagerDialog *sm_dialog = XFCE_SETTINGS_MANAGER_DIALOG(dialog);

    if(response == GTK_RESPONSE_CLOSE) {
        xfce_settings_manager_dialog_closed(GTK_WIDGET(dialog), NULL);
    }

    /* Make sure the currently embedded dialog is killed before exiting */
    xfce_settings_manager_dialog_recreate_socket(sm_dialog);
}

static gboolean
xfce_settings_manager_dialog_show_client(XfceSettingsManagerDialog *dialog)
{
    g_return_val_if_fail(XFCE_IS_SETTINGS_MANAGER_DIALOG(dialog), FALSE);

    gtk_widget_show(dialog->client_frame);
    return FALSE;
}

static void
xfce_settings_manager_dialog_plug_added(GtkSocket *socket,
                                        XfceSettingsManagerDialog *dialog)
{
    g_return_if_fail(XFCE_IS_SETTINGS_MANAGER_DIALOG(dialog));

    g_timeout_add(250, (GSourceFunc) xfce_settings_manager_dialog_show_client, 
                  dialog);
}

static gboolean
xfce_settings_manager_dialog_plug_removed(GtkSocket *socket,
                                          XfceSettingsManagerDialog *dialog)
{
    /* Return true to be able to re-use the socket for another plug */
    return TRUE;
}

static GtkWidget *
xfce_settings_manager_dialog_recreate_socket(XfceSettingsManagerDialog *dialog)
{
    if(GTK_IS_WIDGET(dialog->socket))
        gtk_widget_destroy (dialog->socket);

    dialog->socket = gtk_socket_new();
    gtk_widget_show(dialog->socket);
    gtk_container_add(GTK_CONTAINER(dialog->socket_viewport), dialog->socket);
    
    /* Handle newly added plugs in a callback */
    g_signal_connect(dialog->socket, "plug-added", 
                     G_CALLBACK(xfce_settings_manager_dialog_plug_added),
                     dialog);

    /* Add plug-removed callback to be able to re-use the socket when plugs
     * are removed */
    g_signal_connect(dialog->socket, "plug-removed", 
                     G_CALLBACK(xfce_settings_manager_dialog_plug_removed),
                     dialog);

    return dialog->socket;
}

static void
xfce_settings_manager_dialog_compute_default_size (XfceSettingsManagerDialog *dialog,
                                                   gint *width,
                                                   gint *height)
{
    GdkRectangle screen_size;
    GdkScreen *screen;
    gint monitor;
  
    screen = gtk_widget_get_screen(GTK_WIDGET(dialog));
  
    gtk_widget_realize(GTK_WIDGET(dialog));
    monitor = gdk_screen_get_monitor_at_window(screen, GTK_WIDGET(dialog)->window);
    gtk_widget_unrealize(GTK_WIDGET(dialog));
  
    gdk_screen_get_monitor_geometry (screen, monitor, &screen_size);
  
    *width = MIN(screen_size.width * 2.0 / 3, WINDOW_MAX_WIDTH);
    *height = MIN(screen_size.height * 2.0 / 3, WINDOW_MAX_HEIGHT);
}

static gboolean
xfce_settings_manager_dialog_closed (GtkWidget *dialog,
                                     GdkEvent *event)
{
    XfconfChannel *channel;
    gint width, height;

    g_return_val_if_fail(XFCE_IS_SETTINGS_MANAGER_DIALOG(dialog),FALSE);

    channel = xfconf_channel_get("xfce4-settings-manager");
    gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);
    xfconf_channel_set_int(channel, "/window-width", width);
    xfconf_channel_set_int(channel, "/window-height", height);

    return FALSE;
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



void 
xfce_settings_manager_dialog_show_dialog (XfceSettingsManagerDialog *dialog,
                                          const gchar *dialog_name)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gchar *name;

  g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));
  g_return_if_fail (dialog_name != NULL);

  model = exo_icon_view_get_model(EXO_ICON_VIEW(dialog->icon_view));
  
  if(G_LIKELY(gtk_tree_model_get_iter_first(model, &iter))) {
      do {
          gtk_tree_model_get(model, &iter, COL_DIALOG_NAME, &name, -1);

          if(G_UNLIKELY(name != NULL && g_str_equal(name, dialog_name))) {
              path = gtk_tree_model_get_path(model, &iter);
              xfce_settings_manager_dialog_item_activated(EXO_ICON_VIEW(dialog->icon_view),
                                                          path, dialog);
              gtk_tree_path_free(path);
              break;
          }

          g_free(name);
      } while(gtk_tree_model_iter_next(model, &iter));
  }
}
