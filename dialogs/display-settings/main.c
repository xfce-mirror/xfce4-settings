/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010 Lionel Le Folgoc <lionel@lefolgoc.net>
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
#include "identity-popup_ui.h"

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



typedef struct _XfceRelation XfceRelation;
typedef struct _XfceRotation XfceRotation;



struct _XfceRelation
{
    XfceOutputRelation  relation;
    const gchar        *name;
};



struct _XfceRotation
{
    Rotation     rotation;
    const gchar *name;
};



/* Xrandr relation name conversion */
static const XfceRelation relation_names[] =
{
    { XFCE_RANDR_PLACEMENT_MIRROR, N_("Same as") },
    { XFCE_RANDR_PLACEMENT_UP,     N_("Above") },
    { XFCE_RANDR_PLACEMENT_DOWN,   N_("Below") },
    { XFCE_RANDR_PLACEMENT_RIGHT,  N_("Right of") },
    { XFCE_RANDR_PLACEMENT_LEFT,   N_("Left of") }
};



/* Xrandr rotation name conversion */
static const XfceRotation rotation_names[] =
{
    { RR_Rotate_0,   N_("None") },
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
static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static gboolean minimal = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "minimal", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &minimal, N_("Minimal interface to set up an external output"), NULL},
    { NULL }
};

/* Global xfconf channel */
static XfconfChannel *display_channel;

/* output currently selected in the iconview */
static guint active_output;

/* Pointer to the used randr structure */
static XfceRandr *xfce_randr = NULL;

/* event base for XRandR notifications */
static gint randr_event_base;

/* Used to identify the display */
static GHashTable *display_popups;

gboolean supports_alpha = FALSE;



static void display_settings_minimal_only_display1_toggled   (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_mirror_displays_toggled (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_extend_right_toggled    (GtkToggleButton *button,
                                                              GtkBuilder      *builder);

static void display_settings_minimal_only_display2_toggled   (GtkToggleButton *button,
                                                              GtkBuilder      *builder);


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

static void
display_setting_toggle_identity_popup (gpointer   key,
                                       GtkWidget *value,
                                       gpointer   show)
{
    if (!GPOINTER_TO_INT (show))
        gtk_widget_hide (value);
    else
        gtk_widget_show (value);
}

static gboolean
display_setting_toggle_identity_popups (GtkWidget *widget,
                                        GdkEvent  *event,
                                        gpointer   show)
{
    g_hash_table_foreach (display_popups,
                          (GHFunc) display_setting_toggle_identity_popup,
                          show);
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
        gchar   *label_string;

        label_string = g_strdup_printf (_("The previous configuration will be restored in %i"
                                          " seconds if you do not reply to this question."),
                                        confirmation_dialog->count);

        label = gtk_builder_get_object (confirmation_dialog->builder, "label2");
        gtk_label_set_text (GTK_LABEL (label), label_string);
        g_free (label_string);

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

        g_signal_connect (G_OBJECT (dialog), "focus-out-event", G_CALLBACK (display_setting_toggle_identity_popups),
                          GINT_TO_POINTER (FALSE));

        g_signal_connect (G_OBJECT (dialog), "focus-in-event", G_CALLBACK (display_setting_toggle_identity_popups),
                          GINT_TO_POINTER (TRUE));

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
display_setting_positions_changed (GtkComboBox *combobox,
                                   GtkBuilder  *builder)
{
    GObject            *display_combobox;
    XfceOutputRelation  previous_relation;
    gint                value, selected_display, previous_related_to;

    display_combobox = gtk_builder_get_object (builder, "randr-active-displays");

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    if (!display_setting_combo_box_get_value (GTK_COMBO_BOX (display_combobox),
                                              &selected_display))
        return;

    /* Skip if the display combobox hasn't made a selection yet */
    if (selected_display == -1) return;

    /* back up */
    previous_relation = xfce_randr->relation[active_output];
    previous_related_to = xfce_randr->related_to[active_output];

    /* Save changes to currently active display */
    xfce_randr->relation[active_output] = value;
    xfce_randr->related_to[active_output] = selected_display;
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            active_output, TRUE);

    /* Save changes to selected display */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            selected_display, FALSE);

    /* Apply all changes */
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        /* Restore the currently active display */
        xfce_randr->relation[active_output] = previous_relation;
        xfce_randr->related_to[active_output] = previous_related_to;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output, TRUE);
        /* Restore the selected display */
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                selected_display, FALSE);

        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}



static void
display_setting_positions_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox, *label, *mirror_displays;
    GtkTreeIter   iter;
    guint         n;

    /* Get the positions combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-position");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    label = gtk_builder_get_object (builder, "label-position");
    if (xfce_randr->noutput > 1)
    {
        gtk_widget_show (GTK_WIDGET (label));
        gtk_widget_show (GTK_WIDGET (combobox));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (label));
        gtk_widget_hide (GTK_WIDGET (combobox));
        return;
    }

    /* Only make the combobox interactive if there is more than one output,
       and if they are not in mirror mode */
    mirror_displays = gtk_builder_get_object (builder, "mirror-displays");
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mirror_displays)) ||
        display_settings_get_n_active_outputs () <= 1)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_positions_changed,
                                     builder);

    /* Try to insert the relations */
    for (n = 0; n < G_N_ELEMENTS (relation_names); n++)
    {
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COLUMN_COMBO_NAME, _(relation_names[n].name),
                            COLUMN_COMBO_VALUE, relation_names[n].relation, -1);

        /* Select the active relation */
        if (relation_names[n].relation == xfce_randr->relation[active_output])
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_positions_changed,
                                       builder);
}

