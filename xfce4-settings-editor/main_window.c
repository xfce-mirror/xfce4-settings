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

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfce4-settings-editor_ui.h"
#include "main_window.h"

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 380
#define HPANED_POSITION 200

static GtkBuilder *builder = NULL;
static XfconfChannel *current_channel = NULL;
static gchar *current_property = NULL;

enum {
    PROP_TYPE_EMPTY,
    PROP_TYPE_STRING,
    PROP_TYPE_INT,
    PROP_TYPE_UINT,
    PROP_TYPE_INT64,
    PROP_TYPE_UINT64,
    PROP_TYPE_DOUBLE,
    PROP_TYPE_BOOLEAN,
    PROP_TYPE_ARRAY /* Not used yet */
};

static void
load_channels (GtkListStore *store, GtkTreeView *treeview);
static void
load_properties (XfconfChannel *channel, GtkTreeStore *store, GtkTreeView *treeview);

static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, GtkBuilder *builder);
static void
cb_property_treeview_selection_changed (GtkTreeSelection *selection, GtkBuilder *builder);
static void
cb_property_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);

static void
cb_property_new_button_clicked (GtkButton *button, gpointer user_data);
static void
cb_property_edit_button_clicked (GtkButton *button, gpointer user_data);
static void
cb_property_revert_button_clicked (GtkButton *button, gpointer user_data);
static void
xfce_settings_editor_dialog_response (GtkWidget *dialog, gint response, gpointer user_data);



GtkDialog *
xfce4_settings_editor_main_window_new(void)
{
    gint width, height, position;
    GObject *dialog;
    GObject *channel_treeview;
    GObject *property_treeview;
    GObject *hpaned;
    XfconfChannel *channel;
    GtkListStore *channel_list_store;
    GtkTreeStore *property_tree_store;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GObject *property_edit_button, *property_new_button, *property_revert_button;

    if (!builder)
    {
        /* hook to make sure the libxfce4ui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
            return NULL;

        builder = gtk_builder_new ();
        gtk_builder_add_from_string (builder, xfce4_settings_editor_ui, xfce4_settings_editor_ui_length, NULL);

        dialog = gtk_builder_get_object (builder, "main_dialog");
        g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, builder);
    }
    else
    {
        dialog = gtk_builder_get_object (builder, "main_dialog");
    }

    hpaned = gtk_builder_get_object (builder, "hpaned2");

    /* Set the default size of the window */
    channel = xfconf_channel_get ("xfce4-settings-editor");
    width = xfconf_channel_get_int (channel, "/window-width", WINDOW_WIDTH);
    height = xfconf_channel_get_int (channel, "/window-height", WINDOW_HEIGHT);
    position = xfconf_channel_get_int (channel, "/hpaned-position", HPANED_POSITION);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width, height);
    gtk_paned_set_position (GTK_PANED (hpaned), position);

    g_signal_connect (dialog, "response", G_CALLBACK (xfce_settings_editor_dialog_response), NULL);

    channel_treeview = gtk_builder_get_object (builder, "channel_treeview");
    property_treeview = gtk_builder_get_object (builder, "property_treeview");

    property_edit_button = gtk_builder_get_object (builder, "property_edit_button");
    property_new_button = gtk_builder_get_object (builder, "property_new_button");
    property_revert_button = gtk_builder_get_object (builder, "property_revert_button");

    gtk_widget_set_sensitive (GTK_WIDGET (property_edit_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (property_revert_button), FALSE);

    /*
     * Channel List
     */
    channel_list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

    gtk_tree_view_set_model (GTK_TREE_VIEW (channel_treeview), GTK_TREE_MODEL (channel_list_store));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 0, NULL, renderer, "icon-name", 0, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 1, _("Channel"), renderer, "text", 1, NULL);

    /*
     * property list
     */
    property_tree_store = gtk_tree_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);

    gtk_tree_view_set_model (GTK_TREE_VIEW (property_treeview), GTK_TREE_MODEL (property_tree_store));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 0, _("Property"), renderer, "text", 0, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 1, _("Type"), renderer, "text", 1, NULL);

    renderer = gtk_cell_renderer_toggle_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 2, _("Locked"), renderer, "active", 2, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (property_treeview), 3, _("Value"), renderer, "text", 3, NULL);

    /* improve usability by expanding nodes when clicking on them */
    g_signal_connect (G_OBJECT (property_treeview), "row-activated", G_CALLBACK (cb_property_treeview_row_activated), NULL);

    /* selection handling */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (channel_treeview));
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_channel_treeview_selection_changed), builder);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (property_treeview));
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_property_treeview_selection_changed), builder);


    /* Connect signal-handlers to toolbar buttons */
    g_signal_connect (G_OBJECT (property_new_button), "clicked", G_CALLBACK (cb_property_new_button_clicked), property_treeview);
    g_signal_connect (G_OBJECT (property_edit_button), "clicked", G_CALLBACK (cb_property_edit_button_clicked), property_treeview);
    g_signal_connect (G_OBJECT (property_revert_button), "clicked", G_CALLBACK (cb_property_revert_button_clicked), property_treeview);

    load_channels (channel_list_store, GTK_TREE_VIEW(channel_treeview));

    return GTK_DIALOG(dialog);
}

