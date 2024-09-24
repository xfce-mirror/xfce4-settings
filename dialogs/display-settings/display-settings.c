/*
 *  Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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
#include "config.h"
#endif

#include "display-settings.h"
#include "identity-popup_ui.h"
#include "scrollarea.h"

#ifdef ENABLE_WAYLAND
#include "display-settings-wayland.h"

#include <gdk/gdkwayland.h>
#endif

#ifdef HAVE_XRANDR
#include "display-settings-x11.h"

#include <gdk/gdkx.h>
#define WINDOWING_IS_X11() GDK_IS_X11_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_X11() FALSE
#endif

#include "common/display-profiles.h"

#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#else
#define gtk_layer_is_supported() FALSE
#endif



#define get_instance_private(instance) \
    ((XfceDisplaySettingsPrivate *) xfce_display_settings_get_instance_private (XFCE_DISPLAY_SETTINGS (instance)))

static void
xfce_display_settings_finalize (GObject *object);



typedef struct _XfceDisplaySettingsPrivate
{
    XfconfChannel *channel;
    GtkBuilder *builder;
    GtkWidget *scroll_area;
    GHashTable *popups;
    GList *outputs;
    guint selected_output_id;
    gboolean supports_alpha;
    gboolean opt_minimal;
} XfceDisplaySettingsPrivate;



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfceDisplaySettings, xfce_display_settings, G_TYPE_OBJECT);



static void
xfce_display_settings_class_init (XfceDisplaySettingsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = xfce_display_settings_finalize;

    g_signal_new ("outputs-changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
xfce_display_settings_init (XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);

    priv->channel = xfconf_channel_get ("displays");
    priv->builder = gtk_builder_new ();
    priv->scroll_area = (GtkWidget *) foo_scroll_area_new ();
    g_signal_connect (priv->scroll_area, "destroy", G_CALLBACK (gtk_widget_destroyed), &priv->scroll_area);
    priv->popups = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) gtk_widget_destroy);
}



static void
free_output (gpointer data)
{
    XfceOutput *output = data;
    g_free (output->mode);
    for (guint n = 0; n < output->n_modes; n++)
        g_free (output->modes[n]);
    g_free (output->modes);
    g_free (output);
}



static void
xfce_display_settings_finalize (GObject *object)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (object);

    g_object_unref (priv->builder);
    if (priv->scroll_area != NULL)
        gtk_widget_destroy (priv->scroll_area);
    g_hash_table_destroy (priv->popups);
    g_list_free_full (priv->outputs, free_output);

    G_OBJECT_CLASS (xfce_display_settings_parent_class)->finalize (object);
}



XfceDisplaySettings *
xfce_display_settings_new (gboolean opt_minimal,
                           GError **error)
{
    XfceDisplaySettings *settings = NULL;

    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

#ifdef HAVE_XRANDR
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        settings = xfce_display_settings_x11_new (error);
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
        settings = xfce_display_settings_wayland_new (error);
#endif

    if (settings != NULL)
    {
        get_instance_private (settings)->opt_minimal = opt_minimal;
    }
    else if (error != NULL && *error == NULL)
    {
        g_set_error (error, 0, 0, "Display settings are not supported on this windowing environment");
    }

    return settings;
}



gboolean
xfce_display_settings_is_minimal (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), FALSE);
    return get_instance_private (settings)->opt_minimal;
}



void
xfce_display_settings_set_minimal (XfceDisplaySettings *settings,
                                   gboolean minimal)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    get_instance_private (settings)->opt_minimal = minimal;
}



GtkBuilder *
xfce_display_settings_get_builder (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return get_instance_private (settings)->builder;
}



XfconfChannel *
xfce_display_settings_get_channel (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return get_instance_private (settings)->channel;
}



GtkWidget *
xfce_display_settings_get_scroll_area (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return get_instance_private (settings)->scroll_area;
}



GHashTable *
xfce_display_settings_get_popups (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return get_instance_private (settings)->popups;
}



GList *
xfce_display_settings_get_outputs (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return get_instance_private (settings)->outputs;
}



void
xfce_display_settings_set_outputs (XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    guint n_outputs;
    gint x = 0, y = 0;

    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));

    if (priv->outputs != NULL)
    {
        g_list_free_full (priv->outputs, free_output);
        priv->outputs = NULL;
    }

    n_outputs = xfce_display_settings_get_n_outputs (settings);
    for (guint n = 0; n < n_outputs; n++)
    {
        XfceOutput *output = XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_output (settings, n);
        priv->outputs = g_list_prepend (priv->outputs, output);
    }
    priv->outputs = g_list_reverse (priv->outputs);

    /* lay out monitors horizontally (active first) to avoid having them overlapped initially */
    for (GList *lp = priv->outputs; lp != NULL; lp = lp->next)
    {
        XfceOutput *output = lp->data;
        if (output->active)
        {
            GdkRectangle geom;
            xfce_display_settings_get_geometry (settings, output->id, &geom);
            y = MAX (output->y, y);
            x = MAX (output->x + geom.width, x);
        }
    }

    for (GList *lp = priv->outputs; lp != NULL; lp = lp->next)
    {
        XfceOutput *output = lp->data;
        if (!output->active)
        {
            output->x = x;
            output->y = y;
            xfce_display_settings_set_position (settings, output->id, x, y);
            x += output->mode->width;
        }
    }
}



