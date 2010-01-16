/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2009-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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
#include <gdk/gdkkeysyms.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfce4-settings-editor_ui.h"
#include "main_window.h"

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 380
#define HPANED_POSITION 200

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
cb_property_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GtkBuilder *builder);
static gboolean
cb_property_treeview_key_pressed (GtkWidget *widget, GdkEventKey *event, GtkBuilder *builder);
static void
cb_property_new_button_clicked (GtkButton *button, GtkBuilder *builder);
static void
cb_property_edit_button_clicked (GtkButton *button, GtkBuilder *builder);
static void
cb_property_revert_button_clicked (GtkButton *button, GtkBuilder *builder);
static void
cb_settings_editor_dialog_response (GtkWidget *dialog, gint response, GtkBuilder *builder);
static void
cb_channel_property_changed (XfconfChannel *channel, gchar *property, GValue *value, GtkBuilder *builder);
static gboolean
xfconf_property_is_valid(const gchar *property, GError **error);
static void
channel_treeview_popup_menu (GtkWidget *widget, GdkEventButton *event, GtkBuilder *builder);
static gboolean
cb_channel_treeview_button_press_event (GtkWidget *widget, GdkEventButton *event, GtkBuilder *builder);
static gboolean
cb_channel_treeview_popup_menu (GtkWidget *widget, GtkBuilder *builder);
static void
cb_channel_popup_menu_remove_item_activate (GtkMenuItem *item, GtkBuilder *builder);
void print_list (gpointer data, gpointer user_data);


GtkDialog *
xfce4_settings_editor_main_window_new(void)
{
    gint width, height, position;
    GObject *dialog;
    GObject *channel_treeview;
    GObject *property_treeview;
    GObject *hpaned;
    XfconfChannel *channel;
    GtkBuilder *builder = NULL;
    GtkListStore *channel_list_store;
    GtkTreeStore *property_tree_store;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GObject *property_edit_button, *property_new_button, *property_revert_button;

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return NULL;

    builder = gtk_builder_new ();
    gtk_builder_add_from_string (builder, xfce4_settings_editor_ui, xfce4_settings_editor_ui_length, NULL);

    dialog = gtk_builder_get_object (builder, "main_dialog");
    g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, builder);

    gtk_widget_add_events (GTK_WIDGET (dialog), GDK_KEY_PRESS_MASK);

    hpaned = gtk_builder_get_object (builder, "hpaned2");

    /* Set the default size of the window */
    channel = xfconf_channel_get ("xfce4-settings-editor");
    width = xfconf_channel_get_int (channel, "/window-width", WINDOW_WIDTH);
    height = xfconf_channel_get_int (channel, "/window-height", WINDOW_HEIGHT);
    position = xfconf_channel_get_int (channel, "/hpaned-position", HPANED_POSITION);
    gtk_window_set_default_size (GTK_WINDOW (dialog), width, height);
    gtk_paned_set_position (GTK_PANED (hpaned), position);

    g_signal_connect (dialog, "response", G_CALLBACK (cb_settings_editor_dialog_response), builder);

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
    channel_list_store = gtk_list_store_new (1, G_TYPE_STRING);

    gtk_tree_view_set_model (GTK_TREE_VIEW (channel_treeview), GTK_TREE_MODEL (channel_list_store));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (channel_treeview), 1, _("Channel"), renderer, "text", 0, NULL);

    /* Set sorting */
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (channel_list_store), 0, GTK_SORT_ASCENDING);

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

    /* Allow the user to resize the columns */
    gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (property_treeview), 0), TRUE);
    gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (property_treeview), 1), TRUE);
    gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (property_treeview), 2), TRUE);
    gtk_tree_view_column_set_resizable (gtk_tree_view_get_column (GTK_TREE_VIEW (property_treeview), 3), TRUE);

    /* Set sorting */
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (property_tree_store), 0, GTK_SORT_ASCENDING);

    /* improve usability by expanding nodes when clicking on them */
    g_signal_connect (G_OBJECT (property_treeview), "row-activated", G_CALLBACK (cb_property_treeview_row_activated), builder);

    /* Set a handler for key-press-event */
    g_signal_connect (G_OBJECT (property_treeview), "key-press-event", G_CALLBACK (cb_property_treeview_key_pressed), builder);

    /* selection handling */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (channel_treeview));
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_channel_treeview_selection_changed), builder);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (property_treeview));
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cb_property_treeview_selection_changed), builder);

    /* Connect signal-handlers for the popup-menu and button-press-event events */
    g_signal_connect (G_OBJECT (channel_treeview), "popup-menu", G_CALLBACK (cb_channel_treeview_popup_menu), builder);
    g_signal_connect (G_OBJECT (channel_treeview), "button-press-event", G_CALLBACK (cb_channel_treeview_button_press_event), builder);

    /* Connect signal-handlers to toolbar buttons */
    g_signal_connect (G_OBJECT (property_new_button), "clicked", G_CALLBACK (cb_property_new_button_clicked), builder);
    g_signal_connect (G_OBJECT (property_edit_button), "clicked", G_CALLBACK (cb_property_edit_button_clicked), builder);
    g_signal_connect (G_OBJECT (property_revert_button), "clicked", G_CALLBACK (cb_property_revert_button_clicked), builder);

    load_channels (channel_list_store, GTK_TREE_VIEW (channel_treeview));

    return GTK_DIALOG (dialog);
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
        GtkTreeSelection *selection;

        _channel_names_iter = channel_names;
        while (*_channel_names_iter)
        {
            gtk_list_store_append (store, &iter);
            g_value_init (&value, G_TYPE_STRING);
            g_value_set_string (&value, *_channel_names_iter);
            gtk_list_store_set_value (store, &iter, 0, &value);
            g_value_unset (&value);

            _channel_names_iter++;
        }
        g_strfreev (channel_names);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
          gtk_tree_selection_select_iter (selection, &iter);
    }
}