/**
 * load_channels
 *
 * get the available channels from xfconf and put them in the treemodel
 */
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

/**
 * load_properties
 *
 * get the available properties from xfconf and put them in the treemodel
 */
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
    GValue property_value= {0,};
    GValue child_name = {0,};
    GValue child_type = {0,};
    GValue child_locked = {0,};

    GHashTable *hash_table;
    gchar **components;

    g_value_init (&child_name, G_TYPE_STRING);
    g_value_init (&child_locked, G_TYPE_BOOLEAN);
    g_value_init (&child_type, G_TYPE_STRING);
    g_value_init (&child_value, G_TYPE_STRING);

    hash_table = xfconf_channel_get_properties (channel, NULL);

    if (hash_table != NULL)
    {
        keys = g_hash_table_get_keys (hash_table);
        for(_keys = keys; _keys != NULL; _keys = g_list_next (_keys))
        {
            key = _keys->data;
            value = g_hash_table_lookup (hash_table, key);
            components = g_strsplit (key, "/", 0);

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
                            g_value_set_string (&child_name, components[i]);
                            gtk_tree_store_set_value (store, &child_iter, 0, &child_name);
                            g_value_reset (&child_name);

                            if (components[i+1] == NULL)
                            {
                                xfconf_channel_get_property (channel, key, &property_value);
                                switch (G_VALUE_TYPE(&property_value))
                                {
                                    case G_TYPE_INT:
                                        g_value_set_string (&child_type, "Int");
                                        g_value_transform (&property_value, &child_value);
                                        break;
                                    case G_TYPE_UINT:
                                        g_value_set_string (&child_type, "Unsigned Int");
                                        g_value_transform (&property_value, &child_value);
                                        break;
                                    case G_TYPE_INT64:
                                        g_value_set_string (&child_type, "Int64");
                                        g_value_transform (&property_value, &child_value);
                                        break;
                                    case G_TYPE_UINT64:
                                        g_value_set_string (&child_type, "Unsigned Int64");
                                        g_value_transform (&property_value, &child_value);
                                        break;
                                    case G_TYPE_DOUBLE:
                                        g_value_set_string (&child_type, "Double");
                                        g_value_transform (&property_value, &child_value);
                                        break;
                                    case G_TYPE_STRING:
                                        g_value_set_string (&child_type, "String");
                                        g_value_copy (&property_value, &child_value);
                                        break;
                                    case G_TYPE_BOOLEAN:
                                        g_value_set_string (&child_type, "Bool");
                                        g_value_transform (&property_value, &child_value);
                                        break;
                                    default:
                                        g_value_set_string (&child_type, g_type_name (G_VALUE_TYPE(&property_value)));
                                        break;
                                }
                                g_value_unset (&property_value);
                            }
                            else
                            {
                                g_value_set_string (&child_type, _("Empty"));
                            }
                            gtk_tree_store_set_value (store, &child_iter, 1, &child_type);
                            g_value_reset (&child_type);

                            g_value_set_boolean (&child_locked, xfconf_channel_is_property_locked (channel, key));
                            gtk_tree_store_set_value (store, &child_iter, 2, &child_locked);
                            g_value_reset (&child_locked);

                            gtk_tree_store_set_value (store, &child_iter, 3, &child_value);
                            g_value_reset (&child_value);
                            break;
                        }
                    }
                }
                else
                {
                    /* If the parent does not have any children, create this one */
                    gtk_tree_store_append (store, &child_iter, i==1?NULL:&parent_iter);
                    g_value_set_string (&child_name, components[i]);
                    gtk_tree_store_set_value (store, &child_iter, 0, &child_name);
                    g_value_reset (&child_name);

                    if (components[i+1] == NULL)
                    {
                        xfconf_channel_get_property (channel, key, &property_value);
                        switch (G_VALUE_TYPE(&property_value))
                        {
                            case G_TYPE_INT:
                                g_value_set_string (&child_type, "Int");
                                g_value_transform (&property_value, &child_value);
                                break;
                            case G_TYPE_UINT:
                                g_value_set_string (&child_type, "Unsigned Int");
                                g_value_transform (&property_value, &child_value);
                                break;
                            case G_TYPE_INT64:
                                g_value_set_string (&child_type, "Int64");
                                g_value_transform (&property_value, &child_value);
                                break;
                            case G_TYPE_UINT64:
                                g_value_set_string (&child_type, "Unsigned Int64");
                                g_value_transform (&property_value, &child_value);
                                break;
                            case G_TYPE_DOUBLE:
                                g_value_set_string (&child_type, "Double");
                                g_value_transform (&property_value, &child_value);
                                break;
                            case G_TYPE_STRING:
                                g_value_set_string (&child_type, "String");
                                g_value_copy (&property_value, &child_value);
                                break;
                            case G_TYPE_BOOLEAN:
                                g_value_set_string (&child_type, "Bool");
                                g_value_transform (&property_value, &child_value);
                                break;
                            default:
                                g_value_set_string (&child_type, g_type_name (G_VALUE_TYPE(&property_value)));
                                g_value_set_string (&child_value, "...");
                                break;
                        }
                        g_value_unset (&property_value);
                    }
                    else
                    {
                        g_value_set_string (&child_type, _("Empty"));
                    }
                    gtk_tree_store_set_value (store, &child_iter, 1, &child_type);
                    g_value_reset (&child_type);

                    g_value_set_boolean (&child_locked, xfconf_channel_is_property_locked (channel, key));
                    gtk_tree_store_set_value (store, &child_iter, 2, &child_locked);
                    g_value_reset (&child_locked);

                    gtk_tree_store_set_value (store, &child_iter, 3, &child_value);
                    g_value_reset (&child_value);
                }
                parent_iter = child_iter;
            }

            g_strfreev (components);

        }
    }
}

