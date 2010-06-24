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
#include "xfce-randr-legacy.h"
#include "display-dialog_ui.h"
#ifdef HAS_RANDR_ONE_POINT_TWO
#include "minimal-display-dialog_ui.h"
#endif

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



/* xrandr rotation name conversion */
static const XfceRotation rotation_names[] =
{
    { RR_Rotate_0,   N_("Normal") },
    { RR_Rotate_90,  N_("Left") },
    { RR_Rotate_180, N_("Inverted") },
    { RR_Rotate_270, N_("Right") }
};



#ifdef HAS_RANDR_ONE_POINT_TWO
/* xrandr reflection name conversion */
static const XfceRotation reflection_names[] =
{
    { 0,                         N_("None") },
    { RR_Reflect_X,              N_("Horizontal") },
    { RR_Reflect_Y,              N_("Vertical") },
    { RR_Reflect_X|RR_Reflect_Y, N_("Both") }
};
#endif



/* option entries */
static gboolean opt_version = FALSE;
#ifdef HAS_RANDR_ONE_POINT_TWO
static gboolean minimal = FALSE;
#endif
static GOptionEntry option_entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    #ifdef HAS_RANDR_ONE_POINT_TWO
    {
    "minimal", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &minimal,
    N_("Minimal interface to set up an external output"),
    NULL
    },
    #endif
    { NULL }
};

/* global xfconf channel */
static XfconfChannel *display_channel;

/* pointer to the used randr structure */
#ifdef HAS_RANDR_ONE_POINT_TWO
XfceRandr *xfce_randr = NULL;
#endif
XfceRandrLegacy *xfce_randr_legacy = NULL;



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



#ifdef HAS_RANDR_ONE_POINT_TWO
static void
display_setting_reflections_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    if (xfce_randr)
    {
        /* remove existing reflection */
        XFCE_RANDR_ROTATION (xfce_randr) &= ~XFCE_RANDR_REFLECTIONS_MASK;
        /* set the new one */
        XFCE_RANDR_ROTATION (xfce_randr) |= value;
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

    /* get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-reflection");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    if (xfce_randr)
    {
        /* disable it if no mode is selected */
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), XFCE_RANDR_MODE (xfce_randr) != None);

        /* load only supported reflections */
        reflections = XFCE_RANDR_ROTATIONS (xfce_randr) & XFCE_RANDR_REFLECTIONS_MASK;
        active_reflection = XFCE_RANDR_ROTATION (xfce_randr) & XFCE_RANDR_REFLECTIONS_MASK;

        /* try to insert the reflections */
        for (n = 0; n < G_N_ELEMENTS (reflection_names); n++)
        {
            if ((reflections & reflection_names[n].rotation) == reflection_names[n].rotation)
            {
                /* insert */
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                    COLUMN_COMBO_NAME, _(reflection_names[n].name),
                                    COLUMN_COMBO_VALUE, reflection_names[n].rotation, -1);

                /* select active reflection */
                if (xfce_randr && XFCE_RANDR_MODE (xfce_randr) != None)
                {
                    if ((reflection_names[n].rotation & active_reflection) == reflection_names[n].rotation)
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
                }
            }
        }
    }
}
#endif



static void
display_setting_rotations_changed (GtkComboBox *combobox,
                                   GtkBuilder  *builder)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* set new rotation */
#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        XFCE_RANDR_ROTATION (xfce_randr) &= ~XFCE_RANDR_ROTATIONS_MASK;
        XFCE_RANDR_ROTATION (xfce_randr) |= value;
    }
    else
#endif
        XFCE_RANDR_LEGACY_ROTATION (xfce_randr_legacy) = value;
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

    /* get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-rotation");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        /* disable it if no mode is selected */
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), XFCE_RANDR_MODE (xfce_randr) != None);

        /* load only supported rotations */
        rotations = XFCE_RANDR_ROTATIONS (xfce_randr) & XFCE_RANDR_ROTATIONS_MASK;
        active_rotation = XFCE_RANDR_ROTATION (xfce_randr) & XFCE_RANDR_ROTATIONS_MASK;
    }
    else