void print_list (gpointer data, gpointer user_data)
{
  TRACE ("%s", (gchar *) data);
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
        g_list_foreach (keys, (GFunc) print_list, NULL);
        for(_keys = keys; _keys != NULL; _keys = g_list_next (_keys))
        {
            key = _keys->data;
            TRACE ("Key: %s", key);
            value = g_hash_table_lookup (hash_table, key);
            components = g_strsplit (key, "/", 0);

            /* components[0] will be empty because properties start with '/'*/
            for (i = 1; components[i]; ++i)
            {
                TRACE ("Component: %s", components[i]);

                /* Check if this parent has children */
                if (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child_iter, i==1?NULL:&parent_iter))
                {
                    TRACE ("Parent has children");

                    while (1)
                    {
                        /* Check if the component already exists, if so, return this child */
                        gtk_tree_model_get_value (GTK_TREE_MODEL(store), &child_iter, 0, &parent_val);
                        if (!strcmp (components[i], g_value_get_string (&parent_val)))
                        {
                            GValue current_parent_value = {0, };

                            TRACE ("Component already exists");
                            g_value_unset (&parent_val);

                            gtk_tree_model_get_value (GTK_TREE_MODEL(store), &child_iter, 3, &current_parent_value);

                            if (!g_value_get_string (&current_parent_value))
                            {
                                if (components[i+1] == NULL)
                                {
                                    TRACE ("Components i+1 is NULL");

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
                                    TRACE ("Empty property");
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

                            break;
                        }
                        else
                            g_value_unset (&parent_val);

                        /* If we are at the end of the list of children, the required child is not available and should be created */
                        if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &child_iter))
                        {
                            TRACE ("Create the children");
                            gtk_tree_store_append (store, &child_iter, i==1?NULL:&parent_iter);
                            g_value_set_string (&child_name, components[i]);
                            gtk_tree_store_set_value (store, &child_iter, 0, &child_name);
                            g_value_reset (&child_name);

                            if (components[i+1] == NULL)
                            {
                                TRACE ("Components i+1 is NULL");

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
                                TRACE ("Empty property");
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
                    TRACE ("Parent has no children");
                    gtk_tree_store_append (store, &child_iter, i==1?NULL:&parent_iter);
                    g_value_set_string (&child_name, components[i]);
                    gtk_tree_store_set_value (store, &child_iter, 0, &child_name);
                    g_value_reset (&child_name);

                    if (components[i+1] == NULL)
                    {
                        TRACE ("Component i+1 is NULL");
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
                        TRACE ("Empty property");
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

        g_list_free (keys);
        g_hash_table_destroy (hash_table);
    }
}

static void
cb_property_treeview_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, GtkBuilder *builder)
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
        cb_property_edit_button_clicked (NULL, builder);
}

static void
cb_channel_treeview_selection_changed (GtkTreeSelection *selection, GtkBuilder *builder)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    XfconfChannel *channel;
    GObject *property_treeview;
    GObject *property_new_button;
    GtkTreeModel *tree_store = NULL;
    GValue value = {0, };

    property_new_button = gtk_builder_get_object (builder, "property_new_button");

    if (current_channel)
    {
        g_object_unref (G_OBJECT(current_channel));
        current_channel = NULL;
    }

    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (property_new_button), FALSE);
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (property_new_button), TRUE);

    property_treeview = gtk_builder_get_object (builder, "property_treeview");
    tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));

    gtk_tree_model_get_value (model, &iter, 0, &value);

    g_return_if_fail (G_VALUE_HOLDS_STRING (&value));

    channel = xfconf_channel_new (g_value_get_string (&value));

    gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
    load_properties (channel, GTK_TREE_STORE(tree_store), GTK_TREE_VIEW(property_treeview));

    current_channel = channel;

    g_signal_connect (channel, "property-changed", G_CALLBACK (cb_channel_property_changed), builder);
}