static void
popup_screen_changed (GtkWidget *widget,
                      GdkScreen *old_screen,
                      XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    GdkScreen *screen = gtk_widget_get_screen (widget);
    GdkVisual *visual;

    if (gdk_screen_is_composited (screen))
    {
        visual = gdk_screen_get_rgba_visual (screen);
        priv->supports_alpha = TRUE;
    }
    else
    {
        visual = gdk_screen_get_system_visual (screen);
        priv->supports_alpha = FALSE;
    }

    gtk_widget_set_visual (widget, visual);
}



static gboolean
popup_draw (GtkWidget *popup,
            cairo_t *cr,
            XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    GtkAllocation allocation;
    cairo_pattern_t *vertical_gradient, *innerstroke_gradient, *selected_gradient, *selected_innerstroke_gradient;
    gint radius = 10;
    gboolean selected = (g_hash_table_lookup (priv->popups, GINT_TO_POINTER (priv->selected_output_id)) == popup);

    gtk_widget_get_allocation (GTK_WIDGET (popup), &allocation);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    /* create the various gradients */
    vertical_gradient = cairo_pattern_create_linear (0, 0, 0, allocation.height);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0, 0.25, 0.25, 0.25);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0.24, 0.15, 0.15, 0.15);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0.6, 0.0, 0.0, 0.0);

    innerstroke_gradient = cairo_pattern_create_linear (0, 0, 0, allocation.height);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0, 0.35, 0.35, 0.35);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.4, 0.25, 0.25, 0.25);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.7, 0.15, 0.15, 0.15);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.85, 0.0, 0.0, 0.0);

    selected_gradient = cairo_pattern_create_linear (0, 0, 0, allocation.height);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0, 0.05, 0.20, 0.46);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.4, 0.05, 0.12, 0.25);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.6, 0.05, 0.10, 0.20);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.8, 0.0, 0.02, 0.05);

    selected_innerstroke_gradient = cairo_pattern_create_linear (0, 0, 0, allocation.height);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0, 0.15, 0.45, 0.75);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0.7, 0.0, 0.15, 0.25);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0.85, 0.0, 0.0, 0.0);

    /* compositing is not available, so just set the background color */
    if (!priv->supports_alpha)
    {
        /* draw a filled rectangle with outline */
        cairo_set_line_width (cr, 1.0);
        cairo_set_source (cr, vertical_gradient);
        if (selected)
            cairo_set_source (cr, selected_gradient);
        cairo_paint (cr);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_rectangle (cr, 0.5, 0.5, allocation.width - 0.5, allocation.height - 0.5);
        cairo_stroke (cr);

        /* draw the inner stroke */
        cairo_set_source_rgb (cr, 0.35, 0.35, 0.35);
        if (selected)
            cairo_set_source_rgb (cr, 0.15, 0.45, 0.75);
        cairo_move_to (cr, 1.5, 1.5);
        cairo_line_to (cr, allocation.width - 1, 1.5);
        cairo_stroke (cr);
        cairo_set_source (cr, innerstroke_gradient);
        if (selected)
            cairo_set_source (cr, selected_innerstroke_gradient);
        cairo_move_to (cr, 1.5, 1.5);
        cairo_line_to (cr, 1.5, allocation.height - 1.0);
        cairo_move_to (cr, allocation.width - 1.5, 1.5);
        cairo_line_to (cr, allocation.width - 1.5, allocation.height - 1.0);
        cairo_stroke (cr);
    }
    /* draw rounded corners */
    else
    {
        cairo_set_source_rgba (cr, 0, 0, 0, 0);
        cairo_paint (cr);

        /* draw a filled rounded rectangle with outline */
        cairo_set_line_width (cr, 1.0);
        cairo_move_to (cr, 0.5, allocation.height + 0.5);
        cairo_line_to (cr, 0.5, radius + 0.5);
        cairo_arc (cr, radius + 0.5, radius + 0.5, radius, 3.14, 3.0 * 3.14 / 2.0);
        cairo_line_to (cr, allocation.width - 0.5 - radius, 0.5);
        cairo_arc (cr, allocation.width - 0.5 - radius, radius + 0.5, radius, 3.0 * 3.14 / 2.0, 0.0);
        cairo_line_to (cr, allocation.width - 0.5, allocation.height + 0.5);
        cairo_set_source (cr, vertical_gradient);
        if (selected)
            cairo_set_source (cr, selected_gradient);
        cairo_fill_preserve (cr);
        cairo_set_source_rgb (cr, 0.05, 0.05, 0.05);
        cairo_stroke (cr);

        /* draw the inner stroke */
        cairo_set_source_rgb (cr, 0.35, 0.35, 0.35);
        if (selected)
            cairo_set_source_rgb (cr, 0.15, 0.45, 0.75);
        cairo_arc (cr, radius + 1.5, radius + 1.5, radius, 3.14, 3.0 * 3.14 / 2.0);
        cairo_line_to (cr, allocation.width - 1.5 - radius, 1.5);
        cairo_arc (cr, allocation.width - 1.5 - radius, radius + 1.5, radius, 3.0 * 3.14 / 2.0, 0.0);
        cairo_stroke (cr);
        cairo_set_source (cr, innerstroke_gradient);
        if (selected)
            cairo_set_source (cr, selected_innerstroke_gradient);
        cairo_move_to (cr, 1.5, radius + 1.0);
        cairo_line_to (cr, 1.5, allocation.height - 1.0);
        cairo_move_to (cr, allocation.width - 1.5, radius + 1.0);
        cairo_line_to (cr, allocation.width - 1.5, allocation.height - 1.0);
        cairo_stroke (cr);

        cairo_close_path (cr);
    }

    cairo_pattern_destroy (vertical_gradient);
    cairo_pattern_destroy (innerstroke_gradient);
    cairo_pattern_destroy (selected_gradient);
    cairo_pattern_destroy (selected_innerstroke_gradient);

    return FALSE;
}

