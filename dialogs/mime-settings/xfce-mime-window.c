/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <gio/gdesktopappinfo.h>

#include "xfce-mime-window.h"
#include "xfce-mime-chooser.h"



static void     xfce_mime_window_finalize          (GObject              *object);
static gint     xfce_mime_window_mime_model        (XfceMimeWindow       *window);
static void     xfce_mime_window_filter_changed    (GtkEntry             *entry,
                                                    XfceMimeWindow       *window);
static void     xfce_mime_window_filter_clear      (GtkEntry             *entry,
                                                    GtkEntryIconPosition  icon_pos,
                                                    GdkEvent             *event,
                                                    gpointer              user_data);
static void     xfce_mime_window_statusbar_count   (XfceMimeWindow       *window,
                                                    gint                 n_mime_types);
static gboolean xfce_mime_window_row_visible_func  (GtkTreeModel         *model,
                                                    GtkTreeIter          *iter,
                                                    gpointer              data);
static void     xfce_mime_window_row_activated     (GtkTreeView          *tree_view,
                                                    GtkTreePath          *path,
                                                    GtkTreeViewColumn    *column,
                                                    XfceMimeWindow       *window);
static void     xfce_mime_window_selection_changed (GtkTreeSelection     *selection,
                                                    XfceMimeWindow       *window);
static void     xfce_mime_window_column_clicked    (GtkTreeViewColumn    *column,
                                                    XfceMimeWindow       *window);
static void     xfce_mime_window_combo_populate    (GtkCellRenderer      *renderer,
                                                    GtkCellEditable      *editable,
                                                    const gchar          *path_string,
                                                    XfceMimeWindow       *window);



struct _XfceMimeWindowClass
{
    XfceTitledDialogClass __parent__;
};

struct _XfceMimeWindow
{
    XfceTitledDialog  __parent__;

    GtkWidget     *treeview;

    PangoAttrList *attrs_bold;
    GtkTreeModel  *mime_model;

    GtkTreeModel  *filter_model;
    gchar         *filter_text;

    /* status bar stuff */
    GtkWidget     *statusbar;
    guint          desc_id;
    guint          count_id;
};



enum
{
    COLUMN_MIME_TYPE,
    COLUMN_MIME_STATUS,
    COLUMN_MIME_DEFAULT,
    COLUMN_MIME_ATTRS,
    N_MIME_COLUMNS
};

enum
{
    COLUMN_APP_NAME,
    COLUMN_APP_INFO,
    COLUMN_APP_GICON,
    COLUMN_APP_TYPE,
    N_APP_COLUMNS
};

enum
{
    APP_TYPE_APP,
    APP_TYPE_SEPARATOR,
    APP_TYPE_CHOOSER,
    APP_TYPE_RESET
};



G_DEFINE_TYPE (XfceMimeWindow, xfce_mime_window, XFCE_TYPE_TITLED_DIALOG)



static void
xfce_mime_window_class_init (XfceMimeWindowClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_mime_window_finalize;
}



