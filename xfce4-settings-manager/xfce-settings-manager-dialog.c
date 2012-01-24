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

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>
#include <garcon/garcon.h>
#include <exo/exo.h>

#include "xfce-settings-manager-dialog.h"
#include "xfce-text-renderer.h"

#define ITEM_WIDTH (128)



struct _XfceSettingsManagerDialog
{
    XfceTitledDialog __parent__;

    GarconMenu   *menu;

    GtkListStore *store;

    GtkWidget    *category_box;
    GList        *category_iconviews;

    GtkWidget    *button_previous;
};

struct _XfceSettingsManagerDialogClass
{
    XfceTitledDialogClass __parent__;
};

enum
{
    COLUMN_NAME,
    COLUMN_ICON_NAME,
    COLUMN_TOOLTIP,
    COLUMN_MENU_ITEM,
    COLUMN_MENU_DIRECTORY,
    N_COLUMNS
};



static void xfce_settings_manager_dialog_finalize    (GObject                   *object);
static void xfce_settings_manager_dialog_response    (GtkDialog                 *widget,
                                                      gint                       response_id);
static void xfce_settings_manager_dialog_set_title   (XfceSettingsManagerDialog *dialog,
                                                      const gchar               *title,
                                                      const gchar               *icon_name,
                                                      const gchar               *subtitle);
static void xfce_settings_manager_dialog_menu_reload (XfceSettingsManagerDialog *dialog);



G_DEFINE_TYPE (XfceSettingsManagerDialog, xfce_settings_manager_dialog, XFCE_TYPE_TITLED_DIALOG)



static void
xfce_settings_manager_dialog_class_init (XfceSettingsManagerDialogClass *klass)
{
    GObjectClass   *gobject_class;
    GtkDialogClass *gtkdialog_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_settings_manager_dialog_finalize;

    gtkdialog_class = GTK_DIALOG_CLASS (klass);
    gtkdialog_class->response = xfce_settings_manager_dialog_response;
}



static void
xfce_settings_manager_dialog_init (XfceSettingsManagerDialog *dialog)
{
    GtkWidget *scroll;
    GtkWidget *area;
    GtkWidget *viewport;
    GtkWidget *vbox;
    gchar     *path;

    dialog->store = gtk_list_store_new (N_COLUMNS,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        GARCON_TYPE_MENU_ITEM,
                                        GARCON_TYPE_MENU_DIRECTORY);

    path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "menus/xfce-settings-manager.menu");
    dialog->menu = garcon_menu_new_for_path (path);
    g_free (path);

    gtk_window_set_default_size (GTK_WINDOW (dialog), 640, 500);
    xfce_settings_manager_dialog_set_title (dialog, NULL, NULL, NULL);

    dialog->button_previous = xfce_gtk_button_new_mixed (GTK_STOCK_GO_BACK, _("_All Settings"));
    area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
    gtk_container_add (GTK_CONTAINER (area), dialog->button_previous);
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (area), dialog->button_previous, TRUE);
    gtk_widget_set_sensitive (dialog->button_previous, FALSE);
    gtk_widget_show (dialog->button_previous);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP, NULL);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_add (GTK_CONTAINER (area), scroll);
    gtk_container_set_border_width (GTK_CONTAINER (scroll), 6);
    gtk_widget_show (scroll);

    viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), viewport);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_modify_bg (viewport, GTK_STATE_NORMAL, &viewport->style->white);
    gtk_widget_show (viewport);

    dialog->category_box = vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_add (GTK_CONTAINER (viewport), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
    gtk_widget_set_size_request (vbox,
                                 ITEM_WIDTH /* text */
                                 + 48       /* icon */
                                 + (5 * 6)  /* borders */, -1);
    gtk_widget_show (vbox);

    xfce_settings_manager_dialog_menu_reload (dialog);

    g_signal_connect_swapped (G_OBJECT (dialog->menu), "reload-required",
        G_CALLBACK (xfce_settings_manager_dialog_menu_reload), dialog);
}



static void
xfce_settings_manager_dialog_finalize (GObject *object)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (object);

    g_object_unref (G_OBJECT (dialog->menu));
    g_object_unref (G_OBJECT (dialog->store));

    G_OBJECT_CLASS (xfce_settings_manager_dialog_parent_class)->finalize (object);
}



