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

static GladeXML *gxml_main_window = NULL;

static void
load_channels (GtkListStore *store, GtkTreeView *treeview);
static void
load_properties (XfconfChannel *channel, GtkTreeStore *store, GtkTreeView *treeview);

static void
cb_channel_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, gpointer user_data);

/* 
 * Xfce 4.6 depends on glib 2.12, 
 * Glib 2.14 comes with g_hash_table_get_keys(), 
 * until then... use the following function with
 * g_hash_table_foreach()
 */
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

GtkDialog *
xfce4_settings_editor_main_window_new()
{
    GtkWidget *dialog;
    GtkWidget *channel_treeview;
    GtkWidget *property_treeview;
    GtkListStore *channel_list_store;
    GtkTreeStore *property_tree_store;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;

    if (!gxml_main_window)
    {
        gxml_main_window = glade_xml_new_from_buffer (xfce4_settings_editor_glade, xfce4_settings_editor_glade_length, NULL, NULL);
    }

    dialog = glade_xml_get_widget (gxml_main_window, "settings_editor_dialog");
    channel_treeview = glade_xml_get_widget (gxml_main_window, "channel_treeview");
    property_treeview = glade_xml_get_widget (gxml_main_window, "property_treeview");

    /*
     * Channel List
     */
    channel_list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

    gtk_tree_view_set_model (GTK_TREE_VIEW (channel_treeview), GTK_TREE_MODEL (channel_list_store));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 0, NULL, renderer, "icon-name", 0, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 1, N_("Channel"), renderer, "text", 1, NULL);

    /* 
     * property list
     */
    property_tree_store = gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

    gtk_tree_view_set_model (GTK_TREE_VIEW (property_treeview), GTK_TREE_MODEL (property_tree_store));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 0, N_("Property"), renderer, "text", 0, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 1, N_("Type"), renderer, "text", 1, NULL);

    renderer = gtk_cell_renderer_toggle_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 2, N_("Locked"), renderer, "active", 2, NULL);

    /* improve usability by expanding nodes when clicking on them */
    g_signal_connect (G_OBJECT (channel_treeview), "row-activated", G_CALLBACK (cb_channel_treeview_row_activated), NULL);

    /* selection handling */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (channel_treeview));
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_channel_treeview_selection_changed), NULL);

    load_channels (channel_list_store, GTK_TREE_VIEW(channel_treeview));

    return GTK_DIALOG(dialog);
}

static void
load_channels (GtkListStore *store, GtkTreeView *treeview)
{
    GtkTreeIter iter;
    GValue value = {0,};

    gchar **channel_names, **_channel_names_iter;

    channel_names = xfconf_list_channels();
    if (channel_names != NULL)
    {
        _channel_names_iter = channel_names;
        while (*_channel_names_iter)
        {
            gtk_list_store_append (store, &iter);
            g_value_init (&value, G_TYPE_STRING);
            g_value_set_string (&value, *_channel_names_iter);
            gtk_list_store_set_value (store, &iter, 1, &value);
            g_value_unset (&value);

            _channel_names_iter++;
        }
        g_strfreev (channel_names);
    }
}

static void
load_properties (XfconfChannel *channel, GtkTreeStore *store, GtkTreeView *treeview)
{
    gint i = 0;
    gchar *key;
    GValue *value;
    GList *keys, *_keys;
    GtkTreeIter parent_iter;
    GtkTreeIter child_iter;
    GValue parent_val = {0,};
    GValue child_value = {0,};

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
            gchar **components = g_strsplit (key, "/", 0);

            /* components[0] will be empty because properties start with '/'*/
            for (i = 1; components[i]; ++i)
            {
                /* Check if this parent has children */
                if (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child_iter, i==1?NULL:&parent_iter))
                {
                    while (1)
                    {
                        /* Check if the component already exists, if so, return this child */
                        gtk_tree_model_get_value (GTK_TREE_MODEL(store), &child_iter, 0, &parent_val);
                        if (!strcmp (components[i], g_value_get_string (&parent_val)))
                        {
                            g_value_unset (&parent_val);
                            break;
                        }
                        else
                            g_value_unset (&parent_val);

                        /* If we are at the end of the list of children, the required child is not available and should be created */
                        if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &child_iter))
                        {
                            gtk_tree_store_append (store, &child_iter, i==1?NULL:&parent_iter);
                            g_value_init (&child_value, G_TYPE_STRING);
                            g_value_set_string (&child_value, components[i]);
                            gtk_tree_store_set_value (store, &child_iter, 0, &child_value);
                            g_value_unset (&child_value);
                            break;
                        }
                    }
                }
                else
                {
                    /* If the parent does not have any children, create this one */
                    gtk_tree_store_append (store, &child_iter, i==1?NULL:&parent_iter);
                    g_value_init (&child_value, G_TYPE_STRING);
                    g_value_set_string (&child_value, components[i]);
                    gtk_tree_store_set_value (store, &child_iter, 0, &child_value);
                    g_value_unset (&child_value);
                }
                parent_iter = child_iter;
            }

            g_strfreev (components);

        }
    }
}

static void
cb_channel_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
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

static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    XfconfChannel *channel;
    GtkWidget *property_treeview;
    GtkTreeModel *tree_store = NULL;
    GValue value = {0, };

    if (! gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

    property_treeview = glade_xml_get_widget (gxml_main_window, "property_treeview");
    tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));

    gtk_tree_model_get_value (model, &iter, 1, &value);

    g_return_if_fail (G_VALUE_HOLDS_STRING (&value));

    channel = xfconf_channel_new (g_value_get_string (&value));

    gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
    load_properties (channel, GTK_TREE_STORE(tree_store), GTK_TREE_VIEW(property_treeview));
}