static gboolean
cb_channel_treeview_button_press_event (GtkWidget *widget, GdkEventButton *event, GtkBuilder *builder)
{
    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
       channel_treeview_popup_menu (widget, event, builder);

    return FALSE;
}

static gboolean
cb_channel_treeview_popup_menu (GtkWidget *widget, GtkBuilder *builder)
{
  channel_treeview_popup_menu (widget, NULL, builder);
  return TRUE;
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
    const gchar *prop_type;
    gchar *temp = NULL;

    property_edit_button = gtk_builder_get_object (builder, "property_edit_button");
    property_revert_button = gtk_builder_get_object (builder, "property_revert_button");

    if (current_property)
    {
        g_free (prop_name);
        current_property = NULL;
    }

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

    /* If type is Empty, we don't want the user to edit the property */
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get_value (model, &iter, 1, &value);

    prop_type = g_value_get_string (&value);

    if ((g_strcmp0 (prop_type, "Empty") == 0) || g_strcmp0 (prop_type, "GPtrArray_GValue_") == 0)
        gtk_widget_set_sensitive (GTK_WIDGET (property_edit_button), FALSE);
    g_value_unset (&value);
}

static gboolean
cb_property_treeview_key_pressed (GtkWidget *widget, GdkEventKey *event, GtkBuilder *builder)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

  if (event->keyval == GDK_Delete && gtk_tree_selection_get_selected (selection, NULL, NULL))
  {
      cb_property_revert_button_clicked (NULL, builder);
      return TRUE;
  }

  return FALSE;
}

