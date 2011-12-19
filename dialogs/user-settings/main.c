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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_UTMP_H
#include <utmp.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gio/gunixoutputstream.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <act/act-user-manager.h>
#include <exo/exo.h>

#include "user-dialog_ui.h"
#include "avatar_default.h"


#define MAX_ICON_SIZE   128
#define USER_ICON_SIZE  48
#define VALIDNAME_REGEX "^[a-z_][a-z0-9_-]*[$]?$"



/* Timeout id for updates on real name entry changes */
static guint real_name_entry_changed = 0;

/* Quark to bind the ActUserManager on the GtkBuilder */
static GQuark manager_quark = 0;

/* If the interface is updating (and saving should the skipped) */
static gboolean updating_widgets = FALSE;

/* GOptionEntry entry stuff */
static GdkNativeWindow opt_socket_id = 0;
static gboolean        opt_version = FALSE;
static GOptionEntry    opt_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
      &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE,
      &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* Keep in sync with users-model store in glade */
enum
{
    USERS_COL_ICON,
    USERS_COL_UID,
    USERS_COL_NAME,
    USERS_COL_ABSTRACT
};

enum
{
    USER_LANG_CODE,
    USER_LANG_NAME
};



static const gchar *
user_settings_user_real_name (ActUser *user)
{
    const gchar *name;

    name = act_user_get_real_name (user);
    if (exo_str_is_empty (name))
        name = act_user_get_user_name (user);

    return name;
}



/*static const gchar *
user_settings_user_language (ActUser *user)
{
    const gchar         *lang;
    const gchar * const *langs;

    lang = act_user_get_language (user);
    if (exo_str_is_empty (lang))
    {
        lang = setlocale (LC_MESSAGES, NULL);
        if (exo_str_is_empty (lang))
        {
            langs = g_get_language_names ();
            if (langs != NULL && *langs != NULL)
                lang = *langs;
            else
                lang = "en_US";
        }
    }

    return lang;
}*/



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



static ActUser *
user_settings_user_get_selected (GtkBuilder *builder)
{
    gchar            *name = NULL;
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    ActUserManager   *manager;
    ActUser          *user;
    GObject          *treeview;
    GtkTreeSelection *selection;

    /* Get selected iter */
    treeview = gtk_builder_get_object (builder, "users-treeview");
    g_return_val_if_fail (GTK_IS_TREE_VIEW (treeview), NULL);
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return NULL;

    /* Get the Username */
    gtk_tree_model_get (model, &iter, USERS_COL_NAME, &name, -1);
    if (name == NULL)
        return NULL;

    /* Get object from manager */
    manager = g_object_get_qdata (G_OBJECT (builder), manager_quark);
    user = act_user_manager_get_user (manager, name);
    g_free (name);

    g_assert (user != NULL);

    return user;
}



static void
user_settings_user_icon_clicked (GtkWidget  *button,
                                 GtkBuilder *builder)
{
    ActUser       *user;
    GtkWidget     *dialog;
    gchar         *title;
    GtkFileFilter *filter;
    gint           response;
    gchar         *filename;
    GdkPixbuf     *pixbuf;
    GError        *error = NULL;
    GOutputStream *stream;
    gchar         *path = NULL;
    gint           fd;

    g_return_if_fail (GTK_IS_WIDGET (button));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    user = user_settings_user_get_selected (builder);
    title = g_strdup_printf (_("Browse for an icon for %s"),
        user_settings_user_real_name (user));

    dialog = gtk_file_chooser_dialog_new (title,
        GTK_WINDOW (gtk_widget_get_toplevel (button)),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
    exo_gtk_file_chooser_add_thumbnail_preview (GTK_FILE_CHOOSER (dialog));
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);

    /* add file chooser filters */
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("Image Files"));
    gtk_file_filter_add_pixbuf_formats (filter);
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_hide (dialog);

    if (response == GTK_RESPONSE_OK)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        pixbuf = exo_gdk_pixbuf_new_from_file_at_max_size (filename, MAX_ICON_SIZE,
                                                           MAX_ICON_SIZE, TRUE, &error);
        if (pixbuf != NULL)
        {
            fd = g_file_open_tmp (NULL, &path, &error);
            if (fd != -1)
            {
                stream = g_unix_output_stream_new (fd, TRUE);
                if (gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, &error, NULL))
                {
                    act_user_set_icon_file (user, path);
                }
                else
                {
                    g_warning ("Failed to save image: %s", error->message);
                    g_error_free (error);
                }
                g_object_unref (stream);
                g_remove (path);
            }
            else
            {
                g_critical ("Failed to create temporary file for user icon: %s", error->message);
                g_error_free (error);
            }
            g_free (path);
            g_object_unref (pixbuf);
        }
        else
        {
            g_critical ("Failed to load \"%s\": %s", filename, error->message);
            g_error_free (error);
        }

        g_free (filename);
    }

    gtk_widget_destroy (dialog);
}



