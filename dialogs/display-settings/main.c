/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010 Lionel Le Folgoc <lionel@lefolgoc.net>
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
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <math.h>

#include <glib.h>
#include <gtk/gtk.h>
#ifdef HAVE_XRANDR
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#define WINDOWING_IS_X11() GDK_IS_X11_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_X11() FALSE
#endif

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "common/display-profiles.h"
#include "display-dialog_ui.h"
#include "confirmation-dialog_ui.h"
#include "minimal-display-dialog_ui.h"
#include "profile-changed-dialog_ui.h"
#include "scrollarea.h"
#include "display-settings.h"

#define MARGIN  16
#define NOTIFY_PROP_DEFAULT 1

enum
{
    RESOLUTION_COLUMN_COMBO_NAME,
    RESOLUTION_COLUMN_COMBO_MARKUP,
    RESOLUTION_COLUMN_COMBO_VALUE,
    N_RESOLUTION_COMBO_COLUMNS
};

enum
{
    COLUMN_COMBO_NAME,
    COLUMN_COMBO_VALUE,
    N_COMBO_COLUMNS
};



typedef struct _XfceRotation
{
    RotationFlags rotation;
    const gchar *name;
} XfceRotation;

/* Rotation name conversion */
static const XfceRotation rotation_names[] =
{
    { ROTATION_FLAGS_0,   N_("None") },
    { ROTATION_FLAGS_90,  N_("Left") },
    { ROTATION_FLAGS_180, N_("Inverted") },
    { ROTATION_FLAGS_270, N_("Right") }
};

/* Reflection name conversion */
static const XfceRotation reflection_names[] =
{
    { ROTATION_FLAGS_0,                                    N_("None") },
    { ROTATION_FLAGS_REFLECT_X,                            N_("Horizontal") },
    { ROTATION_FLAGS_REFLECT_Y,                            N_("Vertical") },
    { ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_REFLECT_Y, N_("Horizontal and Vertical") }
};



/* Option entries */
static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static gboolean opt_minimal = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "minimal", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_minimal, N_("Minimal interface to set up an external output"), NULL},
    { NULL }
};

/* Keep track of the initially active profile */
gchar *active_profile = NULL;

/* Outputs Combobox */
GtkWidget *apply_button = NULL;

/* Show nice representation of the display ratio */
typedef struct _XfceRatio
{
    gboolean precise;
    gdouble ratio;
    const gchar *desc;
} XfceRatio;

static GHashTable *display_ratio = NULL;
/* adding the +0.5 to result in "rounding" instead of trunc() */
#define _ONE_DIGIT_PRECISION(x) ((gdouble)((gint)((x)*10.0+0.5))/10.0)
#define _TWO_DIGIT_PRECISION(x) ((gdouble)((gint)((x)*100.0+0.5))/100.0)

/* most prominent ratios */
/* adding in least exact order to find most precise */
static XfceRatio ratio_table[] = {
    { FALSE, _ONE_DIGIT_PRECISION(16.0/9.0), "<span font_style='italic'>≈16:9</span>" },
    { FALSE, _TWO_DIGIT_PRECISION(16.0/9.0), "<span font_style='italic'>≈16:9</span>" },
    { TRUE, 16.0/9.0, "16:9" },
    { FALSE, _ONE_DIGIT_PRECISION(16.0/10.0), "<span font_style='italic'>≈16:10</span>" },
    { FALSE, _TWO_DIGIT_PRECISION(16.0/10.0), "<span font_style='italic'>≈16:10</span>" },
    { TRUE, 16.0/10.0, "16:10" },
    /* _ONE_DIGIT_PRECISION(4.0/3.0) would be mixed up with 5/4 */
    { FALSE, _TWO_DIGIT_PRECISION(4.0/3.0), "<span font_style='italic'>≈4:3</span>" },
    { TRUE, 4.0/3.0, "4:3" },
    { FALSE, _ONE_DIGIT_PRECISION(21.0/9.0), "<span font_style='italic'>≈21:9</span>" },
    { FALSE, _TWO_DIGIT_PRECISION(21.0/9.0), "<span font_style='italic'>≈21:9</span>" },
    { TRUE, 21.0/9.0, "21:9" },
    { FALSE, 0.0 , NULL }
};

static void display_setting_mirror_displays_populate         (XfceDisplaySettings *settings);

static void display_settings_minimal_profile_apply           (GtkToggleButton *widget,
                                                              XfconfChannel   *channel);

static void
display_settings_changed (void)
{
    gtk_widget_set_sensitive (GTK_WIDGET (apply_button), TRUE);
}

static XfceOutput*
get_nth_xfce_output (XfceDisplaySettings *settings,
                     guint id)
{
    XfceOutput *output = NULL;
    GList * entry = NULL;

    entry = g_list_nth (xfce_display_settings_get_outputs (settings), id);

    if (entry)
        output = entry->data;

    return output;
}

static gboolean
display_setting_combo_box_get_value (GtkComboBox *combobox,
                                     gint        *value,
                                     gboolean     resolution)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        model = gtk_combo_box_get_model (combobox);
        if (resolution)
            gtk_tree_model_get (model, &iter, RESOLUTION_COLUMN_COMBO_VALUE, value, -1);
        else
            gtk_tree_model_get (model, &iter, COLUMN_COMBO_VALUE, value, -1);

        return TRUE;
    }

    return FALSE;
}

typedef struct _ConfirmationDialog
{
    GtkBuilder *builder;
    gint count;
    guint source_id;
} ConfirmationDialog;

static gboolean
display_settings_update_time_label (gpointer user_data)
{
    ConfirmationDialog *confirmation_dialog = user_data;
    GObject *dialog;

    dialog = gtk_builder_get_object (confirmation_dialog->builder, "dialog1");
    confirmation_dialog->count--;

    if (confirmation_dialog->count <= 0)
    {
        gtk_dialog_response (GTK_DIALOG (dialog), 1);
        confirmation_dialog->source_id = 0;

        return FALSE;
    }
    else
    {
        gchar   *label_string;

        label_string = g_strdup_printf (_("The previous configuration will be restored in <b>%i"
                                          " seconds</b> if you do not reply to this question."),
                                        confirmation_dialog->count);

        gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog), "%s", label_string);
        g_free (label_string);

        return TRUE;
    }

    return TRUE;
}

/* Returns true if the configuration is to be kept or false if it is to
 * be reverted */
static gboolean
display_setting_timed_confirmation (XfceDisplaySettings *settings)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject    *main_dialog;
    GError     *error = NULL;
    gint        response_id;

    /* Lock the main UI */
    main_dialog = gtk_builder_get_object (builder, "display-dialog");

    if (gtk_builder_add_from_string (builder, confirmation_dialog_ui,
                                     confirmation_dialog_ui_length, &error) != 0)
    {
        GObject *dialog;
        ConfirmationDialog *confirmation_dialog;

        confirmation_dialog = g_new0 (ConfirmationDialog, 1);
        confirmation_dialog->builder = builder;
        confirmation_dialog->count = 10;

        dialog = gtk_builder_get_object (builder, "dialog1");

        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_dialog));
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        confirmation_dialog->source_id =
            g_timeout_add_seconds (1, display_settings_update_time_label, confirmation_dialog);

        response_id = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (GTK_WIDGET (dialog));
        if (confirmation_dialog->source_id != 0)
            g_source_remove (confirmation_dialog->source_id);
        g_free (confirmation_dialog);
    }
    else
    {
        response_id = 2;
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    return ((response_id == 2) ? TRUE : FALSE);
}

/*
 * Encapsulates display_setting_timed_confirmation, automatically uses Fallback on FALSE
 * Returns TRUE if the configuration was kept, FALSE if the configuration was replaced with the Fallback
 */
static gboolean
display_setting_ask_fallback (XfceDisplaySettings *settings)
{
    /* Ask user confirmation (or recover to'Fallback on timeout') */
    if (display_setting_timed_confirmation (settings))
    {
        /* Update the Fallback */
        guint n_outputs = xfce_display_settings_get_n_outputs (settings);
        for (guint n = 0; n < n_outputs; n++)
            xfce_display_settings_save (settings, n, "Fallback");
        return TRUE;
    }
    else
    {
        /* Recover to Fallback (will as well overwrite default xfconf settings) */
        xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Fallback");
        foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
        return FALSE;
    }
}

static void
display_setting_custom_scale_changed (GtkSpinButton *spinbutton,
                                      XfceDisplaySettings *settings)
{
    guint selected_id = xfce_display_settings_get_selected_output_id (settings);
    gdouble scale = gtk_spin_button_get_value (spinbutton);
    xfce_display_settings_set_scale_x (settings, selected_id, scale);
    xfce_display_settings_set_scale_y (settings, selected_id, scale);

    display_settings_changed ();
}

static void
display_setting_scale_changed (GtkComboBox *combobox,
                               GtkBuilder  *builder)
{
    GObject      *revealer, *spin_scalex, *spin_scaley;
    GValue        prop = { 0, };
    gdouble       scale;
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        return;

    revealer = gtk_builder_get_object (builder, "revealer-scale");
    spin_scalex = gtk_builder_get_object (builder, "spin-scale-x");
    spin_scaley = gtk_builder_get_object (builder, "spin-scale-y");

    gtk_combo_box_get_active_iter (combobox, &iter);
    model = gtk_combo_box_get_model (combobox);
    gtk_tree_model_get_value (model, &iter, COLUMN_COMBO_VALUE, &prop);
    scale = g_value_get_double (&prop);

    /* Show the spinbuttons if the combobox is set to "Custom:" */
    if (scale == -1.0)
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
    }
    else
    {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scalex), scale);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scaley), scale);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
    }

    g_value_unset (&prop);
    display_settings_changed ();
}

static gboolean
display_setting_scale_set_active (GtkTreeModel *model,
                                  GtkTreePath  *path,
                                  GtkTreeIter  *iter,
                                  gpointer      data)
{
    XfceDisplaySettings *settings = data;
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject *combobox = gtk_builder_get_object (builder, "randr-scale");
    guint selected_id = xfce_display_settings_get_selected_output_id (settings);
    GValue prop = { 0, };
    gboolean found = FALSE;

    gtk_tree_model_get_value (model, iter, COLUMN_COMBO_VALUE, &prop);

    if (g_value_get_double (&prop) == xfce_display_settings_get_scale_x (settings, selected_id))
    {
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), iter);
        found = TRUE;
    }
    else
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), -1);

    g_value_unset (&prop);

    return found;
}

static void
display_setting_scale_populate (XfceDisplaySettings *settings,
                                guint selected_id)
{
    GtkBuilder   *builder = xfce_display_settings_get_builder (settings);
    GtkTreeModel *model;
    GObject      *combobox, *label, *revealer, *spin_scalex, *spin_scaley;
    guint         n;

    combobox = gtk_builder_get_object (builder, "randr-scale");
    label = gtk_builder_get_object (builder, "label-scale");
    revealer = gtk_builder_get_object (builder, "revealer-scale");

    /* disable it if no mode is selected */
    if (!xfce_display_settings_is_active (settings, selected_id))
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), -1);
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Sync the current scale value to the spinbuttons */
    spin_scalex = gtk_builder_get_object (builder, "spin-scale-x");
    spin_scaley = gtk_builder_get_object (builder, "spin-scale-y");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scalex),
                               xfce_display_settings_get_scale_x (settings, selected_id));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_scaley),
                               xfce_display_settings_get_scale_y (settings, selected_id));

    /* Block the "changed" signal while determining the active item */
    g_signal_handlers_block_by_func (combobox, display_setting_scale_changed,
                                     builder);

    /* If the current scale is part of the presets set it as active */
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_tree_model_foreach (model, display_setting_scale_set_active, settings);

    /* If the current scale is not found in the presets we select "Custom:", which
       is the last element of the liststore */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
    {
        GtkTreePath *path;
        GtkTreeIter  iter;

        n = gtk_tree_model_iter_n_children (model, NULL);
        path = gtk_tree_path_new_from_indices (n - 1, -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
        gtk_tree_path_free (path);
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
    }
    else
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_scale_changed,
                                       builder);
}

static void
display_setting_reflections_changed (GtkComboBox *combobox,
                                     XfceDisplaySettings *settings)
{
    RotationFlags rotation;
    gint value;
    guint selected_id;

    if (!display_setting_combo_box_get_value (combobox, &value, FALSE))
        return;

    /* Set new rotation */
    selected_id = xfce_display_settings_get_selected_output_id (settings);
    rotation = xfce_display_settings_get_rotation (settings, selected_id);
    rotation &= ~REFLECTION_MASK;
    rotation |= value;
    xfce_display_settings_set_rotation (settings, selected_id, rotation);

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
}