static void
display_setting_active_displays_changed (GtkComboBox *combobox,
                                         GtkBuilder  *builder)
{
    GObject *position_combobox;
    gint     value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    position_combobox = gtk_builder_get_object (builder, "randr-position");

    display_setting_positions_changed (GTK_COMBO_BOX (position_combobox), builder);
}

static void
display_setting_active_displays_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox, *mirror_displays;
    guint         n;
    GtkTreeIter   iter;

    /* Get the active-displays combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-active-displays");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    if (xfce_randr->noutput > 1)
        gtk_widget_show (GTK_WIDGET (combobox));
    else
    {
        gtk_widget_hide (GTK_WIDGET (combobox));
        return;
    }

    /* Only make the combobox interactive if there is more than one output,
       and if they are not in mirror mode */
    mirror_displays = gtk_builder_get_object (builder, "mirror-displays");
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (mirror_displays)) ||
        display_settings_get_n_active_outputs () <= 1)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_active_displays_changed,
                                     builder);

    /* Insert all active displays */
    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] == None || n == active_output)
            continue;

        /* Insert display name */
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            COLUMN_COMBO_NAME, xfce_randr->friendly_name[n],
                            COLUMN_COMBO_VALUE, n, -1);

        /* Select the active related output */
        if (n == xfce_randr->related_to[active_output])
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_active_displays_changed,
                                       builder);
}



static void
display_setting_reflections_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    gint value;
    Rotation old_rotation;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    old_rotation = xfce_randr->rotation[active_output];

    /* Remove existing reflection */
    xfce_randr->rotation[active_output] &= ~XFCE_RANDR_REFLECTIONS_MASK;

    /* Set the new one */
    xfce_randr->rotation[active_output] |= value;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            active_output, FALSE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        xfce_randr->rotation[active_output] = old_rotation;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output, FALSE);
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
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_reflections_changed,
                                     builder);

    /* Load only supported reflections */
    reflections = xfce_randr->rotations[active_output] & XFCE_RANDR_REFLECTIONS_MASK;
    active_reflection = xfce_randr->rotation[active_output] & XFCE_RANDR_REFLECTIONS_MASK;

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
            if (xfce_randr && xfce_randr->mode[active_output] != None)
            {
                if ((reflection_names[n].rotation & active_reflection) == reflection_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_reflections_changed,
                                       builder);
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
    old_rotation = xfce_randr->rotation[active_output];
    xfce_randr->rotation[active_output] &= ~XFCE_RANDR_ROTATIONS_MASK;
    xfce_randr->rotation[active_output] |= value;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            active_output, TRUE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        xfce_randr->rotation[active_output] = old_rotation;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output, TRUE);
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
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_rotations_changed,
                                     builder);

    /* Load only supported rotations */
    rotations = xfce_randr->rotations[active_output] & XFCE_RANDR_ROTATIONS_MASK;
    active_rotation = xfce_randr->rotation[active_output] & XFCE_RANDR_ROTATIONS_MASK;

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
            if (xfce_randr && (xfce_randr->mode[active_output] != None))
            {
                if ((rotation_names[n].rotation & active_rotation) == rotation_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_rotations_changed,
                                       builder);
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
    old_mode = xfce_randr->mode[active_output];
    xfce_randr->mode[active_output] = value;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            active_output, FALSE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        xfce_randr->mode[active_output] = old_mode;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output, FALSE);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}



static void
display_setting_refresh_rates_populate (GtkBuilder *builder)
{
    GtkTreeModel     *model;
    GObject          *combobox;
    GtkTreeIter       iter;
    gchar            *name = NULL;
    gint              nmode, n;
    GObject          *res_combobox;
    const XfceRRMode *modes, *current_mode;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* Disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_refresh_rates_changed,
                                     builder);

    /* Fetch the selected resolution */
    res_combobox = gtk_builder_get_object (builder, "randr-resolution");
    if (!display_setting_combo_box_get_value (GTK_COMBO_BOX (res_combobox), &n))
        return;

    current_mode = xfce_randr_find_mode_by_id (xfce_randr, active_output, n);
    if (!current_mode)
        return;

    /* Walk all supported modes */
    modes = xfce_randr_get_modes (xfce_randr, active_output, &nmode);
    for (n = 0; n < nmode; ++n)
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
        if (modes[n].id == xfce_randr->mode[active_output])
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* If a new resolution was selected, set a refresh rate */
    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox)) == -1)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_refresh_rates_changed,
                                       builder);
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
    old_mode = xfce_randr->mode[active_output];
    xfce_randr->mode[active_output] = value;

    /* Update refresh rates */
    display_setting_refresh_rates_populate (builder);

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            active_output, TRUE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        xfce_randr->mode[active_output] = old_mode;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output, TRUE);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}



