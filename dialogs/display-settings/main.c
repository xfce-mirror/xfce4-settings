/* $Id$ */
/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <xfconf/xfconf.h>
#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "xfce-randr.h"
#include "display-dialog_ui.h"
#include "confirmation-dialog_ui.h"
#include "minimal-display-dialog_ui.h"

enum
{
    COLUMN_OUTPUT_NAME,
    COLUMN_OUTPUT_ICON,
    COLUMN_OUTPUT_ID,
    N_OUTPUT_COLUMNS
};

enum
{
    COLUMN_COMBO_NAME,
    COLUMN_COMBO_VALUE,
    N_COMBO_COLUMNS
};



/* Xrandr rotation name conversion */
static const XfceRotation rotation_names[] =
{
    { RR_Rotate_0,   N_("Normal") },
    { RR_Rotate_90,  N_("Left") },
    { RR_Rotate_180, N_("Inverted") },
    { RR_Rotate_270, N_("Right") }
};



/* Xrandr reflection name conversion */
static const XfceRotation reflection_names[] =
{
    { 0,                         N_("None") },
    { RR_Reflect_X,              N_("Horizontal") },
    { RR_Reflect_Y,              N_("Vertical") },
    { RR_Reflect_X|RR_Reflect_Y, N_("Both") }
};



/* Confirmation dialog data */
typedef struct
{
    GtkBuilder *builder;
    gint count;
} ConfirmationDialog;



/* Option entries */
static gboolean opt_version = FALSE;
static gboolean minimal = FALSE;
static GOptionEntry option_entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    {
    "minimal", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &minimal,
    N_("Minimal interface to set up an external output"),
    NULL
    },
    { NULL }
};

/* Global xfconf channel */
static XfconfChannel *display_channel;
static gboolean       bound_to_channel = FALSE;

/* Pointer to the used randr structure */
XfceRandr *xfce_randr = NULL;



static guint
display_settings_get_n_active_outputs (void)
{
    guint n, count = 0;

    g_assert (xfce_randr != NULL);

    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] != None)
            ++count;
    }
    return count;
}



static gboolean
display_setting_combo_box_get_value (GtkComboBox *combobox,
                                     gint        *value)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        model = gtk_combo_box_get_model (combobox);
        gtk_tree_model_get (model, &iter, COLUMN_COMBO_VALUE, value, -1);

        return TRUE;
    }

    return FALSE;
}

static gboolean
display_settings_update_time_label (ConfirmationDialog *confirmation_dialog)
{
    GObject *dialog;

    dialog = gtk_builder_get_object (confirmation_dialog->builder, "dialog1");
    confirmation_dialog->count--;

    if (confirmation_dialog->count <= 0)
    {
        gtk_dialog_response (GTK_DIALOG (dialog), 1);

        return FALSE;
    }
    else
    {
        GObject *label;
        gchar   *string;

        string = g_strdup_printf (_("The previous configuration will be restored in %i"
                                    " seconds if you do not reply to this question."),
                                  confirmation_dialog->count);

        label = gtk_builder_get_object (confirmation_dialog->builder, "label2");
        gtk_label_set_text (GTK_LABEL (label), string);

        return TRUE;
    }

    return TRUE;
}


/* Returns true if the configuration is to be kept or false if it is to
 * be reverted */