static GtkWidget *
popup_get (XfceDisplaySettings *settings,
           gint output_id)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    GtkWidget *popup = NULL;
    GObject *label;
    GdkRectangle geom;
    const gchar *color_hex = "#FFFFFF";
    gchar *text;
    gint window_width, window_height;

    if (gtk_builder_add_from_string (priv->builder, identity_popup_ui, identity_popup_ui_length, NULL) != 0)
    {
        popup = GTK_WIDGET (gtk_builder_get_object (priv->builder, "popup"));
        gtk_widget_set_name (popup, "XfceDisplayDialogPopup");

        gtk_widget_set_app_paintable (popup, TRUE);
        g_signal_connect (G_OBJECT (popup), "draw", G_CALLBACK (popup_draw), settings);
        g_signal_connect (G_OBJECT (popup), "screen-changed", G_CALLBACK (popup_screen_changed), settings);

        label = gtk_builder_get_object (priv->builder, "display_number");
        if (xfce_display_settings_get_n_outputs (settings) > 1)
        {
            text = g_markup_printf_escaped ("<span foreground='%s' font='Bold 28'>%d</span>",
                                            color_hex, output_id + 1);
            gtk_label_set_markup (GTK_LABEL (label), text);
            g_free (text);
        }
        else
        {
            gtk_label_set_text (GTK_LABEL (label), NULL);
            gtk_widget_set_margin_start (GTK_WIDGET (label), 0);
            gtk_widget_set_margin_end (GTK_WIDGET (label), 0);
        }

        label = gtk_builder_get_object (priv->builder, "display_name");
        text = g_markup_printf_escaped ("<span foreground='%s' font='Bold 10'>%s %s</span>",
                                        color_hex, _("Display:"),
                                        xfce_display_settings_get_friendly_name (settings, output_id));
        gtk_label_set_markup (GTK_LABEL (label), text);
        g_free (text);

        xfce_display_settings_get_geometry (settings, output_id, &geom);
        label = gtk_builder_get_object (priv->builder, "display_details");
        text = g_markup_printf_escaped ("<span foreground='%s' font='Light 10'>%s %i x %i</span>", color_hex,
                                        _("Resolution:"), geom.width, geom.height);
        gtk_label_set_markup (GTK_LABEL (label), text);
        g_free (text);

        popup_screen_changed (GTK_WIDGET (popup), NULL, settings);
        gtk_window_get_size (GTK_WINDOW (popup), &window_width, &window_height);

#ifdef HAVE_GTK_LAYER_SHELL
        if (gtk_layer_is_supported ())
        {
            GdkMonitor *monitor = xfce_display_settings_get_monitor (settings, output_id);
            if (monitor != NULL)
            {
                gtk_layer_init_for_window (GTK_WINDOW (popup));
                gtk_layer_set_layer (GTK_WINDOW (popup), GTK_LAYER_SHELL_LAYER_OVERLAY);
                gtk_layer_set_exclusive_zone (GTK_WINDOW (popup), -1);
                gtk_layer_set_anchor (GTK_WINDOW (popup), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (popup), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
                gtk_layer_set_margin (GTK_WINDOW (popup), GTK_LAYER_SHELL_EDGE_LEFT, (geom.width - window_width) / 2);
                gtk_layer_set_margin (GTK_WINDOW (popup), GTK_LAYER_SHELL_EDGE_TOP, geom.height - window_height);
                gtk_layer_set_monitor (GTK_WINDOW (popup), monitor);
                gtk_widget_set_size_request (popup, window_width, window_height);
                gtk_window_present (GTK_WINDOW (popup));
            }
        }
        else
#endif
        {
            gtk_window_move (GTK_WINDOW (popup),
                             geom.x + (geom.width - window_width) / 2,
                             geom.y + geom.height - window_height);
            gtk_window_present (GTK_WINDOW (popup));
        }
    }

    return popup;
}