static void
display_setting_reflections_populate (XfceDisplaySettings *settings,
                                      guint selected_id)
{
    GtkBuilder   *builder = xfce_display_settings_get_builder (settings);
    GtkTreeModel *model;
    GObject      *combobox, *label;
    RotationFlags reflections;
    RotationFlags active_reflection;
    guint         n;
    GtkTreeIter   iter;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-reflection");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
    label = gtk_builder_get_object (builder, "label-reflection");

    /* disable it if no mode is selected */
    if (!xfce_display_settings_is_active (settings, selected_id))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_reflections_changed, settings);

    /* Load only supported reflections */
    reflections = xfce_display_settings_get_rotations (settings, selected_id) & REFLECTION_MASK;
    active_reflection = xfce_display_settings_get_rotation (settings, selected_id) & REFLECTION_MASK;

    /* Try to insert the reflections */
    for (n = 0; n < G_N_ELEMENTS (reflection_names); n++)
    {
        if ((reflections & reflection_names[n].rotation) == reflection_names[n].rotation)
        {
            /* Insert */
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, _(reflection_names[n].name),
                                COLUMN_COMBO_VALUE, reflection_names[n].rotation, -1);

            /* Select active reflection */
            if (xfce_display_settings_is_active (settings, selected_id)
                && (reflection_names[n].rotation & active_reflection) == reflection_names[n].rotation)
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_reflections_changed, settings);
}

static void
display_setting_rotations_changed (GtkComboBox *combobox,
                                   XfceDisplaySettings *settings)
{
    RotationFlags rotation;
    gint value;
    guint selected_id;

    if (!display_setting_combo_box_get_value (combobox, &value, FALSE))
        return;

    /* Set new rotation */
    selected_id = xfce_display_settings_get_selected_output_id (settings);
    rotation = xfce_display_settings_get_rotation (settings, selected_id);
    rotation &= ~ROTATION_MASK;
    rotation |= value;
    xfce_display_settings_set_rotation (settings, selected_id, rotation);

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
}

static void
display_setting_rotations_populate (XfceDisplaySettings *settings,
                                    guint selected_id)
{
    GtkBuilder   *builder = xfce_display_settings_get_builder (settings);
    GtkTreeModel *model;
    GObject      *combobox, *label;
    RotationFlags rotations;
    RotationFlags active_rotation;
    guint         n;
    GtkTreeIter   iter;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-rotation");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
    label = gtk_builder_get_object (builder, "label-rotation");

    /* Disable it if no mode is selected */
    if (!xfce_display_settings_is_active (settings, selected_id))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_rotations_changed, settings);

    /* Load only supported rotations */
    rotations = xfce_display_settings_get_rotations (settings, selected_id) & ROTATION_MASK;
    active_rotation = xfce_display_settings_get_rotation (settings, selected_id) & ROTATION_MASK;

    /* Try to insert the rotations */
    for (n = 0; n < G_N_ELEMENTS (rotation_names); n++)
    {
        if ((rotations & rotation_names[n].rotation) == rotation_names[n].rotation)
        {
            /* Insert */
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, _(rotation_names[n].name),
                                COLUMN_COMBO_VALUE, rotation_names[n].rotation, -1);

            /* Select active rotation */
            if (xfce_display_settings_is_active (settings, selected_id)
                && (rotation_names[n].rotation & active_rotation) == rotation_names[n].rotation)
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_rotations_changed, settings);
}

static void
display_setting_refresh_rates_changed (GtkComboBox *combobox,
                                       XfceDisplaySettings *settings)
{
    guint selected_id;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value, FALSE))
        return;

    /* Set new mode */
    selected_id = xfce_display_settings_get_selected_output_id (settings);
    xfce_display_settings_set_mode (settings, selected_id, value);

    /* In any case, check if we're now in mirror mode */
    display_setting_mirror_displays_populate (settings);

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
}

static void
display_setting_refresh_rates_populate (XfceDisplaySettings *settings,
                                        guint selected_id)
{
    GtkBuilder       *builder = xfce_display_settings_get_builder (settings);
    GtkTreeModel     *model;
    GObject          *combobox, *label;
    GtkTreeIter       iter;
    gchar            *name = NULL;
    XfceOutput       *output;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
    label = gtk_builder_get_object (builder, "label-refresh-rate");

    /* Disable it if no mode is selected */
    if (!xfce_display_settings_is_active (settings, selected_id))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_refresh_rates_changed, settings);

    /* Walk all supported modes */
    output = get_nth_xfce_output (settings, selected_id);
    for (guint n = 0; n < output->n_modes; n++)
    {
        /* The mode resolution does not match the selected one */
        if (output->modes[n]->width != output->mode->width
            || output->modes[n]->height != output->mode->height)
            continue;

        /* Insert the mode */
        name = g_strdup_printf (_("%.2f Hz"), output->modes[n]->rate);
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COLUMN_COMBO_NAME, name,
                            COLUMN_COMBO_VALUE, output->modes[n]->id, -1);
        g_free (name);

        /* Select the active mode */
        if (output->modes[n]->id == output->mode->id)
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* If a new resolution was selected, set a refresh rate */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

    /* In any case, check if we're now in mirror mode */
    display_setting_mirror_displays_populate (settings);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_refresh_rates_changed, settings);
}

static void
display_setting_resolutions_changed (GtkComboBox *combobox,
                                     XfceDisplaySettings *settings)
{
    guint selected_id;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value, TRUE))
        return;

    /* Set new resolution */
    selected_id = xfce_display_settings_get_selected_output_id (settings);
    xfce_display_settings_set_mode (settings, selected_id, value);

    /* Update refresh rates */
    display_setting_refresh_rates_populate (settings, selected_id);

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
}

/* Greatest common divisor */
static guint
gcd (guint a,
     guint b)
{
    if (b == 0)
      return a;

    return gcd (b, a % b);
}

/* Initialize valid display aspect ratios */
static void
display_settings_aspect_ratios_populate (void)
{
    XfceRatio *i;

    display_ratio = g_hash_table_new (g_double_hash, g_double_equal);
    for (i = ratio_table; i->ratio != 0.0; i++)
    {
        g_hash_table_insert (display_ratio, &i->ratio, (gpointer) i);
    }
}

static void
display_setting_resolutions_populate (XfceDisplaySettings *settings,
                                      guint selected_id)
{
    GtkBuilder       *builder = xfce_display_settings_get_builder (settings);
    GtkTreeModel     *model;
    GObject          *combobox, *label;
    gchar            *name;
    gchar            *rratio;
    GtkTreeIter       iter;
    XfceOutput       *output;
    XfceMode        **modes;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    label = gtk_builder_get_object (builder, "label-resolution");

    output = get_nth_xfce_output (settings, selected_id);

    /* Disable it if no mode is selected */
    if (!xfce_display_settings_is_active (settings, selected_id))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        display_setting_refresh_rates_populate (settings, selected_id);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (label), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_resolutions_changed, settings);

    /* Walk all supported modes */
    modes = output->modes;
    for (guint n = 0; n < output->n_modes; n++)
    {
        /* Try to avoid duplicates */
        if (n == 0 || (n > 0 && (modes[n]->width != modes[n - 1]->width
            || modes[n]->height != modes[n - 1]->height)))
        {
            /* Insert mode and ratio */
            gdouble    ratio = (double) modes[n]->width / (double) modes[n]->height;
            gdouble    rough_ratio;
            gchar     *ratio_text = NULL;
            XfceRatio *ratio_info = g_hash_table_lookup (display_ratio, &ratio);

            /* Highlight the preferred mode with an asterisk */
            if (output->pref_width == modes[n]->width
                && output->pref_height == modes[n]->height)
                name = g_strdup_printf ("%dx%d*", modes[n]->width,
                                        modes[n]->height);
            else
                name = g_strdup_printf ("%dx%d", modes[n]->width,
                                        modes[n]->height);

            if (ratio_info)
                ratio_text = g_strdup (ratio_info->desc);

            if (!ratio_info)
            {
                rough_ratio = _TWO_DIGIT_PRECISION (ratio);
                ratio_info = g_hash_table_lookup (display_ratio, &rough_ratio);
                if (ratio_info)
                {
                    /* if the lookup finds a precise ratio
                     * although we did round the current ratio
                     * we also mark this as not precise */
                    if (ratio_info->precise)
                        ratio_text = g_strdup_printf ("<span font_style='italic'>≈%s</span>", ratio_info->desc);
                    else
                        ratio_text = g_strdup (ratio_info->desc);
                }
            }

            if (!ratio_info)
            {
                rough_ratio = _ONE_DIGIT_PRECISION (ratio);
                ratio_info = g_hash_table_lookup (display_ratio, &rough_ratio);
                if (ratio_info)
                {
                    if (ratio_info->precise)
                        ratio_text = g_strdup_printf ("<span font_style='italic'>≈%s</span>", ratio_info->desc);
                    else
                        ratio_text = g_strdup (ratio_info->desc);
                }
            }

            if (!ratio_info)
            {
                guint gcd_tmp = gcd (modes[n]->width, modes[n]->height);
                guint format_x = modes[n]->width / gcd_tmp;
                guint format_y = modes[n]->height / gcd_tmp;
                rratio = g_strdup_printf ("<span fgalpha='50%%'>%d:%d</span>", format_x, format_y);
            }
            else
            {
                rratio = g_strdup_printf ("<span fgalpha='50%%'>%s</span>", ratio_text);
            }
            g_free (ratio_text);

            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                RESOLUTION_COLUMN_COMBO_NAME, name,
                                RESOLUTION_COLUMN_COMBO_MARKUP, rratio,
                                RESOLUTION_COLUMN_COMBO_VALUE, modes[n]->id, -1);
            g_free (name);
            g_free (rratio);
        }

        /* Select the active mode */
        if (modes[n]->id == output->mode->id)
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_resolutions_changed, settings);
}

static void
display_setting_mirror_displays_toggled (GtkToggleButton *togglebutton,
                                         XfceDisplaySettings *settings)
{
    /* reset the inconsistent state, since the mirror checkbutton is being toggled */
    if (gtk_toggle_button_get_inconsistent (togglebutton))
        gtk_toggle_button_set_inconsistent (togglebutton, FALSE);

    if (gtk_toggle_button_get_active (togglebutton))
    {
        xfce_display_settings_mirror (settings);
    }
    else
    {
        xfce_display_settings_unmirror (settings);
    }

    /* Apply the changes */
    display_settings_changed ();
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
}

static void
display_setting_mirror_displays_populate (XfceDisplaySettings *settings)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject *check;
    MirroredState state;

    check = gtk_builder_get_object (builder, "mirror-displays");

    if (xfce_display_settings_get_n_outputs (settings) > 1)
        gtk_widget_show (GTK_WIDGET (check));
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (check), xfce_display_settings_is_clonable (settings));

    state = xfce_display_settings_get_mirrored_state (settings);
    if (state == MIRRORED_STATE_NONE)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
        return;
    }

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_mirror_displays_toggled, settings);

    /* if two displays are 'mirrored', i.e. their resolutions are not the same
       we set the checkbutton to the inconsistent state */
    gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (check), state == MIRRORED_STATE_MIRRORED);
    if (state == MIRRORED_STATE_CLONED)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (check, display_setting_mirror_displays_toggled, settings);
}

static gboolean
display_setting_primary_toggled (GtkWidget *widget,
                                 gboolean primary,
                                 XfceDisplaySettings *settings)
{
    guint selected_id = xfce_display_settings_get_selected_output_id (settings);

    if (primary)
    {
        guint n_outputs = xfce_display_settings_get_n_outputs (settings);

        /* Set currently active display as primary */
        xfce_display_settings_set_primary (settings, selected_id, TRUE);
        xfce_display_settings_save (settings, selected_id, "Default");

        /* and all others as secondary */
        for (guint n = 0; n < n_outputs; n++)
        {
            if (n != selected_id)
            {
                xfce_display_settings_set_primary (settings, n, FALSE);
                xfce_display_settings_save (settings, n, "Default");
            }
        }
    }
    else
    {
        xfce_display_settings_set_primary (settings, selected_id, FALSE);
        xfce_display_settings_save (settings, selected_id, "Default");
    }

    /* Apply the changes */
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");
    gtk_switch_set_state (GTK_SWITCH (widget), primary);

    return TRUE;
}