#endif
    {
        /* load all possible rotations */
        rotations = XRRConfigRotations (XFCE_RANDR_LEGACY_CONFIG (xfce_randr_legacy), &active_rotation);
        active_rotation = XFCE_RANDR_LEGACY_ROTATION (xfce_randr_legacy);
    }

    /* try to insert the rotations */
    for (n = 0; n < G_N_ELEMENTS (rotation_names); n++)
    {
        if ((rotations & rotation_names[n].rotation) == rotation_names[n].rotation)
        {
            /* insert */
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, _(rotation_names[n].name),
                                COLUMN_COMBO_VALUE, rotation_names[n].rotation, -1);

            /* select active rotation */
#ifdef HAS_RANDR_ONE_POINT_TWO
            if (xfce_randr && XFCE_RANDR_MODE (xfce_randr) != None)
#endif
            {
                if ((rotation_names[n].rotation & active_rotation) == rotation_names[n].rotation)
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
            }
        }
    }
}



static void
display_setting_refresh_rates_changed (GtkComboBox *combobox,
                                       GtkBuilder  *builder)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* set new mode (1.2) or rate (1.1) */
#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
        XFCE_RANDR_MODE (xfce_randr) = value;
    else
#endif
        XFCE_RANDR_LEGACY_RATE (xfce_randr_legacy) = value;
}



static void
display_setting_refresh_rates_populate (GtkBuilder *builder)
{
    GtkTreeModel *model;
    GObject      *combobox;
    gshort       *rates;
    gint          nrates;
    GtkTreeIter   iter;
    gchar        *name = NULL;
    gint          n, active = -1;
    gshort        diff, active_diff = G_MAXSHORT;
#ifdef HAS_RANDR_ONE_POINT_TWO
    GObject      *res_combobox;
    GtkTreeIter   dummy;
    XfceRRMode   *modes, *current_mode;
#endif

    /* get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        /* disable it if no mode is selected */
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), XFCE_RANDR_MODE (xfce_randr) != None);

        /* fetch the selected resolution */
        res_combobox = gtk_builder_get_object (builder, "randr-resolution");
        if (!display_setting_combo_box_get_value (GTK_COMBO_BOX (res_combobox), &n))
            return;

        current_mode = xfce_randr_find_mode_by_id (xfce_randr, xfce_randr->active_output, n);
        if (!current_mode)
            return;

        /* walk all supported modes */
        modes = XFCE_RANDR_SUPPORTED_MODES (xfce_randr);
        for (n = 0; n < XFCE_RANDR_OUTPUT_INFO (xfce_randr)->nmode; ++n)
        {
            /* the mode resolution does not match the selected one */
            if (modes[n].width != current_mode->width
                || modes[n].height != current_mode->height)
                continue;

            /* insert the mode */
            name = g_strdup_printf (_("%.1f Hz"), modes[n].rate);
            gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, name,
                                COLUMN_COMBO_VALUE, modes[n].id, -1);
            g_free (name);

            /* select the active mode */
            if (modes[n].id == XFCE_RANDR_MODE (xfce_randr))
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }

        /* if a new resolution was selected, set a refresh rate */
        if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &dummy))
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }
    else
#endif
    {
        /* get the refresh rates */
        rates = XRRConfigRates (XFCE_RANDR_LEGACY_CONFIG (xfce_randr_legacy),
                                XFCE_RANDR_LEGACY_RESOLUTION (xfce_randr_legacy), &nrates);

        for (n = 0; n < nrates; n++)
        {
            /* insert */
            name = g_strdup_printf (_("%d Hz"), rates[n]);
            gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COLUMN_COMBO_NAME, name,
                                COLUMN_COMBO_VALUE, rates[n], -1);
            g_free (name);

            /* get the active rate closest to the current diff */
            diff = ABS (rates[n] - XFCE_RANDR_LEGACY_RATE (xfce_randr_legacy));

            /* store active */
            if (active_diff > diff)
            {
                active = nrates - n - 1;
                active_diff = diff;
            }
        }

        /* set closest refresh rate */
        if (G_LIKELY (active != -1))
            gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), active);
    }
}



static void
display_setting_resolutions_changed (GtkComboBox *combobox,
                                     GtkBuilder  *builder)
{
    gint value;

    if (!display_setting_combo_box_get_value (combobox, &value))
        return;

    /* set new resolution */
    if (xfce_randr_legacy)
        XFCE_RANDR_LEGACY_RESOLUTION (xfce_randr_legacy) = value;

    /* update refresh rates */
    display_setting_refresh_rates_populate (builder);
}