static void
cb_type_combo_changed (GtkComboBox *widget, GtkBuilder *builder)
{
    GObject *prop_value_text_entry = gtk_builder_get_object (builder, "property_value_text_entry");
    GObject *prop_value_spin_button = gtk_builder_get_object (builder, "property_value_spin_button");
    GObject *prop_value_sw = gtk_builder_get_object (builder, "property_value_sw");
    GObject *prop_value_checkbox = gtk_builder_get_object (builder, "property_value_checkbutton");

    switch (gtk_combo_box_get_active (widget))
    {
        case PROP_TYPE_STRING:
            gtk_widget_hide (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_show (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
            break;
        case PROP_TYPE_INT:
            gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), G_MININT, G_MAXINT);
            gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
            break;
        case PROP_TYPE_UINT:
            gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), 0, G_MAXINT);
            gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
            break;
        case PROP_TYPE_INT64:
            gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), G_MININT64, G_MAXINT64);
            gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
            break;
        case PROP_TYPE_UINT64:
            gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), 0, G_MAXUINT64);
            gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
            break;
        case PROP_TYPE_DOUBLE:
            gtk_widget_show (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_hide (GTK_WIDGET (prop_value_checkbox));
            gtk_spin_button_set_range (GTK_SPIN_BUTTON (prop_value_spin_button), G_MINDOUBLE, G_MAXDOUBLE);
            gtk_spin_button_set_digits (GTK_SPIN_BUTTON (prop_value_spin_button), 2);
            break;
        case PROP_TYPE_BOOLEAN:
            gtk_widget_hide (GTK_WIDGET (prop_value_spin_button));
            gtk_widget_hide (GTK_WIDGET (prop_value_text_entry));
            gtk_widget_hide (GTK_WIDGET (prop_value_sw));
            gtk_widget_show (GTK_WIDGET (prop_value_checkbox));
            break;
        default:
            return;
            break;
    }
}

static void
cb_property_new_button_clicked (GtkButton *button, GtkBuilder *builder)
{
    GObject *dialog = gtk_builder_get_object (builder, "edit_settings_dialog");
    GObject *prop_name_entry = gtk_builder_get_object (builder, "property_name_entry");
    GObject *prop_type_combo = gtk_builder_get_object (builder, "property_type_combo");

    GObject *prop_value_text_entry = gtk_builder_get_object (builder, "property_value_text_entry");
    GObject *prop_value_spin_button = gtk_builder_get_object (builder, "property_value_spin_button");
    GObject *prop_value_checkbox = gtk_builder_get_object (builder, "property_value_checkbutton");

    g_signal_connect (prop_type_combo, "changed", G_CALLBACK (cb_type_combo_changed), builder);

    /* Default to string properties */
    gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_STRING);
    gtk_widget_set_sensitive (GTK_WIDGET (prop_type_combo), TRUE);

    /* Reset all the value fields */
    gtk_entry_set_text (GTK_ENTRY (prop_name_entry), "");
    gtk_entry_set_text (GTK_ENTRY (prop_value_text_entry), "");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (prop_value_spin_button), 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prop_value_checkbox), TRUE);

    if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY)
    {
        const gchar *prop_name = gtk_entry_get_text (GTK_ENTRY (prop_name_entry));
        GValue value = {0, };
        GError *error = NULL;
        GObject *property_treeview;
        GtkTreeModel *tree_store;

        gtk_widget_hide (GTK_WIDGET (dialog));

        if (!xfconf_property_is_valid (prop_name, &error))
          {
              GObject *main_window = gtk_builder_get_object (builder, "main_dialog");

              xfce_dialog_show_error (GTK_WINDOW (main_window), error, _("This property name is not valid."));

              g_error_free (error);
              return;
          }

        switch (gtk_combo_box_get_active (GTK_COMBO_BOX (prop_type_combo)))
        {
            case PROP_TYPE_EMPTY:
                break;
            case PROP_TYPE_STRING:
                g_value_init (&value, G_TYPE_STRING);
                g_value_set_string (&value, gtk_entry_get_text (GTK_ENTRY (prop_value_text_entry)));
                break;
            case PROP_TYPE_INT:
                g_value_init (&value, G_TYPE_INT);
                g_value_set_int (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_UINT:
                g_value_init (&value, G_TYPE_UINT);
                g_value_set_uint (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_INT64:
                g_value_init (&value, G_TYPE_INT64);
                g_value_set_int64 (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_UINT64:
                g_value_init (&value, G_TYPE_UINT64);
                g_value_set_uint64 (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_DOUBLE:
                g_value_init (&value, G_TYPE_DOUBLE);
                g_value_set_double (&value, gtk_spin_button_get_value (GTK_SPIN_BUTTON (prop_value_spin_button)));
                break;
            case PROP_TYPE_BOOLEAN:
                g_value_init (&value, G_TYPE_BOOLEAN);
                g_value_set_boolean (&value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prop_value_checkbox)));
                break;
            case PROP_TYPE_ARRAY:
                break;
        }

        xfconf_channel_set_property (current_channel, prop_name, &value);

        /* Refresh the view */
        property_treeview = gtk_builder_get_object (builder, "property_treeview");
        tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));

        gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
        load_properties (current_channel, GTK_TREE_STORE (tree_store), GTK_TREE_VIEW (property_treeview));
    }
    else
      gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