static void
display_setting_primary_populate (XfceDisplaySettings *settings,
                                  guint selected_id)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject *check, *label, *primary_indicator, *primary_info;
    gboolean output_on = TRUE;
    gboolean multiple_displays = TRUE;
    gboolean primary;

    primary = xfce_display_settings_is_primary (settings, selected_id);
    if (xfce_display_settings_get_n_outputs (settings) <= 1)
        multiple_displays = FALSE;
    check = gtk_builder_get_object (builder, "primary");
    label = gtk_builder_get_object (builder, "label-primary");
    primary_info = gtk_builder_get_object (builder, "primary-info-button");
    primary_indicator = gtk_builder_get_object (builder, "primary-indicator");

    /* If there's only one display we hide the primary option as it is meaningless */
    gtk_widget_set_visible (GTK_WIDGET (check), multiple_displays);
    gtk_widget_set_visible (GTK_WIDGET (label), multiple_displays);
    gtk_widget_set_visible (GTK_WIDGET (primary_info), multiple_displays);
    gtk_widget_set_visible (GTK_WIDGET (primary_indicator), multiple_displays);
    if (!multiple_displays)
        return;

    if (!xfce_display_settings_is_active (settings, selected_id))
        output_on = FALSE;
    gtk_widget_set_sensitive (GTK_WIDGET (check), output_on);
    gtk_widget_set_sensitive (GTK_WIDGET (label), output_on);
    gtk_widget_set_visible (GTK_WIDGET (primary_indicator), primary);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_primary_toggled, settings);
    gtk_switch_set_state (GTK_SWITCH (check), primary);
    g_signal_handlers_unblock_by_func (check, display_setting_primary_toggled, settings);
}

static gboolean
display_setting_output_toggled (GtkSwitch  *widget,
                                gboolean output_on,
                                XfceDisplaySettings *settings)
{
    guint selected_id, n_outputs;

    if (xfce_display_settings_get_n_outputs (settings) <= 1)
        return FALSE;

    selected_id = xfce_display_settings_get_selected_output_id (settings);

    if (output_on)
        xfce_display_settings_set_active (settings, selected_id, TRUE);
    else
    {
        if (xfce_display_settings_get_n_active_outputs (settings) == 1)
        {
            xfce_dialog_show_warning (NULL,
                                      _("The last active output must not be disabled, the system would"
                                        " be unusable."),
                                      _("Selected output not disabled"));
            return FALSE;
        }
        xfce_display_settings_set_active (settings, selected_id, FALSE);
    }

    /* Apply the changes */
    n_outputs = xfce_display_settings_get_n_outputs (settings);
    for (guint n = 0; n < n_outputs; n++)
        xfce_display_settings_save (settings, n, "Default");
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));

    return display_setting_ask_fallback (settings);
}

static void
display_setting_output_status_populate (XfceDisplaySettings *settings,
                                        guint selected_id)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject *check;

    check = gtk_builder_get_object (builder, "output-on");

    if (xfce_display_settings_get_n_outputs (settings) > 1)
        gtk_widget_show (GTK_WIDGET (check));
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        return;
    }

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_output_toggled, settings);
    gtk_switch_set_state (GTK_SWITCH (check), xfce_display_settings_is_active (settings, selected_id));
    g_signal_handlers_unblock_by_func (check, display_setting_output_toggled, settings);
}

static void
display_settings_combobox_selection_changed (GtkComboBox *combobox,
                                             XfceDisplaySettings *settings)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    GtkWidget    *popup;
    gint          selected_id, previous_id;

    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        GHashTable *display_popups;

        /* Get the output info */
        model = gtk_combo_box_get_model (combobox);
        gtk_tree_model_get (model, &iter, COLUMN_OUTPUT_ID, &selected_id, -1);

        /* Get the new active screen or output */
        previous_id = xfce_display_settings_get_selected_output_id (settings);
        xfce_display_settings_set_selected_output_id (settings, selected_id);

        /* Update the combo boxes */
        display_setting_output_status_populate (settings, selected_id);
        if (WINDOWING_IS_X11 ())
            display_setting_primary_populate (settings, selected_id);
        display_setting_mirror_displays_populate (settings);
        display_setting_resolutions_populate (settings, selected_id);
        display_setting_refresh_rates_populate (settings, selected_id);
        display_setting_rotations_populate (settings, selected_id);
        display_setting_reflections_populate (settings, selected_id);
        display_setting_scale_populate (settings, selected_id);

        /* redraw the two (old active, new active) popups */
        display_popups = xfce_display_settings_get_popups (settings);
        popup = g_hash_table_lookup (display_popups, GINT_TO_POINTER (previous_id));
        if (popup)
            gtk_widget_queue_draw (popup);
        popup = g_hash_table_lookup (display_popups, GINT_TO_POINTER (selected_id));
        if (popup)
            gtk_widget_queue_draw (popup);

        foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
    }
}

static void
display_settings_minimal_profile_populate (XfceDisplaySettings *settings)
{
    XfconfChannel *channel = xfce_display_settings_get_channel (settings);
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject  *profile_box, *profile_display1;
    GList    *profiles = NULL;
    GList    *current;
    gchar   **display_infos;

    profile_box  = gtk_builder_get_object (builder, "profile-box");
    profile_display1  = gtk_builder_get_object (builder, "display1");

    display_infos = xfce_display_settings_get_display_infos (settings);
    profiles = display_settings_get_profiles (display_infos, channel);
    g_strfreev (display_infos);

    current = g_list_first (profiles);
    while (current)
    {
        GtkWidget *box, *profile_radio, *label, *image;
        gchar *property;
        gchar *profile_name;

        /* use the display string value of the profile hash property */
        property = g_strdup_printf ("/%s", (gchar *)current->data);
        profile_name = xfconf_channel_get_string (channel, property, NULL);

        label = gtk_label_new (profile_name);
        image = gtk_image_new_from_icon_name ("xfce-display-profile", 128);
        gtk_image_set_pixel_size (GTK_IMAGE (image), 128);

        profile_radio = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (profile_display1));
        gtk_container_add (GTK_CONTAINER (profile_radio), image);
        g_object_set_data (G_OBJECT (profile_radio), "profile", (gchar *)current->data);
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (profile_radio), FALSE);
        gtk_widget_set_size_request (GTK_WIDGET (profile_radio), 128, 128);

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start (GTK_BOX (box), profile_radio, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 3);
        gtk_widget_set_margin_start (GTK_WIDGET (box), 24);
        gtk_box_pack_start (GTK_BOX (profile_box), box, FALSE, FALSE, 0);

        g_signal_connect (profile_radio, "toggled", G_CALLBACK (display_settings_minimal_profile_apply), channel);

        current = g_list_next (current);
        g_free (property);
        g_free (profile_name);
    }

    gtk_widget_show_all (GTK_WIDGET (profile_box));
}

static void
display_settings_profile_list_init (GtkBuilder *builder)
{
    GtkListStore      *store;
    GObject           *treeview;
    GtkCellRenderer   *renderer;
    GtkTreeViewColumn *column;

    store = gtk_list_store_new (N_COLUMNS,
                                G_TYPE_ICON,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

    treeview = gtk_builder_get_object (builder, "randr-profile");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    /* Setup Profile name column */
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "gicon", COLUMN_ICON, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    /* Setup Profile name column */
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (column, _("Profiles matching the currently connected displays"));
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    /* Setup Profile hash column */
    column = gtk_tree_view_column_new ();
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_HASH, NULL);
    gtk_tree_view_column_set_visible (column, FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    g_object_unref (G_OBJECT (store));
}

static void
display_settings_combo_box_create (GtkComboBox *combobox,
                                   gboolean     resolution)
{
    GtkCellRenderer *renderer;
    GtkListStore    *store;

    /* Create and set the combobox model */
    if (resolution)
        store = gtk_list_store_new (N_RESOLUTION_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    else
        store = gtk_list_store_new (N_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));

    /* Setup renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
    if (resolution)
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", RESOLUTION_COLUMN_COMBO_NAME);
    else
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", COLUMN_COMBO_NAME);

    /* Add another column for the resolution combobox to display the ratio */
    if (resolution)
    {
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "markup", RESOLUTION_COLUMN_COMBO_MARKUP);
        gtk_cell_renderer_set_alignment (renderer, 1.0, 0.5);
    }
}

static void
display_settings_dialog_response (GtkDialog *dialog,
                                  gint response_id,
                                  XfceDisplaySettings *settings)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "display",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else if (response_id == GTK_RESPONSE_CLOSE)
    {
        XfconfChannel *channel = xfce_display_settings_get_channel (settings);
        gchar *new_active_profile = xfconf_channel_get_string (channel, "/ActiveProfile", NULL);
        gchar *property = g_strdup_printf ("/%s", active_profile);
        gchar *profile_name = xfconf_channel_get_string (channel, property, NULL);

        if (g_strcmp0 (active_profile, new_active_profile) != 0 &&
            profile_name != NULL &&
            g_strcmp0 (active_profile, "Default") != 0)
        {
            GtkBuilder *profile_changed_builder = xfce_display_settings_get_builder (settings);
            GError     *error = NULL;
            gint        profile_response_id;

            if (gtk_builder_add_from_string (profile_changed_builder, profile_changed_dialog_ui,
                                             profile_changed_dialog_ui_length, &error) != 0)
            {
                GObject *profile_changed_dialog, *label, *button;
                const char *str;
                const char *format = "<big><b>\%s</b></big>";
                char *markup;
                gchar *button_label;

                profile_changed_dialog = gtk_builder_get_object (profile_changed_builder, "profile-changed-dialog");

                gtk_window_set_transient_for (GTK_WINDOW (profile_changed_dialog), GTK_WINDOW (dialog));
                gtk_window_set_modal (GTK_WINDOW (profile_changed_dialog), TRUE);

                label = gtk_builder_get_object (profile_changed_builder, "header");
                str = g_strdup_printf(_("Update changed display profile '%s'?"), profile_name);
                markup = g_markup_printf_escaped (format, str);
                gtk_label_set_markup (GTK_LABEL (label), markup);

                button = gtk_builder_get_object (profile_changed_builder, "button-update");
                button_label = g_strdup_printf (_("_Update '%s'"), profile_name);
                gtk_button_set_label (GTK_BUTTON (button), button_label);

                profile_response_id = gtk_dialog_run (GTK_DIALOG (profile_changed_dialog));
                gtk_widget_destroy (GTK_WIDGET (profile_changed_dialog));
                g_free (markup);
                g_free (button_label);
            }
            else
            {
                profile_response_id = 2;
                g_error ("Failed to load the UI file: %s.", error->message);
                g_error_free (error);
            }

            /* update the profile */
            if (profile_response_id == GTK_RESPONSE_OK)
            {
                guint n_outputs = xfce_display_settings_get_n_outputs (settings);
                for (guint n = 0; n < n_outputs; n++)
                    xfce_display_settings_save (settings, n, active_profile);

                xfconf_channel_set_string (channel, "/ActiveProfile", active_profile);
            }
        }
        g_free (profile_name);
        g_free (property);
        g_free (new_active_profile);
        g_free (active_profile);
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static gboolean
on_identify_displays_toggled (GtkWidget *widget,
                              gboolean state,
                              XfceDisplaySettings *settings)
{
    xfce_display_settings_set_popups_visible (settings, state);
    gtk_switch_set_state (GTK_SWITCH (widget), state);

    return TRUE;
}

static void
display_setting_apply (GtkWidget *widget, XfceDisplaySettings *settings)
{
    guint n_outputs = xfce_display_settings_get_n_outputs (settings);
    for (guint n = 0; n < n_outputs; n++)
        xfce_display_settings_save (settings, n, "Default");
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");

    display_setting_ask_fallback (settings);

    gtk_widget_set_sensitive(widget, FALSE);
}

static void
display_settings_profile_changed (GtkTreeSelection *selection, GtkBuilder *builder)
{
    GObject *button;
    GtkTreeModel      *model;
    GtkTreeIter        iter;
    gboolean selected;

    selected = gtk_tree_selection_get_selected (selection, &model, &iter);

    button = gtk_builder_get_object (builder, "button-profile-save");
    gtk_widget_set_sensitive (GTK_WIDGET (button), selected);
    button = gtk_builder_get_object (builder, "button-profile-delete");
    gtk_widget_set_sensitive (GTK_WIDGET (button), selected);
    button = gtk_builder_get_object (builder, "button-profile-apply");
    gtk_widget_set_sensitive (GTK_WIDGET (button), selected);
}

static void
display_settings_minimal_profile_apply (GtkToggleButton *widget, XfconfChannel *channel)
{
    gchar  *profile_hash;

    profile_hash = (gchar *) g_object_get_data (G_OBJECT (widget), "profile");
    xfconf_channel_set_string (channel, "/Schemes/Apply", profile_hash);
}

static void
display_settings_profile_save (GtkWidget *widget, XfceDisplaySettings *settings)
{
    GtkBuilder        *builder = xfce_display_settings_get_builder (settings);
    GObject           *treeview;
    GtkTreeSelection  *selection;
    GtkTreeModel      *model;
    GtkTreeIter        iter;

    treeview = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        XfconfChannel *channel = xfce_display_settings_get_channel (settings);
        guint n_outputs = xfce_display_settings_get_n_outputs (settings);
        gchar *property;
        gchar *profile_hash;
        gchar *profile_name;

        gtk_tree_model_get (model, &iter, COLUMN_NAME, &profile_name, COLUMN_HASH, &profile_hash, -1);
        property = g_strdup_printf ("/%s", profile_hash);

        for (guint n = 0; n < n_outputs; n++)
            xfce_display_settings_save (settings, n, profile_hash);

        /* save the human-readable name of the profile as string value */
        xfconf_channel_set_string (channel, property, profile_name);
        xfconf_channel_set_string (channel, "/ActiveProfile", profile_hash);

        xfce_display_settings_populate_profile_list (settings);
        gtk_widget_set_sensitive (widget, FALSE);

        g_free (property);
        g_free (profile_hash);
        g_free (profile_name);
    }
    else
        gtk_widget_set_sensitive (widget, TRUE);
}