static void
display_setting_resolutions_populate (GtkBuilder *builder)
{
    GtkTreeModel  *model;
    GObject       *combobox;
    XRRScreenSize *screen_sizes;
    gint           n, nsizes;
    gchar         *name;
    GtkTreeIter    iter;
#ifdef HAS_RANDR_ONE_POINT_TWO
    XfceRRMode   *modes;
#endif

    /* get the combo box store and clear it */
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        /* disable it if no mode is selected */
        gtk_widget_set_sensitive (GTK_WIDGET (combobox), XFCE_RANDR_MODE (xfce_randr) != None);

        /* walk all supported modes */
        modes = XFCE_RANDR_SUPPORTED_MODES (xfce_randr);
        for (n = 0; n < XFCE_RANDR_OUTPUT_INFO (xfce_randr)->nmode; ++n)
        {
            /* try to avoid duplicates */
            if (n == 0 || (n > 0 && modes[n].width != modes[n - 1].width
                && modes[n].height != modes[n - 1].height))
            {

                /* insert the mode */
                name = g_strdup_printf ("%dx%d", modes[n].width, modes[n].height);
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                    COLUMN_COMBO_NAME, name,
                                    COLUMN_COMBO_VALUE, modes[n].id, -1);
                g_free (name);
            }

            /* select the active mode */
            if (modes[n].id == XFCE_RANDR_MODE (xfce_randr))
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }
    }
    else
#endif
    {
        /* get the possible screen sizes for this screen */
        screen_sizes = XRRConfigSizes (XFCE_RANDR_LEGACY_CONFIG (xfce_randr_legacy), &nsizes);

        for (n = 0; n < nsizes; n++)
        {
            /* insert in the model */
            name = g_strdup_printf ("%dx%d", screen_sizes[n].width, screen_sizes[n].height);
            gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, n,
                                               COLUMN_COMBO_NAME, name,
                                               COLUMN_COMBO_VALUE, n, -1);
            g_free (name);

            /* select active */
            if (G_UNLIKELY (XFCE_RANDR_LEGACY_RESOLUTION (xfce_randr_legacy) == n))
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
        }
    }
}



#ifdef HAS_RANDR_ONE_POINT_TWO
static void
display_setting_output_toggled (GtkToggleButton *togglebutton,
                                GtkBuilder      *builder)
{
    GObject *radio;
    gint     is_active;

    if (!xfce_randr)
        return;

    radio = gtk_builder_get_object (builder, "randr-on");
    is_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));

    if (is_active && XFCE_RANDR_MODE (xfce_randr) == None)
        XFCE_RANDR_MODE (xfce_randr) =
            XFCE_RANDR_OUTPUT_INFO (xfce_randr)->modes[XFCE_RANDR_OUTPUT_INFO (xfce_randr)->npreferred];
    else if (!is_active && XFCE_RANDR_MODE (xfce_randr) != None)
        XFCE_RANDR_MODE (xfce_randr) = None;

    display_setting_resolutions_populate (builder);
    display_setting_refresh_rates_populate (builder);
    display_setting_rotations_populate (builder);
    display_setting_reflections_populate (builder);
}


static void
display_setting_output_status_populate (GtkBuilder *builder)
{
    GObject *radio_on, *radio_off;

    if (!xfce_randr)
        return;

    radio_on = gtk_builder_get_object (builder, "randr-on");
    radio_off = gtk_builder_get_object (builder, "randr-off");

    if (XFCE_RANDR_MODE (xfce_randr) != None)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_on), TRUE);
    else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_off), TRUE);
}
#endif



static void
display_settings_treeview_selection_changed (GtkTreeSelection *selection,
                                             GtkBuilder       *builder)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gint          active_id;

    /* get the selection */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get the output info */
        gtk_tree_model_get (model, &iter, COLUMN_OUTPUT_ID, &active_id, -1);

        /* set the new active screen or output */
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            xfce_randr->active_output = active_id;
        else