static void
xfce_settings_manager_dialog_response (GtkDialog *widget,
                                       gint       response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
    {

    }
    else
    {
        gtk_main_quit ();
    }
}



static void
xfce_settings_manager_dialog_set_title (XfceSettingsManagerDialog *dialog,
                                        const gchar               *title,
                                        const gchar               *icon_name,
                                        const gchar               *subtitle)
{
    g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));

    if (icon_name == NULL)
        icon_name = "preferences-desktop";
    if (title == NULL)
        title = _("Settings");
    if (subtitle == NULL)
        subtitle = _("Customize your desktop");

    gtk_window_set_title (GTK_WINDOW (dialog), title);
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), subtitle);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
}



static gboolean
xfce_settings_manager_dialog_iconview_keynav_failed (ExoIconView               *current_view,
                                                     GtkDirectionType           direction,
                                                     XfceSettingsManagerDialog *dialog)
{
    GList        *li;
    GtkTreePath  *path;
    ExoIconView  *new_view;
    gboolean      result = FALSE;
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gint          col_old, col_new;
    gint          dist_prev, dist_new;
    GtkTreePath  *sel_path;

    if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
    {
        li = g_list_find (dialog->category_iconviews, current_view);
        if (direction == GTK_DIR_DOWN)
            li = g_list_next (li);
        else
            li = g_list_previous (li);

        /* leave there is no view obove or below this one */
        if (li == NULL)
            return FALSE;

        new_view = EXO_ICON_VIEW (li->data);

        if (exo_icon_view_get_cursor (current_view, &path, NULL))
        {
            col_old = exo_icon_view_get_item_column (current_view, path);
            gtk_tree_path_free (path);

            dist_prev = 1000;
            sel_path = NULL;

            model = exo_icon_view_get_model (new_view);
            if (gtk_tree_model_get_iter_first (model, &iter))
            {
                do
                {
                     path = gtk_tree_model_get_path (model, &iter);
                     col_new = exo_icon_view_get_item_column (new_view, path);
                     dist_new = ABS (col_new - col_old);

                     if ((direction == GTK_DIR_UP && dist_new <= dist_prev)
                         || (direction == GTK_DIR_DOWN  && dist_new < dist_prev))
                     {
                         if (sel_path != NULL)
                             gtk_tree_path_free (sel_path);

                         sel_path = path;
                         dist_prev = dist_new;
                     }
                     else
                     {
                         gtk_tree_path_free (path);
                     }
                }
                while (gtk_tree_model_iter_next (model, &iter));
            }

            if (G_LIKELY (sel_path != NULL))
            {
                /* move cursor, grab-focus will handle the selection */
                exo_icon_view_set_cursor (new_view, sel_path, NULL, FALSE);
                gtk_tree_path_free (sel_path);

                gtk_widget_grab_focus (GTK_WIDGET (new_view));

                result = TRUE;
            }
        }
    }

    return result;
}



static gboolean
xfce_settings_manager_dialog_query_tooltip (GtkWidget                 *iconview,
                                            gint                       x,
                                            gint                       y,
                                            gboolean                   keyboard_mode,
                                            GtkTooltip                *tooltip,
                                            XfceSettingsManagerDialog *dialog)
{
    GtkTreePath    *path;
    GValue          value = { 0, };
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    GarconMenuItem *item;
    const gchar    *comment;

    if (keyboard_mode)
    {
        if (!exo_icon_view_get_cursor (EXO_ICON_VIEW (iconview), &path, NULL))
            return FALSE;
    }
    else
    {
        path = exo_icon_view_get_path_at_pos (EXO_ICON_VIEW (iconview), x, y);
        if (G_UNLIKELY (path == NULL))
            return FALSE;
    }

    model = exo_icon_view_get_model (EXO_ICON_VIEW (iconview));
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get_value (model, &iter, COLUMN_MENU_ITEM, &value);
        item = g_value_get_object (&value);
        g_assert (GARCON_IS_MENU_ITEM (item));

        comment = garcon_menu_item_get_comment (item);
        if (!exo_str_is_empty (comment))
            gtk_tooltip_set_text (tooltip, comment);

        g_value_unset (&value);
    }

    gtk_tree_path_free (path);

    return TRUE;
}