cb_property_edit_button_clicked (GtkButton *button, GtkBuilder *builder)
{
    GValue value = {0, };

    GObject *dialog = gtk_builder_get_object (builder, "edit_settings_dialog");
    GObject *prop_name_entry = gtk_builder_get_object (builder, "property_name_entry");
    GObject *prop_type_combo = gtk_builder_get_object (builder, "property_type_combo");

    GObject *prop_value_text_entry = gtk_builder_get_object (builder, "property_value_text_entry");
    GObject *prop_value_spin_button = gtk_builder_get_object (builder, "property_value_spin_button");
    GObject *prop_value_sw = gtk_builder_get_object (builder, "property_value_sw");
    GObject *prop_value_checkbox = gtk_builder_get_object (builder, "property_value_checkbutton");

    /* Set the correct properties in the ui */
    gtk_entry_set_text (GTK_ENTRY(prop_name_entry), current_property);
    gtk_widget_set_sensitive (GTK_WIDGET (prop_type_combo), FALSE);

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
        gtk_combo_box_set_active (GTK_COMBO_BOX (prop_type_combo), PROP_TYPE_EMPTY);

    if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY)
    {
        gchar *prop_name;
        GError *error = NULL;

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

        prop_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (prop_name_entry)));

        if (!xfconf_property_is_valid (prop_name, &error))
        {
            GObject *main_window = gtk_builder_get_object (builder, "main_dialog");

            xfce_dialog_show_error (GTK_WINDOW (main_window), error, _("This property name is not valid."));

            g_error_free (error);
            g_free (prop_name);

            return;
        }

        if (g_strcmp0 (prop_name, current_property) != 0)
        {
            xfconf_channel_reset_property (current_channel, current_property, FALSE);
            g_free (current_property);
            current_property = prop_name;
        }
        else
            g_free (prop_name);

        xfconf_channel_set_property (current_channel, current_property, &value);
    }
    else
        gtk_widget_hide (GTK_WIDGET (dialog));
}

/**
 * cb_property_revert_button_clicked
 *
 * Resets a property to it's system-default, it removes the property if it does not exist as a system default.
 */
static void
cb_property_revert_button_clicked (GtkButton *button, GtkBuilder *builder)
{
    gboolean response;

    response = xfce_dialog_confirm (GTK_WINDOW (gtk_builder_get_object (builder, "main_window")),
                                    GTK_STOCK_YES,
                                    _("Reset"),
                                    _("Resetting a property will permanently remove those custom settings."),
                                    _("Are you sure you want to reset property \"%s\"?"),
                                    current_property);

    if (response)
        xfconf_channel_reset_property (current_channel, current_property, FALSE);
}

static void
cb_settings_editor_dialog_response (GtkWidget *dialog, gint response, GtkBuilder *builder)
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

static void
cb_channel_property_changed (XfconfChannel *channel, gchar *property, GValue *value, GtkBuilder *builder)
{
  if (g_strcmp0 (property, current_property) == 0)
  {
      GObject *property_treeview;
      GtkTreeModel *tree_store = NULL;

      property_treeview = gtk_builder_get_object (builder, "property_treeview");
      tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));

      gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
      load_properties (current_channel, GTK_TREE_STORE (tree_store), GTK_TREE_VIEW (property_treeview));
  }
}