static void
user_settings_user_passwd_clicked (GtkWidget  *button,
                                   GtkBuilder *builder)
{
    GObject     *dialog;
    gchar       *title;
    ActUser     *user;
    guint        i;
    GObject     *object;
    const gchar *pwd_entries[] = { "pwd-old", "pwd-new",
                                   "pwd-verify", "pwd-hint" };

    g_return_if_fail (GTK_IS_WIDGET (button));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    dialog = gtk_builder_get_object (builder, "pwd-dialog");
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
        GTK_WINDOW (gtk_widget_get_toplevel (button)));
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);

    user = user_settings_user_get_selected (builder);
    title = g_strdup_printf (_("Changing password for %s"),
        user_settings_user_real_name (user));
    gtk_window_set_title (GTK_WINDOW (dialog), title);
    g_free (title);

    /* Make sure the text is not visible */
    object = gtk_builder_get_object (builder, "pwd-visible");
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (object));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), FALSE);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == 1)
    {
        /* Todo */
    }

    gtk_widget_hide (GTK_WIDGET (dialog));

    /* Clear the passwords */
    for (i = 0; i < G_N_ELEMENTS (pwd_entries); i++)
    {
        object = gtk_builder_get_object (builder, pwd_entries[i]);
        g_return_if_fail (GTK_IS_ENTRY (object));
        gtk_entry_set_text (GTK_ENTRY (object), "");
    }
}



static void
user_settings_user_update (ActUser    *user,
                           GtkBuilder *builder)
{
    GObject      *object;
    GdkPixbuf    *icon;
    const gchar  *lang;
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gchar        *code;

    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    updating_widgets = TRUE;

    object = gtk_builder_get_object (builder, "user-name");
    g_return_if_fail (GTK_IS_ENTRY (object));
    gtk_entry_set_text (GTK_ENTRY (object),
        user_settings_user_real_name (user));

    object = gtk_builder_get_object (builder, "user-type-combo");
    g_return_if_fail (GTK_IS_COMBO_BOX (object));
    gtk_combo_box_set_active (GTK_COMBO_BOX (object),
        act_user_get_account_type (user));

    object = gtk_builder_get_object (builder, "user-lang-combo");
    g_return_if_fail (GTK_IS_COMBO_BOX (object));
    lang = act_user_get_language (user);

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (object));
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gtk_tree_model_get (model, &iter, USER_LANG_CODE, &code, -1);
            if (g_strcmp0 (lang, code) == 0)
            {
                g_free (code);
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (object), &iter);
                break;
            }
            g_free (code);
        }
        while (gtk_tree_model_iter_next (model, &iter));
    }

    object = gtk_builder_get_object (builder, "user-auto-login");
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (object));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object),
        act_user_get_automatic_login (user));

    object = gtk_builder_get_object (builder, "user-icon");
    g_return_if_fail (GTK_IS_IMAGE (object));
    gtk_widget_set_size_request (GTK_WIDGET (object), USER_ICON_SIZE, USER_ICON_SIZE);
    icon = user_settings_user_icon (user, USER_ICON_SIZE);
    gtk_image_set_from_pixbuf (GTK_IMAGE (object), icon);
    if (icon != NULL)
        g_object_unref (G_OBJECT (icon));

    updating_widgets = FALSE;
}



static void
user_settings_user_set_model (ActUser      *user,
                              GtkListStore *model,
                              GtkTreeIter  *iter)
{
    gchar       *abstract;
    const gchar *name;
    const gchar *role;
    GdkPixbuf   *icon;

    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_LIST_STORE (model));

    name = user_settings_user_real_name (user);

    if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR)
        role = _("Administrator");
    else
        role = _("Standard");

    abstract = g_markup_printf_escaped ("%s\n<small>%s</small>", name, role);

    icon = user_settings_user_icon (user, 32);

    gtk_list_store_set (GTK_LIST_STORE (model), iter,
                        USERS_COL_ICON, icon,
                        USERS_COL_UID, act_user_get_uid (user),
                        USERS_COL_NAME, act_user_get_user_name (user),
                        USERS_COL_ABSTRACT, abstract,
                        -1);

    if (icon != NULL)
        g_object_unref (G_OBJECT (icon));
    g_free (abstract);
}