static void
xfce_mime_window_init (XfceMimeWindow *window)
{
    GtkWidget         *area;
    GtkWidget         *vbox;
    GtkWidget         *hbox;
    GtkWidget         *label;
    GtkWidget         *entry;
    GtkWidget         *scroll;
    GtkWidget         *statusbar;
    GtkWidget         *treeview;
    GtkTreeSelection  *selection;
    gint               n_mime_types;
    GtkTreeViewColumn *column;
    GtkCellRenderer   *renderer;

    window->attrs_bold = pango_attr_list_new ();
    pango_attr_list_insert (window->attrs_bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

    n_mime_types = xfce_mime_window_mime_model (window);

    gtk_window_set_title (GTK_WINDOW (window), _("Mime Type Editor"));
    gtk_window_set_icon_name (GTK_WINDOW (window), "application-default-icon");
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (window),
        _("Configure the application accossiated with a mime type"));

    /* don't act like a dialog, hide the button box */
    area = gtk_dialog_get_action_area (GTK_DIALOG (window));
    gtk_widget_hide (area);

    vbox = gtk_vbox_new (FALSE, 2);
    area = gtk_dialog_get_content_area (GTK_DIALOG (window));
    gtk_box_pack_start (GTK_BOX (area), vbox, TRUE, TRUE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show (hbox);

    label = gtk_label_new_with_mnemonic (_("_Filter:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
    gtk_widget_show (label);

    entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_entry_set_icon_from_stock (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, _("Clear filter"));
    g_signal_connect (G_OBJECT (entry), "icon-release",
        G_CALLBACK (xfce_mime_window_filter_clear), NULL);
    g_signal_connect (G_OBJECT (entry), "changed",
        G_CALLBACK (xfce_mime_window_filter_changed), window);
    gtk_widget_show (entry);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
    gtk_widget_show (scroll);

    window->statusbar = statusbar = gtk_statusbar_new ();
    gtk_box_pack_start (GTK_BOX (vbox), statusbar, FALSE, TRUE, 0);
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (statusbar), TRUE);
    window->desc_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "desc");
    window->count_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "count");
    xfce_mime_window_statusbar_count (window, n_mime_types);
    gtk_widget_show (statusbar);

    window->filter_model = gtk_tree_model_filter_new (window->mime_model, NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (window->filter_model),
                                            xfce_mime_window_row_visible_func,
                                            window, NULL);

    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (window->filter_model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), FALSE);
    gtk_container_add (GTK_CONTAINER (scroll), treeview);
    gtk_widget_show (treeview);
    window->treeview = treeview;
    g_signal_connect (G_OBJECT (treeview), "row-activated",
        G_CALLBACK (xfce_mime_window_row_activated), window);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    g_signal_connect (G_OBJECT (selection), "changed",
        G_CALLBACK (xfce_mime_window_selection_changed), window);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, _("Mime Type"));
    gtk_tree_view_column_set_clickable (column, TRUE);
    gtk_tree_view_column_set_sort_indicator (column, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    g_signal_connect (G_OBJECT (column), "clicked",
        G_CALLBACK (xfce_mime_window_column_clicked), window);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    /* HACK, wont work in gtk3 */
    /* give the first column some initial size that doesn't restrict anything
     * later. Expanding this column will resize the view when a setting
     * is changed */
    column->resized_width = 300;
    column->use_resized_width = TRUE;

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
    g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                    "attributes", COLUMN_MIME_ATTRS,
                                    "text", COLUMN_MIME_TYPE, NULL);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, _("Status"));
    gtk_tree_view_column_set_clickable (column, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    g_signal_connect (G_OBJECT (column), "clicked",
        G_CALLBACK (xfce_mime_window_column_clicked), window);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                    "attributes", COLUMN_MIME_ATTRS,
                                    "text", COLUMN_MIME_STATUS, NULL);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, _("Default Application"));
    gtk_tree_view_column_set_clickable (column, TRUE);
    gtk_tree_view_column_set_resizable (column, TRUE);
    g_signal_connect (G_OBJECT (column), "clicked",
        G_CALLBACK (xfce_mime_window_column_clicked), window);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_combo_new ();
    g_signal_connect (G_OBJECT (renderer), "editing-started",
        G_CALLBACK (xfce_mime_window_combo_populate), window);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
    g_object_set (renderer,
                  "text-column", COLUMN_APP_NAME,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  "has-entry", FALSE,
                  "editable", TRUE, NULL);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                    "attributes", COLUMN_MIME_ATTRS,
                                    "text", COLUMN_MIME_DEFAULT, NULL);
}



static void
xfce_mime_window_finalize (GObject *object)
{
    XfceMimeWindow *window = XFCE_MIME_WINDOW (object);

    g_free (window->filter_text);

    g_object_unref (G_OBJECT (window->filter_model));
    g_object_unref (G_OBJECT (window->mime_model));

    pango_attr_list_unref (window->attrs_bold);

    (*G_OBJECT_CLASS (xfce_mime_window_parent_class)->finalize) (object);
}



static GHashTable *
xfce_mime_window_mime_user (void)
{
    gchar       *filename;
    GHashTable  *table;
    XfceRc      *rc;
    guint        i;
    gchar      **mimes;
    guint        n;
    const gchar *groups[] = { "Added Associations", "Default Applications" };

    table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    filename = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/mimeapps.list", FALSE);
    if (filename == NULL)
        return table;

    rc = xfce_rc_simple_open (filename, TRUE);
    g_free (filename);

    if (G_LIKELY (rc != NULL))
    {
        for (n = 0; n < G_N_ELEMENTS (groups); n++)
        {
            mimes = xfce_rc_get_entries (rc, groups[n]);
            if (G_LIKELY (mimes != NULL))
            {
                for (i = 0; mimes[i] != NULL; i++)
                    g_hash_table_insert (table, mimes[i], GINT_TO_POINTER (TRUE));
                g_free (mimes);
            }
        }

        xfce_rc_close (rc);
    }

    return table;
}