static void
display_setting_resolutions_populate (GtkBuilder *builder)
{
    GtkTreeModel     *model;
    GObject          *combobox;
    gint              nmode, n;
    gchar            *name;
    GtkTreeIter       iter;
    const XfceRRMode *modes;

    /* Get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* Disable it if no mode is selected */
    if (xfce_randr->mode[active_output] == None)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), FALSE);
        display_setting_refresh_rates_populate (builder);
        return;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (combobox), TRUE);

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (combobox, display_setting_resolutions_changed,
                                     builder);

    /* Walk all supported modes */
    modes = xfce_randr_get_modes (xfce_randr, active_output, &nmode);
    for (n = 0; n < nmode; ++n)
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
        if (modes[n].id == xfce_randr->mode[active_output])
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (combobox, display_setting_resolutions_changed,
                                       builder);
}

static void
display_setting_screen_changed (GtkWidget *widget,
                                GdkScreen *old_screen,
                                gpointer   userdata)
{
    GdkScreen   *screen = gtk_widget_get_screen (widget);
    GdkColormap *colormap = gdk_screen_get_rgba_colormap (screen);

    if (gdk_screen_is_composited (screen))
        supports_alpha = TRUE;
    else
    {
        colormap = gdk_screen_get_rgb_colormap (screen);
        supports_alpha = FALSE;
    }

    gtk_widget_set_colormap (widget, colormap);
}

static gboolean
display_setting_identity_popup_expose (GtkWidget      *popup,
                                       GdkEventExpose *event,
                                       GtkBuilder     *builder)
{
    cairo_t         *cr = gdk_cairo_create (popup->window);
    cairo_pattern_t *vertical_gradient, *innerstroke_gradient, *selected_gradient, *selected_innerstroke_gradient;
    gint             radius;
    gboolean         selected = (g_hash_table_lookup (display_popups, GINT_TO_POINTER (active_output)) == popup);

    radius = 10;
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    /* Create the various gradients */
    vertical_gradient = cairo_pattern_create_linear (0, 0, 0, popup->allocation.height);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0, 0.25, 0.25, 0.25);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0.24, 0.15, 0.15, 0.15);
    cairo_pattern_add_color_stop_rgb (vertical_gradient, 0.6, 0.0, 0.0, 0.0);

    innerstroke_gradient = cairo_pattern_create_linear (0, 0, 0, popup->allocation.height);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0, 0.35, 0.35, 0.35);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.4, 0.25, 0.25, 0.25);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.7, 0.15, 0.15, 0.15);
    cairo_pattern_add_color_stop_rgb (innerstroke_gradient, 0.85, 0.0, 0.0, 0.0);

    selected_gradient = cairo_pattern_create_linear (0, 0, 0, popup->allocation.height);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0, 0.05, 0.20, 0.46);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.4, 0.05, 0.12, 0.25);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.6, 0.05, 0.10, 0.20);
    cairo_pattern_add_color_stop_rgb (selected_gradient, 0.8, 0.0, 0.02, 0.05);

    selected_innerstroke_gradient = cairo_pattern_create_linear (0, 0, 0, popup->allocation.height);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0, 0.15, 0.45, 0.75);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0.7, 0.0, 0.15, 0.25);
    cairo_pattern_add_color_stop_rgb (selected_innerstroke_gradient, 0.85, 0.0, 0.0, 0.0);

    /* Compositing is not available, so just set the background color. */
    if (!supports_alpha)
    {
        /* Draw a filled rectangle with outline */
        cairo_set_line_width (cr, 1.0);
        cairo_set_source (cr, vertical_gradient);
        if (selected)
            cairo_set_source (cr, selected_gradient);
        cairo_paint (cr);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_rectangle (cr, 0.5, 0.5, popup->allocation.width-0.5, popup->allocation.height-0.5);
        cairo_stroke (cr);

        /* Draw the inner stroke */
        cairo_set_source_rgb (cr, 0.35, 0.35, 0.35);
        if (selected)
            cairo_set_source_rgb (cr, 0.15, 0.45, 0.75);
        cairo_move_to (cr, 1.5, 1.5);
        cairo_line_to (cr, popup->allocation.width-1, 1.5);
        cairo_stroke (cr);
        cairo_set_source (cr, innerstroke_gradient);
        if (selected)
            cairo_set_source (cr, selected_innerstroke_gradient);
        cairo_move_to (cr, 1.5, 1.5);
        cairo_line_to (cr, 1.5, popup->allocation.height-1.0);
        cairo_move_to (cr, popup->allocation.width-1.5, 1.5);
        cairo_line_to (cr, popup->allocation.width-1.5, popup->allocation.height-1.0);
        cairo_stroke (cr);
    }
    /* Draw rounded corners. */
    else
    {
        cairo_set_source_rgba (cr, 0, 0, 0, 0);
        cairo_paint (cr);

        /* Draw a filled rounded rectangle with outline */
        cairo_set_line_width (cr, 1.0);
        cairo_move_to (cr, 0.5, popup->allocation.height+0.5);
        cairo_line_to (cr, 0.5, radius+0.5);
        cairo_arc (cr, radius+0.5, radius+0.5, radius, 3.14, 3.0*3.14/2.0);
        cairo_line_to (cr, popup->allocation.width-0.5 - radius, 0.5);
        cairo_arc (cr, popup->allocation.width-0.5 - radius, radius+0.5, radius, 3.0*3.14/2.0, 0.0);
        cairo_line_to (cr, popup->allocation.width-0.5, popup->allocation.height+0.5);
        cairo_set_source (cr, vertical_gradient);
        if (selected)
            cairo_set_source (cr, selected_gradient);
        cairo_fill_preserve (cr);
        cairo_set_source_rgb (cr, 0.05, 0.05, 0.05);
        cairo_stroke (cr);

        /* Draw the inner stroke */
        cairo_set_source_rgb (cr, 0.35, 0.35, 0.35);
        if (selected)
            cairo_set_source_rgb (cr, 0.15, 0.45, 0.75);
        cairo_arc (cr, radius+1.5, radius+1.5, radius, 3.14, 3.0*3.14/2.0);
        cairo_line_to (cr, popup->allocation.width-1.5 - radius, 1.5);
        cairo_arc (cr, popup->allocation.width-1.5 - radius, radius+1.5, radius, 3.0*3.14/2.0, 0.0);
        cairo_stroke (cr);
        cairo_set_source (cr, innerstroke_gradient);
        if (selected)
            cairo_set_source (cr, selected_innerstroke_gradient);
        cairo_move_to (cr, 1.5, radius+1.0);
        cairo_line_to (cr, 1.5, popup->allocation.height-1.0);
        cairo_move_to (cr, popup->allocation.width-1.5, radius+1.0);
        cairo_line_to (cr, popup->allocation.width-1.5, popup->allocation.height-1.0);
        cairo_stroke (cr);

        cairo_close_path (cr);
    }

    cairo_destroy (cr);
    cairo_pattern_destroy (vertical_gradient);
    cairo_pattern_destroy (innerstroke_gradient);
    cairo_pattern_destroy (selected_gradient);
    cairo_pattern_destroy (selected_innerstroke_gradient);

    return FALSE;
}