/* reset the widget states if the user starts editing the profile name */
static void
display_settings_profile_entry_text_changed (GtkEditable *entry,
                                             GtkBuilder  *builder)
{
    GObject *infobar, *button;

    button = gtk_builder_get_object (builder, "button-profile-create-cb");
    infobar = gtk_builder_get_object (builder, "profile-exists");

    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), "error");
    gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
    gtk_widget_hide (GTK_WIDGET (infobar));
}

static void
display_settings_profile_create_cb (GtkWidget *widget, XfceDisplaySettings *settings)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    XfconfChannel *channel = xfce_display_settings_get_channel (settings);
    const gchar *profile_name;
    GtkWidget *popover;
    GObject *infobar, *entry, *button;

    entry = gtk_builder_get_object (builder, "entry-profile-create");
    profile_name = gtk_entry_get_text (GTK_ENTRY (entry));

    /* check if the profile name is already taken */
    if (!display_settings_profile_name_exists (channel, profile_name))
    {
        button = gtk_builder_get_object (builder, "button-profile-create-cb");
        infobar = gtk_builder_get_object (builder, "profile-exists");

        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), "error");
        gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
        gtk_widget_show_all (GTK_WIDGET (infobar));

        g_signal_connect (G_OBJECT (entry), "changed",
                          G_CALLBACK (display_settings_profile_entry_text_changed), builder);
        return;
    }

    if (profile_name)
    {
        guint n_outputs = xfce_display_settings_get_n_outputs (settings);
        gchar *property;
        gchar *profile_hash;

        profile_hash = g_compute_checksum_for_string (G_CHECKSUM_SHA1, profile_name, strlen(profile_name));
        property = g_strdup_printf ("/%s", profile_hash);
        for (guint n = 0; n < n_outputs; n++)
            xfce_display_settings_save (settings, n, profile_hash);

        /* save the human-readable name of the profile as string value */
        xfconf_channel_set_string (channel, property, profile_name);
        xfconf_channel_set_string (channel, "/ActiveProfile", profile_hash);
        xfce_display_settings_populate_profile_list (settings);

        g_free (property);
        g_free (profile_hash);
    }
    popover = gtk_widget_get_ancestor (widget, GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
display_settings_profile_create (GtkWidget *widget, XfceDisplaySettings *settings)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GObject *popover, *entry, *button, *infobar;

    /* Create a popover dialog for saving a new profile */
    popover = gtk_builder_get_object (builder, "popover-create-profile");
    entry = gtk_builder_get_object (builder, "entry-profile-create");
    button = gtk_builder_get_object (builder, "button-profile-create-cb");
    infobar = gtk_builder_get_object (builder, "profile-exists");

    gtk_widget_show (GTK_WIDGET (popover));
    gtk_widget_hide (GTK_WIDGET (infobar));
    gtk_widget_grab_focus (GTK_WIDGET (entry));
    gtk_widget_grab_default (GTK_WIDGET (button));

    g_signal_connect (button, "clicked", G_CALLBACK (display_settings_profile_create_cb), settings);
}

static void
display_settings_profile_apply (GtkWidget *widget, XfceDisplaySettings *settings)
{
    GtkBuilder        *builder = xfce_display_settings_get_builder (settings);
    GObject           *treeview;
    GtkTreeSelection  *selection;
    GtkTreeModel      *model;
    GtkTreeIter        iter;

    treeview = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        XfconfChannel *channel = xfce_display_settings_get_channel (settings);
        gchar *profile_hash;
        gchar *old_profile_hash;

        old_profile_hash = xfconf_channel_get_string (channel, "/ActiveProfile", "Default");
        gtk_tree_model_get (model, &iter, COLUMN_HASH, &profile_hash, -1);
        xfconf_channel_set_string (channel, "/Schemes/Apply", profile_hash);
        xfconf_channel_set_string (channel, "/ActiveProfile", profile_hash);

        if (!display_setting_timed_confirmation (settings))
        {
            xfconf_channel_set_string (channel, "/Schemes/Apply", old_profile_hash);
            xfconf_channel_set_string (channel, "/ActiveProfile", old_profile_hash);

            foo_scroll_area_invalidate (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)));
        }
        xfce_display_settings_populate_profile_list (settings);

        g_free (profile_hash);
    }
}

static void
display_settings_profile_row_activated (GtkTreeView       *tree_view,
                                        GtkTreePath       *path,
                                        GtkTreeViewColumn *column,
                                        XfceDisplaySettings *settings)
{
    display_settings_profile_apply (NULL, settings);
}

static void
display_settings_profile_delete (GtkWidget *widget, XfceDisplaySettings *settings)
{
    GtkBuilder        *builder = xfce_display_settings_get_builder (settings);
    GObject           *treeview;
    GtkTreeSelection  *selection;
    GtkTreeModel      *model;
    GtkTreeIter        iter;

    treeview = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        XfconfChannel *channel = xfce_display_settings_get_channel (settings);
        gchar *profile_name;
        gchar *profile_hash;
        gint   response;
        gchar *primary_message;

        gtk_tree_model_get (model, &iter, COLUMN_NAME, &profile_name, COLUMN_HASH, &profile_hash, -1);
        primary_message = g_strdup_printf (_("Do you want to delete the display profile '%s'?"), profile_name);

        response = xfce_message_dialog (NULL, _("Delete Profile"),
                                        "user-trash",
                                        primary_message,
                                        _("Once a display profile is deleted it cannot be restored."),
                                        _("Cancel"), GTK_RESPONSE_NO,
                                        _("Delete"), GTK_RESPONSE_YES,
                                        NULL);

        g_free (primary_message);

        if (response == GTK_RESPONSE_YES)
        {
            GString *property;

            property = g_string_new (profile_hash);
            g_string_prepend_c (property, '/');

            xfconf_channel_reset_property (channel, property->str, TRUE);
            xfconf_channel_set_string (channel, "/ActiveProfile", "Default");
            xfce_display_settings_populate_profile_list (settings);
            g_free (profile_name);
        }
        else
        {
            g_free (profile_name);
            return;
        }
    }
}

static void
display_setting_minimal_autoconnect_mode_changed (GtkComboBox *combobox,
                                                  GtkBuilder  *builder)
{
    gint value;
    gboolean state = TRUE;
    GObject *auto_enable_profiles;

    value = gtk_combo_box_get_active (combobox);
    /* On "Do nothing" disable the "auto-enable-profiles" option */
    if (value == 0)
      state = FALSE;

    auto_enable_profiles = gtk_builder_get_object (builder, "auto-enable-profiles");
    gtk_widget_set_sensitive (GTK_WIDGET (auto_enable_profiles), state);
    auto_enable_profiles = gtk_builder_get_object (builder, "auto-enable-profiles-label");
    gtk_widget_set_sensitive (GTK_WIDGET (auto_enable_profiles), state);
}

static void
display_settings_launch_settings_dialogs (GtkButton *button,
                                          gpointer   user_data)
{
    gchar    *command = user_data;
    GAppInfo *app_info = NULL;
    GError   *error = NULL;

    app_info = g_app_info_create_from_commandline (command, "Xfce Settings", G_APP_INFO_CREATE_NONE, &error);

    if (G_UNLIKELY (app_info == NULL)) {
        g_warning ("Could not find application %s", error->message);
        return;
    }
    if (error != NULL)
        g_error_free (error);

    if (!g_app_info_launch (app_info, NULL, NULL, &error))
        g_warning ("Could not launch the application %s", error->message);
    if (error != NULL)
        g_error_free (error);
}

static void
display_settings_primary_status_info_populate (GtkBuilder *builder)
{
    GObject          *widget;
    GtkWidget        *image;
    XfconfChannel    *channel;
    gchar            *primary_status_panel;
    gint              primary_status;
    gint              panels = 0;
    gint              panels_with_primary = 0;
    gchar            *property;

    widget = gtk_builder_get_object (builder, "primary-info-button");
    image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_BUTTON);
    gtk_container_add (GTK_CONTAINER (widget), image);
    gtk_widget_show (image);

    channel = xfconf_channel_new ("xfce4-panel");
    widget = gtk_builder_get_object (builder, "panel-ok");
    property = g_strdup_printf ("/panels/panel-%u/output-name", panels);
    /* Check all panels and show the ok icon on the first occurence of a panel set to "Primary" */
    for (panels = 0; xfconf_channel_has_property (channel, property); panels++)
    {
        primary_status_panel = xfconf_channel_get_string (channel, property, "Automatic");
        if (g_strcmp0 (primary_status_panel, "Primary") == 0)
        {
            gtk_widget_show (GTK_WIDGET (widget));
            panels_with_primary++;
        }
        else
            gtk_widget_hide (GTK_WIDGET (widget));
        property = g_strdup_printf ("/panels/panel-%u/output-name", panels + 1);
        g_free (primary_status_panel);
    }
    if (panels_with_primary > 1)
    {
        gchar *label;
        widget = gtk_builder_get_object (builder, "panel-label");
        label = g_strdup_printf (_("%d Xfce Panels"), panels_with_primary);
        gtk_label_set_text (GTK_LABEL (widget), label);
        g_free (label);
    }
    g_free (property);
    g_object_unref (G_OBJECT (channel));
    widget = gtk_builder_get_object (builder, "panel-configure");
    g_signal_connect (widget, "clicked", G_CALLBACK (display_settings_launch_settings_dialogs), "xfce4-panel --preferences");

    channel = xfconf_channel_new ("xfce4-desktop");
    primary_status = xfconf_channel_get_bool (channel, "/desktop-icons/primary", FALSE);
    widget = gtk_builder_get_object (builder, "desktop-ok");
    gtk_widget_set_visible (GTK_WIDGET (widget), primary_status);
    g_object_unref (G_OBJECT (channel));
    widget = gtk_builder_get_object (builder, "desktop-configure");
    g_signal_connect (widget, "clicked", G_CALLBACK (display_settings_launch_settings_dialogs), "xfdesktop-settings");

    channel = xfconf_channel_new ("xfce4-notifyd");
    primary_status = xfconf_channel_get_uint (channel, "/primary-monitor", 0);
    widget = gtk_builder_get_object (builder, "notifications-ok");
    gtk_widget_set_visible (GTK_WIDGET (widget), primary_status);
    g_object_unref (G_OBJECT (channel));
    widget = gtk_builder_get_object (builder, "notifications-configure");
    g_signal_connect (widget, "clicked", G_CALLBACK (display_settings_launch_settings_dialogs), "xfce4-notifyd-config");
}