void
xfce_display_settings_populate_profile_list (XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    GObject *treeview = gtk_builder_get_object (priv->builder, "randr-profile");
    GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
    gchar **display_infos = xfce_display_settings_get_display_infos (settings);
    GList *profiles = display_settings_get_profiles (display_infos, priv->channel, FALSE);
    gchar *active_profile = xfconf_channel_get_string (priv->channel, "/ActiveProfile", "Default");

    gtk_list_store_clear (store);

    /* populate treeview */
    for (GList *lp = profiles; lp != NULL; lp = lp->next)
    {
        GtkTreeIter iter;
        gchar *property, *profile_name, *markup;
        const gchar *profile = lp->data;
        GIcon *icon = NULL;
        gboolean matches = display_settings_profile_matches (profile, display_infos, priv->channel);

        /* use the display string value of the profile hash property */
        property = g_strdup_printf ("/%s", profile);
        profile_name = xfconf_channel_get_string (priv->channel, property, NULL);
        markup = matches ? g_strdup (profile_name) : g_strdup_printf ("<span alpha=\"50%%\">%s</span>", profile_name);

        /* highlight the currently active profile */
        if (g_strcmp0 (profile, active_profile) == 0)
        {
            icon = g_themed_icon_new_with_default_fallbacks ("object-select-symbolic");
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_ICON, icon,
                            COLUMN_NAME, profile_name,
                            COLUMN_HASH, profile,
                            COLUMN_MARKUP, markup,
                            COLUMN_MATCHES, matches,
                            -1);

        g_free (property);
        g_free (profile_name);
        g_free (markup);
        if (icon != NULL)
            g_object_unref (icon);
    }

    g_free (active_profile);
    g_strfreev (display_infos);
    g_list_free_full (profiles, g_free);
}