static GtkWidget *
display_setting_identity_display (gint display_id)
{
    GtkBuilder       *builder;
    GtkWidget        *popup;
    GObject          *display_name, *display_details;
    const XfceRRMode *current_mode;
    gchar            *color_hex = "#FFFFFF", *name_label, *details_label;
    gint              screen_pos_x, screen_pos_y;
    gint              window_width, window_height, screen_width, screen_height;

    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, identity_popup_ui,
                                     identity_popup_ui_length, NULL) != 0)
    {
        popup = GTK_WIDGET (gtk_builder_get_object (builder, "popup"));
        gtk_widget_set_name (popup, "XfceDisplayDialogPopup");

        gtk_widget_set_app_paintable (popup, TRUE);
        g_signal_connect (G_OBJECT (popup), "expose-event", G_CALLBACK (display_setting_identity_popup_expose), builder);
        g_signal_connect (G_OBJECT (popup), "screen-changed", G_CALLBACK (display_setting_screen_changed), NULL);

        display_name = gtk_builder_get_object (builder, "display_name");
        display_details = gtk_builder_get_object (builder, "display_details");

        if (display_settings_get_n_active_outputs() > 1)
        {
            current_mode = xfce_randr_find_mode_by_id (xfce_randr, display_id,
                                                       xfce_randr->mode[display_id]);
            if (!xfce_randr_get_positions (xfce_randr, display_id,
                                           &screen_pos_x, &screen_pos_y))
            {
                screen_pos_x = 0;
                screen_pos_y = 0;
            }
            screen_width = xfce_randr_mode_width (current_mode, xfce_randr->rotation[display_id]);
            screen_height = xfce_randr_mode_height (current_mode, xfce_randr->rotation[display_id]);
        }
        else
        {
            screen_pos_x = 0;
            screen_pos_y = 0;
            screen_width = gdk_screen_width ();
            screen_height = gdk_screen_height ();
        }

        name_label = g_markup_printf_escaped ("<span foreground='%s'><big><b>%s %s</b></big></span>",
                                              color_hex, _("Display:"), xfce_randr->friendly_name[display_id]);
        gtk_label_set_markup (GTK_LABEL (display_name), name_label);
        g_free (name_label);

        details_label = g_markup_printf_escaped ("<span foreground='%s'>%s %i x %i</span>", color_hex,
                                                 _("Resolution:"), screen_width, screen_height);
        gtk_label_set_markup (GTK_LABEL (display_details), details_label);
        g_free (details_label);

        gtk_window_get_size (GTK_WINDOW(popup), &window_width, &window_height);

        gtk_window_move (GTK_WINDOW (popup),
                         screen_pos_x + (screen_width - window_width)/2,
                         screen_pos_y + screen_height - window_height);

        display_setting_screen_changed (GTK_WIDGET (popup), NULL, NULL);

        gtk_window_present (GTK_WINDOW (popup));
    }

    /* Release the builder */
    g_object_unref (G_OBJECT (builder));

    return popup;
}

static void
display_setting_identity_popups_populate (void)
{
    guint n;

    g_assert (xfce_randr);

    display_popups = g_hash_table_new_full (g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            (GDestroyNotify) gtk_widget_destroy);

    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] != None)
            g_hash_table_insert (display_popups,
                                 GINT_TO_POINTER (n),
                                 display_setting_identity_display (n));
    }
}

