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

static void
check_properties (GtkTreeStore *tree_store, GtkTreeView *tree_view, GtkTreePath *path, XfconfChannel *channel)
{
    GValue parent_val = {0,};
    GValue child_value = {0,};
    const gchar *key;
    const GValue *value;
    GHashTableIter hash_iter;
    GtkTreeIter child_iter;
    GtkTreeIter parent_iter;
    gint i = 0;

    GHashTable *hash_table = xfconf_channel_get_properties (channel, NULL);
    if (hash_table != NULL)
    {
        g_hash_table_iter_init (&hash_iter, hash_table);
        while (g_hash_table_iter_next (&hash_iter, (gpointer *)&key, (gpointer *)&value)) 
        {
            gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_store), &parent_iter, path);
            gchar **components = g_strsplit (key, "/", 0);
            for (i = 1; components[i]; ++i)
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
    }
}

static void
check_channel (GtkTreeStore *tree_store, GtkTreeView *tree_view, const gchar *channel_name)
{
    GtkTreeIter iter;
    GtkTreeIter child_iter;
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

    GtkWidget *dialog = glade_xml_get_widget (gxml, "settings_editor_dialog");
    GtkWidget *channel_treeview = glade_xml_get_widget (gxml, "channel_treeview");
    GtkWidget *property_treeview = glade_xml_get_widget (gxml, "property_treeview");

    tree_store = gtk_tree_store_new (1, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (channel_treeview), GTK_TREE_MODEL (tree_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 0, N_("Channel"), renderer, "text", 0, NULL);


    check_channel (tree_store, GTK_TREE_VIEW(channel_treeview), "xfwm4");
    check_channel (tree_store, GTK_TREE_VIEW(channel_treeview), "xsettings");
    check_channel (tree_store, GTK_TREE_VIEW(channel_treeview), "xfce4-desktop");
    check_channel (tree_store, GTK_TREE_VIEW(channel_treeview), "accessx");
    check_channel (tree_store, GTK_TREE_VIEW(channel_treeview), "keyboards");

    list_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (property_treeview), GTK_TREE_MODEL (list_store));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 0, N_("Property"), renderer, "text", 0, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 1, N_("Type"), renderer, "text", 1, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 2, N_("Value"), renderer, "text", 2, NULL);

    g_signal_connect (G_OBJECT (channel_treeview), "row-activated", G_CALLBACK (cb_channel_treeview_row_activated), property_treeview);

    gtk_widget_show_all(GTK_WIDGET(GTK_DIALOG(dialog)->vbox));

    return GTK_DIALOG(dialog);
}

static void
cb_channel_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GtkTreeView *property_treeview)
{
    GtkTreeIter iter;
    GHashTable *hash_table = NULL;
    XfconfChannel *channel = NULL;
    const gchar *key;
    const GValue *hash_value;
    GHashTableIter hash_iter;
    GValue value = {0, };
    GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
    GtkTreeModel *property_model = gtk_tree_view_get_model (property_treeview);
    GtkTreePath *root_path = gtk_tree_path_copy (path);
    gchar *temp, *prop_name = NULL;
    gchar *str_val = NULL;

    gtk_list_store_clear (GTK_LIST_STORE (property_model));

    gint i;

    while (gtk_tree_path_get_depth(root_path) > 1)
    {
        gtk_tree_model_get_iter (model, &iter, root_path);
        gtk_tree_model_get_value (model, &iter, 0, &value);
        temp = g_strconcat ("/", g_value_get_string (&value), prop_name, NULL);
        g_value_unset (&value);

        if (prop_name)
            g_free (prop_name);
        prop_name = temp;
        if (!gtk_tree_path_up (root_path));
            break; /* this should not happen */
    }

    if (gtk_tree_path_get_depth (root_path) == 1)
    {
        gtk_tree_model_get_iter (model, &iter, root_path);
        gtk_tree_model_get_value (model, &iter, 0, &value);
        channel = xfconf_channel_new (g_value_get_string (&value));
        hash_table = xfconf_channel_get_properties (channel, prop_name);
        g_hash_table_iter_init (&hash_iter, hash_table);
        g_value_unset (&value);
        while (g_hash_table_iter_next (&hash_iter, (gpointer *)&key, (gpointer *)&hash_value))
        {
            gchar **components = g_strsplit (key, "/", 0);
            if (components [gtk_tree_path_get_depth (path)+2] == NULL)
            {
                gtk_list_store_append (GTK_LIST_STORE(property_model), &iter);
                g_value_init (&value, G_TYPE_STRING);
                g_value_set_string (&value, components[gtk_tree_path_get_depth(path)+1]);
                gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 0, &value);
                g_value_reset (&value);

                switch (G_VALUE_TYPE (hash_value))
                {
                    case G_TYPE_STRING:
                        g_value_set_string (&value, "String");
                        gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &value);
                        g_value_reset (&value);
                        g_value_set_string (&value, g_value_get_string (hash_value));
                        gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &value);
                        g_value_reset (&value);
                        break;
                    case G_TYPE_INT:
                        str_val = g_strdup_printf ("%d", g_value_get_int (hash_value));
                        g_value_set_string (&value, "Int");
                        gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &value);
                        g_value_reset (&value);
                        g_value_set_string (&value, str_val);
                        gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &value);
                        g_value_reset (&value);
                        g_free (str_val);
                        str_val = NULL;
                        break;
                    case G_TYPE_BOOLEAN:
                        str_val = g_strdup_printf ("%s", g_value_get_boolean (hash_value)==TRUE?"true":"false");
                        g_value_set_string (&value, "Bool");
                        gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 1, &value);
                        g_value_reset (&value);
                        g_value_set_string (&value, str_val);
                        gtk_list_store_set_value (GTK_LIST_STORE (property_model), &iter, 2, &value);
                        g_value_reset (&value);
                        g_free (str_val);
                        str_val = NULL;
                }
                g_value_unset (&value);
            }
            
            g_strfreev (components);
        }
    }
}