static gboolean
xfce_settings_manager_dialog_iconview_focus (GtkWidget                 *iconview,
                                             GdkEventFocus             *event,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkTreePath *path;

    if (event->in)
    {
        /* a mouse click will have focus, tab events not */
        if (!exo_icon_view_get_cursor (EXO_ICON_VIEW (iconview), &path, NULL))
        {
           path = gtk_tree_path_new_from_indices (0, -1);
           exo_icon_view_set_cursor (EXO_ICON_VIEW (iconview), path, NULL, FALSE);
        }

        exo_icon_view_select_path (EXO_ICON_VIEW (iconview), path);
        gtk_tree_path_free (path);
    }
    else
    {
        exo_icon_view_unselect_all (EXO_ICON_VIEW (iconview));
    }

    return FALSE;
}



static void
xfce_settings_manager_dialog_item_activated (ExoIconView               *iconview,
                                             GtkTreePath               *path,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    GarconMenuItem *item;
    const gchar    *command;
    gboolean        snotify;
    GdkScreen      *screen;
    GError         *error = NULL;

    model = exo_icon_view_get_model (iconview);
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, COLUMN_MENU_ITEM, &item, -1);
        g_assert (GARCON_IS_MENU_ITEM (item));

        screen = gtk_window_get_screen (GTK_WINDOW (dialog));
        command = garcon_menu_item_get_command (item);
        snotify = garcon_menu_item_supports_startup_notification (item);

        if (!xfce_spawn_command_line_on_screen (screen, command, FALSE, snotify, &error))
        {
            g_error_free (error);
        }

        g_object_unref (G_OBJECT (item));
    }
}



static gboolean
xfce_settings_manager_dialog_filter_category (GtkTreeModel *model,
                                              GtkTreeIter  *iter,
                                              gpointer      data)
{
    GValue   value = { 0, };
    gboolean visible;

    gtk_tree_model_get_value (model, iter, COLUMN_MENU_DIRECTORY, &value);
    visible = g_value_get_object (&value) == data;
    g_value_unset (&value);

    return visible;
}



static void
xfce_settings_manager_dialog_add_category (XfceSettingsManagerDialog *dialog,
                                           GarconMenuDirectory       *directory)
{
    GtkTreeModel    *filter;
    GtkWidget       *frame;
    GtkWidget       *label;
    GtkWidget       *iconview;
    PangoAttrList   *attrs;
    GtkCellRenderer *render;

    /* filter category from main store */
    filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (dialog->store), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
        xfce_settings_manager_dialog_filter_category,
        g_object_ref (directory), g_object_unref);

    frame = gtk_frame_new (NULL);
    gtk_box_pack_start (GTK_BOX (dialog->category_box), frame, FALSE, TRUE, 0);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    gtk_container_set_resize_mode (GTK_CONTAINER (frame), GTK_RESIZE_IMMEDIATE);
    gtk_widget_show (frame);

    label = gtk_label_new (garcon_menu_directory_get_name (directory));
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes (GTK_LABEL (label), attrs);
    pango_attr_list_unref (attrs);
    gtk_frame_set_label_widget (GTK_FRAME (frame), label);
    gtk_widget_show (label);

    iconview = exo_icon_view_new_with_model (GTK_TREE_MODEL (filter));
    gtk_container_add (GTK_CONTAINER (frame), iconview);
    exo_icon_view_set_orientation (EXO_ICON_VIEW (iconview), GTK_ORIENTATION_HORIZONTAL);
    exo_icon_view_set_margin (EXO_ICON_VIEW (iconview), 0);
    exo_icon_view_set_single_click (EXO_ICON_VIEW (iconview), TRUE);
    exo_icon_view_set_enable_search (EXO_ICON_VIEW (iconview), FALSE);
    exo_icon_view_set_item_width (EXO_ICON_VIEW (iconview), ITEM_WIDTH + 48);
    gtk_widget_show (iconview);

    /* list used for unselecting */
    dialog->category_iconviews = g_list_append (dialog->category_iconviews, iconview);

    gtk_widget_set_has_tooltip (iconview, TRUE);
    g_signal_connect (G_OBJECT (iconview), "query-tooltip",
        G_CALLBACK (xfce_settings_manager_dialog_query_tooltip), dialog);
    g_signal_connect (G_OBJECT (iconview), "focus-in-event",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_focus), dialog);
    g_signal_connect (G_OBJECT (iconview), "focus-out-event",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_focus), dialog);
    g_signal_connect (G_OBJECT (iconview), "keynav-failed",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_keynav_failed), dialog);
    g_signal_connect (G_OBJECT (iconview), "item-activated",
        G_CALLBACK (xfce_settings_manager_dialog_item_activated), dialog);

    render = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (iconview), render, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iconview), render, "icon-name", COLUMN_ICON_NAME);
    g_object_set (G_OBJECT (render),
                  "stock-size", GTK_ICON_SIZE_DIALOG,
                  "follow-state", TRUE,
                  NULL);

    render = xfce_text_renderer_new ();
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (iconview), render, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iconview), render, "text", COLUMN_NAME);
    g_object_set (G_OBJECT (render),
                  "wrap-mode", PANGO_WRAP_WORD,
                  "wrap-width", ITEM_WIDTH,
                  "follow-prelit", TRUE,
                  "follow-state", TRUE,
                  NULL);

    g_object_unref (G_OBJECT (filter));
}