static void
display_setting_mirror_displays_toggled (GtkToggleButton *togglebutton,
                                         GtkBuilder      *builder)
{
    GObject *positions, *active_displays;
    guint    n;
    RRMode   mode;

    if (!xfce_randr)
        return;

    positions = gtk_builder_get_object (builder, "randr-position");
    active_displays = gtk_builder_get_object (builder, "randr-active-displays");

    if (display_settings_get_n_active_outputs () <= 1)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (positions), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (active_displays), FALSE);
        return;
    }

    if (gtk_toggle_button_get_active (togglebutton))
    {
        /* Activate mirror-mode with a single mode for all of them */
        mode = xfce_randr_clonable_mode (xfce_randr);
        /* Apply mirror settings to each output */
        for (n = 0; n < xfce_randr->noutput; n++)
        {
            if (xfce_randr->mode[n] == None)
                continue;

            if (mode != None)
                xfce_randr->mode[n] = mode;
            xfce_randr->relation[n] = XFCE_RANDR_PLACEMENT_MIRROR;
            xfce_randr->related_to[n] = active_output;

            xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                    n, TRUE);
        }

        xfce_randr_apply (xfce_randr, "Default", display_channel);

        /* Disable the position comboboxes */
        gtk_widget_set_sensitive (GTK_WIDGET (positions), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (active_displays), FALSE);
    }
    else
    {
        /* Deactivate mirror-mode, use the preferred mode of each output */
        for (n = 0; n < xfce_randr->noutput; n++)
        {
            xfce_randr->mode[n] = xfce_randr_preferred_mode (xfce_randr, n);
            xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                    n, TRUE);
        }

        xfce_randr_apply (xfce_randr, "Default", display_channel);

        /* Re-enable the position comboboxes */
        gtk_widget_set_sensitive (GTK_WIDGET (positions), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (active_displays), TRUE);
    }
}


static void
display_setting_mirror_displays_populate (GtkBuilder *builder)
{
    GObject *check;
    RRMode   mode = None;
    guint    n;
    gint     cloned = TRUE;

    if (!xfce_randr)
        return;

    check = gtk_builder_get_object (builder, "mirror-displays");

    if (xfce_randr->noutput > 1)
        gtk_widget_show (GTK_WIDGET (check));
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        return;
    }

    /* Can outputs be cloned? */
    if (display_settings_get_n_active_outputs () > 1)
        mode = xfce_randr_clonable_mode (xfce_randr);

    gtk_widget_set_sensitive (GTK_WIDGET (check), mode != None);
    if (mode == None)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
        return;
    }

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_mirror_displays_toggled,
                                     builder);

    /* Check if mirror settings are on */
    for (n = 0; n < xfce_randr->noutput; n++)
    {
        if (xfce_randr->mode[n] == None)
            continue;

        cloned &= (xfce_randr->mode[n] == mode &&
                   xfce_randr->relation[n] == XFCE_RANDR_PLACEMENT_MIRROR);

        if (!cloned)
            break;
    }

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), cloned);

    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (check, display_setting_mirror_displays_toggled,
                                       builder);
}



static void
display_setting_output_toggled (GtkToggleButton *togglebutton,
                                GtkBuilder      *builder)
{
    RRMode old_mode;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    old_mode = xfce_randr->mode[active_output];

    if (gtk_toggle_button_get_active (togglebutton))
        xfce_randr->mode[active_output] =
            xfce_randr_preferred_mode (xfce_randr, active_output);
    else
    {
        if (display_settings_get_n_active_outputs () == 1)
        {
            xfce_dialog_show_warning (NULL,
                                      _("The last active output must not be disabled, the system would"
                                        " be unusable."),
                                      _("Selected output not disabled"));
            /* Set it back to active */
            g_signal_handlers_block_by_func (togglebutton, display_setting_output_toggled,
                                             builder);
            gtk_toggle_button_set_active (togglebutton, TRUE);
            /* Unblock the signal */
            g_signal_handlers_unblock_by_func (togglebutton, display_setting_output_toggled,
                                               builder);
            return;
        }
        xfce_randr->mode[active_output] = None;
    }

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel,
                            active_output, FALSE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    /* Ask user confirmation */
    if (!display_setting_timed_confirmation (builder))
    {
        xfce_randr->mode[active_output] = old_mode;
        xfce_randr_save_output (xfce_randr, "Default", display_channel,
                                active_output, FALSE);
        xfce_randr_apply (xfce_randr, "Default", display_channel);
    }
}


static void
display_setting_output_status_populate (GtkBuilder *builder)
{
    GObject *check;

    if (!xfce_randr)
        return;

    check = gtk_builder_get_object (builder, "output-on");

    if (xfce_randr->noutput > 1)
        gtk_widget_show (GTK_WIDGET (check));
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        return;
    }

    /* Block the "changed" signal to avoid triggering the confirmation dialog */
    g_signal_handlers_block_by_func (check, display_setting_output_toggled,
                                     builder);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                  xfce_randr->mode[active_output] != None);
    /* Unblock the signal */
    g_signal_handlers_unblock_by_func (check, display_setting_output_toggled,
                                       builder);
}