static gboolean
display_setting_timed_confirmation (GtkBuilder *main_builder)
{
    GtkBuilder *builder;
    GObject    *main_dialog;
    GError     *error = NULL;
    gint        response_id;
    gint        source_id;

    /* Lock the main UI */
    main_dialog = gtk_builder_get_object (main_builder, "display-dialog");
    gtk_widget_set_sensitive (GTK_WIDGET (main_dialog), FALSE);

    builder = gtk_builder_new ();

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
        source_id = g_timeout_add_seconds (1, (GSourceFunc) display_settings_update_time_label,
                                           confirmation_dialog);

        response_id = gtk_dialog_run (GTK_DIALOG (dialog));
        g_source_remove (source_id);
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
    else
    {
        response_id = 2;
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    g_object_unref (G_OBJECT (builder));

    /* Unlock the main UI */
    gtk_widget_set_sensitive (GTK_WIDGET (main_dialog), TRUE);

    return ((response_id == 2) ? TRUE : FALSE);
}



static void
display_setting_reflections_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    gint value;
    Rotation old_rotation;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    old_rotation = XFCE_RANDR_ROTATION (xfce_randr);

    /* Remove existing reflection */
    XFCE_RANDR_ROTATION (xfce_randr) &= ~XFCE_RANDR_REFLECTIONS_MASK;

    /* Set the new one */
    XFCE_RANDR_ROTATION (xfce_randr) |= value;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            xfce_randr->active_output);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        XFCE_RANDR_ROTATION (xfce_randr) = old_rotation;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                xfce_randr->active_output);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}


static void
display_setting_reflections_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox;
    Rotation      reflections;
    Rotation      active_reflection;
    guint         n;
    GtkTreeIter   iter;

    if (!xfce_randr)
        return;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-reflection");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* disable it if no mode is selected */
    if (XFCE_RANDR_MODE (xfce_randr) == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Disconnect the "changed" signal to avoid triggering the confirmation
     * dialog */
    g_object_disconnect (combobox, "any_signal::changed",
                         display_setting_reflections_changed,
                         builder, NULL);

    /* Load only supported reflections */
    reflections = XFCE_RANDR_ROTATIONS (xfce_randr) & XFCE_RANDR_REFLECTIONS_MASK;
    active_reflection = XFCE_RANDR_ROTATION (xfce_randr) & XFCE_RANDR_REFLECTIONS_MASK;

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
            if (xfce_randr && XFCE_RANDR_MODE (xfce_randr) != None)
            {
                if ((reflection_names[n].rotation & active_reflection) == reflection_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }

    /* Reconnect the signal */
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_reflections_changed), builder);
}



static void
display_setting_rotations_changed (GtkComboBox *combobox,
                                   GtkBuilder  *builder)
{
    Rotation old_rotation;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* Set new rotation */
    old_rotation = XFCE_RANDR_ROTATION (xfce_randr);
    XFCE_RANDR_ROTATION (xfce_randr) &= ~XFCE_RANDR_ROTATIONS_MASK;
    XFCE_RANDR_ROTATION (xfce_randr) |= value;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            xfce_randr->active_output);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        XFCE_RANDR_ROTATION (xfce_randr) = old_rotation;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                xfce_randr->active_output);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}



static void
display_setting_rotations_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox;
    Rotation      rotations;
    Rotation      active_rotation;
    guint         n;
    GtkTreeIter   iter;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-rotation");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* Disable it if no mode is selected */
    if (XFCE_RANDR_MODE (xfce_randr) == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Disconnect the "changed" signal to avoid triggering the confirmation
     * dialog */
    g_object_disconnect (combobox, "any_signal::changed",
                         display_setting_rotations_changed,
                         builder, NULL);

    /* Load only supported rotations */
    rotations = XFCE_RANDR_ROTATIONS (xfce_randr) & XFCE_RANDR_ROTATIONS_MASK;
    active_rotation = XFCE_RANDR_ROTATION (xfce_randr) & XFCE_RANDR_ROTATIONS_MASK;

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
            if (xfce_randr && XFCE_RANDR_MODE (xfce_randr) != None)
            {
                if ((rotation_names[n].rotation & active_rotation) == rotation_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }

    /* Reconnect the signal */
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), builder);
}



static void
display_setting_refresh_rates_changed (GtkComboBox *combobox,
                                       GtkBuilder  *builder)
{
    RRMode old_mode;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* Set new mode */
    old_mode = XFCE_RANDR_MODE (xfce_randr);
    XFCE_RANDR_MODE (xfce_randr) = value;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            xfce_randr->active_output);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        XFCE_RANDR_MODE (xfce_randr) = old_mode;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                xfce_randr->active_output);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}