void
xfce_display_settings_populate_combobox (XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    GtkListStore *store;
    GObject *combobox;
    GtkTreeIter iter;
    guint n_output = xfce_display_settings_get_n_outputs (settings);
    gboolean selected = FALSE;

    /* create a new list store */
    store = gtk_list_store_new (N_OUTPUT_COLUMNS,
                                G_TYPE_STRING, /* COLUMN_OUTPUT_NAME */
                                G_TYPE_INT); /* COLUMN_OUTPUT_ID */

    /* set up the new combobox which will replace the above combobox */
    combobox = gtk_builder_get_object (priv->builder, "randr-outputs");
    gtk_combo_box_set_model (GTK_COMBO_BOX (combobox), GTK_TREE_MODEL (store));

    /* walk all the connected outputs */
    for (guint n = 0; n < n_output; n++)
    {
        const gchar *friendly_name = xfce_display_settings_get_friendly_name (settings, n);
        gchar *display_name;

        /* insert the output in the store */
        if (n_output > 1)
            display_name = g_strdup_printf ("%d - %s", n + 1, friendly_name);
        else
            display_name = g_strdup (friendly_name);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_OUTPUT_NAME, display_name,
                            COLUMN_OUTPUT_ID, n, -1);
        g_free (display_name);

        /* re-select output */
        if (n == xfce_display_settings_get_selected_output_id (settings))
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), n);
            selected = TRUE;
        }
    }

    /* if nothing was selected the previously selected output is no longer valid,
     * select the last in the list */
    if (!selected)
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), n_output - 1);

    /* release the store */
    g_object_unref (G_OBJECT (store));
}



void
xfce_display_settings_populate_popups (XfceDisplaySettings *settings)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));

    if (WINDOWING_IS_X11 () || gtk_layer_is_supported ())
    {
        XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
        guint n_outputs = xfce_display_settings_get_n_outputs (settings);

        g_hash_table_remove_all (priv->popups);
        for (guint n = 0; n < n_outputs; n++)
        {
            if (xfce_display_settings_is_active (settings, n))
                g_hash_table_insert (priv->popups, GINT_TO_POINTER (n), popup_get (settings, n));
        }
    }
}



void
xfce_display_settings_set_popups_visible (XfceDisplaySettings *settings,
                                          gboolean visible)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    GHashTableIter iter;
    gpointer popup;

    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));

    g_hash_table_iter_init (&iter, priv->popups);
    while (g_hash_table_iter_next (&iter, NULL, &popup))
        gtk_widget_set_visible (popup, visible);
}



void
xfce_display_settings_reload (XfceDisplaySettings *settings)
{
    XfceDisplaySettingsPrivate *priv = get_instance_private (settings);
    if (!priv->opt_minimal)
    {
        gboolean visible = xfconf_channel_get_bool (priv->channel, "/IdentityPopups", FALSE);

        xfce_display_settings_set_outputs (settings);

        xfce_display_settings_populate_combobox (settings);
        xfce_display_settings_populate_profile_list (settings);
        xfce_display_settings_populate_popups (settings);
        xfce_display_settings_set_popups_visible (settings, visible);

        foo_scroll_area_invalidate (FOO_SCROLL_AREA (priv->scroll_area));
    }
}



guint
xfce_display_settings_get_selected_output_id (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), 0);
    return get_instance_private (settings)->selected_output_id;
}



void
xfce_display_settings_set_selected_output_id (XfceDisplaySettings *settings,
                                              guint output_id)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    get_instance_private (settings)->selected_output_id = output_id;
}



guint
xfce_display_settings_get_n_outputs (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), 0);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_n_outputs (settings);
}



guint
xfce_display_settings_get_n_active_outputs (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), 0);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_n_active_outputs (settings);
}



gchar **
xfce_display_settings_get_display_infos (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_display_infos (settings);
}