static void
display_settings_iconview_selection_changed (GtkIconView *iconview,
                                             GtkBuilder       *builder)
{
    GtkTreeModel *model;
    GtkTreePath  *path = NULL;
    GtkTreeIter   iter;
    GtkWidget    *popup;
    gint          active_id, previous_id;
    GList        *selected_items;
    guint        n;

    model = gtk_icon_view_get_model(iconview);
    
    selected_items = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (iconview));
    
    if (g_list_length(selected_items) != 0)
    {
        path = selected_items->data;
    
        gtk_tree_model_get_iter(model, &iter, path);
    
        /* Get the output info */
        gtk_tree_model_get (model, &iter, COLUMN_OUTPUT_ID, &active_id, -1);

        /* Get the new active screen or output */
        previous_id = active_output;
        active_output = active_id;

        /* Update the combo boxes */
        display_setting_positions_populate (builder);
        display_setting_active_displays_populate (builder);
        display_setting_output_status_populate (builder);
        display_setting_mirror_displays_populate (builder);
        display_setting_resolutions_populate (builder);
        display_setting_refresh_rates_populate (builder);
        display_setting_rotations_populate (builder);
        display_setting_reflections_populate (builder);

        /* redraw the two (old active, new active) popups */
        popup = g_hash_table_lookup (display_popups, GINT_TO_POINTER (previous_id));
        if (popup)
            gtk_widget_queue_draw (popup);
        popup = g_hash_table_lookup (display_popups, GINT_TO_POINTER (active_id));
        if (popup)
            gtk_widget_queue_draw (popup);
    }
    else
    {
        gtk_tree_model_get_iter_first(model, &iter);
        for (n=0; n<active_output; n++)
        gtk_tree_model_iter_next(model, &iter);
        path = gtk_tree_model_get_path(model, &iter);
        gtk_icon_view_select_path(GTK_ICON_VIEW(iconview), path);
    }
    
    /* Release the path */
    gtk_tree_path_free(path);
}



static void
display_settings_iconview_populate (GtkBuilder *builder)
{
    guint             m;
    GtkListStore     *store;
    GObject          *iconview;
    GtkTreeIter       iter;
    GdkPixbuf        *display_icon, *lucent_display_icon;
    GtkTreePath      *path = NULL;
    gboolean          selected = FALSE;

    /* Create a new list store */
    store = gtk_list_store_new (N_OUTPUT_COLUMNS,
                                G_TYPE_STRING, /* COLUMN_OUTPUT_NAME */
                                GDK_TYPE_PIXBUF, /* COLUMN_OUTPUT_ICON */
                                G_TYPE_INT);   /* COLUMN_OUTPUT_ID */

    /* Set the iconview model */
    iconview = gtk_builder_get_object (builder, "randr-outputs");
    gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), GTK_TREE_MODEL (store));
    gtk_icon_view_set_item_width (GTK_ICON_VIEW (iconview), 48*2);

    /* Get the display icon */
    display_icon =
        gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), "display-icon",
                                  48,
                                  GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                  NULL);

    lucent_display_icon = NULL;

    /* Walk all the connected outputs */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        if (xfce_randr->mode[m] == None && lucent_display_icon == NULL)
            lucent_display_icon =
                exo_gdk_pixbuf_lucent (display_icon, 50);

        /* Insert the output in the store */
        gtk_list_store_append (store, &iter);
        if (xfce_randr->mode[m] == None)
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, xfce_randr->friendly_name[m],
                                COLUMN_OUTPUT_ICON, lucent_display_icon,
                                COLUMN_OUTPUT_ID, m, -1);
        else
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, xfce_randr->friendly_name[m],
                                COLUMN_OUTPUT_ICON, display_icon,
                                COLUMN_OUTPUT_ID, m, -1);

        /* Select active output */
        if (m == active_output)
        {
            path = gtk_tree_model_get_path(GTK_TREE_MODEL (store), &iter);
            gtk_icon_view_select_path(GTK_ICON_VIEW(iconview), path);
            selected = TRUE;
        }
    }

    /* If nothing was selected the active output is no longer valid,
     * select the last display in the list. */
    if (!selected)
    {
        path = gtk_tree_model_get_path(GTK_TREE_MODEL (store), &iter);
        gtk_icon_view_select_path(GTK_ICON_VIEW(iconview), path);
    }

    /* Release the store */
    g_object_unref (G_OBJECT (store));

    /* Release the icons */
    g_object_unref (display_icon);
    if (lucent_display_icon != NULL)
        g_object_unref (lucent_display_icon);
        
    /* Release the path */
    gtk_tree_path_free(path);
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
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help (GTK_WINDOW (dialog), "xfce4-settings", "display", NULL);
    else
        gtk_main_quit ();
}



static GtkWidget *
display_settings_dialog_new (GtkBuilder *builder)
{
    GObject          *iconview;
    GtkCellRenderer  *renderer;
    GObject          *combobox;
    GObject          *label, *check, *mirror;

    /* Get the iconview */
    iconview = gtk_builder_get_object (builder, "randr-outputs");
    gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), COLUMN_OUTPUT_NAME);
    gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), COLUMN_OUTPUT_ICON);

    /* Icon renderer */
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DND, NULL);

    /* Text renderer */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Identification popups */
    display_setting_identity_popups_populate ();

    /* Iconview selection */
    g_signal_connect (GTK_ICON_VIEW(iconview), "selection-changed", G_CALLBACK (display_settings_iconview_selection_changed), builder);

    /* Setup the combo boxes */
    check = gtk_builder_get_object (builder, "output-on");
    mirror = gtk_builder_get_object (builder, "mirror-displays");
    g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (display_setting_output_toggled), builder);
    g_signal_connect (G_OBJECT (mirror), "toggled", G_CALLBACK (display_setting_mirror_displays_toggled), builder);
    if (xfce_randr->noutput > 1)
    {
        gtk_widget_show (GTK_WIDGET (check));
        gtk_widget_show (GTK_WIDGET (mirror));
    }
    else
    {
        gtk_widget_hide (GTK_WIDGET (check));
        gtk_widget_hide (GTK_WIDGET (mirror));
    }

    label = gtk_builder_get_object (builder, "label-reflection");
    gtk_widget_show (GTK_WIDGET (label));

    combobox = gtk_builder_get_object (builder, "randr-reflection");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    gtk_widget_show (GTK_WIDGET (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_reflections_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-position");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_positions_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-active-displays");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_active_displays_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-resolution");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-rotation");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), builder);

    /* Populate the iconview */
    display_settings_iconview_populate (builder);

    return GTK_WIDGET (gtk_builder_get_object (builder, "display-dialog"));
}