static GtkWidget *
display_settings_dialog_new (XfceDisplaySettings *settings)
{
    XfconfChannel    *channel = xfce_display_settings_get_channel (settings);
    GtkBuilder       *builder = xfce_display_settings_get_builder (settings);
    GObject          *combobox;
    GtkCellRenderer  *renderer;
    GObject          *label, *check, *primary, *mirror, *identify, *primary_indicator;
    GObject          *revealer, *spinbutton;
    GtkWidget        *button;
    GtkTreeSelection *selection;

    /* Get the combobox */
    combobox = gtk_builder_get_object (builder, "randr-outputs");

    /* Text renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), renderer, "text", COLUMN_OUTPUT_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Identification popups */
    xfce_display_settings_populate_popups (settings);
    identify = gtk_builder_get_object (builder, "identify-displays");
    g_signal_connect (G_OBJECT (identify), "state-set", G_CALLBACK (on_identify_displays_toggled), settings);
    xfconf_g_property_bind (channel, "/IdentityPopups", G_TYPE_BOOLEAN, identify,
                            "active");
    xfce_display_settings_set_popups_visible (settings, gtk_switch_get_active (GTK_SWITCH (identify)));

    /* Display selection combobox */
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_settings_combobox_selection_changed), settings);

    /* Setup the combo boxes */
    check = gtk_builder_get_object (builder, "output-on");
    mirror = gtk_builder_get_object (builder, "mirror-displays");
    g_signal_connect (G_OBJECT (check), "state-set", G_CALLBACK (display_setting_output_toggled), settings);
    g_signal_connect (G_OBJECT (mirror), "toggled", G_CALLBACK (display_setting_mirror_displays_toggled), settings);
    if (xfce_display_settings_get_n_outputs (settings) > 1)
    {
        gtk_widget_show (GTK_WIDGET (check));
        gtk_widget_show (GTK_WIDGET (mirror));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        gtk_widget_hide (GTK_WIDGET (mirror));
    }

    /* Set up primary monitor widgets */
    primary = gtk_builder_get_object (builder, "primary");
    primary_indicator = gtk_builder_get_object (builder, "primary-indicator");
    label = gtk_builder_get_object (builder, "label-primary");
    button = GTK_WIDGET (gtk_builder_get_object (builder, "primary-info-button"));
    if (WINDOWING_IS_X11 ())
    {
        g_signal_connect (G_OBJECT (primary), "state-set", G_CALLBACK (display_setting_primary_toggled), settings);
        display_settings_primary_status_info_populate (builder);
        gtk_widget_set_visible (GTK_WIDGET (primary_indicator), gtk_switch_get_active (GTK_SWITCH (primary)));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (primary));
        gtk_widget_hide (GTK_WIDGET (primary_indicator));
        gtk_widget_hide (GTK_WIDGET (label));
        gtk_widget_hide (button);
    }

    label = gtk_builder_get_object (builder, "label-reflection");
    gtk_widget_show (GTK_WIDGET (label));

    combobox = gtk_builder_get_object (builder, "randr-scale");
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_scale_changed), builder);
    revealer = gtk_builder_get_object (builder, "revealer-scale");
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
    else
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);

    spinbutton = gtk_builder_get_object (builder, "spin-scale-x");
    g_signal_connect (G_OBJECT (spinbutton), "value-changed", G_CALLBACK (display_setting_custom_scale_changed), settings);

    combobox = gtk_builder_get_object (builder, "randr-reflection");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), FALSE);
    gtk_widget_show (GTK_WIDGET (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_reflections_changed), settings);

    display_settings_aspect_ratios_populate ();
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), TRUE);
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), settings);

    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), FALSE);
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), settings);

    combobox = gtk_builder_get_object (builder, "randr-rotation");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox), FALSE);
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), settings);

    combobox = gtk_builder_get_object (builder, "randr-profile");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (combobox));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (combobox), FALSE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (display_settings_profile_changed), builder);
    g_signal_connect (G_OBJECT (combobox), "row-activated", G_CALLBACK (display_settings_profile_row_activated), settings);

    combobox = gtk_builder_get_object (builder, "autoconnect-mode");
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_minimal_autoconnect_mode_changed), builder);
    xfconf_g_property_bind (channel, "/Notify", G_TYPE_INT, combobox,
                            "active");
    /* Correctly initialize the state of the auto-enable-profiles setting based on autoconnect-mode */
    if (xfconf_channel_get_int (channel, "/Notify", -1) == -1)
    {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), NOTIFY_PROP_DEFAULT);
        display_setting_minimal_autoconnect_mode_changed ((GTK_COMBO_BOX (combobox)), builder);
    }

    apply_button = GTK_WIDGET (gtk_builder_get_object (builder, "apply"));
    g_signal_connect (G_OBJECT (apply_button), "clicked", G_CALLBACK (display_setting_apply), settings);
    gtk_widget_set_sensitive (apply_button, FALSE);

    check = gtk_builder_get_object (builder, "auto-enable-profiles");
    xfconf_g_property_bind (channel, "/AutoEnableProfiles", G_TYPE_BOOLEAN, check,
                            "active");

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-save"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_save), settings);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-delete"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_delete), settings);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-apply"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_apply), settings);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "button-profile-create"));
    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (display_settings_profile_create), settings);

    /* Populate the combobox */
    xfce_display_settings_populate_combobox (settings);
    display_settings_profile_list_init (builder);
    xfce_display_settings_populate_profile_list (settings);

    return GTK_WIDGET (gtk_builder_get_object (builder, "display-dialog"));
}

static void
display_settings_minimal_only_display1_toggled (GtkToggleButton *button,
                                                XfceDisplaySettings *settings)
{
    GObject *buttons;

    if (!gtk_toggle_button_get_active (button))
        return;

    if (xfce_display_settings_get_n_outputs (settings) <= 1)
        return;

    buttons = gtk_builder_get_object (xfce_display_settings_get_builder (settings), "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Put Display1 in its preferred mode and deactivate Display2 */
    xfce_display_settings_set_active (settings, 0, TRUE);
    xfce_display_settings_set_active (settings, 1, FALSE);

    /* Apply the changes */
    xfce_display_settings_save (settings, 0, "Default");
    xfce_display_settings_save (settings, 1, "Default");
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_only_display2_toggled (GtkToggleButton *button,
                                                XfceDisplaySettings *settings)
{
    GObject *buttons;

    if (!gtk_toggle_button_get_active(button) )
        return;

    if (xfce_display_settings_get_n_outputs (settings) <= 1)
        return;

    buttons = gtk_builder_get_object (xfce_display_settings_get_builder (settings), "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Put Display2 in its preferred mode and deactivate Display1 */
    xfce_display_settings_set_active (settings, 1, TRUE);
    xfce_display_settings_set_active (settings, 0, FALSE);

    /* Apply the changes */
    xfce_display_settings_save (settings, 0, "Default");
    xfce_display_settings_save (settings, 1, "Default");
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_mirror_displays_toggled (GtkToggleButton *button,
                                                  XfceDisplaySettings *settings)
{
    GObject *buttons;
    guint n_outputs;

    if (!gtk_toggle_button_get_active(button))
        return;

    if (xfce_display_settings_get_n_outputs (settings) <= 1)
        return;

    buttons = gtk_builder_get_object (xfce_display_settings_get_builder (settings), "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Activate mirror-mode with a single mode for all of them */
    xfce_display_settings_mirror (settings);
    n_outputs = xfce_display_settings_get_n_outputs (settings);
    for (guint n = 0; n < n_outputs; n++)
        xfce_display_settings_save (settings, n, "Default");

    /* Apply all changes */
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_extend_right_toggled (GtkToggleButton *button,
                                               XfceDisplaySettings *settings)
{
    GObject *buttons;
    guint n_outputs;

    if (!gtk_toggle_button_get_active(button))
        return;

    n_outputs = xfce_display_settings_get_n_outputs (settings);
    if (n_outputs <= 1)
        return;

    buttons = gtk_builder_get_object (xfce_display_settings_get_builder (settings), "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET (buttons), FALSE);

    /* Activate all inactive displays */
    for (guint n = 0; n < n_outputs; n++)
    {
        if (!xfce_display_settings_is_active (settings, n))
            xfce_display_settings_set_active (settings, n, TRUE);
    }

    xfce_display_settings_extend_right (settings, 0, 1);

    /* Save changes to both displays */
    xfce_display_settings_save (settings, 0, "Default");
    xfce_display_settings_save (settings, 1, "Default");

    /* Apply all changes */
    xfconf_channel_set_string (xfce_display_settings_get_channel (settings), "/Schemes/Apply", "Default");

    gtk_widget_set_sensitive (GTK_WIDGET (buttons), TRUE);
}

/* Xfce RANDR GUI **TODO** Place these functions in a sensible location */
static void
on_viewport_changed (FooScrollArea *scroll_area,
                     GdkRectangle  *old_viewport,
                     GdkRectangle  *new_viewport)
{
    foo_scroll_area_set_size (scroll_area,
                              new_viewport->width,
                              new_viewport->height);

    foo_scroll_area_invalidate (scroll_area);
}

static void
layout_set_font (PangoLayout *layout, const char *font)
{
    PangoFontDescription *desc =
        pango_font_description_from_string (font);

    if (desc)
    {
        pango_layout_set_font_description (layout, desc);

        pango_font_description_free (desc);
    }
}

static void
get_geometry (XfceOutput *output, int *w, int *h)
{
    if (output->active)
    {
        if (output->scale_x > 0 && output->scale_x != 1.0
            && output->scale_y > 0 && output->scale_y != 1.0)
        {
            *h = output->mode->height * output->scale_y;
            *w = output->mode->width * output->scale_x;
        }
        else
        {
            *h = output->mode->height;
            *w = output->mode->width;
        }
    }
    else
    {
        *h = output->pref_height;
        *w = output->pref_width;
    }
    if ((output->rotation == ROTATION_FLAGS_90) || (output->rotation == ROTATION_FLAGS_270))
    {
        int tmp;
        tmp = *h;
        *h = *w;
        *w = tmp;
    }
}

static void
initialize_connected_outputs_at_zero (XfceDisplaySettings *settings)
{
    GList *outputs = xfce_display_settings_get_outputs (settings);
    GList *list = NULL;
    gint start_x, start_y;

    start_x = G_MAXINT;
    start_y = G_MAXINT;

    /* Get the left-most and top-most coordinates */
    for (list = outputs; list != NULL; list = list->next)
    {
        XfceOutput *output = list->data;

        start_x = MIN(start_x, output->x);
        start_y = MIN(start_y, output->y);
    }

    /* Realign at zero */
    for (list = outputs; list != NULL; list = list->next)
    {
        XfceOutput *output = list->data;
        xfce_display_settings_set_position (settings, output->id, output->x - start_x, output->y - start_y);
    }
}

static void
get_total_size (GList *outputs,
                gint *total_w,
                gint *total_h)
{
    gint dummy;
    GList *list = NULL;

    if (!total_w)
        total_w = &dummy;
    if (!total_h)
        total_h = &dummy;

    *total_w = 0;
    *total_h = 0;

    for (list = outputs; list != NULL; list = list->next)
    {
        XfceOutput *output = list->data;

        int w, h;

        get_geometry (output, &w, &h);

        *total_w = MAX(*total_w, w + output->x);
        *total_h = MAX(*total_h, h + output->y);
    }
}

static double
compute_scale (XfceDisplaySettings *settings)
{
    int available_w, available_h;
    gint total_w, total_h;
    GdkRectangle viewport;

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)), &viewport);

    get_total_size (xfce_display_settings_get_outputs (settings), &total_w, &total_h);

    available_w = viewport.width - 2 * MARGIN;
    available_h = viewport.height - 2 * MARGIN;

    return MIN ((double)available_w / (double)total_w, (double)available_h / (double)total_h);
}

typedef struct Edge
{
    XfceOutput *output;
    int x1, y1;
    int x2, y2;
} Edge;

typedef struct Snap
{
    Edge *snapper;      /* Edge that should be snapped */
    Edge *snappee;
    int dy, dx;
} Snap;

static void
add_edge (XfceOutput *output, int x1, int y1, int x2, int y2, GArray *edges)
{
    Edge e;

    e.x1 = x1;
    e.x2 = x2;
    e.y1 = y1;
    e.y2 = y2;
    e.output = output;

    g_array_append_val (edges, e);
}

static void
list_edges_for_output (XfceOutput *output, GArray *edges)
{
    int x, y, w, h;

    x = output->x;
    y = output->y;
    get_geometry (output, &w, &h);

    /* Top, Bottom, Left, Right */
    add_edge (output, x, y, x + w, y, edges);
    add_edge (output, x, y + h, x + w, y + h, edges);
    add_edge (output, x, y, x, y + h, edges);
    add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (GList *outputs,
            GArray *edges)
{
    for (GList *list = outputs; list != NULL; list = list->next)
    {
        list_edges_for_output (list->data, edges);
    }
}

static gboolean
overlap (int s1, int e1, int s2, int e2)
{
    return (!(e1 < s2 || s1 >= e2));
}

static gboolean
horizontal_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->y1 != snapper->y2 || snappee->y1 != snappee->y2)
        return FALSE;

    return overlap (snapper->x1, snapper->x2, snappee->x1, snappee->x2);
}

static gboolean
vertical_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->x1 != snapper->x2 || snappee->x1 != snappee->x2)
        return FALSE;

    return overlap (snapper->y1, snapper->y2, snappee->y1, snappee->y2);
}

static void
add_snap (GArray *snaps, Snap snap)
{
    if (ABS (snap.dx) <= 200 || ABS (snap.dy) <= 200)
        g_array_append_val (snaps, snap);
}

static void
add_edge_snaps (Edge *snapper, Edge *snappee, GArray *snaps)
{
    Snap snap;

    snap.snapper = snapper;
    snap.snappee = snappee;

    if (horizontal_overlap (snapper, snappee))
    {
        snap.dx = 0;
        snap.dy = snappee->y1 - snapper->y1;

        add_snap (snaps, snap);
    }
    else if (vertical_overlap (snapper, snappee))
    {
        snap.dy = 0;
        snap.dx = snappee->x1 - snapper->x1;

        add_snap (snaps, snap);
    }

    /* Corner snaps */
    /* 1->1 */
    snap.dx = snappee->x1 - snapper->x1;
    snap.dy = snappee->y1 - snapper->y1;

    add_snap (snaps, snap);

    /* 1->2 */
    snap.dx = snappee->x2 - snapper->x1;
    snap.dy = snappee->y2 - snapper->y1;

    add_snap (snaps, snap);

    /* 2->2 */
    snap.dx = snappee->x2 - snapper->x2;
    snap.dy = snappee->y2 - snapper->y2;

    add_snap (snaps, snap);

    /* 2->1 */
    snap.dx = snappee->x1 - snapper->x2;
    snap.dy = snappee->y1 - snapper->y2;

    add_snap (snaps, snap);
}

static void
list_snaps (XfceOutput *output, GArray *edges, GArray *snaps)
{
    guint i;

    for (i = 0; i < edges->len; ++i)
    {
        Edge *output_edge = &(g_array_index (edges, Edge, i));

        if (output_edge->output == output)
        {
            guint j;

            for (j = 0; j < edges->len; ++j)
            {
                Edge *edge = &(g_array_index (edges, Edge, j));

                if (edge->output != output)
                    add_edge_snaps (output_edge, edge, snaps);
            }
        }
    }
}

static gboolean
corner_on_edge (int x, int y, Edge *e)
{
    if (x == e->x1 && x == e->x2 && y >= e->y1 && y <= e->y2)
        return TRUE;

    if (y == e->y1 && y == e->y2 && x >= e->x1 && x <= e->x2)
        return TRUE;

    return FALSE;
}

static gboolean
edges_align (Edge *e1, Edge *e2)
{
    if (corner_on_edge (e1->x1, e1->y1, e2))
        return TRUE;

    if (corner_on_edge (e2->x1, e2->y1, e1))
        return TRUE;

    return FALSE;
}

static gboolean
output_is_aligned (XfceOutput *output, GArray *edges)
{
    gboolean result = FALSE;
    guint i;

    for (i = 0; i < edges->len; ++i)
    {
        Edge *output_edge = &(g_array_index (edges, Edge, i));

        if (output_edge->output == output)
        {
            guint j;

            for (j = 0; j < edges->len; ++j)
            {
                Edge *edge = &(g_array_index (edges, Edge, j));

                /* We are aligned if an output edge matches
                 * an edge of another output
                 */
                if (edge->output != output_edge->output)
                {
                    if (edges_align (output_edge, edge))
                    {
                        result = TRUE;
                        goto done;
                    }
                }
            }
        }
    }
done:
    return result;
}

static void
get_output_rect (XfceOutput *output, GdkRectangle *rect)
{
    int w, h;

    get_geometry (output, &w, &h);

    rect->width = w;
    rect->height = h;
    rect->x = output->x;
    rect->y = output->y;
}

static gboolean
output_overlaps (GList *outputs,
                 XfceOutput *output)
{
    GdkRectangle output_rect;

    get_output_rect (output, &output_rect);

    for (GList *list = outputs; list != NULL; list = list->next)
    {
        XfceOutput *other = list->data;
        if (other != output)
        {
            GdkRectangle other_rect;
            get_output_rect (other, &other_rect);
            if (gdk_rectangle_intersect (&output_rect, &other_rect, NULL))
                return TRUE;
        }
    }

    return FALSE;
}

static gboolean
xfce_rr_config_is_aligned (GList *outputs,
                           GArray *edges)
{
    for (GList *list = outputs; list != NULL; list = list->next)
    {
        XfceOutput *output = list->data;
        if (!output_is_aligned (output, edges) || output_overlaps (outputs, output))
            return FALSE;
    }

    return TRUE;
}

typedef struct _GrabInfo
{
    int grab_x;
    int grab_y;
    int output_x;
    int output_y;
} GrabInfo;

static gboolean
is_corner_snap (const Snap *s)
{
    return s->dx != 0 && s->dy != 0;
}

static int
compare_snaps (gconstpointer v1, gconstpointer v2)
{
    const Snap *s1 = v1;
    const Snap *s2 = v2;
    int sv1 = MAX (ABS (s1->dx), ABS (s1->dy));
    int sv2 = MAX (ABS (s2->dx), ABS (s2->dy));
    int d;

    d = sv1 - sv2;

    /* This snapping algorithm is good enough for rock'n'roll, but
     * this is probably a better:
     *
     *    First do a horizontal/vertical snap, then
     *    with the new coordinates from that snap,
     *    do a corner snap.
     *
     * Right now, it's confusing that corner snapping
     * depends on the distance in an axis that you can't actually see.
     *
     */
    if (d == 0)
    {
        if (is_corner_snap (s1) && !is_corner_snap (s2))
            return -1;
        else if (is_corner_snap (s2) && !is_corner_snap (s1))
            return 1;
        else
            return 0;
    }
    else
    {
        return d;
    }
}

/* Sets a mouse cursor for a widget's window.  As a hack, you can pass
 * GDK_BLANK_CURSOR to mean "set the cursor to NULL" (i.e. reset the widget's
 * window's cursor to its default).
 */
static void
set_cursor (GtkWidget *widget, GdkCursorType type)
{
    GdkCursor *cursor;
    GdkWindow *window;

    if (type == GDK_BLANK_CURSOR)
        cursor = NULL;
    else
        cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), type);

    window = gtk_widget_get_window (widget);

    if (window)
        gdk_window_set_cursor (window, cursor);

    if (cursor)
        g_object_unref (cursor);
}