static void
user_settings_user_added (ActUserManager *manager,
                          ActUser        *user,
                          GtkBuilder     *builder)
{
    GObject     *model;
    GtkTreeIter  iter;

    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    model = gtk_builder_get_object (builder, "users-model");
    g_return_if_fail (GTK_IS_LIST_STORE (model));

    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    user_settings_user_set_model (user, GTK_LIST_STORE (model), &iter);
}



static gboolean
user_settings_user_model_find (ActUser       *user,
                               GtkBuilder    *builder,
                               GtkTreeModel **model_return,
                               GtkTreeIter   *iter_return)
{
    GtkTreeIter   iter;
    GtkTreeModel *model;
    gboolean      result = FALSE;
    guint         find_uid, uid;

    model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "users-model"));
    g_return_val_if_fail (GTK_IS_LIST_STORE (model), FALSE);

    find_uid = act_user_get_uid (user);

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gtk_tree_model_get (model, &iter, USERS_COL_UID, &uid, -1);
            if (uid == find_uid)
            {
                result = TRUE;
                break;
            }
        }
        while (gtk_tree_model_iter_next (model, &iter));
    }

    if (model_return != NULL)
        *model_return = GTK_TREE_MODEL (model);
    if (iter_return != NULL)
        *iter_return = iter;

    return result;
}



static void
user_settings_user_removed (ActUserManager *manager,
                            ActUser        *user,
                            GtkBuilder     *builder)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;

    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    if (user_settings_user_model_find (user, builder, &model, &iter))
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}



static void
user_settings_user_changed (ActUserManager *manager,
                            ActUser        *user,
                            GtkBuilder     *builder)
{
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    GObject          *treeview;
    GtkTreeSelection *selection;

    g_return_if_fail (ACT_IS_USER_MANAGER (manager));
    g_return_if_fail (ACT_IS_USER (user));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    if (user_settings_user_model_find (user, builder, &model, &iter))
    {
        /* Set new values for the user selector */
        user_settings_user_set_model (user, GTK_LIST_STORE (model), &iter);

        /* Check if we need to update the widgets */
        treeview = gtk_builder_get_object (builder, "users-treeview");
        g_return_if_fail (GTK_IS_TREE_VIEW (treeview));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        if (gtk_tree_selection_iter_is_selected (selection, &iter))
            user_settings_user_update (user, builder);
    }
}



static gboolean
user_settings_user_name_done_timeout (gpointer data)
{
    GtkBuilder  *builder = GTK_BUILDER (data);
    GObject     *entry;
    const gchar *name;
    ActUser     *user;

    g_return_val_if_fail (GTK_IS_BUILDER (builder), FALSE);

    entry = gtk_builder_get_object (builder, "user-name");
    name = gtk_entry_get_text (GTK_ENTRY (entry));

    user = user_settings_user_get_selected (builder);
    act_user_set_real_name (user, name);

    return FALSE;
}



static void
user_settings_user_name_done_timeout_destroyed (gpointer data)
{
    real_name_entry_changed = 0;
}



static void
user_settings_user_change_name_now (GtkBuilder *builder)
{
    if (real_name_entry_changed != 0)
    {
        g_source_remove (real_name_entry_changed);
        user_settings_user_name_done_timeout (builder);
    }
}



static void
user_settings_user_change_name (GtkBuilder *builder)
{
    g_return_if_fail (GTK_IS_BUILDER (builder));

    /* Leave if widgets are updating */
    if (updating_widgets)
        return;

    /* Abort pending update */
    if (real_name_entry_changed != 0)
        g_source_remove (real_name_entry_changed);

    /* Schedule a new update */
    real_name_entry_changed = g_timeout_add_seconds_full (
        G_PRIORITY_DEFAULT_IDLE, 1,
        user_settings_user_name_done_timeout,
        builder, user_settings_user_name_done_timeout_destroyed);
}



static void
user_settings_user_change_language (GtkWidget  *combobox,
                                    GtkBuilder *builder)
{
    GtkTreeIter   iter;
    GtkTreeModel *model;
    gchar        *code;
    ActUser      *user;

    g_return_if_fail (GTK_IS_BUILDER (builder));
    g_return_if_fail (GTK_IS_COMBO_BOX (combobox));

    /* Leave if widgets are updating */
    if (updating_widgets)
        return;

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter))
    {
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
        gtk_tree_model_get (model, &iter, USER_LANG_CODE, &code, -1);

        user = user_settings_user_get_selected (builder);
        act_user_set_language (user, code);
        g_free (code);
    }
}