static void
display_setting_refresh_rates_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox;
    GtkTreeIter   iter;
    gchar        *name = NULL;
    gint          n;
    GObject      *res_combobox;
    XfceRRMode   *modes, *current_mode;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* Disable it if no mode is selected */
    if (XFCE_RANDR_MODE (xfce_randr) == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Disconnect the "changed" signal to avoid triggering the confirmation
     * dialog */
    g_object_disconnect (combobox, "any_signal::changed",
                         display_setting_refresh_rates_changed,
                         builder, NULL);

    /* Fetch the selected resolution */
    res_combobox = gtk_builder_get_object (builder, "randr-resolution");
    if (!display_setting_combo_box_get_value (GTK_COMBO_BOX (res_combobox), &n))
        return;

    current_mode = xfce_randr_find_mode_by_id (xfce_randr, xfce_randr->active_output, n);
    if (!current_mode)
        return;

    /* Walk all supported modes */
    modes = XFCE_RANDR_SUPPORTED_MODES (xfce_randr);
    for (n = 0; n < XFCE_RANDR_OUTPUT_INFO (xfce_randr)->nmode; ++n)
    {
        /* The mode resolution does not match the selected one */
        if (modes[n].width != current_mode->width
            || modes[n].height != current_mode->height)
            continue;

        /* Insert the mode */
        name = g_strdup_printf (_("%.1f Hz"), modes[n].rate);
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COLUMN_COMBO_NAME, name,
                            COLUMN_COMBO_VALUE, modes[n].id, -1);
        g_free (name);

        /* Select the active mode */
        if (modes[n].id == XFCE_RANDR_MODE (xfce_randr))
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* If a new resolution was selected, set a refresh rate */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

    /* Reconnect the signal */
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), builder);
}



static void
display_setting_resolutions_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    RRMode old_mode;
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* Set new resolution */
    old_mode = XFCE_RANDR_MODE (xfce_randr);
    XFCE_RANDR_MODE (xfce_randr) = value;

    /* Update refresh rates */
    display_setting_refresh_rates_populate (builder);

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            xfce_randr->active_output);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        XFCE_RANDR_MODE (xfce_randr) = old_mode;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                xfce_randr->active_output);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}



static void
display_setting_resolutions_populate (GtkBuilder *builder)
{
    GtkTreeModel  *model;
    GObject       *combobox;
    gint           n;
    gchar         *name;
    GtkTreeIter    iter;
    XfceRRMode   *modes;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* Disable it if no mode is selected */
    if (XFCE_RANDR_MODE (xfce_randr) == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        display_setting_refresh_rates_populate (builder);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Disconnect the "changed" signal to avoid triggering the confirmation
     * dialog */
    g_object_disconnect (combobox, "any_signal::changed",
                         display_setting_resolutions_changed,
                         builder, NULL);

    /* Walk all supported modes */
    modes = XFCE_RANDR_SUPPORTED_MODES (xfce_randr);
    for (n = 0; n < XFCE_RANDR_OUTPUT_INFO (xfce_randr)->nmode; ++n)
    {
        /* Try to avoid duplicates */
        if (n == 0 || (n > 0 && (modes[n].width != modes[n - 1].width
            || modes[n].height != modes[n - 1].height)))
        {

            /* Insert the mode */
            name = g_strdup_printf ("%dx%d", modes[n].width, modes[n].height);
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, name,
                                COLUMN_COMBO_VALUE, modes[n].id, -1);
            g_free (name);
        }

        /* Select the active mode */
        if (modes[n].id == XFCE_RANDR_MODE (xfce_randr))
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* Reconnect the signal */
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), builder);
}