static gint
xfce_mime_window_mime_model (XfceMimeWindow *window)
{
    GtkListStore *model;
    GList        *mime_types, *li;
    gchar        *mime_type;
    const gchar  *app_name;
    GAppInfo     *app_default;
    GHashTable   *user_mime;
    gboolean      is_user_set;
    gint          n;
    const gchar  *status;

    model = gtk_list_store_new (N_MIME_COLUMNS,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                PANGO_TYPE_ATTR_LIST);

    /* get sorted list of known mime types */
    mime_types = g_content_types_get_registered ();
    mime_types = g_list_sort (mime_types, (GCompareFunc) g_strcmp0);

    /* mime types the user has set */
    user_mime = xfce_mime_window_mime_user ();

    for (li = mime_types, n = 0; li != NULL; li = li->next)
    {
        mime_type = li->data;

        app_default = g_app_info_get_default_for_type (mime_type, FALSE);

        if (G_LIKELY (app_default != NULL))
            app_name = g_app_info_get_name (app_default);
        else
            app_name = NULL;

        /* check if the user locally override this mime handler */
        is_user_set = g_hash_table_remove (user_mime, mime_type);
        if (is_user_set)
            status = _("User Set");
        else
            status = _("Default");

        gtk_list_store_insert_with_values (model, NULL, n++,
                                           COLUMN_MIME_TYPE, mime_type,
                                           COLUMN_MIME_DEFAULT, app_name,
                                           COLUMN_MIME_STATUS, status,
                                           COLUMN_MIME_ATTRS,
                                               is_user_set ? window->attrs_bold : NULL,
                                           -1);

        g_free (mime_type);
        if (G_LIKELY (app_default != NULL))
          g_object_unref (app_default);
    }

    g_list_free (mime_types);
    g_hash_table_destroy (user_mime);

    window->mime_model = GTK_TREE_MODEL (model);

    return n;
}



static void
xfce_mime_window_filter_changed (GtkEntry       *entry,
                                   XfceMimeWindow *window)
{
    const gchar *text;
    gint         count;

    g_free (window->filter_text);

    text = gtk_entry_get_text (GTK_ENTRY (entry));
    if (text == NULL || *text == '\0')
        window->filter_text = NULL;
    else
        window->filter_text = g_utf8_casefold (text, -1);

    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (window->filter_model));

    count = gtk_tree_model_iter_n_children (window->filter_model, NULL);
    xfce_mime_window_statusbar_count (window, count);
}



static void
xfce_mime_window_filter_clear (GtkEntry            *entry,
                                 GtkEntryIconPosition icon_pos,
                                 GdkEvent            *event,
                                 gpointer             user_data)
{
    if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
        gtk_entry_set_text (entry, "");
}



static void
xfce_mime_window_statusbar_count (XfceMimeWindow *window,
                                    gint            n_mime_types)
{
    gchar *msg;

    gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar), window->count_id);

    msg = g_strdup_printf (ngettext ("%d mime type found",
                                     "%d mime types found",
                                     n_mime_types), n_mime_types);
    gtk_statusbar_push (GTK_STATUSBAR (window->statusbar), window->count_id, msg);
    g_free (msg);
}



static gboolean
xfce_mime_window_row_visible_func (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     gpointer      data)
{
    XfceMimeWindow *window = XFCE_MIME_WINDOW (data);
    gchar          *mime_type;
    gboolean        visible = FALSE;

    if (window->filter_text == NULL)
        return TRUE;

    gtk_tree_model_get (model, iter, COLUMN_MIME_TYPE, &mime_type, -1);
    if (G_UNLIKELY (mime_type == NULL))
        return FALSE;

    visible = strstr (mime_type, window->filter_text) != NULL;
    g_free (mime_type);

    return visible;
}