static void
cb_property_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
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
    else
        cb_property_edit_button_clicked (NULL, NULL);
}

static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, GtkBuilder *builder)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    XfconfChannel *channel;
    GObject *property_treeview;
    GObject *property_edit_button;
    GObject *property_revert_button;
    GtkTreeModel *tree_store = NULL;
    GValue value = {0, };

    property_edit_button = gtk_builder_get_object (builder, "property_edit_button");
    property_revert_button = gtk_builder_get_object (builder, "property_revert_button");

    if (current_channel)
    {
        g_object_unref (G_OBJECT(current_channel));
        current_channel = NULL;
    }



    if (! gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

    property_treeview = gtk_builder_get_object (builder, "property_treeview");
    tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));

    gtk_tree_model_get_value (model, &iter, 1, &value);

    g_return_if_fail (G_VALUE_HOLDS_STRING (&value));

    channel = xfconf_channel_new (g_value_get_string (&value));

    gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
    load_properties (channel, GTK_TREE_STORE(tree_store), GTK_TREE_VIEW(property_treeview));

    current_channel = channel;
}

static void
cb_property_treeview_selection_changed (GtkTreeSelection *selection, GtkBuilder *builder)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeIter p_iter;
    gboolean locked;
    GObject *property_edit_button;
    GObject *property_revert_button;
    GValue value = {0, };
    gchar *prop_name = NULL;
    gchar *temp = NULL;

    property_edit_button = gtk_builder_get_object (builder, "property_edit_button");
    property_revert_button = gtk_builder_get_object (builder, "property_revert_button");

    if (current_property)
    {
        g_free (prop_name);
        current_property = NULL;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (property_edit_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (property_revert_button), FALSE);

    /* return if no property is selected */
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (property_edit_button), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (property_revert_button), FALSE);

        return;
    }

    /* create the complete property-name */
    gtk_tree_model_get_value (model, &iter, 0, &value);
    prop_name = g_strconcat ("/", g_value_get_string(&value), NULL);
    g_value_unset (&value);

    /* to create the complete property-name, we need it's parents too */
    while (gtk_tree_model_iter_parent (model, &p_iter, &iter))
    {
        gtk_tree_model_get_value (model, &p_iter, 0, &value);
        temp = g_strconcat ("/", g_value_get_string(&value), prop_name, NULL);
        g_value_unset (&value);

        if (prop_name)
            g_free (prop_name);
        prop_name = temp;

        iter = p_iter;
    }

    current_property = prop_name;

    /* Set the state of the edit and reset buttons */
    locked = xfconf_channel_is_property_locked (current_channel, current_property);

    gtk_widget_set_sensitive (GTK_WIDGET (property_edit_button), !locked);
    gtk_widget_set_sensitive (GTK_WIDGET (property_revert_button), !locked);
}