static void
set_monitors_tooltip (XfceDisplaySettings *settings,
                      gchar *tooltip_text)
{
    const char *text;

    if (tooltip_text)
        text = g_strdup (tooltip_text);

    else
        text = _("Select a monitor to change its properties; drag it to rearrange its placement.");

    gtk_widget_set_tooltip_text (xfce_display_settings_get_scroll_area (settings), text);
}

static void
on_output_event (FooScrollArea      *area,
                 FooScrollAreaEvent *event,
                 gpointer            data)
{
    XfceOutput *output = data;
    XfceDisplaySettings *settings = g_object_get_data (G_OBJECT (area), "settings");
    MirroredState state = xfce_display_settings_get_mirrored_state (settings);

    /* If the mouse is inside the outputs, set the cursor to "you can move me".  See
     * on_canvas_event() for where we reset the cursor to the default if it
     * exits the outputs' area.
     */
    if (event->type == FOO_MOTION_OUTSIDE)
        return;

    if (state == MIRRORED_STATE_NONE && xfce_display_settings_get_n_outputs (settings) > 1)
        set_cursor (GTK_WIDGET (area), GDK_FLEUR);

    if (event->type == FOO_BUTTON_PRESS)
    {
        GrabInfo *info;
        gchar *tooltip_text;
        GtkBuilder *builder = xfce_display_settings_get_builder (settings);
        GtkWidget *combobox = GTK_WIDGET (gtk_builder_get_object (builder, "randr-outputs"));

        gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), output->id);

        if (state == MIRRORED_STATE_NONE && xfce_display_settings_get_n_outputs (settings) > 1)
        {
            foo_scroll_area_begin_grab (area, on_output_event, data);

            info = g_new0 (GrabInfo, 1);
            info->grab_x = event->x;
            info->grab_y = event->y;
            info->output_x = output->x;
            info->output_y = output->y;

            tooltip_text = g_strdup_printf(_("(%i, %i)"), output->x, output->y);
            set_monitors_tooltip (settings, tooltip_text);
            g_free (tooltip_text);

            output->user_data = info;
        }

        foo_scroll_area_invalidate (area);
    }
    else
    {
        if (foo_scroll_area_is_grabbed (area))
        {
            GList *outputs = xfce_display_settings_get_outputs (settings);
            GrabInfo *info = output->user_data;
            double scale = compute_scale (settings);
            int new_x, new_y;
            guint i;
            GArray *edges, *snaps;

            new_x = info->output_x + (event->x - info->grab_x) / scale;
            new_y = info->output_y + (event->y - info->grab_y) / scale;

            output->x = new_x;
            output->y = new_y;

            edges = g_array_new (TRUE, TRUE, sizeof (Edge));
            snaps = g_array_new (TRUE, TRUE, sizeof (Snap));

            list_edges (outputs, edges);
            list_snaps (output, edges, snaps);

            g_array_sort (snaps, compare_snaps);

            output->x = info->output_x;
            output->y = info->output_y;

            for (i = 0; i < snaps->len; ++i)
            {
                Snap *snap = &(g_array_index (snaps, Snap, i));
                GArray *new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

                output->x = new_x + snap->dx;
                output->y = new_y + snap->dy;

                g_array_set_size (new_edges, 0);
                list_edges (outputs, new_edges);

                if (xfce_rr_config_is_aligned (outputs, new_edges))
                {
                    g_array_free (new_edges, TRUE);
                    break;
                }
                else
                {
                    output->x = info->output_x;
                    output->y = info->output_y;
                    g_array_free (new_edges, TRUE);
                }
            }

            g_array_free (snaps, TRUE);
            g_array_free (edges, TRUE);

            if (event->type == FOO_BUTTON_RELEASE)
            {
                foo_scroll_area_end_grab (area);
                set_monitors_tooltip (settings, NULL);

                g_free (output->user_data);
                output->user_data = NULL;

                initialize_connected_outputs_at_zero (settings);
                display_settings_changed ();
            }
            else
            {
                set_monitors_tooltip (settings, g_strdup_printf(_("(%i, %i)"), output->x, output->y) );
            }

            foo_scroll_area_invalidate (area);
        }
    }
}

static void
on_canvas_event (FooScrollArea      *area,
                 FooScrollAreaEvent *event,
                 gpointer            data)
{
    /* If the mouse exits the outputs, reset the cursor to the default.  See
     * on_output_event() for where we set the cursor to the movement cursor if
     * it is over one of the outputs.
     */
    set_cursor (GTK_WIDGET (area), GDK_BLANK_CURSOR);
}

static void
paint_background (FooScrollArea *area,
                  cairo_t       *cr)
{
    GdkRectangle viewport;
    GtkWidget *widget;
    GtkStyleContext *ctx;

    widget = GTK_WIDGET (area);

    foo_scroll_area_get_viewport (area, &viewport);
    ctx = gtk_widget_get_style_context (widget);
    gtk_style_context_add_class (ctx, "view");

    foo_scroll_area_add_input_from_fill (area, cr, on_canvas_event, NULL);
}