static void
xfce_mime_window_set_default_for_type (XfceMimeWindow *window,
                                       GAppInfo       *app_info,
                                       const gchar    *mime_type,
                                       GtkTreePath    *filter_path)
{
    GAppInfo    *app_default;
    GError      *error = NULL;
    GtkTreeIter  mime_iter;
    GtkTreeIter  filter_iter;

    g_return_if_fail (G_IS_APP_INFO (app_info));
    g_return_if_fail (XFCE_IS_MIME_WINDOW (window));
    g_return_if_fail (mime_type != NULL);

    /* do nothing if the new app is the same as the default */
    app_default = g_app_info_get_default_for_type (mime_type, FALSE);
    if (app_default == NULL
        || !g_app_info_equal (app_default, app_info))
    {
        if (g_app_info_set_as_default_for_type (app_info, mime_type, &error))
        {
            if (gtk_tree_model_get_iter (window->filter_model, &filter_iter, filter_path))
            {
                gtk_tree_model_filter_convert_iter_to_child_iter (
                    GTK_TREE_MODEL_FILTER (window->filter_model),
                    &mime_iter, &filter_iter);

                gtk_list_store_set (GTK_LIST_STORE (window->mime_model), &mime_iter,
                                    COLUMN_MIME_DEFAULT, g_app_info_get_name (app_info),
                                    COLUMN_MIME_STATUS, _("User Set"),
                                    COLUMN_MIME_ATTRS, window->attrs_bold,
                                    -1);
            }
        }
        else
        {
            xfce_dialog_show_error (GTK_WINDOW (window), error,
                _("Failed to set application \"%s\" for mime type \"%s\"."),
                g_app_info_get_name (app_info), mime_type);
            g_error_free (error);
        }
    }

    if (app_default != NULL)
        g_object_unref (G_OBJECT (app_default));
}



static void
xfce_mime_window_row_activated (GtkTreeView       *tree_view,
                                GtkTreePath       *path,
                                GtkTreeViewColumn *column,
                                XfceMimeWindow    *window)
{
    GtkTreeIter  iter;
    gchar       *mime_type;
    GtkWidget   *dialog;
    GAppInfo    *app_info;

    if (gtk_tree_model_get_iter (window->filter_model, &iter, path))
    {
        gtk_tree_model_get (window->filter_model, &iter, COLUMN_MIME_TYPE, &mime_type, -1);

        dialog = g_object_new (XFCE_TYPE_MIME_CHOOSER, NULL);
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
        xfce_mime_chooser_set_mime_type (XFCE_MIME_CHOOSER (dialog), mime_type);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
        {
            app_info = xfce_mime_chooser_get_app_info (XFCE_MIME_CHOOSER (dialog));
            if (G_LIKELY (app_info != NULL))
            {
                xfce_mime_window_set_default_for_type (window, app_info, mime_type, path);
                g_object_unref (G_OBJECT (app_info));
            }
        }

        gtk_widget_destroy (dialog);
    }
}



static void
xfce_mime_window_selection_changed (GtkTreeSelection *selection,
                                    XfceMimeWindow   *window)
{
    gchar        *mime_type;
    gchar        *description;
    GtkTreeModel *model;
    GtkTreeIter   iter;

    gtk_statusbar_pop (GTK_STATUSBAR (window->statusbar),
                       window->desc_id);

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, COLUMN_MIME_TYPE, &mime_type, -1);
        description = g_content_type_get_description (mime_type);
        g_free (mime_type);

        if (G_LIKELY (description != NULL))
        {
            gtk_statusbar_push (GTK_STATUSBAR (window->statusbar),
                                window->desc_id, description);
            g_free (description);
        }
    }
}



static void
xfce_mime_window_column_clicked (GtkTreeViewColumn *column,
                                   XfceMimeWindow    *window)
{
    GtkSortType  sort_type;
    GList       *columns, *li;

    columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (window->treeview));

    if (gtk_tree_view_column_get_sort_indicator (column))
    {
        /* invert sort order */
        sort_type = gtk_tree_view_column_get_sort_order (column);
        if (sort_type == GTK_SORT_ASCENDING)
            sort_type = GTK_SORT_DESCENDING;
        else
            sort_type = GTK_SORT_ASCENDING;
    }
    else
    {
        /* update indicator visibility */
        for (li = columns; li != NULL; li = li->next)
            gtk_tree_view_column_set_sort_indicator (li->data, li->data == column);

        /* always start asc sort on first click */
        sort_type = GTK_SORT_ASCENDING;
    }

    /* update arrow and sort column */
    gtk_tree_view_column_set_sort_order (column, sort_type);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (window->mime_model),
        g_list_index (columns, column), sort_type);
    g_list_free (columns);
}



