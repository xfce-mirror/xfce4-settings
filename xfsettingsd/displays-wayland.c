/*
 *  Copyright (c) 2023 Gaël Bonithon <gael@xfce.org>
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

#include "displays-wayland.h"

#include "common/debug.h"
#include "common/display-profiles.h"
#include "common/edid.h"
#include "common/xfce-wlr-output-manager.h"

#include <gdk/gdkwayland.h>
#include <libxfce4ui/libxfce4ui.h>



static void
xfce_displays_helper_wayland_finalize (GObject *object);
static GPtrArray *
xfce_displays_helper_wayland_get_outputs (XfceDisplaysHelper *helper);
static void
xfce_displays_helper_wayland_toggle_internal (gpointer *power,
                                              gboolean lid_is_closed,
                                              XfceDisplaysHelper *helper);
static gchar **
xfce_displays_helper_wayland_get_display_infos (XfceDisplaysHelper *helper);
static void
xfce_displays_helper_wayland_channel_apply (XfceDisplaysHelper *helper,
                                            const gchar *scheme);

static void
manager_listener (XfceWlrOutputManager *manager, struct zwlr_output_manager_v1 *wl_manager, uint32_t serial);
static void
configuration_succeeded (void *data, struct zwlr_output_configuration_v1 *config);
static void
configuration_failed (void *data, struct zwlr_output_configuration_v1 *config);
static void
configuration_cancelled (void *data, struct zwlr_output_configuration_v1 *config);



struct _XfceDisplaysHelperWayland
{
    XfceDisplaysHelper __parent__;

    XfceWlrOutputManager *manager;
    uint32_t serial;
    guint previous_n_outputs;
    gboolean config_cancelled;
};

static const struct zwlr_output_configuration_v1_listener configuration_listener = {
    .succeeded = configuration_succeeded,
    .failed = configuration_failed,
    .cancelled = configuration_cancelled,
};



G_DEFINE_TYPE (XfceDisplaysHelperWayland, xfce_displays_helper_wayland, XFCE_TYPE_DISPLAYS_HELPER);



static void
xfce_displays_helper_wayland_class_init (XfceDisplaysHelperWaylandClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    XfceDisplaysHelperClass *helper_class = XFCE_DISPLAYS_HELPER_CLASS (klass);

    gobject_class->finalize = xfce_displays_helper_wayland_finalize;

    helper_class->get_outputs = xfce_displays_helper_wayland_get_outputs;
    helper_class->toggle_internal = xfce_displays_helper_wayland_toggle_internal;
    helper_class->get_display_infos = xfce_displays_helper_wayland_get_display_infos;
    helper_class->channel_apply = xfce_displays_helper_wayland_channel_apply;
}



static void
xfce_displays_helper_wayland_init (XfceDisplaysHelperWayland *helper)
{
    helper->manager = xfce_wlr_output_manager_new (manager_listener, helper);
}



static void
xfce_displays_helper_wayland_finalize (GObject *object)
{
    XfceDisplaysHelperWayland *helper = XFCE_DISPLAYS_HELPER_WAYLAND (object);

    g_object_unref (helper->manager);

    G_OBJECT_CLASS (xfce_displays_helper_wayland_parent_class)->finalize (object);
}



static GPtrArray *
xfce_displays_helper_wayland_get_outputs (XfceDisplaysHelper *helper)
{
    return xfce_wlr_output_manager_get_outputs (XFCE_DISPLAYS_HELPER_WAYLAND (helper)->manager);
}



static void
load_from_xfconf (XfceDisplaysHelperWayland *helper,
                  const gchar *scheme,
                  GHashTable *saved_outputs,
                  XfceWlrOutput *output)
{
    gchar *property = g_strdup_printf (OUTPUT_FMT, scheme, output->name);
    GValue *value = g_hash_table_lookup (saved_outputs, property);
    GList *lp;
    const gchar *str_value;
    gint int_value;
    gdouble double_value;
    g_free (property);

    if (!G_VALUE_HOLDS_STRING (value))
        return;

    /* status */
    property = g_strdup_printf (ACTIVE_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (!G_VALUE_HOLDS_BOOLEAN (value))
        return;

    output->enabled = g_value_get_boolean (value);
    if (!output->enabled)
        return;

    /* resolution */
    property = g_strdup_printf (RESOLUTION_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_STRING (value))
        str_value = g_value_get_string (value);
    else
        str_value = "";

    /* refresh rate */
    property = g_strdup_printf (RRATE_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        double_value = g_value_get_double (value);
    else
        double_value = 0.0;

    /* check mode validity */
    for (lp = output->modes; lp != NULL; lp = lp->next)
    {
        XfceWlrMode *mode = lp->data;
        gchar *resolution = g_strdup_printf ("%dx%d", mode->width, mode->height);
        gboolean match = (g_strcmp0 (resolution, str_value) == 0);
        g_free (resolution);
        if (match && mode->refresh == (int32_t) (double_value * 1000))
            break;
    }

    if (lp == NULL)
    {
        /* unsupported mode, abort for this output */
        g_warning (WARNING_MESSAGE_UNKNOWN_MODE, str_value, double_value, output->name);
        return;
    }
    else
    {
        XfceWlrMode *mode = lp->data;
        output->wl_mode = mode->wl_mode;
    }

    /* rotation */
    property = g_strdup_printf (ROTATION_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_INT (value))
        int_value = g_value_get_int (value);
    else
        int_value = 0;

    /* reflection */
    property = g_strdup_printf (REFLECTION_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_STRING (value))
        str_value = g_value_get_string (value);
    else
        str_value = "0";

    /* convert to a wl_output::transform */
    if (g_strcmp0 (str_value, "X") == 0)
    {
        switch (int_value)
        {
            case 90: output->transform = XFCE_WLR_TRANSFORM_FLIPPED_90; break;
            case 180: output->transform = XFCE_WLR_TRANSFORM_FLIPPED_180; break;
            case 270: output->transform = XFCE_WLR_TRANSFORM_FLIPPED_270; break;
            default: output->transform = XFCE_WLR_TRANSFORM_FLIPPED; break;
        }
    }
    else if (g_strcmp0 (str_value, "Y") == 0)
    {
        switch (int_value)
        {
            case 90: output->transform = XFCE_WLR_TRANSFORM_FLIPPED_270; break;
            case 180: output->transform = XFCE_WLR_TRANSFORM_FLIPPED; break;
            case 270: output->transform = XFCE_WLR_TRANSFORM_FLIPPED_90; break;
            default: output->transform = XFCE_WLR_TRANSFORM_FLIPPED_180; break;
        }
    }
    else if (g_strcmp0 (str_value, "XY") == 0)
    {
        switch (int_value)
        {
            case 90: output->transform = XFCE_WLR_TRANSFORM_270; break;
            case 180: output->transform = XFCE_WLR_TRANSFORM_NORMAL; break;
            case 270: output->transform = XFCE_WLR_TRANSFORM_90; break;
            default: output->transform = XFCE_WLR_TRANSFORM_180; break;
        }
    }
    else
    {
        switch (int_value)
        {
            case 90: output->transform = XFCE_WLR_TRANSFORM_90; break;
            case 180: output->transform = XFCE_WLR_TRANSFORM_180; break;
            case 270: output->transform = XFCE_WLR_TRANSFORM_270; break;
            default: output->transform = XFCE_WLR_TRANSFORM_NORMAL; break;
        }
    }

    /* scaling */
    property = g_strdup_printf (SCALE_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        double_value = g_value_get_double (value);
    else
        double_value = 1.0;

    output->scale = wl_fixed_from_double (double_value);

    /* position, x */
    property = g_strdup_printf (POSX_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_INT (value))
        output->x = g_value_get_int (value);
    else
        output->x = 0;

    /* position, y */
    property = g_strdup_printf (POSY_PROP, scheme, output->name);
    value = g_hash_table_lookup (saved_outputs, property);
    g_free (property);
    if (G_VALUE_HOLDS_INT (value))
        output->y = g_value_get_int (value);
    else
        output->y = 0;
}



static void
apply (gpointer data,
       gpointer user_data)
{
    XfceWlrOutput *output = data;
    struct zwlr_output_configuration_v1 *config = user_data;
    if (output->enabled)
    {
        struct zwlr_output_configuration_head_v1 *config_head = zwlr_output_configuration_v1_enable_head (config, output->wl_head);
        zwlr_output_configuration_head_v1_set_mode (config_head, output->wl_mode);
        zwlr_output_configuration_head_v1_set_position (config_head, output->x, output->y);
        zwlr_output_configuration_head_v1_set_transform (config_head, output->transform);
        zwlr_output_configuration_head_v1_set_scale (config_head, output->scale);
    }
    else
    {
        zwlr_output_configuration_v1_disable_head (config, output->wl_head);
    }
}



static void
apply_all (XfceDisplaysHelperWayland *helper)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (helper->manager);
    struct zwlr_output_manager_v1 *wl_manager = xfce_wlr_output_manager_get_wl_manager (helper->manager);
    struct zwlr_output_configuration_v1 *config = zwlr_output_manager_v1_create_configuration (wl_manager, helper->serial);
    zwlr_output_configuration_v1_add_listener (config, &configuration_listener, helper);
    g_ptr_array_foreach (outputs, apply, config);
    zwlr_output_configuration_v1_apply (config);
}



static void
xfce_displays_helper_wayland_toggle_internal (gpointer *power,
                                              gboolean lid_is_closed,
                                              XfceDisplaysHelper *_helper)
{
    XfceDisplaysHelperWayland *helper = XFCE_DISPLAYS_HELPER_WAYLAND (_helper);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (helper->manager);
    XfceWlrOutput *lvds = NULL;

    if (outputs->len == 1)
    {
        /* if there's only one output left and we pass here, it's supposed to be the
         * internal display or an output we want to reactivate anyway */
        lvds = g_ptr_array_index (outputs, 0);
    }
    else
    {
        for (guint n = 0; n < outputs->len; n++)
        {
            XfceWlrOutput *output = g_ptr_array_index (outputs, n);

            /* try to find the internal display */
            if (display_name_is_laptop_name (output->name))
            {
                lvds = output;
                break;
            }
        }
    }

    if (lvds == NULL
        || (lvds->enabled && !lid_is_closed)
        || (!lvds->enabled && lid_is_closed))
        return;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_TOGGLING_INTERNAL, lvds->name);

    if (lvds->enabled && lid_is_closed)
    {
        /* if active and the lid is closed, deactivate it */
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_DISABLING_INTERNAL, lvds->name);
        lvds->enabled = FALSE;
    }
    else
    {
        /* re-activate it because the user opened the lid */
        XfconfChannel *channel = xfce_displays_helper_get_channel (XFCE_DISPLAYS_HELPER (helper));
        GHashTable *saved_outputs = xfconf_channel_get_properties (channel, "/" DEFAULT_SCHEME_NAME);

        if (saved_outputs != NULL)
        {
            /* reload settings, especially positions */
            for (guint n = 0; n < outputs->len; n++)
                load_from_xfconf (helper, DEFAULT_SCHEME_NAME, saved_outputs, g_ptr_array_index (outputs, n));

            g_hash_table_destroy (saved_outputs);
        }

        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_ENABLING_INTERNAL, lvds->name);
        lvds->enabled = TRUE;
    }

    /* apply settings */
    apply_all (helper);
}