static void
user_settings_user_change_type (GtkWidget  *combobox,
                                GtkBuilder *builder)
{
    gint     account_type;
    ActUser *user;

    g_return_if_fail (GTK_IS_BUILDER (builder));
    g_return_if_fail (GTK_IS_COMBO_BOX (combobox));

    /* Leave if widgets are updating */
    if (updating_widgets)
        return;

    account_type = gtk_combo_box_get_active (GTK_COMBO_BOX (combobox));
    user = user_settings_user_get_selected (builder);
    act_user_set_account_type (user, account_type);
}



static void
user_settings_user_selection_changed (GtkTreeSelection *selection,
                                      GtkBuilder       *builder)
{
    ActUser *user;

    g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    /* Save pending update */
    user_settings_user_change_name_now (builder);

    /* Update widgets */
    user = user_settings_user_get_selected (builder);
    user_settings_user_update (user, builder);
}



static void
user_settings_user_add_check_name (GtkWidget  *entry,
                                   GtkBuilder *builder)
{
    const gchar *name;
    gboolean     is_valid = FALSE;
    GObject     *object;
    gchar       *msg = NULL;

    g_return_if_fail (GTK_IS_ENTRY (entry));
    g_return_if_fail (GTK_IS_BUILDER (builder));

    name = gtk_entry_get_text (GTK_ENTRY (entry));
    if (exo_str_is_empty (name))
        goto check_done;

    /* Check if the name does not exceed the maximum name length */
    if (strlen (name) > UT_NAMESIZE)
    {
        msg = g_strdup_printf (_("Usernames may only be up to %d characters long."), UT_NAMESIZE);
        goto check_done;
    }

    /* Check if the name already exists */
    if (getpwnam (name) != NULL)
    {
        msg = g_strdup_printf (_("A user with the username '%s' already exists."), name);
        goto check_done;
    }

    /* Validate the username based on the useradd rule */
    if (!g_regex_match_simple (VALIDNAME_REGEX, name, 0, 0))
    {
        msg = g_strdup (_("Usernames must start with a lower case letter "
                          "or an underscore, followed by lower case letters, "
                          "digits, underscores, or dashes. They can end with "
                          "a dollar sign."));
        goto check_done;
    }

    /* Yeey */
    is_valid = TRUE;

    check_done:

    object = gtk_builder_get_object (builder, "new-dialog-button");
    g_return_if_fail (GTK_IS_BUTTON (object));
    gtk_widget_set_sensitive (GTK_WIDGET (object), is_valid);

    gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   msg == NULL ? NULL : GTK_STOCK_DIALOG_ERROR);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     msg);
    g_free (msg);
}



static void
user_settings_user_add (GtkWidget  *button,
                        GtkBuilder *builder)
{
    GObject        *dialog;
    GObject        *combo_type;
    GObject        *entry_name;
    GObject        *entry_full;
    GObject        *dialog_button;
    ActUserManager *manager;
    GError         *error = NULL;
    gint            retval;

    g_return_if_fail (GTK_IS_BUILDER (builder));

    dialog = gtk_builder_get_object (builder, "new-dialog");
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
        GTK_WINDOW (gtk_widget_get_toplevel (button)));
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);

    /* Prepare widgets */
    combo_type = gtk_builder_get_object (builder, "new-account-type");
    g_return_if_fail (GTK_IS_COMBO_BOX (combo_type));
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo_type), 0);

    entry_name = gtk_builder_get_object (builder, "new-account-name");
    g_return_if_fail (GTK_IS_ENTRY (entry_name));
    gtk_entry_set_text (GTK_ENTRY (entry_name), "");
    gtk_widget_grab_focus (GTK_WIDGET (entry_name));

    entry_full = gtk_builder_get_object (builder, "new-account-full");
    g_return_if_fail (GTK_IS_ENTRY (entry_full));
    gtk_entry_set_text (GTK_ENTRY (entry_full), "");

    dialog_button = gtk_builder_get_object (builder, "new-dialog-button");
    g_return_if_fail (GTK_IS_BUTTON (dialog_button));
    gtk_widget_set_sensitive (GTK_WIDGET (dialog_button), FALSE);

    retval = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_hide (GTK_WIDGET (dialog));

    if (retval == 1)
    {
        /* Try to create the new user */
        manager = g_object_get_qdata (G_OBJECT (builder), manager_quark);
        if (act_user_manager_create_user (manager,
                                          gtk_entry_get_text (GTK_ENTRY (entry_name)),
                                          gtk_entry_get_text (GTK_ENTRY (entry_full)),
                                          gtk_combo_box_get_active (GTK_COMBO_BOX (combo_type)),
                                          &error))
        {
            /* Todo: select new user */
        }
        else
        {
            xfce_dialog_show_error (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                    error, _("Failed to create new user"));
            g_error_free (error);
        }
    }
}



