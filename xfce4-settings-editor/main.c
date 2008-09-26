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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfce4-settings-editor_glade.h"

static void
cb_channel_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GtkTreeView *property_treeview);
static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, GtkTreeView *property_treeview);


static GtkDialog *xfce4_settings_editor_init_dialog (GladeXML *gxml);

/* option entries */
static gboolean opt_version = FALSE;

static GOptionEntry option_entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

gint
main(gint argc, gchar **argv)
{
    GladeXML       *gxml;
    GtkDialog      *dialog;
    GError         *error = NULL;
    gint            result = 0;

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

    gxml = glade_xml_new_from_buffer (xfce4_settings_editor_glade, xfce4_settings_editor_glade_length, NULL, NULL);

    dialog = xfce4_settings_editor_init_dialog (gxml);

    while ((result != GTK_RESPONSE_CLOSE) && (result != GTK_RESPONSE_DELETE_EVENT) && (result != GTK_RESPONSE_NONE))
    {
        result = gtk_dialog_run (dialog);
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}

#if !GLIB_CHECK_VERSION (2,14,0)
static void
xfce4_settings_get_list_keys_foreach (gpointer key,
                                    gpointer value,
                                    gpointer user_data)
{
    GList **keys = user_data;
    *keys = g_list_prepend (*keys, key);
}
#endif

static void
check_properties (GtkTreeStore *tree_store, GtkTreeView *tree_view, GtkTreePath *path, XfconfChannel *channel)
{
    GValue parent_val = {0,};
    GValue child_value = {0,};
    GList *keys, *_keys;
    const gchar *key;
    const GValue *value;
    GtkTreeIter child_iter;
    GtkTreeIter parent_iter;
    gint i = 0;

    GHashTable *hash_table = xfconf_channel_get_properties (channel, NULL);
    if (hash_table != NULL)
    {
        keys = NULL;
#if !GLIB_CHECK_VERSION (2,14,0)
        g_hash_table_foreach (hash_table, xfce4_settings_get_list_keys_foreach, &keys);
#else
        keys = g_hash_table_get_keys (hash_table);
#endif    
        for(_keys = keys; _keys != NULL; _keys = g_list_next (_keys))
        {
            key = _keys->data;
            value = g_hash_table_lookup (hash_table, key);
            gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_store), &parent_iter, path);
            gchar **components = g_strsplit (key, "/", 0);
            for (i = 1; components[i] && components[i+1]; ++i)
            {
                /* Check if this parent has children */
                if (gtk_tree_model_iter_children (GTK_TREE_MODEL (tree_store), &child_iter, &parent_iter))
                {
                    while (1)
                    {
                        /* Check if the component already exists, if so, return this child */
                        gtk_tree_model_get_value (GTK_TREE_MODEL(tree_store), &child_iter, 0, &parent_val);
                        if (!strcmp (components[i], g_value_get_string (&parent_val)))
                        {
                            g_value_unset (&parent_val);
                            break;
                        }
                        else
                            g_value_unset (&parent_val);

                        /* If we are at the end of the list of children, the required child is not available and should be created */
                        if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (tree_store), &child_iter))
                        {
                            gtk_tree_store_append (tree_store, &child_iter, &parent_iter);
                            g_value_init (&child_value, G_TYPE_STRING);
                            g_value_set_string (&child_value, components[i]);
                            gtk_tree_store_set_value (tree_store, &child_iter, 0, &child_value);
                            g_value_unset (&child_value);
                            break;
                        }
                    }
                }
                else
                {
                    /* If the parent does not have any children, create this one */
                    gtk_tree_store_append (tree_store, &child_iter, &parent_iter);
                    g_value_init (&child_value, G_TYPE_STRING);
                    g_value_set_string (&child_value, components[i]);
                    gtk_tree_store_set_value (tree_store, &child_iter, 0, &child_value);
                    g_value_unset (&child_value);
                }
                parent_iter = child_iter;
            }

            g_strfreev (components);
        }
        g_list_free(keys);
    }
}