static void
xfce_settings_manager_dialog_menu_collect (GarconMenu  *menu,
                                           GList      **items)
{
    GList *elements, *li;

    g_return_if_fail (GARCON_IS_MENU (menu));

    elements = garcon_menu_get_elements (menu);

    for (li = elements; li != NULL; li = li->next)
    {
        if (GARCON_IS_MENU_ITEM (li->data))
        {
            /* only add visible items */
            if (garcon_menu_element_get_visible (li->data))
                *items = g_list_prepend (*items, li->data);
        }
        else if (GARCON_IS_MENU (li->data))
        {
            /* we collect only 1 level deep in a category, so
             * add the submenu items too (should never happen tho) */
            xfce_settings_manager_dialog_menu_collect (li->data, items);
        }
    }

    g_list_free (elements);
}



static gint
xfce_settings_manager_dialog_menu_sort (gconstpointer a,
                                        gconstpointer b)
{
    return g_utf8_collate (garcon_menu_item_get_name (GARCON_MENU_ITEM (a)),
                           garcon_menu_item_get_name (GARCON_MENU_ITEM (b)));
}



static void
xfce_settings_manager_dialog_menu_reload (XfceSettingsManagerDialog *dialog)
{
    GError              *error = NULL;
    GList               *elements, *li;
    GarconMenuDirectory *directory;
    GList               *items, *lp;
    gint                 i = 0;

    g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));
    g_return_if_fail (GARCON_IS_MENU (dialog->menu));

    if (garcon_menu_load (dialog->menu, NULL, &error))
    {
        /* get all menu elements (preserve layout) */
        elements = garcon_menu_get_elements (dialog->menu);
        for (li = elements; li != NULL; li = li->next)
        {
            /* only accept toplevel menus */
            if (!GARCON_IS_MENU (li->data))
                continue;

            directory = garcon_menu_get_directory (li->data);
            if (G_UNLIKELY (directory == NULL))
                continue;

            items = NULL;

            xfce_settings_manager_dialog_menu_collect (li->data, &items);

            /* add the new category if it has visible items */
            if (G_LIKELY (items != NULL))
            {
                /* insert new items in main store */
                items = g_list_sort (items, xfce_settings_manager_dialog_menu_sort);
                for (lp = items; lp != NULL; lp = lp->next)
                {
                    gtk_list_store_insert_with_values (dialog->store, NULL, i++,
                        COLUMN_NAME, garcon_menu_item_get_name (lp->data),
                        COLUMN_ICON_NAME, garcon_menu_item_get_icon_name (lp->data),
                        COLUMN_TOOLTIP, garcon_menu_item_get_comment (lp->data),
                        COLUMN_MENU_ITEM, lp->data,
                        COLUMN_MENU_DIRECTORY, directory, -1);
                }
                g_list_free (items);

                /* add the new category to the box */
                xfce_settings_manager_dialog_add_category (dialog, directory);
            }
        }

        g_list_free (elements);
    }
    else
    {
        g_critical ("Failed to load menu: %s", error->message);
        g_error_free (error);
    }
}


GtkWidget *
xfce_settings_manager_dialog_new (void)
{
    return g_object_new (XFCE_TYPE_SETTINGS_MANAGER_DIALOG, NULL);
}



void
xfce_settings_manager_dialog_show_dialog (XfceSettingsManagerDialog *dialog,
                                          const gchar               *dialog_name)
{

}
