/*
 *  Copyright (c) 2011 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <act/act-user-manager.h>
#include <xfconf/xfconf.h>
#include <exo/exo.h>

#include "user-dialog_ui.h"
#include "avatar_default.h"



static GdkNativeWindow opt_socket_id = 0;
static gboolean        opt_version = FALSE;

static GQuark manager_quark;
static GOptionEntry entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

enum
{
    USERS_COL_ICON,
    USERS_COL_NAME,
    USERS_COL_ABSTRACT
};



static GdkPixbuf *
user_settings_user_icon (ActUser *user,
                         gint     size)
{
    const gchar *file;
    GdkPixbuf   *icon = NULL;
    GError      *error = NULL;
    GdkPixbuf   *scaled;

    g_return_val_if_fail (ACT_IS_USER (user), NULL);

    file = act_user_get_icon_file (user);
    if (file != NULL && g_file_test (file, G_FILE_TEST_EXISTS))
    {
        icon = gdk_pixbuf_new_from_file (file, &error);
        if (icon == NULL)
        {
            g_warning ("Failed to load icon \"%s\": %s", file, error->message);
            g_error_free (error);
        }
    }

    /* Fallback icon */
    if (icon == NULL)
        icon = gdk_pixbuf_new_from_inline (-1, avatar_default, FALSE, NULL);

    /* Scale to requested size */
    if (icon != NULL)
    {
        scaled = exo_gdk_pixbuf_scale_ratio (icon, size);
        g_object_unref (G_OBJECT (icon));
        icon = scaled;
    }

    return icon;
}



static void
user_settings_user_update (ActUserManager *manager,
                           ActUser        *user,
                           GtkBuilder     *builder)
{
    GObject     *object;
    const gchar *name;
    GdkPixbuf   *icon;

    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    name = act_user_get_real_name (user);
    if (name == NULL || *name == '\0')
      name = act_user_get_user_name (user);

    object = gtk_builder_get_object (builder, "user-name");
    g_return_if_fail (GTK_IS_ENTRY (object));
    gtk_entry_set_text (GTK_ENTRY (object), name);

    object = gtk_builder_get_object (builder, "user-type-combo");
    g_return_if_fail (GTK_IS_COMBO_BOX (object));
    gtk_combo_box_set_active (GTK_COMBO_BOX (object),
        act_user_get_account_type (user));

    object = gtk_builder_get_object (builder, "user-lang-combo");
    g_return_if_fail (GTK_IS_COMBO_BOX (object));

    object = gtk_builder_get_object (builder, "user-auto-login");
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (object));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
        act_user_get_automatic_login (user));

    object = gtk_builder_get_object (builder, "user-icon");
    g_return_if_fail (GTK_IS_IMAGE (object));
    icon = user_settings_user_icon (user, 48);
    gtk_image_set_from_pixbuf (GTK_IMAGE (object), icon);
    if (icon != NULL)
        g_object_unref (G_OBJECT (icon));
}



static void
user_settings_user_added (ActUserManager *manager,
                          ActUser        *user,
                          GtkBuilder     *builder,
                          GtkTreeIter    *iter_return)
{
    gchar       *abstract;
    GObject     *model;
    GtkTreeIter  iter;
    const gchar *name;
    const gchar *role;
    GdkPixbuf   *icon;

    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    model = gtk_builder_get_object (builder, "users-model");
    g_return_if_fail (GTK_IS_LIST_STORE (model));

    name = act_user_get_real_name (user);
    if (name == NULL || *name == '\0')
      name = act_user_get_user_name (user);

    if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR)
      role = _("Administrator");
    else
      role = _("Standard");

    abstract = g_markup_printf_escaped ("%s\n<small>%s</small>", name, role);

    icon = user_settings_user_icon (user, 32);

    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        USERS_COL_ICON, icon,
                        USERS_COL_NAME, act_user_get_user_name (user),
                        USERS_COL_ABSTRACT, abstract,
                        -1);

    if (icon != NULL)
        g_object_unref (G_OBJECT (icon));
    g_free (abstract);

    if (iter_return != NULL)
        *iter_return = iter;
}



static void
user_settings_user_removed (ActUserManager *manager,
                            ActUser        *user,
                            GtkBuilder     *builder)
{
    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    g_message ("user %s removed", act_user_get_user_name (user));
}