#endif
            xfce_randr_legacy->active_screen = active_id;

        /* update the combo boxes */
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            display_setting_output_status_populate (builder);
#endif
        display_setting_resolutions_populate (builder);
        display_setting_refresh_rates_populate (builder);
        display_setting_rotations_populate (builder);
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            display_setting_reflections_populate (builder);
#endif
    }
}



static void
display_settings_treeview_populate (GtkBuilder *builder)
{
    gint              n;
    GtkListStore     *store;
    GObject          *treeview;
    GtkTreeIter       iter;
    gchar            *name;
    GdkPixbuf        *display_icon, *lucent_display_icon;
    GtkTreeSelection *selection;

    /* create a new list store */
    store = gtk_list_store_new (N_OUTPUT_COLUMNS,
                                G_TYPE_STRING, /* COLUMN_OUTPUT_NAME */
                                GDK_TYPE_PIXBUF, /* COLUMN_OUTPUT_ICON */
                                G_TYPE_INT);   /* COLUMN_OUTPUT_ID */

    /* set the treeview model */
    treeview = gtk_builder_get_object (builder, "randr-outputs");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

    /* get the display icon */
    display_icon =
        gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), "display-icon",
                                  32,
                                  GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                  NULL);

    lucent_display_icon = NULL;

#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr)
    {
        /* walk all the outputs */
        for (n = 0; n < xfce_randr->resources->noutput; n++)
        {
            /* only show screen that are primary or secondary */
            if (xfce_randr->status[n] == XFCE_OUTPUT_STATUS_NONE)
                continue;

            /* get a friendly name for the output */
            name = xfce_randr_friendly_name (xfce_randr,
                                             xfce_randr->resources->outputs[n],
                                             xfce_randr->output_info[n]->name);

            if (xfce_randr->mode[n] == None && lucent_display_icon == NULL)
                lucent_display_icon =
                    exo_gdk_pixbuf_lucent (display_icon, 50);

            /* insert the output in the store */
            gtk_list_store_append (store, &iter);
            if (xfce_randr->mode[n] == None)
                gtk_list_store_set (store, &iter,
                                    COLUMN_OUTPUT_NAME, name,
                                    COLUMN_OUTPUT_ICON, lucent_display_icon,
                                    COLUMN_OUTPUT_ID, n, -1);
            else
                gtk_list_store_set (store, &iter,
                                    COLUMN_OUTPUT_NAME, name,
                                    COLUMN_OUTPUT_ICON, display_icon,
                                    COLUMN_OUTPUT_ID, n, -1);

            g_free (name);

            /* select active output */
            if (n == xfce_randr->active_output)
                gtk_tree_selection_select_iter (selection, &iter);
        }
    }
    else
#endif
    {
        /* walk all the screens */
        for (n = 0; n < xfce_randr_legacy->num_screens; n++)
        {
            /* create name */
            name = g_strdup_printf (_("Screen %d"), n + 1);

            /* insert the output in the store */
            gtk_list_store_append (store, &iter);
            gtk_list_store_set (store, &iter,
                                COLUMN_OUTPUT_NAME, name,
                                COLUMN_OUTPUT_ICON, display_icon,
                                COLUMN_OUTPUT_ID, n, -1);

            /* cleanup */
            g_free (name);

            /* select active screen */
            if (n == xfce_randr_legacy->active_screen)
                gtk_tree_selection_select_iter (selection, &iter);
        }
    }

    /* release the store */
    g_object_unref (G_OBJECT (store));

    /* release the icons */
    g_object_unref (display_icon);
    if (lucent_display_icon != NULL)
        g_object_unref (lucent_display_icon);
}



static void
display_settings_combo_box_create (GtkComboBox *combobox)
{
    GtkCellRenderer *renderer;
    GtkListStore    *store;

    /* create and set the combobox model */
    store = gtk_list_store_new (N_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));

    /* setup renderer */
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
    if (response_id == 1)
    {
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            xfce_randr_save (xfce_randr, "Default", display_channel);
        else
#endif
            xfce_randr_legacy_save (xfce_randr_legacy, "Default", display_channel);
    }
    else if (response_id == 2)
    {
        /* reload randr information */
#ifdef HAS_RANDR_ONE_POINT_TWO
        if (xfce_randr)
            xfce_randr_reload (xfce_randr);
        else
#endif
            xfce_randr_legacy_reload (xfce_randr_legacy);

        /* update dialog */
        display_settings_treeview_populate (builder);
    }
    else
    {
        /* close */
        gtk_main_quit ();
    }
}