static void
check_channel (GtkTreeStore *tree_store, GtkTreeView *tree_view, const gchar *channel_name)
{
    GtkTreeIter iter;
    GValue value = {0,};
    XfconfChannel *channel = NULL;

    gtk_tree_store_append (tree_store, &iter, NULL);

    channel = xfconf_channel_new (channel_name);
    
    check_properties (tree_store, tree_view, gtk_tree_model_get_path (GTK_TREE_MODEL (tree_store), &iter), channel);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, channel_name);
    gtk_tree_store_set_value (tree_store, &iter, 0, &value);
    g_value_unset (&value);
}

static GtkDialog *
xfce4_settings_editor_init_dialog (GladeXML *gxml)
{
    GtkCellRenderer *renderer;
    GtkTreeStore *tree_store;
    GtkListStore *list_store;
    gchar **channels, **_channels_iter;

    GtkWidget *dialog = glade_xml_get_widget (gxml, "settings_editor_dialog");
    GtkWidget *channel_treeview = glade_xml_get_widget (gxml, "channel_treeview");
    GtkWidget *property_treeview = glade_xml_get_widget (gxml, "property_treeview");
    GtkTreeSelection *selection;

    tree_store = gtk_tree_store_new (1, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (channel_treeview), GTK_TREE_MODEL (tree_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 0, N_("Channel"), renderer, "text", 0, NULL);

    channels = xfconf_list_channels();
    if (channels != NULL)
    {
        _channels_iter = channels;
        while (*_channels_iter)
        {
            check_channel (tree_store, GTK_TREE_VIEW(channel_treeview), (*_channels_iter));
            _channels_iter++;
        }
        g_strfreev (channels);
    }

    list_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (property_treeview), GTK_TREE_MODEL (list_store));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 0, N_("Property"), renderer, "text", 0, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 1, N_("Type"), renderer, "text", 1, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 2, N_("Value"), renderer, "text", 2, NULL);

    g_signal_connect (G_OBJECT (channel_treeview), "row-activated", G_CALLBACK (cb_channel_treeview_row_activated), property_treeview);

    /* selection handling */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (channel_treeview));
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_channel_treeview_selection_changed), property_treeview);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    gtk_widget_show_all(GTK_WIDGET(GTK_DIALOG(dialog)->vbox));

    return GTK_DIALOG(dialog);
}