static void
display_setting_output_toggled (GtkToggleButton *togglebutton,
                                GtkBuilder      *builder)
{
    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    if (gtk_toggle_button_get_active (togglebutton))
    {
        XFCE_RANDR_MODE (xfce_randr) =
            xfce_randr_preferred_mode (xfce_randr, xfce_randr->active_output);
        /* Apply the changes */
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                xfce_randr->active_output);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
    else
    {
        /* Prevents the user from disabling everything… */
        if (display_settings_get_n_active_outputs () > 1)
        {
            XFCE_RANDR_MODE (xfce_randr) = None;
            /* Apply the changes */
            xfce_randr_apply (xfce_randr, "Default", display_channel);
        }
        else
        {
            xfce_dialog_show_warning (NULL,
                                      _("The last active output must not be disabled, the system would"
                                        " be unusable."),
                                      _("Selected output not disabled"));
            /* Set it back to active */
            gtk_toggle_button_set_active (togglebutton, TRUE);
        }
    }
}


static void
display_setting_output_status_populate (GtkBuilder *builder)
{
    GObject *check;
    gchar    property[512];

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    check = gtk_builder_get_object (builder, "output-on");
    /* Unbind any existing property, and rebind it */
    if (bound_to_channel)
    {
        xfconf_g_property_unbind_all (check);
        bound_to_channel = FALSE;
    }

    /* Disconnect the "toggled" signal to avoid writing the config again */
    g_object_disconnect (check, "any_signal::toggled",
                         display_setting_output_toggled,
                         builder, NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  XFCE_RANDR_MODE (xfce_randr) != None);
    /* Reconnect the signal */
    g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (display_setting_output_toggled),
                      builder);

    g_snprintf (property, sizeof (property), "/Default/%s/Active",
                xfce_randr->output_info[xfce_randr->active_output]->name);
    xfconf_g_property_bind (display_channel, property, G_TYPE_BOOLEAN, check,
                            "active");
    bound_to_channel = TRUE;
}



static void
display_settings_treeview_selection_changed (GtkTreeSelection *selection,
                                             GtkBuilder       *builder)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gint          active_id;

    /* Get the selection */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* Get the output info */
        gtk_tree_model_get (model, &iter, COLUMN_OUTPUT_ID, &active_id, -1);

        /* Get the new active screen or output */
        xfce_randr->active_output = active_id;

        /* Update the combo boxes */
        display_setting_output_status_populate (builder);
        display_setting_resolutions_populate (builder);
        display_setting_refresh_rates_populate (builder);
        display_setting_rotations_populate (builder);
        display_setting_reflections_populate (builder);
    }
}



static void
display_settings_treeview_populate (GtkBuilder *builder)
{
    guint             m;
    GtkListStore     *store;
    GObject          *treeview;
    GtkTreeIter       iter;
    gchar            *name;
    GdkPixbuf        *display_icon, *lucent_display_icon;
    GtkTreeSelection *selection;

    /* Create a new list store */
    store = gtk_list_store_new (N_OUTPUT_COLUMNS,
                                G_TYPE_STRING, /* COLUMN_OUTPUT_NAME */
                                GDK_TYPE_PIXBUF, /* COLUMN_OUTPUT_ICON */
                                G_TYPE_INT);   /* COLUMN_OUTPUT_ID */

    /* Set the treeview model */
    treeview = gtk_builder_get_object (builder, "randr-outputs");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    /* Get the display icon */
    display_icon =
        gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), "display-icon",
                                  32,
                                  GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                  NULL);

    lucent_display_icon = NULL;

    /* Save the current status of all outputs, if the user doesn't change
     * anything after, it means she's happy with that. */
    xfce_randr_save_all (xfce_randr, "Default", display_channel);

    /* Walk all the connected outputs */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        /* Get a friendly name for the output */
        name = xfce_randr_friendly_name (xfce_randr,
                                         xfce_randr->resources->outputs[m],
                                         xfce_randr->output_info[m]->name);

        if (xfce_randr->mode[m] == None && lucent_display_icon == NULL)
            lucent_display_icon =
                exo_gdk_pixbuf_lucent (display_icon, 50);

        /* Insert the output in the store */
        gtk_list_store_append (store, &iter);
        if (xfce_randr->mode[m] == None)
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, name,
                                COLUMN_OUTPUT_ICON, lucent_display_icon,
                                COLUMN_OUTPUT_ID, m, -1);
        else
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, name,
                                COLUMN_OUTPUT_ICON, display_icon,
                                COLUMN_OUTPUT_ID, m, -1);

        g_free (name);

        /* Select active output */
        if (m == xfce_randr->active_output)
            gtk_tree_selection_select_iter (selection, &iter);
    }

    /* Release the store */
    g_object_unref (G_OBJECT (store));

    /* Release the icons */
    g_object_unref (display_icon);
    if (lucent_display_icon != NULL)
        g_object_unref (lucent_display_icon);
}