static void
paint_output (XfceDisplaySettings *settings,
              cairo_t *cr,
              int i,
              gint scale_factor,
              double *snap_x,
              double *snap_y)
{
    int w, h;
    double x, y, end_x, end_y;
    gint total_w, total_h;
    GList *outputs = xfce_display_settings_get_outputs (settings);
    double scale = compute_scale (settings);
    XfceOutput *output = NULL;
    GList *entry = NULL;
    PangoLayout *layout;
    PangoRectangle ink_extent, log_extent;
    GdkRectangle viewport;
    cairo_pattern_t *pat_lin = NULL, *pat_radial = NULL;
    double alpha = 1.0;
    double available_w;
    double factor = 1.0;
    const char *text;
    MirroredState state = xfce_display_settings_get_mirrored_state (settings);
    guint selected_id = xfce_display_settings_get_selected_output_id (settings);

    get_total_size (outputs, &total_w, &total_h);

    cairo_save (cr);

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)), &viewport);

    entry = g_list_nth (outputs, i);
    if (entry)
        output = entry->data;
    if (output)
        get_geometry (output, &w, &h);
    else
        return;

    viewport.height -= 2 * MARGIN;
    viewport.width -= 2 * MARGIN;

    /* Center the displayed outputs in the viewport */
    x = ceil (output->x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0);
    y = ceil (output->y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0);

    /* Align endpoints */
    end_x = x + ceil (w * scale);
    end_y = y + ceil (h * scale);
    if ( abs((int)end_x-(int)*snap_x) <= 1 )
    {
        end_x = *snap_x;
    }
    if ( abs((int)end_y-(int)*snap_y) <= 1 )
    {
        end_y = *snap_y;
    }
    *snap_x = end_x;
    *snap_y = end_y;

    cairo_translate (cr,
                     x + (w * scale) / 2,
                     y + (h * scale) / 2);

    /* rotation is already applied in get_geometry */

    if (output->rotation == ROTATION_FLAGS_REFLECT_X)
        cairo_scale (cr, -1, 1);

    if (output->rotation == ROTATION_FLAGS_REFLECT_Y)
        cairo_scale (cr, 1, -1);

    cairo_translate (cr,
                     - x - (w * scale) / 2,
                     - y - (h * scale) / 2);

    cairo_rectangle (cr, x, y, end_x - x, end_y - y);
    cairo_clip_preserve (cr);

    foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (xfce_display_settings_get_scroll_area (settings)),
                                         cr, on_output_event, output);

    cairo_set_line_width (cr, 1.0);

    /* Make overlapping displays ('mirrored') more transparent so both displays can
       be recognized more easily */
    if (output->id != selected_id && state == MIRRORED_STATE_MIRRORED)
        alpha = 0.5;
    /* When displays are cloned it makes no sense to make them semi-transparent
       because they overlay each other completely */
    else if (state == MIRRORED_STATE_CLONED)
        alpha = 1.0;
    /* the inactive display should be more transparent and the overlapping one as
       well */
    else if (output->id != selected_id || state == MIRRORED_STATE_MIRRORED)
        alpha = 0.7;

    if (output->active)
    {
        /* Background gradient for active display */
        pat_lin = cairo_pattern_create_linear (x, y, x, y + (h * scale));
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.0, 0.56, 0.85, 0.92, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.2, 0.33, 0.75, 0.92, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.7, 0.25, 0.57, 0.77, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 1.0, 0.17, 0.39, 0.63, alpha);
        cairo_set_source (cr, pat_lin);
        cairo_fill_preserve (cr);

        cairo_set_source_rgba (cr, 0.11, 0.26, 0.52, alpha);
        cairo_stroke (cr);
    }
    else
    {
        /* Background gradient for disabled display */
        pat_lin = cairo_pattern_create_linear (x, y, x, y + (h * scale));
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.0, 0.24, 0.3, 0.31, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.2, 0.17, 0.20, 0.22, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 0.7, 0.14, 0.16, 0.18, alpha);
        cairo_pattern_add_color_stop_rgba (pat_lin, 1.0, 0.07, 0.07, 0.07, alpha);
        cairo_set_source (cr, pat_lin);
        cairo_fill_preserve (cr);

        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, alpha);
        cairo_stroke (cr);
    }

    /* Draw inner stroke */
    cairo_rectangle (cr, x + 1.5, y + 1.5, end_x - x - 3, end_y - y - 3);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha - 0.75);
    cairo_stroke (cr);

    /* Draw reflection as radial gradient on a polygon */
    pat_radial = cairo_pattern_create_radial ((end_x -x) /2 + x, y, 1, (end_x -x) /2 + x, y, h * scale);
    cairo_pattern_add_color_stop_rgba (pat_radial, 0.0, 1.0, 1.0, 1.0, 0.4);
    cairo_pattern_add_color_stop_rgba (pat_radial, 0.5, 1.0, 1.0, 1.0, 0.15);
    cairo_pattern_add_color_stop_rgba (pat_radial, 0.8, 1.0, 1.0, 1.0, 0.0);

    cairo_move_to (cr, x + 1.5, y + 1.5);
    cairo_line_to (cr, end_x - 1.5, y + 1.5);
    cairo_line_to (cr, end_x - 1.5, y + (end_y - y) / 3);
    cairo_line_to (cr, x + 1.5, y + (end_y - y) / 1.5);
    cairo_close_path (cr);
    cairo_set_source (cr, pat_radial);
    cairo_fill (cr);

    /* Draw a panel type rectangle to show which monitor is primary */
    if (xfce_display_settings_is_primary (settings, output->id))
    {
        GdkPixbuf   *pixbuf;
        GtkIconInfo *icon_info;
        GdkRGBA      fg;
        gint         icon_size;

        gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, &icon_size);
        icon_info = gtk_icon_theme_lookup_icon_for_scale (gtk_icon_theme_get_default (),
                                                          "help-about-symbolic",
                                                          icon_size,
                                                          scale_factor,
                                                          GTK_ICON_LOOKUP_GENERIC_FALLBACK
                                                          | GTK_ICON_LOOKUP_FORCE_SIZE);

        gdk_rgba_parse (&fg, "#000000");
        pixbuf = gtk_icon_info_load_symbolic (icon_info, &fg, NULL, NULL, NULL, NULL, NULL);
        if (G_LIKELY (pixbuf != NULL))
        {
            cairo_save (cr);
            cairo_translate (cr, x + scale_factor, y + scale_factor);
            cairo_scale (cr, 1.0 / scale_factor, 1.0 / scale_factor);
            gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
            cairo_paint (cr);
            cairo_restore (cr);

            g_object_unref (pixbuf);
        }
    }

    /* Display name label*/
    if (state == MIRRORED_STATE_CLONED)
    {
    /* Translators:  this is the feature where what you see on your laptop's
     * screen is the same as your external monitor.  Here, "Mirror" is being
     * used as an adjective, not as a verb.  For example, the Spanish
     * translation could be "Pantallas en Espejo", *not* "Espejar Pantallas".
     */
        text = _("Mirror Screens");
    }
    else
    {
        text = output->friendly_name;
    }
    layout = gtk_widget_create_pango_layout (xfce_display_settings_get_scroll_area (settings), text);
    layout_set_font (layout, "Sans Bold 12");
    pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);

    available_w = w * scale + 0.5 - 6; /* Same as the inner rectangle's width, minus 1 pixel of padding on each side */

    cairo_scale (cr, factor, factor);

    if (available_w < ink_extent.width)
    {
        pango_layout_set_width (layout, available_w * PANGO_SCALE);
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
        log_extent.width = available_w;
    }

    cairo_move_to (cr,
                   x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                   y + ((h * scale + 0.5) - factor * log_extent.height) / 2 - 1);
    /* Try to make the text as readable as possible for overlapping displays */
    if (output->id == selected_id && state == MIRRORED_STATE_MIRRORED)
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, alpha);
    else
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, alpha - 0.6);

    pango_cairo_show_layout (cr, layout);

    cairo_move_to (cr,
                   x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                   y + ((h * scale + 0.5) - factor * log_extent.height) / 2);

    /* Try to make the text as readable as possible for overlapping displays - the
       currently selected one could be painted below the other display*/
    if (output->id == selected_id && state == MIRRORED_STATE_MIRRORED)
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
    else
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha);

    pango_cairo_show_layout (cr, layout);

    /* Display state label */
    if (!output->active)
    {
        PangoLayout *display_state;

        display_state = gtk_widget_create_pango_layout (xfce_display_settings_get_scroll_area (settings), _("(Disabled)"));
        layout_set_font (display_state, "Sans 8");
        pango_layout_get_pixel_extents (display_state, &ink_extent, &log_extent);

        available_w = w * scale + 0.5 - 6;
        if (available_w < ink_extent.width)
            factor = available_w / ink_extent.width;
        else
            factor = 1.0;
        cairo_move_to (cr,
                       x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                       y + ((h * scale + 0.5) - factor * log_extent.height) / 2 + 18);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
        pango_cairo_show_layout (cr, display_state);
        g_object_unref (display_state);
    }

    /* Show display number in the left bottom corner if there's more than 1*/
    if (xfce_display_settings_get_n_outputs (settings) > 1)
    {
        PangoLayout *display_number;
        gchar *display_num;


        display_num = g_strdup_printf ("%d", i + 1);
        display_number = gtk_widget_create_pango_layout (xfce_display_settings_get_scroll_area (settings), display_num);
        layout_set_font (display_number, "Mono Bold 9");
        pango_layout_get_pixel_extents (display_number, &ink_extent, &log_extent);

        available_w = w * scale + 0.5 - 6;
        if (available_w < ink_extent.width)
            factor = available_w / ink_extent.width;
        else
            factor = 1.0;

        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.4);
        cairo_arc (cr,
                   x + (w * scale + 0.5) / 2,
                   y + ((h * scale + 0.5)) - (factor * log_extent.height / 2) - 3.5,
                   factor * log_extent.height / 2 + 2.5, 0.0, 2 * M_PI);
        cairo_fill (cr);
        cairo_move_to (cr,
                       x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                       y + ((h * scale + 0.5) - factor * log_extent.height) - 3.5);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
        pango_cairo_show_layout (cr, display_number);
        g_object_unref (display_number);
        g_free (display_num);
    }

    cairo_restore (cr);

    if (pat_lin)
        cairo_pattern_destroy (pat_lin);
    if (pat_radial)
        cairo_pattern_destroy (pat_radial);

    g_object_unref (layout);
}

static void
on_area_paint (FooScrollArea *area,
               cairo_t *cr,
               XfceDisplaySettings *settings)
{
    GList *outputs = xfce_display_settings_get_outputs (settings);
    GList *list;
    double x = 0.0, y = 0.0;
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (area));
    guint selected_id = xfce_display_settings_get_selected_output_id (settings);

    paint_background (area, cr);

    for (list = outputs; list != NULL; list = list->next)
    {
        gint i;

        i = g_list_position (outputs, list);
        /* Always paint the currently selected display last, i.e. on top, so it's
           visible and the name is readable */
        if (i >= 0 && (guint)i == selected_id) {
            continue;
        }
        paint_output (settings, cr, i, scale_factor, &x, &y);

        if (xfce_display_settings_get_mirrored_state (settings) == MIRRORED_STATE_CLONED)
            break;
    }
    /* Finally also paint the active output */
    paint_output (settings, cr, selected_id, scale_factor, &x, &y);
}
/* Xfce RANDR GUI */

static void
display_settings_show_main_dialog (XfceDisplaySettings *settings)
{
    XfconfChannel *channel = xfce_display_settings_get_channel (settings);
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GtkWidget *dialog;
    GError *error = NULL;
    GtkWidget *gui_container;
    GtkWidget *scroll_area;

    /* Load the Gtk user-interface file */
    if (gtk_builder_add_from_string (builder, display_dialog_ui,
                                     display_dialog_ui_length, &error) != 0)
    {
        xfce_display_settings_set_outputs (settings);

        /* Build the dialog */
        dialog = display_settings_dialog_new (settings);

        /* Scroll Area */
        scroll_area = xfce_display_settings_get_scroll_area (settings);
        g_object_set_data (G_OBJECT (scroll_area), "settings", settings);

        set_monitors_tooltip (settings, NULL);

        /* FIXME: this should be computed dynamically */
        foo_scroll_area_set_min_size (FOO_SCROLL_AREA (scroll_area), -1, 200);
        gtk_widget_show (scroll_area);
        g_signal_connect (scroll_area, "paint", G_CALLBACK (on_area_paint), settings);
        g_signal_connect (scroll_area, "viewport_changed", G_CALLBACK (on_viewport_changed), NULL);

        gui_container = GTK_WIDGET (gtk_builder_get_object (builder, "randr-dnd"));
        gtk_container_add (GTK_CONTAINER (gui_container), scroll_area);
        gtk_widget_show_all (gui_container);

        /* Keep track of the profile that was active when the dialog was launched */
        active_profile = xfconf_channel_get_string (channel, "/ActiveProfile", "Default");

#ifdef HAVE_XRANDR
        if (opt_socket_id != 0 && GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
            GtkWidget *plug;
            GObject *plug_child;

            /* Create plug widget */
            plug = gtk_plug_new (opt_socket_id);
            g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
            gtk_widget_show (plug);

            /* Get plug child widget */
            plug_child = gtk_builder_get_object (builder, "plug-child");
            xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));
        }
        else
#endif
        {
            g_signal_connect (G_OBJECT (dialog), "response",
                              G_CALLBACK (display_settings_dialog_response), settings);
            g_signal_connect (G_OBJECT (dialog), "destroy",
                              G_CALLBACK (gtk_main_quit), NULL);
            /* Show the dialog */
            gtk_window_present (GTK_WINDOW (dialog));
        }

        /* Enter the main loop */
        gtk_main ();

        gtk_widget_destroy (dialog);
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }
}