static gchar **
xfce_displays_helper_wayland_get_display_infos (XfceDisplaysHelper *helper)
{
    return xfce_wlr_output_manager_get_display_infos (XFCE_DISPLAYS_HELPER_WAYLAND (helper)->manager);
}



static void
xfce_displays_helper_wayland_channel_apply (XfceDisplaysHelper *_helper,
                                            const gchar *scheme)
{
    XfceDisplaysHelperWayland *helper = XFCE_DISPLAYS_HELPER_WAYLAND (_helper);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (helper->manager);
    XfconfChannel *channel = xfce_displays_helper_get_channel (_helper);
    gchar *property = g_strdup_printf ("/%s", scheme);
    GHashTable *saved_outputs = xfconf_channel_get_properties (channel, property);
    guint n_enabled = 0;
    g_free (property);

    xfconf_channel_set_string (channel, ACTIVE_PROFILE, scheme);
    if (saved_outputs == NULL)
        return;

    for (guint n = 0; n < outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (outputs, n);
        load_from_xfconf (helper, scheme, saved_outputs, output);
        if (output->enabled)
            n_enabled++;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_TOTAL_ACTIVE, n_enabled);

    if (n_enabled == 0)
    {
        g_warning (WARNING_MESSAGE_ALL_DISABLED);
    }
    else
    {
        apply_all (helper);
    }

    g_hash_table_destroy (saved_outputs);
}