static void
display_settings_combo_box_create (GtkComboBox *combobox)
{
    GtkCellRenderer *renderer;
    GtkListStore    *store;

    /* Create and set the combobox model */
    store = gtk_list_store_new (N_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));

    /* Setup renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", COLUMN_COMBO_NAME);
}



static void
display_settings_dialog_response (GtkDialog  *dialog,
                                  gint        response_id,
                                  GtkBuilder *builder)
{
    gtk_main_quit ();
}



static GtkWidget *
display_settings_dialog_new (GtkBuilder *builder)
{
    GObject          *treeview;
    GtkCellRenderer  *renderer;
    GtkTreeSelection *selection;
    GObject          *combobox;
    GObject          *label, *check;

    /* Get the treeview */
    treeview = gtk_builder_get_object (builder, "randr-outputs");
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_OUTPUT_NAME);

    /* Icon renderer */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 0, "", renderer, "pixbuf", COLUMN_OUTPUT_ICON, NULL);
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DND, NULL);

    /* Text renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 1, "", renderer, "text", COLUMN_OUTPUT_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Treeview selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (display_settings_treeview_selection_changed), builder);

    /* Setup the combo boxes */
    check = gtk_builder_get_object (builder, "output-on");
    if (xfce_randr->noutput > 1)
    {
        gtk_widget_show (GTK_WIDGET (check));
        g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (display_setting_output_toggled), builder);
    }
    else
        gtk_widget_hide (GTK_WIDGET (check));

    label = gtk_builder_get_object (builder, "label-reflection");
    gtk_widget_show (GTK_WIDGET (label));

    combobox = gtk_builder_get_object (builder, "randr-reflection");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    gtk_widget_show (GTK_WIDGET (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_reflections_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-resolution");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-rotation");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), builder);

    /* Populate the treeview */
    display_settings_treeview_populate (builder);

    return GTK_WIDGET (gtk_builder_get_object (builder, "display-dialog"));
}



static GdkFilterReturn
screen_on_event (GdkXEvent *xevent,
                 GdkEvent  *event,
                 gpointer   data)
{
    GtkBuilder *builder = data;
    XEvent     *e = xevent;
    gint        event_num;

    if (!e)
        return GDK_FILTER_CONTINUE;

    event_num = e->type - XFCE_RANDR_EVENT_BASE (xfce_randr);

    if (event_num == RRScreenChangeNotify)
    {
        xfce_randr_reload (xfce_randr);
        display_settings_treeview_populate (builder);
    }

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}