static void
user_settings_user_delete (GtkWidget  *button,
                           GtkBuilder *builder)
{
    ActUser        *user;
    ActUserManager *manager;
    GtkWidget      *toplevel;
    gchar          *msg;
    gint            retval;
    GError         *error = NULL;

    g_return_if_fail (GTK_IS_BUILDER (builder));

    toplevel = gtk_widget_get_toplevel (button);

    user = user_settings_user_get_selected (builder);
    if (act_user_get_uid (user) == getuid ())
    {
        xfce_message_dialog (GTK_WINDOW (toplevel),
                             _("Failed to delete account"),
                             GTK_STOCK_DIALOG_ERROR,
                             _("You cannot delete your own account"), NULL,
                             GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    }
    else if (act_user_is_logged_in (user))
    {
        msg = g_strdup_printf (_("%s is still logged in"),
                               user_settings_user_real_name (user));
        xfce_message_dialog (GTK_WINDOW (toplevel),
                             _("Failed to delete account"),
                             GTK_STOCK_DIALOG_ERROR, msg,
                             _("Deleting a user while they are logged in can "
                               "leave the system in an inconsistent state."),
                             GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        g_free (msg);
    }
    else
    {
        msg = g_strdup_printf (_("Do you want to keep %s's files?"),
                               user_settings_user_real_name (user));
        retval = xfce_message_dialog (GTK_WINDOW (toplevel),
                                      _("Remove user account"),
                                      GTK_STOCK_DIALOG_QUESTION, msg,
                                      _("It is possible to keep the home directory, "
                                        "mail spool and temporary files around when "
                                        "deleting a user account."),
                                      _("_Delete Files"), GTK_RESPONSE_NO,
                                      _("_Keep Files"), GTK_RESPONSE_YES,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      NULL);
        g_free (msg);

        if (retval == GTK_RESPONSE_NO || retval == GTK_RESPONSE_YES)
        {
            manager = g_object_get_qdata (G_OBJECT (builder), manager_quark);
            if (!act_user_manager_delete_user (manager, user,
                                               retval == GTK_RESPONSE_NO,
                                               &error))
            {
                xfce_dialog_show_error (GTK_WINDOW (toplevel), error,
                                        _("Failed to delete account"));
                g_error_free (error);
            }
        }
    }
}



static void
user_settings_languages_foreach (gpointer key,
                                 gpointer value,
                                 gpointer data)
{
    GtkListStore *store = GTK_LIST_STORE (data);
    const gchar  *code = key;
    const gchar  *name = value;
    GtkTreeIter   iter;

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        USER_LANG_CODE, code,
                        USER_LANG_NAME, name,
                        -1);
}



static void
user_settings_languages (GtkBuilder     *builder,
                         ActUserManager *manager)
{
    GHashTable *langs;
    GObject    *store;

    store = gtk_builder_get_object (builder, "user-lang-model");
    g_return_if_fail (GTK_IS_LIST_STORE (store));

    langs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    g_hash_table_insert (langs, g_strdup ("en_US.utf8"), g_strdup (_("English")));

    g_hash_table_foreach (langs, user_settings_languages_foreach, store);
    g_hash_table_destroy (langs);
}



static void
user_settings_manager_is_loaded (ActUserManager *manager,
                                 GParamSpec     *pspec,
                                 GtkBuilder     *builder)
{
    GSList           *users, *li;
    GtkTreeIter       iter;
    GObject          *object;
    GtkTreeSelection *selection;
    GObject          *model;
    uid_t             current_user;

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

    current_user = getuid ();

    model = gtk_builder_get_object (builder, "users-model");
    g_return_if_fail (GTK_IS_LIST_STORE (model));

    /* Add known languages */
    user_settings_languages (builder, manager);

    /* Add the known users */
    users = act_user_manager_list_users (manager);
    for (li = users; li != NULL; li = li->next)
    {
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        user_settings_user_set_model (li->data, GTK_LIST_STORE (model), &iter);

        /* Select current user */
        if (act_user_get_uid (li->data) == current_user)
        {
            object = gtk_builder_get_object (builder, "users-treeview");
            g_return_if_fail (GTK_IS_TREE_VIEW (object));

            selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
            gtk_tree_selection_select_iter (selection, &iter);
        }
    }
    g_slist_free (users);
}



gint
main (gint argc, gchar **argv)
{
    GObject          *dialog = NULL;
    GObject          *plug_child;
    GObject          *checkbutton;
    GtkWidget        *plug;
    GtkBuilder       *builder;
    GError           *error = NULL;
    ActUserManager   *manager;
    GObject          *object;
    GtkTreeSelection *selection;
    const gchar      *pwd_entries[] = { "pwd-old", "pwd-new", "pwd-verify" };
    guint             i;

    /* Init quark */
    manager_quark = g_quark_from_static_string ("user-manager");

    /* Setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args (&argc, &argv, "", opt_entries, PACKAGE, &error))
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

        object = gtk_builder_get_object (builder, "user-name");
        g_return_val_if_fail (GTK_IS_ENTRY (object), EXIT_FAILURE);
        g_signal_connect_swapped (G_OBJECT (object), "changed",
            G_CALLBACK (user_settings_user_change_name), builder);

        object = gtk_builder_get_object (builder, "user-lang-combo");
        g_return_val_if_fail (GTK_IS_COMBO_BOX (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "changed",
            G_CALLBACK (user_settings_user_change_language), builder);

        object = gtk_builder_get_object (builder, "user-type-combo");
        g_return_val_if_fail (GTK_IS_COMBO_BOX (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "changed",
            G_CALLBACK (user_settings_user_change_type), builder);

        object = gtk_builder_get_object (builder, "user-icon-button");
        g_return_val_if_fail (GTK_IS_BUTTON (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "clicked",
            G_CALLBACK (user_settings_user_icon_clicked), builder);

        object = gtk_builder_get_object (builder, "user-passwd-button");
        g_return_val_if_fail (GTK_IS_BUTTON (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "clicked",
            G_CALLBACK (user_settings_user_passwd_clicked), builder);

        checkbutton = gtk_builder_get_object (builder, "pwd-visible");
        g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (checkbutton), EXIT_FAILURE);
        for (i = 0; i <G_N_ELEMENTS (pwd_entries); i++)
        {
            object = gtk_builder_get_object (builder, pwd_entries[i]);
            g_return_val_if_fail (GTK_IS_ENTRY (object), EXIT_FAILURE);
            exo_binding_new (G_OBJECT (checkbutton), "active",
                             G_OBJECT (object), "visibility");
        }

        object = gtk_builder_get_object (builder, "button-add");
        g_return_val_if_fail (GTK_IS_BUTTON (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "clicked",
            G_CALLBACK (user_settings_user_add), builder);

        object = gtk_builder_get_object (builder, "button-delete");
        g_return_val_if_fail (GTK_IS_BUTTON (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "clicked",
            G_CALLBACK (user_settings_user_delete), builder);

        object = gtk_builder_get_object (builder, "pwd-dialog");
        g_return_val_if_fail (GTK_IS_DIALOG (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "delete-event",
            G_CALLBACK (gtk_widget_hide_on_delete), NULL);

        object = gtk_builder_get_object (builder, "new-dialog");
        g_return_val_if_fail (GTK_IS_DIALOG (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "delete-event",
            G_CALLBACK (gtk_widget_hide_on_delete), NULL);

        object = gtk_builder_get_object (builder, "new-account-name");
        g_return_val_if_fail (GTK_IS_ENTRY (object), EXIT_FAILURE);
        g_signal_connect (G_OBJECT (object), "changed",
            G_CALLBACK (user_settings_user_add_check_name), builder);

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            /* Get the dialog widget */
            dialog = gtk_builder_get_object (builder, "dialog");

            gtk_widget_show (GTK_WIDGET (dialog));
            g_signal_connect (dialog, "response", G_CALLBACK (gtk_main_quit), NULL);

            /* To prevent the settings dialog to be saved in the session */
            gdk_set_sm_client_id ("FAKE ID");

            gtk_main ();
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

        /* Save name if there is still an update pending */
        user_settings_user_change_name_now (builder);

        if (dialog != NULL)
            gtk_widget_destroy (GTK_WIDGET (dialog));
        g_object_unref (G_OBJECT (manager));
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    g_object_unref (G_OBJECT (builder));

    return EXIT_SUCCESS;
}