static void
manager_listener (XfceWlrOutputManager *manager,
                  struct zwlr_output_manager_v1 *wl_manager,
                  uint32_t serial)
{
    XfceDisplaysHelperWayland *helper = xfce_wlr_output_manager_get_listener_data (manager);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (manager);
    XfconfChannel *channel = xfce_displays_helper_get_channel (XFCE_DISPLAYS_HELPER (helper));
    gboolean update_needed = helper->config_cancelled;

    /* initialization: nothing to update */
    if (helper->serial == 0)
    {
        helper->serial = serial;
        helper->previous_n_outputs = outputs->len;
        return;
    }

    helper->serial = serial;

    /* we are only interested in outputs being added or removed here, unless previous
     * update was cancelled for some reason */
    if (outputs->len == helper->previous_n_outputs && !update_needed)
        return;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_DIFF_N_OUTPUTS,
                    helper->previous_n_outputs, outputs->len);

    /* check if we have different amount of outputs and a matching profile and
       apply it if there's only one */
    if (outputs->len != helper->previous_n_outputs)
    {
        gint mode = xfconf_channel_get_int (channel, AUTO_ENABLE_PROFILES, AUTO_ENABLE_PROFILES_DEFAULT);
        if (mode == AUTO_ENABLE_PROFILES_ALWAYS
            || (mode == AUTO_ENABLE_PROFILES_ON_CONNECT && outputs->len > helper->previous_n_outputs)
            || (mode == AUTO_ENABLE_PROFILES_ON_DISCONNECT && outputs->len < helper->previous_n_outputs))
        {
            gchar *matching_profile = xfce_displays_helper_get_matching_profile (XFCE_DISPLAYS_HELPER (helper));
            if (matching_profile != NULL)
            {
                xfce_displays_helper_wayland_channel_apply (XFCE_DISPLAYS_HELPER (helper), matching_profile);
                helper->previous_n_outputs = outputs->len;
                g_free (matching_profile);
                return;
            }
        }
        xfconf_channel_set_string (channel, ACTIVE_PROFILE, DEFAULT_SCHEME_NAME);
    }

    if (outputs->len < helper->previous_n_outputs)
    {
        gboolean all_disabled = TRUE;
        for (guint n = 0; n < outputs->len; n++)
        {
            XfceWlrOutput *output = g_ptr_array_index (outputs, n);
            if (output->enabled)
            {
                all_disabled = FALSE;
                break;
            }
        }

        if (all_disabled)
        {
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_ALL_DISABLED);
            xfce_displays_helper_wayland_toggle_internal (NULL, FALSE, XFCE_DISPLAYS_HELPER (helper));
            update_needed = FALSE;
        }
    }
    else
    {
        gint action = xfconf_channel_get_int (channel, NOTIFY_PROP, ACTION_ON_NEW_OUTPUT_DEFAULT);
        update_needed = TRUE;

        for (guint n = 0; n < outputs->len; n++)
        {
            XfceWlrOutput *output = g_ptr_array_index (outputs, n);
            if (!output->new)
                continue;

            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_NEW_OUTPUT, output->name);
            output->new = FALSE;

            /* do nothing or dialog */
            if (action <= ACTION_ON_NEW_OUTPUT_SHOW_DIALOG)
            {
                output->enabled = FALSE;
            }
            if (action == ACTION_ON_NEW_OUTPUT_MIRROR)
            {
                output->enabled = TRUE;
                output->x = 0;
                output->y = 0;
            }
            else if (action == ACTION_ON_NEW_OUTPUT_EXTEND)
            {
                output->enabled = TRUE;
            }
        }

        /* start the display dialog according to the user preferences */
        if (action == ACTION_ON_NEW_OUTPUT_SHOW_DIALOG)
        {
            const gchar *cmd = outputs->len <= 2 ? "xfce4-display-settings -m" : "xfce4-display-settings";
            xfce_spawn_command_line (NULL, cmd, FALSE, FALSE, TRUE, NULL);
        }
    }

    if (update_needed)
        apply_all (helper);

    helper->config_cancelled = FALSE;
    helper->previous_n_outputs = outputs->len;
}



static void
configuration_succeeded (void *data,
                         struct zwlr_output_configuration_v1 *config)
{
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Successfully applied configuration");
    zwlr_output_configuration_v1_destroy (config);
}



static void
configuration_failed (void *data,
                      struct zwlr_output_configuration_v1 *config)
{
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Failed to apply configuration");
    g_warning ("Failed to apply configuration");
    zwlr_output_configuration_v1_destroy (config);
}



static void
configuration_cancelled (void *data,
                         struct zwlr_output_configuration_v1 *config)
{
    XfceDisplaysHelperWayland *helper = data;
    helper->config_cancelled = TRUE;
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Configuration application cancelled");
    zwlr_output_configuration_v1_destroy (config);
}