MirroredState
xfce_display_settings_get_mirrored_state (XfceDisplaySettings *settings)
{
    GdkRectangle geom_1, geom_2;
    guint n_outputs = xfce_display_settings_get_n_outputs (settings);
    gboolean cloned = TRUE;
    gboolean mirrored = TRUE;

    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), MIRRORED_STATE_NONE);

    if (xfce_display_settings_get_n_active_outputs (settings) <= 1)
        return MIRRORED_STATE_NONE;

    /* get geometry of first active output */
    for (guint n = 0; n < n_outputs; n++)
    {
        if (!xfce_display_settings_is_active (settings, n))
            continue;
        xfce_display_settings_get_geometry (settings, n, &geom_1);
        break;
    }

    /* check if mirror settings are on */
    for (guint n = 0; n < n_outputs; n++)
    {
        if (!xfce_display_settings_is_active (settings, n))
            continue;

        xfce_display_settings_get_geometry (settings, n, &geom_2);
        mirrored &= geom_1.x == geom_2.x && geom_1.y == geom_2.y;
        cloned &= gdk_rectangle_equal (&geom_1, &geom_2);
        if (!mirrored)
            break;
    }

    return cloned ? MIRRORED_STATE_CLONED : mirrored ? MIRRORED_STATE_MIRRORED
                                                     : MIRRORED_STATE_NONE;
}



GdkMonitor *
xfce_display_settings_get_monitor (XfceDisplaySettings *settings,
                                   guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_monitor (settings, output_id);
}



const gchar *
xfce_display_settings_get_friendly_name (XfceDisplaySettings *settings,
                                         guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), NULL);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_friendly_name (settings, output_id);
}



void
xfce_display_settings_get_geometry (XfceDisplaySettings *settings,
                                    guint output_id,
                                    GdkRectangle *geometry)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_geometry (settings, output_id, geometry);
}



RotationFlags
xfce_display_settings_get_rotation (XfceDisplaySettings *settings,
                                    guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), ROTATION_FLAGS_0);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_rotation (settings, output_id);
}



void
xfce_display_settings_set_rotation (XfceDisplaySettings *settings,
                                    guint output_id,
                                    RotationFlags rotation)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    g_return_if_fail (xfce_display_settings_get_n_outputs (settings) > output_id);

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->set_rotation (settings, output_id, rotation);
    if (!get_instance_private (settings)->opt_minimal)
    {
        XfceOutput *output = g_list_nth (get_instance_private (settings)->outputs, output_id)->data;
        output->rotation = rotation;
    }
}



RotationFlags
xfce_display_settings_get_rotations (XfceDisplaySettings *settings,
                                     guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), ROTATION_FLAGS_0);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_rotations (settings, output_id);
}



gdouble
xfce_display_settings_get_scale (XfceDisplaySettings *settings,
                                 guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), 0);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->get_scale (settings, output_id);
}



void
xfce_display_settings_set_scale (XfceDisplaySettings *settings,
                                 guint output_id,
                                 gdouble scale)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    g_return_if_fail (xfce_display_settings_get_n_outputs (settings) > output_id);

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->set_scale (settings, output_id, scale);
    if (!get_instance_private (settings)->opt_minimal)
    {
        XfceOutput *output = g_list_nth (get_instance_private (settings)->outputs, output_id)->data;
        output->scale = scale;
    }
}



void
xfce_display_settings_set_mode (XfceDisplaySettings *settings,
                                guint output_id,
                                guint mode_id)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    g_return_if_fail (xfce_display_settings_get_n_outputs (settings) > output_id);

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->set_mode (settings, output_id, mode_id);
    if (!get_instance_private (settings)->opt_minimal)
    {
        XfceOutput *output = g_list_nth (get_instance_private (settings)->outputs, output_id)->data;
        XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->update_output_mode (settings, output, mode_id);
    }
}



void
xfce_display_settings_set_position (XfceDisplaySettings *settings,
                                    guint output_id,
                                    gint x,
                                    gint y)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    g_return_if_fail (xfce_display_settings_get_n_outputs (settings) > output_id);

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->set_position (settings, output_id, x, y);
    if (!get_instance_private (settings)->opt_minimal)
    {
        XfceOutput *output = g_list_nth (get_instance_private (settings)->outputs, output_id)->data;
        output->x = x;
        output->y = y;
    }
}



gboolean
xfce_display_settings_is_active (XfceDisplaySettings *settings,
                                 guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), FALSE);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->is_active (settings, output_id);
}