static gboolean
xfce_mime_window_combo_row_separator_func (GtkTreeModel *model,
                                             GtkTreeIter  *iter,
                                             gpointer      data)
{
    guint type;

    gtk_tree_model_get (model, iter, COLUMN_APP_TYPE, &type, -1);

    return type == APP_TYPE_SEPARATOR;
}



typedef struct
{
    guint           ref_count;
    XfceMimeWindow *window;
    gchar          *mime_type;
    GtkTreePath    *filter_path;
}
MimeChangedData;



static void
xfce_mime_window_combo_unref_data (gpointer user_data)
{
    MimeChangedData *data = user_data;

    if (--data->ref_count > 0)
        return;

    g_free (data->mime_type);
    gtk_tree_path_free (data->filter_path);
    g_slice_free (MimeChangedData, data);
}



static void
xfce_mime_window_chooser_response (GtkWidget       *chooser,
                                   gint             response_id,
                                   MimeChangedData *data)
{
    GAppInfo *app_info;

    gtk_widget_hide (chooser);

    if (response_id == GTK_RESPONSE_YES)
    {
        app_info = xfce_mime_chooser_get_app_info (XFCE_MIME_CHOOSER (chooser));
        if (G_LIKELY (app_info != NULL))
        {
            xfce_mime_window_set_default_for_type (data->window, app_info,
                                                   data->mime_type,
                                                   data->filter_path);
            g_object_unref (G_OBJECT (app_info));
        }
    }

    xfce_mime_window_combo_unref_data (data);

    gtk_widget_destroy (chooser);
}



static void
xfce_mime_window_reset_response (GtkWidget       *dialog,
                                 gint             response_id,
                                 MimeChangedData *data)
{
    GAppInfo    *app_default;
    const gchar *app_name;
    GtkTreeIter  filter_iter;
    GtkTreeIter  mime_iter;

    gtk_widget_destroy (dialog);

    if (response_id == GTK_RESPONSE_YES)
    {
        /* reset the user's default */
        g_app_info_reset_type_associations (data->mime_type);

        /* restore the system default */
        app_default = g_app_info_get_default_for_type (data->mime_type, FALSE);
        if (app_default != NULL)
          app_name = g_app_info_get_name (app_default);
        else
          app_name = NULL;

        if (gtk_tree_model_get_iter (data->window->filter_model, &filter_iter, data->filter_path))
        {
            gtk_tree_model_filter_convert_iter_to_child_iter (
                GTK_TREE_MODEL_FILTER (data->window->filter_model),
                &mime_iter, &filter_iter);

            gtk_list_store_set (GTK_LIST_STORE (data->window->mime_model), &mime_iter,
                                COLUMN_MIME_DEFAULT, app_name,
                                COLUMN_MIME_STATUS, _("Default"),
                                COLUMN_MIME_ATTRS, NULL,
                                -1);
        }

        if (app_default != NULL)
            g_object_unref (app_default);
    }

    xfce_mime_window_combo_unref_data (data);
}



static void
xfce_mime_window_combo_changed (GtkWidget       *combo,
                                MimeChangedData *data)
{
    XfceMimeWindow *window = XFCE_MIME_WINDOW (data->window);
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    guint           type;
    GAppInfo       *app_info;
    GtkWidget      *dialog;
    gchar          *primary;

    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
        return;

    gtk_tree_model_get (model, &iter,
                        COLUMN_APP_TYPE, &type,
                        COLUMN_APP_INFO, &app_info, -1);

    if (type == APP_TYPE_APP
        && app_info != NULL)
    {
        xfce_mime_window_set_default_for_type (data->window, app_info,
                                               data->mime_type,
                                               data->filter_path);
        g_object_unref (app_info);
    }
    else if (type == APP_TYPE_CHOOSER)
    {
        dialog = g_object_new (XFCE_TYPE_MIME_CHOOSER, NULL);
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
        xfce_mime_chooser_set_mime_type (XFCE_MIME_CHOOSER (dialog), data->mime_type);

        /* ref data */
        data->ref_count++;

        g_signal_connect (G_OBJECT (dialog), "response",
            G_CALLBACK (xfce_mime_window_chooser_response), data);
        gtk_window_present (GTK_WINDOW (dialog));
    }
    else if (type == APP_TYPE_RESET)
    {
        primary = g_strdup_printf (_("Are you sure you want to reset content "
                                     "type \"%s\" to its default value?"), data->mime_type);

        dialog = xfce_message_dialog_new (GTK_WINDOW (window),
                                          _("Question"),
                                          GTK_STOCK_DIALOG_QUESTION,
                                          primary,
                                          _("This will remove your custom mime-association "
                                            "and restore the system-wide default."),
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_NO,
                                          XFCE_BUTTON_TYPE_MIXED, GTK_STOCK_REVERT_TO_SAVED,
                                          _("Reset to Default"), GTK_RESPONSE_YES, NULL);
        g_free (primary);

        /* ref data */
        data->ref_count++;

        g_signal_connect (G_OBJECT (dialog), "response",
            G_CALLBACK (xfce_mime_window_reset_response), data);
        gtk_window_present (GTK_WINDOW (dialog));
    }
}