/* Copied from xfconfd/xfconf-backend.c */
static gboolean
xfconf_property_is_valid(const gchar *property, GError **error)
{
    const gchar *p = property;

    if(!p || *p != '/') {
        if(error) {
            g_set_error(error, XFCONF_ERROR, XFCONF_ERROR_INVALID_PROPERTY,
                        _("Property names must start with a '/' character"));
        }
        return FALSE;
    }

    p++;
    if(!*p) {
        if(error) {
            g_set_error(error, XFCONF_ERROR, XFCONF_ERROR_INVALID_PROPERTY,
                        _("The root element ('/') is not a valid property name"));
        }
        return FALSE;
    }

    while(*p) {
        if(!(*p >= 'A' && *p <= 'Z') && !(*p >= 'a' && *p <= 'z')
           && !(*p >= '0' && *p <= '9')
           && *p != '_' && *p != '-' && *p != '/'
           && !(*p == '<' || *p == '>'))
        {
            if(error) {
                g_set_error(error, XFCONF_ERROR,
                            XFCONF_ERROR_INVALID_PROPERTY,
                            _("Property names can only include the ASCII characters A-Z, a-z, 0-9, '_', '-', '<' and '>', as well as '/' as a separator"));
            }
            return FALSE;
        }

        if('/' == *p && '/' == *(p-1)) {
            if(error) {
                g_set_error(error, XFCONF_ERROR,
                            XFCONF_ERROR_INVALID_PROPERTY,
                            _("Property names cannot have two or more consecutive '/' characters"));
            }
            return FALSE;
        }

        p++;
    }

    if(*(p-1) == '/') {
        if(error) {
            g_set_error(error, XFCONF_ERROR, XFCONF_ERROR_INVALID_PROPERTY,
                        _("Property names cannot end with a '/' character"));
        }
        return FALSE;
    }

    return TRUE;
}

static void
channel_treeview_popup_menu (GtkWidget *widget, GdkEventButton *event, GtkBuilder *builder)
{
    GtkWidget *menu;
    GtkWidget *menu_item;
    GtkWidget *image;
    int button, event_time;

    menu = gtk_menu_new ();
    g_signal_connect (menu, "deactivate", G_CALLBACK (gtk_menu_popdown), NULL);

    menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Reset"));
    image = gtk_image_new_from_stock ("gtk-remove", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (cb_channel_popup_menu_remove_item_activate), builder);
    gtk_widget_set_sensitive (menu_item, !xfconf_channel_is_property_locked (current_channel, "/"));
    gtk_widget_show_all (menu_item);

    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}

static void
cb_channel_popup_menu_remove_item_activate (GtkMenuItem *item, GtkBuilder *builder)
{
    gboolean response;
    gchar *channel_name;

    g_object_get (G_OBJECT (current_channel), "channel-name", &channel_name, NULL);

    response = xfce_dialog_confirm (GTK_WINDOW (gtk_builder_get_object (builder, "main_window")),
                                    GTK_STOCK_YES,
                                    _("Reset"),
                                    _("Resetting a channel will permanently remove those custom settings."),
                                    _("Are you sure you want to reset channel \"%s\" and all its properties?"),
                                    channel_name);

    if (response)
    {
        GObject *channel_treeview;
        GObject *property_treeview;
        GtkTreeModel *list_store = NULL;
        GtkTreeModel *tree_store = NULL;

        channel_treeview = gtk_builder_get_object (builder, "channel_treeview");
        list_store = gtk_tree_view_get_model (GTK_TREE_VIEW (channel_treeview));
        xfconf_channel_reset_property (current_channel, "/", TRUE);

        gtk_list_store_clear (GTK_LIST_STORE(list_store));
        load_channels (GTK_LIST_STORE (list_store), GTK_TREE_VIEW (channel_treeview));

        property_treeview = gtk_builder_get_object (builder, "property_treeview");
        tree_store = gtk_tree_view_get_model (GTK_TREE_VIEW (property_treeview));

        gtk_tree_store_clear (GTK_TREE_STORE(tree_store));
        load_properties (current_channel, GTK_TREE_STORE (tree_store), GTK_TREE_VIEW (property_treeview));
    }

    g_free (channel_name);
}