static void
display_settings_minimal_only_display1_toggled (GtkToggleButton *button,
                                                GtkBuilder      *builder)
{
    GObject *buttons;

    if (!gtk_toggle_button_get_active (button))
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Put Display1 in its preferred mode and deactivate Display2 */
    xfce_randr->mode[0] = xfce_randr_preferred_mode (xfce_randr, 0);
    xfce_randr->mode[1] = None;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0, FALSE);
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1, FALSE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_only_display2_toggled (GtkToggleButton *button,
                                                GtkBuilder      *builder)
{
    GObject *buttons;

    if (!gtk_toggle_button_get_active(button) )
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Put Display2 in its preferred mode and deactivate Display1 */
    xfce_randr->mode[1] = xfce_randr_preferred_mode (xfce_randr, 1);
    xfce_randr->mode[0] = None;

    /* Apply the changes */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0, FALSE);
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1, FALSE);
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_mirror_displays_toggled (GtkToggleButton *button,
                                                  GtkBuilder      *builder)
{
    GObject *buttons;

    guint n;

    if (!gtk_toggle_button_get_active(button))
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET(buttons), FALSE);

    /* Activate all inactive displays */
    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] == None)
        {
            xfce_randr->mode[n] = xfce_randr_preferred_mode (xfce_randr, n);
        }
    }

    /* Save changes to primary display */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0, FALSE);

    /* Save changes to secondary display */
    xfce_randr->relation[1] = XFCE_RANDR_PLACEMENT_MIRROR;
    xfce_randr->related_to[1] = 0;
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1, TRUE);

    /* Apply all changes */
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET(buttons), TRUE);
}

static void
display_settings_minimal_extend_right_toggled (GtkToggleButton *button,
                                               GtkBuilder      *builder)
{
    GObject *buttons;
    guint    n;

    if (!gtk_toggle_button_get_active(button))
        return;

    if (!xfce_randr)
        return;

    if (xfce_randr->noutput <= 1)
        return;

    buttons = gtk_builder_get_object (builder, "buttons");
    gtk_widget_set_sensitive (GTK_WIDGET (buttons), FALSE);

    /* Activate all inactive displays */
    for (n = 0; n < xfce_randr->noutput; ++n)
    {
        if (xfce_randr->mode[n] == None)
        {
            xfce_randr->mode[n] = xfce_randr_preferred_mode (xfce_randr, n);
        }
    }

    /* (Re)set Display1 to 0x0 */
    xfce_randr->relation[0] = 0;
    xfce_randr->related_to[0] = 0;

    /* Move Display2 right of Display1 */
    xfce_randr->relation[1] = XFCE_RANDR_PLACEMENT_RIGHT;
    xfce_randr->related_to[1] = 0;

    /* Save changes to both displays */
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 0, FALSE);
    xfce_randr_save_output (xfce_randr, "Default", display_channel, 1, TRUE);

    /* Apply all changes */
    xfce_randr_apply (xfce_randr, "Default", display_channel);

    gtk_widget_set_sensitive (GTK_WIDGET (buttons), TRUE);
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

    event_num = e->type - randr_event_base;

    if (event_num == RRScreenChangeNotify)
    {
        xfce_randr_reload (xfce_randr);
        display_settings_iconview_populate (builder);

        /* recreate the identify display popups */
        g_hash_table_destroy (display_popups);
        display_setting_identity_popups_populate ();
    }

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}

static void
display_settings_show_main_dialog (GdkDisplay *display)
{
    GtkBuilder  *builder;
    GtkWidget   *dialog, *plug;
    GObject     *plug_child;
    GError      *error = NULL;

    /* Load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, display_dialog_ui,
                                     display_dialog_ui_length, &error) != 0)
    {
        /* Build the dialog */
        dialog = display_settings_dialog_new (builder);
        /* Set up notifications */
        XRRSelectInput (gdk_x11_display_get_xdisplay (display),
                        GDK_WINDOW_XID (gdk_get_default_root_window ()),
                        RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (display,
                                              randr_event_base,
                                              RRNotify + 1);
        gdk_window_add_filter (gdk_get_default_root_window (), screen_on_event, builder);

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            g_signal_connect (G_OBJECT (dialog), "response",
                G_CALLBACK (display_settings_dialog_response), builder);

            /* Show the dialog */
            gtk_window_present (GTK_WINDOW (dialog));
        }
        else
        {
            /* Create plug widget */
            plug = gtk_plug_new (opt_socket_id);
            g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
            gtk_widget_show (plug);

            /* Get plug child widget */
            plug_child = gtk_builder_get_object (builder, "plug-child");
            gtk_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));
        }

        g_signal_connect (G_OBJECT (dialog), "focus-out-event", G_CALLBACK (display_setting_toggle_identity_popups),
                          GINT_TO_POINTER (FALSE));

        g_signal_connect (G_OBJECT (dialog), "focus-in-event", G_CALLBACK (display_setting_toggle_identity_popups),
                          GINT_TO_POINTER (TRUE));

        /* To prevent the settings dialog to be saved in the session */
        gdk_set_sm_client_id ("FAKE ID");

        /* Enter the main loop */
        gtk_main ();

        gtk_widget_destroy (dialog);
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