static void
display_settings_minimal_dialog_response (GtkDialog  *dialog,
                                          gint        response_id,
                                          GtkBuilder *builder)
{
    GObject    *first_screen_radio;
    GObject    *second_screen_radio;
    GObject    *both_radio;
    XfceRRMode *mode1, *mode2;
    gboolean    use_first_screen;
    gboolean    use_second_screen;
    gboolean    use_both;
    guint       first, second;
    gint        m, n, found;

    if (response_id == 1)
    {
        /* TODO: handle correctly more than 2 outputs? */
        first = 0;
        second = 1;

        first_screen_radio = gtk_builder_get_object (builder, "radiobutton1");
        second_screen_radio = gtk_builder_get_object (builder, "radiobutton2");
        both_radio = gtk_builder_get_object (builder, "radiobutton3");

        use_first_screen =
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (first_screen_radio));
        use_second_screen =
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (second_screen_radio));
        use_both =
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (both_radio));

        if (use_first_screen)
        {
            xfce_randr->mode[first] = xfce_randr_preferred_mode (xfce_randr, first);
            xfce_randr->mode[second] = None;
        }
        else if (use_second_screen)
        {
            xfce_randr->mode[second] = xfce_randr_preferred_mode (xfce_randr, second);
            xfce_randr->mode[first] = None;
        }
        else
        {
            if (xfce_randr->clone_modes[0] != None)
            {
                xfce_randr->mode[first] = xfce_randr->clone_modes[0];
                xfce_randr->mode[second] = xfce_randr->clone_modes[0];
            }
            else
            {
                found = FALSE;
                /* No clone mode available, try to find a "similar" mode */
                for (n = 0; n < xfce_randr->output_info[first]->nmode; ++n)
                {
                    mode1 = &xfce_randr->modes[first][n];
                    for (m = 0; m < xfce_randr->output_info[second]->nmode; ++m)
                    {
                        mode2 = &xfce_randr->modes[second][m];
                        /* "similar" means same resolution */
                        if (mode1->width == mode2->width
                            && mode1->height == mode2->height)
                        {
                            xfce_randr->mode[first] = mode1->id;
                            xfce_randr->mode[second] = mode2->id;
                            found = TRUE;
                            break;
                        }
                    }

                    if (found)
                        break;
                }
            }
        }
        /* Save the two outputs and apply */
        xfce_randr_save_output (xfce_randr, "MinimalAutoConfig", display_channel,
                                first);
        xfce_randr_save_output (xfce_randr, "MinimalAutoConfig", display_channel,
                                second);
        xfce_randr_apply (xfce_randr, "MinimalAutoConfig", display_channel);
    }

    gtk_main_quit ();
}