static void
user_settings_user_changed (ActUserManager *manager,
                            ActUser        *user,
                            GtkBuilder     *builder)
{
    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    g_message ("user %s changed", act_user_get_user_name (user));
}



static void
user_settings_user_selection_changed (GtkTreeSelection *selection,
                                      GtkBuilder       *builder)
{
    gchar          *name = NULL;
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    ActUserManager *manager;
    ActUser        *user;

    g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    /* Get the Username */
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

    gtk_tree_model_get (model, &iter, USERS_COL_NAME, &name, -1);
    if (name == NULL)
        return;

    /* Get object from manager */
    manager = g_object_get_qdata (G_OBJECT (builder), manager_quark);
    user = act_user_manager_get_user (manager, name);
    g_free (name);

    /* Update widgets */
    user_settings_user_update (manager, user, builder);
}



static void
user_settings_manager_is_loaded (ActUserManager *manager,
                                 GParamSpec     *pspec,
                                 GtkBuilder     *builder)
{
    GSList           *users, *li;
    GtkTreeIter       iter;
    const gchar      *current;
    GObject          *object;
    GtkTreeSelection *selection;

    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    /* Disconnect this function */
    g_signal_handlers_disconnect_by_func (G_OBJECT (manager),
        G_CALLBACK (user_settings_manager_is_loaded), builder);

    /* Watch manager changes */
    g_signal_connect (G_OBJECT (manager), "user-added",
        G_CALLBACK (user_settings_user_added), builder);
    g_signal_connect (G_OBJECT (manager), "user-removed",
        G_CALLBACK (user_settings_user_removed), builder);
    g_signal_connect (G_OBJECT (manager), "user-changed",
        G_CALLBACK (user_settings_user_changed), builder);

    current = g_get_user_name ();

    /* Add the known users */
    users = act_user_manager_list_users (manager);
    for (li = users; li != NULL; li = li->next)
    {
        user_settings_user_added (manager, li->data, builder, &iter);

        /* Select current user */
        if (g_strcmp0 (current, act_user_get_user_name (li->data)) == 0)
        {
            object = gtk_builder_get_object (builder, "users-treeview");
            g_return_if_fail (GTK_IS_TREE_VIEW (object));

            selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
            gtk_tree_selection_select_iter (selection, &iter);

            current = NULL;
        }
    }
    g_slist_free (users);
}



gint
main (gint argc, gchar **argv)
{
    GObject          *dialog, *plug_child;
    GtkWidget        *plug;
    GtkBuilder       *builder;
    GError           *error = NULL;
    ActUserManager   *manager;
    GObject          *object;
    GtkTreeSelection *selection;

    /* Init quark */
    manager_quark = g_quark_from_static_string ("user-manager");

    /* Setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args (&argc, &argv, "", entries, PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* Check if we should print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-2011");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* Initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* Print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* Hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* Load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, user_dialog_ui,
                                     user_dialog_ui_length, &error) != 0)
    {
        /* Wait for the manager to complete... */
        manager = act_user_manager_get_default ();
        g_object_set_qdata (G_OBJECT (builder), manager_quark, manager);
        g_signal_connect (G_OBJECT (manager), "notify::is-loaded",
            G_CALLBACK (user_settings_manager_is_loaded), builder);

        object = gtk_builder_get_object (builder, "users-treeview");
        g_return_val_if_fail (GTK_IS_TREE_VIEW (object), EXIT_FAILURE);
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
        g_signal_connect (G_OBJECT (selection), "changed",
            G_CALLBACK (user_settings_user_selection_changed), builder);

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            /* Get the dialog widget */
            dialog = gtk_builder_get_object (builder, "dialog");

            gtk_widget_show (GTK_WIDGET (dialog));
            g_signal_connect (dialog, "response", G_CALLBACK (gtk_main_quit), NULL);

            /* To prevent the settings dialog to be saved in the session */
            gdk_set_sm_client_id ("FAKE ID");

            gtk_main ();

            gtk_widget_destroy (GTK_WIDGET (dialog));
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
            plug_child = gtk_builder_get_object (builder, "plug-child");
            gtk_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));

            /* To prevent the settings dialog to be saved in the session */
            gdk_set_sm_client_id ("FAKE ID");

            /* Enter main loop */
            gtk_main ();
        }

        g_object_unref (G_OBJECT (manager));
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    g_object_unref (G_OBJECT (builder));

    /* Shutdown xfconf */
    xfconf_shutdown();

    return EXIT_SUCCESS;
}