static void
cb_property_new_button_clicked (GtkButton *button, gpointer user_data)
{

}

static void
cb_property_edit_button_clicked (GtkButton *button, gpointer user_data)
{
    GValue value = {0, };
    gchar *prop_name = NULL;

    GObject *property_treeview = gtk_builder_get_object (builder, "property_treeview");
    GtkTreeModel *tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (property_treeview));
    GObject *dialog = gtk_builder_get_object (builder, "edit_settings_dialog");
    GObject *prop_name_entry = gtk_builder_get_object (builder, "property_name_entry");
    GObject *prop_type_combo = gtk_builder_get_object (builder, "property_type_combo");

    GObject *prop_value_text_entry = gtk_builder_get_object (builder, "property_value_text_entry");
    GObject *prop_value_spin_button = gtk_builder_get_object (builder, "property_value_spin_button");
    GObject *prop_value_sw = gtk_builder_get_object (builder, "property_value_sw");
    GObject *prop_value_checkbox = gtk_builder_get_object (builder, "property_value_checkbutton");

    /* Set the correct properties in the ui */
    gtk_entry_set_text (GTK_ENTRY(prop_name_entry), current_property);
    if (xfconf_channel_get_property (current_channel, current_property, &value))
    {
        switch (G_VALUE_TYPE(&value))
        {
            case G_TYPE_STRING:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_STRING);
                gtk_widget_hide (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_show (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
                gtk_entry_set_text (GTK_ENTRY (prop_value_text_entry), g_value_get_string (&value));
                break;
            case G_TYPE_INT:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_INT);
                gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (prop_value_spin_button), g_value_get_int (&value));
                gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), G_MININT, G_MAXINT);
                gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
                break;
            case G_TYPE_UINT:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_UINT);
                gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (prop_value_spin_button), g_value_get_uint (&value));
                gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), 0, G_MAXINT);
                gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
                break;
            case G_TYPE_INT64:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_INT64);
                gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (prop_value_spin_button), g_value_get_int64 (&value));
                gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), G_MININT64, G_MAXINT64);
                gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
                break;
            case G_TYPE_UINT64:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_UINT64);
                gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (prop_value_spin_button), g_value_get_uint64 (&value));
                gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), 0, G_MAXUINT64);
                gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
                break;
            case G_TYPE_DOUBLE:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_DOUBLE);
                gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (prop_value_spin_button), g_value_get_double (&value));
                gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), G_MINDOUBLE, G_MAXDOUBLE);
                gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 2);
                break;
            case G_TYPE_BOOLEAN:
                gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_BOOLEAN);
                gtk_widget_hide (GTK_WIDGET (prop_value_spin_button));
                gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
                gtk_widget_hide (GTK_WIDGET (prop_value_sw));
                gtk_widget_show (GTK_WIDGET (prop_value_checkbox));
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prop_value_checkbox), g_value_get_boolean (&value));
                break;
            default:
                return;
                break;
        }
    }
    else
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_EMPTY);
    }

    if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY)
    {
        GtkTreeIter iter;
        GValue child_value = {0, };

        gtk_widget_hide (GTK_WIDGET (dialog));
        switch (gtk_combo_box_get_active (GTK_COMBO_BOX (prop_type_combo)))
        {
            case PROP_TYPE_EMPTY:
                break;
            case PROP_TYPE_STRING:
                g_value_set_string (&value, gtk_entry_get_text (GTK_ENTRY (prop_value_text_entry)));
                break;
            case PROP_TYPE_INT:
                g_value_set_int (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_UINT:
                g_value_set_uint (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_INT64:
                g_value_set_int64 (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_UINT64:
                g_value_set_uint64 (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_DOUBLE:
                g_value_set_double (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_BOOLEAN:
                g_value_set_boolean (&value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prop_value_checkbox)));
                break;
            case PROP_TYPE_ARRAY:
                break;
        }
        xfconf_channel_set_property (current_channel, current_property, &value);

        /* Update the tree model so that the view is updated */
        gtk_tree_selection_get_selected (selection, &tree_store, &iter);
        g_value_init (&child_value, G_TYPE_STRING);
        g_value_transform (&value, &child_value);
        gtk_tree_store_set_value (GTK_TREE_STORE(tree_store), &iter, 3, &child_value);

        /* Cleanup */
        g_value_unset (&value);
        g_value_reset (&child_value);
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (dialog));
    }

    if (prop_name)
        g_free (prop_name);
}

/**
 * cb_property_revert_button_clicked
 *
 * Resets a property to it's system-default, it removes the property if it does not exist as a system default.
 */
static void
cb_property_revert_button_clicked (GtkButton *button, gpointer user_data)
{
    GtkWidget *dialog;
    GObject *property_treeview;
    GtkTreeModel *tree_store = NULL;

    dialog = gtk_message_dialog_new_with_markup (
                                 GTK_WINDOW (gtk_builder_get_object (builder, "main_window")),
                                 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
                                 _("Are you sure you want to reset property \"<b>%s</b>\"?"),
                                 current_property);

    if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
    {
        property_treeview = gtk_builder_get_object (builder, "property_treeview");
        tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));
        gtk_widget_hide (dialog);
        xfconf_channel_reset_property (current_channel, current_property, FALSE);
        gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
        load_properties (current_channel, GTK_TREE_STORE (tree_store), GTK_TREE_VIEW (property_treeview));
    }

    gtk_widget_destroy (dialog);
}

static void
xfce_settings_editor_dialog_response (GtkWidget *dialog, gint response, gpointer user_data)
{
    XfconfChannel *channel;
    gint width, height;
    GObject *hpaned;

    hpaned = gtk_builder_get_object (builder, "hpaned2");

    channel = xfconf_channel_get ("xfce4-settings-editor");
    gtk_window_get_size (GTK_WINDOW (dialog), &width, &height);
    xfconf_channel_set_int (channel, "/window-width", width);
    xfconf_channel_set_int (channel, "/window-height", height);
    xfconf_channel_set_int (channel, "/hpaned-position", gtk_paned_get_position (GTK_PANED (hpaned)));

    gtk_widget_destroy (dialog);
}