static GtkWidget *
display_settings_dialog_new (GtkBuilder *builder)
{
    GObject          *treeview;
    GtkCellRenderer  *renderer;
    GtkTreeSelection *selection;
    GObject          *combobox;
#ifdef HAS_RANDR_ONE_POINT_TWO
    GObject          *label, *radio;
#endif

    /* get the treeview */
    treeview = gtk_builder_get_object (builder, "randr-outputs");
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_OUTPUT_NAME);

    /* icon renderer */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 0, "", renderer, "pixbuf", COLUMN_OUTPUT_ICON, NULL);
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DND, NULL);

    /* text renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 1, "", renderer, "text", COLUMN_OUTPUT_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* treeview selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (display_settings_treeview_selection_changed), builder);

    /* setup the combo boxes */
#ifdef HAS_RANDR_ONE_POINT_TWO
    if (xfce_randr != NULL)
    {
        radio = gtk_builder_get_object (builder, "randr-on");
        gtk_widget_show (GTK_WIDGET (radio));
        g_signal_connect (G_OBJECT (radio), "toggled", G_CALLBACK (display_setting_output_toggled), builder);

        radio = gtk_builder_get_object (builder, "randr-off");
        gtk_widget_show (GTK_WIDGET (radio));
        g_signal_connect (G_OBJECT (radio), "toggled", G_CALLBACK (display_setting_output_toggled), builder);

        label = gtk_builder_get_object (builder, "label-reflection");
        gtk_widget_show (GTK_WIDGET (label));

        combobox = gtk_builder_get_object (builder, "randr-reflection");
        display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
        gtk_widget_show (GTK_WIDGET (combobox));
        g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_reflections_changed), builder);
    }
#endif
    combobox = gtk_builder_get_object (builder, "randr-resolution");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_resolutions_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-refresh-rate");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_refresh_rates_changed), builder);

    combobox = gtk_builder_get_object (builder, "randr-rotation");
    display_settings_combo_box_create (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_setting_rotations_changed), builder);

    /* populate the treeview */
    display_settings_treeview_populate (builder);

    return GTK_WIDGET (gtk_builder_get_object (builder, "display-dialog"));
}



#ifdef HAS_RANDR_ONE_POINT_TWO
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
    gint        first, second;
    gint        m, n, found;

    if (response_id == 1)
    {
        /* OK */

        first = second = -1;

        for (n = 0; n < xfce_randr->resources->noutput; n++)
        {
            if (xfce_randr->status[n] != XFCE_OUTPUT_STATUS_NONE)
            {
                if (first < 0)
                    first = n;
                else if (second < 0)
                    second = n;
                else
                    break;
            }
        }

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
            xfce_randr->mode[first] =
                xfce_randr->output_info[first]->modes[xfce_randr->output_info[first]->npreferred];
            xfce_randr->mode[second] = None;
        }
        else if (use_second_screen)
        {
            xfce_randr->mode[second] =
                xfce_randr->output_info[second]->modes[xfce_randr->output_info[second]->npreferred];
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
                /* no clone mode available, try to find a "similar" mode */
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

        xfce_randr_save (xfce_randr, "MinimalAutoConfig", display_channel);
    }

    gtk_main_quit ();
}
#endif