static void
xfce_mime_window_combo_populate (GtkCellRenderer *renderer,
                                 GtkCellEditable *editable,
                                 const gchar     *path_string,
                                 XfceMimeWindow  *window)
{
    GtkTreeIter      iter;
    gchar           *mime_type;
    GList           *app_infos, *li;
    guint            n;
    GtkListStore    *model;
    GAppInfo        *app_info;
    MimeChangedData *data;
    GtkCellRenderer *iconrenderer;
    gint             size = 0;

    if (!gtk_tree_model_get_iter_from_string (window->filter_model, &iter, path_string))
        return;

    model = gtk_list_store_new (N_APP_COLUMNS,
                                G_TYPE_STRING,
                                G_TYPE_APP_INFO,
                                G_TYPE_ICON,
                                G_TYPE_UINT);

    gtk_tree_model_get (window->filter_model, &iter, COLUMN_MIME_TYPE, &mime_type, -1);
    app_infos = g_app_info_get_all_for_type (mime_type);

    for (li = app_infos, n = 0; li != NULL; li = li->next)
    {
        app_info = G_APP_INFO (li->data);
        if (G_UNLIKELY (app_info == NULL))
            continue;

        gtk_list_store_insert_with_values (model, NULL, n++,
                                           COLUMN_APP_NAME, g_app_info_get_name (app_info),
                                           COLUMN_APP_INFO, app_info,
                                           COLUMN_APP_GICON, g_app_info_get_icon (app_info),
                                           COLUMN_APP_TYPE, APP_TYPE_APP,
                                           -1);
        g_object_unref (app_info);
    }

    if (n != 0)
    {
        gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &size, NULL);
        gtk_list_store_insert_with_values (model, NULL, n++,
                                           COLUMN_APP_TYPE, APP_TYPE_SEPARATOR,
                                           -1);
    }

    gtk_list_store_insert_with_values (model, NULL, n++,
                                       COLUMN_APP_NAME, _("Choose Application..."),
                                       COLUMN_APP_TYPE, APP_TYPE_CHOOSER,
                                       -1);

    gtk_list_store_insert_with_values (model, NULL, n,
                                       COLUMN_APP_NAME, _("Reset to Default"),
                                       COLUMN_APP_TYPE, APP_TYPE_RESET,
                                       -1);

    data = g_slice_new0 (MimeChangedData);
    data->window = window;
    data->ref_count = 1;
    data->mime_type = mime_type;
    data->filter_path = gtk_tree_model_get_path (window->filter_model, &iter);

    /* directly update the combo */
    gtk_combo_box_set_model (GTK_COMBO_BOX (editable), GTK_TREE_MODEL (model));
    g_signal_connect_data (G_OBJECT (editable), "changed",
        G_CALLBACK (xfce_mime_window_combo_changed), data,
        (GClosureNotify) xfce_mime_window_combo_unref_data, 0);
    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (editable),
        xfce_mime_window_combo_row_separator_func, NULL, NULL);

    iconrenderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (editable), iconrenderer, FALSE);
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (editable), iconrenderer, 0);
    g_object_set (G_OBJECT (iconrenderer),
                  "stock-size", GTK_ICON_SIZE_MENU,
                  "width", size, NULL);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (editable), iconrenderer,
                                   "gicon", COLUMN_APP_GICON);

    g_list_free (app_infos);
    g_object_unref (G_OBJECT (model));
}