void
xfce_display_settings_set_active (XfceDisplaySettings *settings,
                                  guint output_id,
                                  gboolean active)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    g_return_if_fail (xfce_display_settings_get_n_outputs (settings) > output_id);

    if (xfce_display_settings_is_active (settings, output_id) == active)
        return;

    /* init/reset geometry except position */
    xfce_display_settings_set_mode (settings, output_id, -1U);
    xfce_display_settings_set_scale (settings, output_id, 1.0);
    xfce_display_settings_set_rotation (settings, output_id, ROTATION_FLAGS_0);

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->set_active (settings, output_id, active);

    if (!get_instance_private (settings)->opt_minimal)
    {
        XfceOutput *output = g_list_nth (get_instance_private (settings)->outputs, output_id)->data;
        output->active = active;
        XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->update_output_active (settings, output, active);
    }
}



gboolean
xfce_display_settings_is_primary (XfceDisplaySettings *settings,
                                  guint output_id)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), FALSE);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->is_primary (settings, output_id);
}



void
xfce_display_settings_set_primary (XfceDisplaySettings *settings,
                                   guint output_id,
                                   gboolean primary)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->set_primary (settings, output_id, primary);
}



gboolean
xfce_display_settings_is_mirrored (XfceDisplaySettings *settings,
                                   guint output_id_1,
                                   guint output_id_2)
{
    GdkRectangle geom_1, geom_2;

    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), FALSE);

    if (!xfce_display_settings_is_active (settings, output_id_1)
        || !xfce_display_settings_is_active (settings, output_id_2))
        return FALSE;

    xfce_display_settings_get_geometry (settings, output_id_1, &geom_1);
    xfce_display_settings_get_geometry (settings, output_id_2, &geom_2);
    return geom_1.x == geom_2.x && geom_1.y == geom_2.y;
}



ExtendedMode
xfce_display_settings_get_extended_mode (XfceDisplaySettings *settings,
                                         guint output_id_1,
                                         guint output_id_2)
{
    GdkRectangle geom_1, geom_2;

    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), EXTENDED_MODE_NONE);

    if (!xfce_display_settings_is_active (settings, output_id_1)
        || !xfce_display_settings_is_active (settings, output_id_2))
        return EXTENDED_MODE_NONE;

    xfce_display_settings_get_geometry (settings, output_id_1, &geom_1);
    xfce_display_settings_get_geometry (settings, output_id_2, &geom_2);
    if (geom_2.x == geom_1.x + geom_1.width)
        return EXTENDED_MODE_RIGHT;
    else if (geom_1.x == geom_2.x + geom_2.width)
        return EXTENDED_MODE_LEFT;
    else if (geom_1.y == geom_2.y + geom_2.height)
        return EXTENDED_MODE_UP;
    else if (geom_2.y == geom_1.y + geom_1.height)
        return EXTENDED_MODE_DOWN;
    else
        return EXTENDED_MODE_NONE;
}



gboolean
xfce_display_settings_is_clonable (XfceDisplaySettings *settings)
{
    g_return_val_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings), FALSE);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->is_clonable (settings);
}



void
xfce_display_settings_save (XfceDisplaySettings *settings,
                            const gchar *scheme)
{
    gchar *prop;

    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));

    prop = g_strdup_printf ("/%s", scheme);
    xfconf_channel_reset_property (get_instance_private (settings)->channel, prop, TRUE);
    g_free (prop);

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->save (settings, scheme);
}



void
xfce_display_settings_mirror (XfceDisplaySettings *settings)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));

    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->mirror (settings);
    for (GList *lp = get_instance_private (settings)->outputs; lp != NULL; lp = lp->next)
        XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->update_output_mirror (settings, lp->data);
}



void
xfce_display_settings_unmirror (XfceDisplaySettings *settings)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->unmirror (settings);
    for (GList *lp = get_instance_private (settings)->outputs; lp != NULL; lp = lp->next)
        XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->update_output_mirror (settings, lp->data);
}



void
xfce_display_settings_extend (XfceDisplaySettings *settings,
                              guint output_id_1,
                              guint output_id_2,
                              ExtendedMode mode)
{
    g_return_if_fail (XFCE_IS_DISPLAY_SETTINGS (settings));
    g_return_if_fail (mode != EXTENDED_MODE_NONE);
    return XFCE_DISPLAY_SETTINGS_GET_CLASS (settings)->extend (settings, output_id_1, output_id_2, mode);
}