gint
main (gint argc, gchar **argv)
{
    GtkWidget   *dialog;
    GtkBuilder  *builder;
    GError      *error = NULL;
    GdkDisplay  *display;
    gboolean     succeeded = TRUE;
    gint         event_base, error_base;
    guint        first, second;
    gchar       *command;
    const gchar *alternative = NULL;
    const gchar *alternative_icon = NULL;
    gint         response;

    /* Setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* Initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
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
        g_print ("%s\n", "Copyright (c) 2004-2010");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* Get the default display */
    display = gdk_display_get_default ();

    /* Check if the randr extension is avaible on the system */
    if (!XRRQueryExtension (gdk_x11_display_get_xdisplay (display), &event_base, &error_base))
    {
        g_set_error (&error, 0, 0, _("Unable to query the version of the RandR extension being used"));
        xfce_dialog_show_error (NULL, error, _("Unable to start the Xfce Display Settings"));
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* Initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* Print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* Open the xsettings channel */
    display_channel = xfconf_channel_new ("displays");
    if (G_LIKELY (display_channel))
    {
        /* Create a new xfce randr (>= 1.2) for this display
         * this will only work if there is 1 screen on this display */
        if (gdk_display_get_n_screens (display) == 1)
            xfce_randr = xfce_randr_new (display, &error);

        if (!xfce_randr)
        {
            command = g_find_program_in_path ("nvidia-settings");
            if (command != NULL)
            {
                alternative = _("NVIDIA Settings");
                alternative_icon = "nvidia-settings";
            }
            else
            {
                command = g_find_program_in_path ("amdcccle");
                if (command != NULL)
                {
                    alternative = _("ATI Settings");
                    alternative_icon = "ccc_small";
                }
            }

            response = xfce_message_dialog (NULL, NULL, GTK_STOCK_DIALOG_ERROR,
                                            _("Unable to start the Xfce Display Settings"),
                                            error ? error->message : NULL,
                                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                            alternative != NULL ?XFCE_BUTTON_TYPE_MIXED : NULL,
                                            alternative_icon, alternative, GTK_RESPONSE_OK, NULL);
            g_clear_error (&error);

            if (response == GTK_RESPONSE_OK
                && !g_spawn_command_line_async (command, &error))
            {
                xfce_dialog_show_error (NULL, error, _("Unable to launch the proprietary driver settings"));
                g_error_free (error);
            }

            g_free (command);

            goto cleanup;
        }

        /* Hook to make sure the libxfce4ui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
            return EXIT_FAILURE;

        if (!minimal)
        {
            /* Load the Gtk user-interface file */
            builder = gtk_builder_new ();
            if (gtk_builder_add_from_string (builder, display_dialog_ui,
                                             display_dialog_ui_length, &error) != 0)
            {
                /* Build the dialog */
                dialog = display_settings_dialog_new (builder);
                g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (display_settings_dialog_response), builder);

                XFCE_RANDR_EVENT_BASE (xfce_randr) = event_base;
                /* Set up notifications */
                XRRSelectInput (gdk_x11_display_get_xdisplay (display),
                                GDK_WINDOW_XID (gdk_get_default_root_window ()),
                                RRScreenChangeNotifyMask);
                gdk_x11_register_standard_event_type (display,
                                                      event_base,
                                                      RRNotify + 1);
                gdk_window_add_filter (gdk_get_default_root_window (), screen_on_event, builder);

                /* Show the dialog */
                gtk_widget_show (dialog);

                /* To prevent the settings dialog to be saved in the session */
                gdk_set_sm_client_id ("FAKE ID");

                /* Enter the main loop */
                gtk_main ();
            }
            else
            {
                g_error ("Failed to load the UI file: %s.", error->message);
                g_error_free (error);
            }

            gdk_window_remove_filter (gdk_get_default_root_window (), screen_on_event, builder);

            /* Release the builder */
            g_object_unref (G_OBJECT (builder));
        }
        else
        {
            if (xfce_randr->noutput < 2)
                goto cleanup;

            /* TODO: handle correctly more than 2 outputs? */
            first = 0;
            second = 1;

            builder = gtk_builder_new ();

            if (gtk_builder_add_from_string (builder, minimal_display_dialog_ui,
                                             minimal_display_dialog_ui_length, &error) != 0)
            {
                GObject    *first_screen_radio;
                GObject    *second_screen_radio;
                gchar      *screen_name;

                /* Build the minimal dialog */
                dialog = (GtkWidget *) gtk_builder_get_object (builder, "dialog1");
                g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (display_settings_minimal_dialog_response), builder);

                /* Set the radio buttons captions */
                first_screen_radio =
                    gtk_builder_get_object (builder, "radiobutton1");
                second_screen_radio =
                    gtk_builder_get_object (builder, "radiobutton2");

                screen_name =
                    xfce_randr_friendly_name (xfce_randr,
                                              xfce_randr->resources->outputs[first],
                                              xfce_randr->output_info[first]->name);
                gtk_button_set_label (GTK_BUTTON (first_screen_radio),
                                      screen_name);
                g_free (screen_name);

                screen_name =
                    xfce_randr_friendly_name (xfce_randr,
                                              xfce_randr->resources->outputs[second],
                                              xfce_randr->output_info[second]->name);
                gtk_button_set_label (GTK_BUTTON (second_screen_radio),
                                      screen_name);
                g_free (screen_name);

                /* Show the minimal dialog and start the main loop */
                gtk_widget_show (dialog);
                gtk_main ();
            }
            else
            {
                g_error ("Failed to load the UI file: %s.", error->message);
                g_error_free (error);
            }

            g_object_unref (G_OBJECT (builder));
        }

        cleanup:

        /* Release the channel */
        if (bound_to_channel)
            xfconf_g_property_unbind_all (G_OBJECT (display_channel));
        g_object_unref (G_OBJECT (display_channel));
    }

    /* Free the randr 1.2 backend */
    if (xfce_randr)
        xfce_randr_free (xfce_randr);

    /* Shutdown xfconf */
    xfconf_shutdown ();

    return (succeeded ? EXIT_SUCCESS : EXIT_FAILURE);
}