static void
display_settings_minimal_advanced_clicked (GtkButton  *button,
                                           GtkBuilder *builder)
{
    GtkWidget *dialog;

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
    gtk_widget_hide (dialog);

    display_settings_show_main_dialog (gdk_display_get_default ());

    gtk_main_quit ();
}

static void
display_settings_show_minimal_dialog (GdkDisplay *display)
{
    GtkBuilder *builder;
    GtkWidget  *dialog, *cancel;
    GObject    *only_display1, *only_display2, *mirror_displays;
    GObject    *extend_right, *advanced, *fake_button, *label;
    GError     *error = NULL;

    builder = gtk_builder_new ();

    if (gtk_builder_add_from_string (builder, minimal_display_dialog_ui,
                                     minimal_display_dialog_ui_length, &error) != 0)
    {
        /* Build the minimal dialog */
        dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
        cancel = GTK_WIDGET (gtk_builder_get_object (builder, "cancel_button"));

        g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
        g_signal_connect (cancel, "clicked", G_CALLBACK (gtk_main_quit), NULL);

        only_display1 = gtk_builder_get_object (builder, "display1");
        mirror_displays = gtk_builder_get_object (builder, "mirror");
        extend_right = gtk_builder_get_object (builder, "extend_right");
        only_display2 = gtk_builder_get_object (builder, "display2");
        advanced = gtk_builder_get_object (builder, "advanced_button");
        fake_button = gtk_builder_get_object (builder, "fake_button");

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fake_button), TRUE);

        label = gtk_builder_get_object (builder, "label1");
        gtk_label_set_text (GTK_LABEL (label), xfce_randr->friendly_name[0]);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display1),
                                      xfce_randr->mode[0] != None);

        if (xfce_randr->noutput > 1)
        {
            label = gtk_builder_get_object (builder, "label4");
            gtk_label_set_text (GTK_LABEL (label), xfce_randr->friendly_name[1]);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (only_display2),
                                          xfce_randr->mode[1] != None);

            if (xfce_randr->mode[0] != None && xfce_randr->mode[1] != None)
            {
                /* Check for mirror */
                if ((xfce_randr->relation[1] == XFCE_RANDR_PLACEMENT_MIRROR &&
                    xfce_randr->related_to[1] == 0) || (xfce_randr->related_to[0] == 1 &&
                    xfce_randr->relation[0] == XFCE_RANDR_PLACEMENT_MIRROR))
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mirror_displays),
                                                  TRUE);

                /* Check for Extend Right */
                if ((xfce_randr->relation[1] == XFCE_RANDR_PLACEMENT_RIGHT &&
                    xfce_randr->related_to[1] == 0) || (xfce_randr->related_to[0] == 1 &&
                    xfce_randr->relation[1] == XFCE_RANDR_PLACEMENT_LEFT))
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (extend_right),
                                                  TRUE);
            }
        }
        else
        {
            /* Only one output, disable other buttons */
            gtk_widget_set_sensitive (GTK_WIDGET (mirror_displays), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (extend_right), FALSE);
            gtk_widget_set_sensitive (GTK_WIDGET (only_display2), FALSE);
        }

        g_signal_connect (only_display1, "toggled", G_CALLBACK (display_settings_minimal_only_display1_toggled),
                          builder);
        g_signal_connect (mirror_displays, "toggled", G_CALLBACK (display_settings_minimal_mirror_displays_toggled),
                          builder);
        g_signal_connect (extend_right, "toggled", G_CALLBACK (display_settings_minimal_extend_right_toggled),
                          builder);
        g_signal_connect (only_display2, "toggled", G_CALLBACK (display_settings_minimal_only_display2_toggled),
                          builder);
        g_signal_connect (advanced, "clicked", G_CALLBACK (display_settings_minimal_advanced_clicked),
                          builder);

        /* Show the minimal dialog and start the main loop */
        gtk_window_present (GTK_WINDOW (dialog));
        gtk_main ();
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    g_object_unref (G_OBJECT (builder));
}


gint
main (gint argc, gchar **argv)
{
    GdkDisplay  *display;
    GError      *error = NULL;
    gboolean     succeeded = TRUE;
    gint         error_base;
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
        g_print ("%s\n", "Copyright (c) 2004-2011");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* Get the default display */
    display = gdk_display_get_default ();

    /* Check if the randr extension is avaible on the system */
    if (!XRRQueryExtension (gdk_x11_display_get_xdisplay (display), &randr_event_base, &error_base))
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
            command = g_find_program_in_path ("amdcccle");

            if (command != NULL)
            {
                alternative = _("ATI Settings");
                alternative_icon = "ccc_small";
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

        if (xfce_randr->noutput <= 1 || !minimal)
            display_settings_show_main_dialog (display);
        else
            display_settings_show_minimal_dialog (display);

cleanup:
        /* Release the channel */
        g_object_unref (G_OBJECT (display_channel));
    }

    /* Free the randr 1.2 backend */
    if (xfce_randr)
        xfce_randr_free (xfce_randr);

    /* Shutdown xfconf */
    xfconf_shutdown ();

    return (succeeded ? EXIT_SUCCESS : EXIT_FAILURE);
}