static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, GtkTreeView *property_treeview)
{
    GHashTable *hash_table = NULL;
    XfconfChannel *channel = NULL;
    GList *keys, *_keys;
    const gchar *key;
    const GValue *hash_value;
    GValue value = {0, };
    GValue name_value = {0, };
    GValue type_value = {0, };
    GValue val_value = {0, };
    GtkTreeView *tree_view;
    GtkTreeModel *model;
    GtkTreeModel *property_model = gtk_tree_view_get_model (property_treeview);
    GtkTreeIter iter, parent_iter;
    GtkTreePath *path;
    gchar *temp, *prop_name = NULL;
    gchar *str_val = NULL;
    gint path_depth;

    if (! gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

    g_value_init (&name_value, G_TYPE_STRING);
    g_value_init (&val_value, G_TYPE_STRING);
    g_value_init (&type_value, G_TYPE_STRING);

    gtk_list_store_clear (GTK_LIST_STORE (property_model));
    tree_view = gtk_tree_selection_get_tree_view (selection);
    path = gtk_tree_model_get_path (model, &iter);
    path_depth = gtk_tree_path_get_depth (path);

    /* If it is not the toplevel path (eg, channel-name), set the prop-name
     * otherwise, leave it at NULL
     */
    if (gtk_tree_path_get_depth(path) > 1)
    {
        gtk_tree_model_get_value (model, &iter, 0, &value);
        prop_name = g_strconcat ("/", g_value_get_string (&value), NULL);
        g_value_unset (&value);

        /* Traverse the path upwards */
        while (gtk_tree_path_up (path))
        {
            /**
             * Don't prepend the channel-name, break out of this loop instead.
             */
            if (gtk_tree_path_get_depth(path) <= 1)
                break;
            gtk_tree_model_get_iter (model, &parent_iter, path);
            gtk_tree_model_get_value (model, &parent_iter, 0, &value);
            temp = g_strconcat ("/", g_value_get_string (&value), prop_name, NULL);
            g_value_unset (&value);


            if (prop_name)
                g_free (prop_name);
            prop_name = temp;
        }
    }


    /** the path variable should be set to the root-node, containing the channel-name ... if everything above went well */
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get_value (model, &iter, 0, &value);
    channel = xfconf_channel_new (g_value_get_string(&value));
    hash_table = xfconf_channel_get_properties (channel, prop_name);
    g_value_unset (&value);

    if (hash_table)
    {
        keys = NULL;
#if !GLIB_CHECK_VERSION (2,14,0)
        g_hash_table_foreach (hash_table, xfce4_settings_get_list_keys_foreach, &keys);
#else
        keys = g_hash_table_get_keys (hash_table);
#endif    
        for(_keys = keys; _keys != NULL; _keys = g_list_next (_keys))
        {
            key = _keys->data;
            hash_value = g_hash_table_lookup (hash_table, key);
            gchar **components = g_strsplit (key, "/", 0);
            if (components[path_depth] != NULL)
            {
                if (components[path_depth+1] == NULL)
                {
                    gtk_list_store_append (GTK_LIST_STORE (property_model), &iter);
                    g_value_set_string (&name_value, components[path_depth]);
                    gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 0, &name_value);
                    g_value_reset (&name_value);
                    switch (G_VALUE_TYPE (hash_value))
                    {
                        case G_TYPE_STRING:
                            g_value_set_string (&type_value, "String");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &type_value);
                            g_value_copy (hash_value, &val_value);
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &val_value);
                            g_value_reset (&type_value);
                            g_value_reset (&val_value);
                            break;
                        case G_TYPE_INT:
                            str_val = g_strdup_printf ("%d", g_value_get_int (hash_value));
                            g_value_set_string (&type_value, "Int");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &type_value);
                            g_value_set_string (&val_value, str_val);
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &val_value);
                            g_value_reset (&type_value);
                            g_value_reset (&val_value);
                            g_free (str_val);
                            str_val = NULL;
                            break;
                        case G_TYPE_UINT:
                            str_val = g_strdup_printf ("%u", g_value_get_uint (hash_value));
                            g_value_set_string (&type_value, "Int");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &type_value);
                            g_value_set_string (&val_value, str_val);
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &val_value);
                            g_value_reset (&type_value);
                            g_value_reset (&val_value);
                            g_free (str_val);
                            str_val = NULL;
                            break;
                        case G_TYPE_DOUBLE:
                            str_val = g_strdup_printf ("%f", g_value_get_double (hash_value));
                            g_value_set_string (&type_value, "Double");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &type_value);
                            g_value_set_string (&val_value, str_val);
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &val_value);
                            g_value_reset (&type_value);
                            g_value_reset (&val_value);
                            g_free (str_val);
                            str_val = NULL;
                            break;
                        case G_TYPE_BOOLEAN:
                            str_val = g_strdup_printf ("%s", g_value_get_boolean (hash_value)==TRUE?"true":"false");
                            g_value_set_string (&type_value, "Bool");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &type_value);
                            g_value_set_string (&val_value, str_val);
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &val_value);
                            g_value_reset (&type_value);
                            g_value_reset (&val_value);
                            g_free (str_val);
                            str_val = NULL;
                            break;
                        default:
                            g_value_set_string (&type_value, "Unknown");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &type_value);
                            g_value_set_string (&val_value, "Unknown");
                            gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &val_value);
                            g_value_reset (&type_value);
                            g_value_reset (&val_value);
                            break;
                    }
                }
            }
            g_strfreev (components);
        }
        g_list_free (keys);
    }
    gtk_tree_path_free (path);
}

static void
cb_channel_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GtkTreeView *property_treeview)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (tree_view);
    gtk_tree_model_get_iter (model, &iter, path);

    if (gtk_tree_model_iter_has_child (model, &iter))
    {
        if (gtk_tree_view_row_expanded (tree_view, path))
            gtk_tree_view_collapse_row (tree_view, path);
        else
            gtk_tree_view_expand_row (tree_view, path, FALSE);

    }
}