static gboolean
display_settings_minimal_dialog_key_press_event(GtkWidget *widget,
                                                GdkEventKey *event,
                                                gpointer user_data)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

static void
display_settings_minimal_advanced_clicked (GtkButton *button,
                                           XfceDisplaySettings *settings)
{
    GtkWidget *dialog;

    dialog = GTK_WIDGET (gtk_builder_get_object (xfce_display_settings_get_builder (settings), "dialog"));
    gtk_widget_hide (dialog);

    display_settings_show_main_dialog (settings);

    gtk_main_quit ();
}

static void
display_settings_minimal_get_positions (GtkWidget    *dialog,
                                        GdkRectangle *monitor_rect,
                                        GdkRectangle *window_rect)
{
    GdkDisplay *display;
    GdkSeat    *seat;
    GdkMonitor *monitor;
    gint cursorx, cursory;

    display = gdk_display_get_default ();
    seat = gdk_display_get_default_seat (display);
    gdk_window_get_device_position (gdk_get_default_root_window (),
                                    gdk_seat_get_pointer (seat),
                                    &cursorx, &cursory, NULL);

    monitor = gdk_display_get_monitor_at_point (display, cursorx, cursory);
    gdk_monitor_get_geometry (monitor, monitor_rect);
    gtk_window_get_position (GTK_WINDOW (dialog), &window_rect->x, &window_rect->y);
    gtk_window_get_size (GTK_WINDOW (dialog), &window_rect->width, &window_rect->height);
}

static gboolean
display_settings_minimal_center (gpointer user_data)
{
    GdkRectangle monitor_rect, window_rect;
    GtkWidget *dialog = user_data;

    display_settings_minimal_get_positions (dialog, &monitor_rect, &window_rect);

    gtk_window_move (GTK_WINDOW (dialog),
                     monitor_rect.x + monitor_rect.width / 2 - window_rect.width / 2,
                     monitor_rect.y + monitor_rect.height / 2 - window_rect.height / 2);

    return FALSE;
}

static void
display_settings_minimal_cycle (GtkWidget  *dialog,
                                GtkBuilder *builder)
{
    GtkToggleButton *only_display1, *mirror_displays, *extend_right, *only_display2;

    only_display1 = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "display1"));
    mirror_displays = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "mirror"));
    extend_right = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "extend_right"));
    only_display2 = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "display2"));

    if (gtk_toggle_button_get_active (only_display1))
        if (gtk_widget_get_sensitive (GTK_WIDGET (mirror_displays)))
            gtk_toggle_button_set_active (mirror_displays, TRUE);
        else
            gtk_toggle_button_set_active (extend_right, TRUE);
    else if (gtk_toggle_button_get_active (mirror_displays))
        gtk_toggle_button_set_active (extend_right, TRUE);
    else if (gtk_toggle_button_get_active (extend_right))
        gtk_toggle_button_set_active (only_display2, TRUE);
    else
        gtk_toggle_button_set_active (only_display1, TRUE);

    g_timeout_add_seconds (1, display_settings_minimal_center, dialog);
}

static void
display_settings_minimal_activated (GApplication *application,
                                    XfceDisplaySettings *settings)
{
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GtkWidget  *dialog;
    GdkRectangle monitor_rect, window_rect;

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
    display_settings_minimal_get_positions (dialog, &monitor_rect, &window_rect);

    /* Check if dialog is already at current monitor (where cursor is at) */
    if (gdk_rectangle_intersect (&monitor_rect, &window_rect, NULL))
    {
        /* Select next preset if dialog is already at current monitor */
        display_settings_minimal_cycle (dialog, builder);
    }
    else
    {
        /* Center at current monitor if displayed elsewhere */
        display_settings_minimal_center (dialog);
    }

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
display_settings_minimal_load_icon (GtkBuilder  *builder,
                                    const gchar *img_name,
                                    const gchar *icon_name)
{
    GObject      *dialog;
    GtkImage     *img;
    GtkIconTheme *icon_theme;
    GdkPixbuf    *icon;
    cairo_surface_t *surface = NULL;
    gint scale_factor;

    dialog = gtk_builder_get_object (builder, "dialog");
    img = GTK_IMAGE (gtk_builder_get_object (builder, img_name));
    g_return_if_fail (dialog && img);
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (dialog));

    icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (dialog)));
    icon = gtk_icon_theme_load_icon_for_scale (icon_theme, icon_name, 128, scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    if (G_LIKELY (icon != NULL))
    {
        surface = gdk_cairo_surface_create_from_pixbuf (icon, scale_factor, NULL);
        g_object_unref (icon);
    }
    gtk_image_set_from_surface (GTK_IMAGE (img), surface);

    if (G_LIKELY (surface != NULL))
        cairo_surface_destroy (surface);
}

static void
display_settings_show_minimal_dialog (XfceDisplaySettings *settings)
{
    XfconfChannel *channel = xfce_display_settings_get_channel (settings);
    GtkBuilder *builder = xfce_display_settings_get_builder (settings);
    GtkWidget  *dialog, *cancel;
    GObject    *only_display1, *only_display2, *mirror_displays, *mirror_displays_label;
    GObject    *extend_right, *advanced, *fake_button, *label, *profile_box;
    GError     *error = NULL;
    gboolean    found = FALSE;
    GtkApplication *app;

    if (gtk_builder_add_from_string (builder, minimal_display_dialog_ui,
                                     minimal_display_dialog_ui_length, &error) != 0)
    {
        gchar *only_display1_label, *only_display2_label;

        /* Build the minimal dialog */
        dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        cancel = GTK_WIDGET (gtk_builder_get_object (builder, "cancel_button"));

        g_signal_connect (dialog, "key-press-event", G_CALLBACK (display_settings_minimal_dialog_key_press_event), NULL);
        g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (cancel, "clicked", G_CALLBACK (gtk_main_quit), NULL);

        display_settings_minimal_load_icon (builder, "image1", "xfce-display-internal");
        display_settings_minimal_load_icon (builder, "image2", "xfce-display-mirror");
        display_settings_minimal_load_icon (builder, "image3", "xfce-display-extend");
        display_settings_minimal_load_icon (builder, "image4", "xfce-display-external");

        only_display1 = gtk_builder_get_object (builder, "display1");
        mirror_displays = gtk_builder_get_object (builder, "mirror");
        mirror_displays_label = gtk_builder_get_object (builder, "label2");
        extend_right = gtk_builder_get_object (builder, "extend_right");
        only_display2 = gtk_builder_get_object (builder, "display2");
        advanced = gtk_builder_get_object (builder, "advanced_button");
        fake_button = gtk_builder_get_object (builder, "fake_button");

        /* Create the profile radiobuttons */
        display_settings_minimal_profile_populate (settings);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fake_button), TRUE);

        label = gtk_builder_get_object (builder, "label1");
        only_display1_label = g_strdup_printf (_("Only %s (1)"), xfce_display_settings_get_friendly_name (settings, 0));
        gtk_label_set_text (GTK_LABEL (label), only_display1_label);
        gtk_widget_set_tooltip_text (GTK_WIDGET (label), only_display1_label);
        g_free (only_display1_label);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display1), xfce_display_settings_is_active (settings, 0));

        if (xfce_display_settings_get_n_outputs (settings) > 1)
        {
            gboolean clonable = xfce_display_settings_is_clonable (settings);
            label = gtk_builder_get_object (builder, "label4");
            only_display2_label = g_strdup_printf (_("Only %s (2)"), xfce_display_settings_get_friendly_name (settings, 1));
            gtk_label_set_text (GTK_LABEL (label), only_display2_label);
            gtk_widget_set_tooltip_text (GTK_WIDGET (label), only_display2_label);
            g_free (only_display2_label);
            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays), clonable);
            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays_label), clonable);

            if (xfce_display_settings_is_active (settings, 0))
            {
                if (xfce_display_settings_is_active (settings, 1))
                {
                    /* Check for mirror */
                    if (xfce_display_settings_is_mirrored (settings, 0) && xfce_display_settings_is_mirrored (settings, 1))
                    {
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mirror_displays), TRUE);
                        found = TRUE;
                    }
                    /* Check for Extend Right */
                    if (!found && xfce_display_settings_is_extended (settings, 0, 1))
                    {
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (extend_right), TRUE);
                        found = TRUE;
                    }
                }
                /* Toggle Primary */
                if (!found)
                {
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display1), TRUE);
                }
            }
            /* Toggle Secondary */
            else
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display2), TRUE);
            }
        }
        else
        {
            /* Only one output, disable other buttons */
            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (extend_right), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (only_display2), FALSE);
        }

        /* Initialize application to ensure single instance */
        app = gtk_application_new ("org.xfce.display.settings", 0);

        g_application_register (G_APPLICATION (app), NULL, &error);
        if (error != NULL)
          {
            g_warning ("Unable to register GApplication: %s", error->message);
            g_error_free (error);
            error = NULL;
          }

        if (g_application_get_is_remote (G_APPLICATION (app)))
          {
            g_application_activate (G_APPLICATION (app));
            g_object_unref (app);
            return;
          }

        g_signal_connect (only_display1, "toggled", G_CALLBACK (display_settings_minimal_only_display1_toggled), settings);
        g_signal_connect (mirror_displays, "toggled", G_CALLBACK (display_settings_minimal_mirror_displays_toggled), settings);
        g_signal_connect (extend_right, "toggled", G_CALLBACK (display_settings_minimal_extend_right_toggled), settings);
        g_signal_connect (only_display2, "toggled", G_CALLBACK (display_settings_minimal_only_display2_toggled), settings);
        g_signal_connect (advanced, "clicked", G_CALLBACK (display_settings_minimal_advanced_clicked), settings);

        g_signal_connect (app, "activate", G_CALLBACK (display_settings_minimal_activated), settings);

        /* Auto-apply the first profile in the list */
        if (xfconf_channel_get_bool (channel, "/AutoEnableProfiles", TRUE))
        {
            /* Walk down the widget hierarchy: profile-box -> gtkbox -> gtkradiobutton */
            profile_box  = gtk_builder_get_object (builder, "profile-box");
            if (GTK_IS_CONTAINER (profile_box))
            {
                GList *children = NULL;
                GList *first_profile_box;

                children = gtk_container_get_children (GTK_CONTAINER (profile_box));
                first_profile_box = g_list_first (children);
                if (first_profile_box)
                {
                    GList *grand_children = NULL;
                    GList *current;
                    GtkWidget *box = GTK_WIDGET (first_profile_box->data);

                    grand_children = gtk_container_get_children (GTK_CONTAINER (box));
                    current = g_list_first (grand_children);
                    if (current)
                    {
                        GtkWidget* widget = GTK_WIDGET (grand_children->data);

                        if (GTK_IS_TOGGLE_BUTTON (widget))
                        {
                            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
                        }
                    }
                }
            }
        }

        /* Show the minimal dialog and start the main loop */
        gtk_window_present (GTK_WINDOW (dialog));
        gtk_main ();
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }
}


gint
main (gint argc, gchar **argv)
{
    XfceDisplaySettings *settings;
    GError *error = NULL;

    /* Setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* Initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* Print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* Cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* Print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2023");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* Initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* Print error and exit */
        g_critical ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* Check if X11∕Wayland specific stuff successfully initialized */
    settings = xfce_display_settings_new (opt_minimal, &error);
    if (settings == NULL)
    {
        const gchar *alternative = NULL, *alternative_icon = NULL;
        gchar *command = g_find_program_in_path ("amdcccle");
        gint response;

        if (command != NULL)
        {
            alternative = _("ATI Settings");
            alternative_icon = "ccc_small";
        }

        response = xfce_message_dialog (NULL, NULL, "dialog-error",
                                        _("Unable to start the Xfce Display Settings"),
                                        error->message, _("_Close"), GTK_RESPONSE_CLOSE,
                                        alternative != NULL ? XFCE_BUTTON_TYPE_MIXED : NULL,
                                        alternative_icon, alternative, GTK_RESPONSE_OK, NULL);
        g_clear_error (&error);

        if (response == GTK_RESPONSE_OK && !g_spawn_command_line_async (command, &error))
        {
            xfce_dialog_show_error (NULL, error, _("Unable to launch the proprietary driver settings"));
            g_error_free (error);
        }

        g_free (command);
        xfconf_shutdown ();

        return EXIT_FAILURE;
    }

    if (xfce_display_settings_get_n_outputs (settings) <= 1 || !opt_minimal)
        display_settings_show_main_dialog (settings);
    else
        display_settings_show_minimal_dialog (settings);

    /* Cleanup */
    g_object_unref (settings);
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