gint
main (gint argc, gchar **argv)
{
    GtkWidget  *dialog;
    GtkBuilder *builder;
    GError     *error = NULL;
    GdkDisplay *display;
    gboolean    succeeded = TRUE;
    gint        event_base, error_base;
    guint       ui_ret;

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
        g_print ("%s\n", "Copyright (c) 2004-2010");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* get the default display */
    display = gdk_display_get_default ();

    /* check if the randr extension is avaible on the system */
    if (!XRRQueryExtension (gdk_x11_display_get_xdisplay (display), &event_base, &error_base))
    {
        dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
                                         _("RandR extension missing on display \"%s\""),
                                         gdk_display_get_name (display));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  _("The Resize and Rotate extension (RandR) is not enabled on "
                                                    "this display. Try to enable it and run the dialog again."));
        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_QUIT, GTK_RESPONSE_CLOSE);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        return EXIT_FAILURE;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    display_channel = xfconf_channel_new ("displays");
    if (G_LIKELY (display_channel))
    {
#ifdef HAS_RANDR_ONE_POINT_TWO
        /* create a new xfce randr (>= 1.2) for this display
         * this will only work if there is 1 screen on this display */
        if (gdk_display_get_n_screens (display) == 1)
            xfce_randr = xfce_randr_new (display, NULL);

        /* fall back on the legacy backend */
        if (xfce_randr == NULL)
#endif
        {
            xfce_randr_legacy = xfce_randr_legacy_new (display, &error);
            if (G_UNLIKELY (xfce_randr_legacy == NULL))
            {
                /* show an error dialog the version is too old */
                dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
                                                 _("Failed to use the RandR extension"));
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
                gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_QUIT, GTK_RESPONSE_CLOSE);
                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);
                g_error_free (error);

                /* leave and cleanup the data */
                goto err1;
            }
        }

        /* hook to make sure the libxfce4ui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
            return EXIT_FAILURE;

#ifdef HAS_RANDR_ONE_POINT_TWO
        if (!minimal)
        {
#endif
            /* load the Gtk user-interface file */
            builder = gtk_builder_new ();
            if (gtk_builder_add_from_string (builder, display_dialog_ui,
                                             display_dialog_ui_length, &error) != 0)
            {
                /* build the dialog */
                dialog = display_settings_dialog_new (builder);
                g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (display_settings_dialog_response), builder);

    #ifdef HAS_RANDR_ONE_POINT_TWO
                if (xfce_randr != NULL)
                {
                    XFCE_RANDR_EVENT_BASE (xfce_randr) = event_base;
                    /* set up notifications */
                    XRRSelectInput (gdk_x11_display_get_xdisplay (display),
                                    GDK_WINDOW_XID (gdk_get_default_root_window ()),
                                    RRScreenChangeNotifyMask);
                    gdk_x11_register_standard_event_type (display,
                                                          event_base,
                                                          RRNotify + 1);
                    gdk_window_add_filter (gdk_get_default_root_window (), screen_on_event, builder);
                }
    #endif

                /* show the dialog */
                gtk_widget_show (dialog);

                /* To prevent the settings dialog to be saved in the session */
                gdk_set_sm_client_id ("FAKE ID");

                /* enter the main loop */
                gtk_main ();
            }
            else
            {
                g_error ("Failed to load the UI file: %s.", error->message);
                g_error_free (error);
            }

    #ifdef HAS_RANDR_ONE_POINT_TWO
            if (xfce_randr != NULL)
                gdk_window_remove_filter (gdk_get_default_root_window (), screen_on_event, builder);
    #endif

            /* release the builder */
            g_object_unref (G_OBJECT (builder));

 #ifdef HAS_RANDR_ONE_POINT_TWO
        }
        else
        {
            gint n;
            gint first, second;

            if (xfce_randr->resources->noutput < 2 || xfce_randr == NULL)
                goto err1;

            first = second = -1;

            for (n = 0; n < xfce_randr->resources->noutput; n++)
            {
                if (xfce_randr->status[n] != XFCE_OUTPUT_STATUS_NONE)
                {
                    if (first < 0)
                        first = n;
                    else if (second < 0)
                        second = n;
                    else
                        break;
                }
            }

            if (first < 0 || second < 0)
                goto err1;

            builder = gtk_builder_new ();

            ui_ret =
                gtk_builder_add_from_string (builder,
                                             minimal_display_dialog_ui,
                                             minimal_display_dialog_ui_length,
                                             &error);

            if (ui_ret != 0)
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
#endif

        err1:

        /* release the channel */
        g_object_unref (G_OBJECT (display_channel));
    }

#ifdef HAS_RANDR_ONE_POINT_TWO
    /* free the randr 1.2 backend */
    if (xfce_randr)
        xfce_randr_free (xfce_randr);
#endif

    /* free the legacy backend */
    if (xfce_randr_legacy)
        xfce_randr_legacy_free (xfce_randr_legacy);

    /* shutdown xfconf */
    xfconf_shutdown ();

    return (succeeded ? EXIT_SUCCESS : EXIT_FAILURE);

}
