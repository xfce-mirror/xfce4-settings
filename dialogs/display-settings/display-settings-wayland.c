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

#include "display-settings-wayland.h"
#include "scrollarea.h"

#include "common/edid.h"
#include "common/xfce-wlr-output-manager.h"

#include <gdk/gdkwayland.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_MATH_H
#include <math.h>
#endif



static void
xfce_display_settings_wayland_finalize (GObject *object);
static guint
xfce_display_settings_wayland_get_n_outputs (XfceDisplaySettings *settings);
static guint
xfce_display_settings_wayland_get_n_active_outputs (XfceDisplaySettings *settings);
static gchar **
xfce_display_settings_wayland_get_display_infos (XfceDisplaySettings *settings);
static GdkMonitor *
xfce_display_settings_wayland_get_monitor (XfceDisplaySettings *settings,
                                           guint output_id);
static const gchar *
xfce_display_settings_wayland_get_friendly_name (XfceDisplaySettings *settings,
                                                 guint output_id);
static void
xfce_display_settings_wayland_get_geometry (XfceDisplaySettings *settings,
                                            guint output_id,
                                            GdkRectangle *geometry);
static RotationFlags
xfce_display_settings_wayland_get_rotation (XfceDisplaySettings *settings,
                                            guint output_id);
static void
xfce_display_settings_wayland_set_rotation (XfceDisplaySettings *settings,
                                            guint output_id,
                                            RotationFlags rotation);
static RotationFlags
xfce_display_settings_wayland_get_rotations (XfceDisplaySettings *settings,
                                             guint output_id);
static gdouble
xfce_display_settings_wayland_get_scale (XfceDisplaySettings *settings,
                                         guint output_id);
static void
xfce_display_settings_wayland_set_scale (XfceDisplaySettings *settings,
                                         guint output_id,
                                         gdouble scale);
static void
xfce_display_settings_wayland_set_mode (XfceDisplaySettings *settings,
                                        guint output_id,
                                        guint mode_id);
static void
xfce_display_settings_wayland_update_output_mode (XfceDisplaySettings *settings,
                                                  XfceOutput *output,
                                                  guint mode_id);
static void
xfce_display_settings_wayland_set_position (XfceDisplaySettings *settings,
                                            guint output_id,
                                            gint x,
                                            gint y);
static XfceOutput *
xfce_display_settings_wayland_get_output (XfceDisplaySettings *settings,
                                          guint output_id);
static gboolean
xfce_display_settings_wayland_is_active (XfceDisplaySettings *settings,
                                         guint output_id);
static void
xfce_display_settings_wayland_set_active (XfceDisplaySettings *settings,
                                          guint output_id,
                                          gboolean active);
static void
xfce_display_settings_wayland_update_output_active (XfceDisplaySettings *settings,
                                                    XfceOutput *output,
                                                    gboolean active);
static gboolean
xfce_display_settings_wayland_is_primary (XfceDisplaySettings *settings,
                                          guint output_id);
static void
xfce_display_settings_wayland_set_primary (XfceDisplaySettings *settings,
                                           guint output_id,
                                           gboolean primary);
static gboolean
xfce_display_settings_wayland_is_clonable (XfceDisplaySettings *settings);
static void
xfce_display_settings_wayland_save (XfceDisplaySettings *settings,
                                    const gchar *scheme);
static void
xfce_display_settings_wayland_mirror (XfceDisplaySettings *settings);
static void
xfce_display_settings_wayland_unmirror (XfceDisplaySettings *settings);
static void
xfce_display_settings_wayland_update_output_mirror (XfceDisplaySettings *settings,
                                                    XfceOutput *output);
static void
xfce_display_settings_wayland_extend (XfceDisplaySettings *settings,
                                      guint output_id_1,
                                      guint output_id_2,
                                      ExtendedMode mode);



struct _XfceDisplaySettingsWayland
{
    XfceDisplaySettings __parent__;

    XfceWlrOutputManager *manager;
    uint32_t serial;
    XfceWlrMode *dummy_mode;
};



G_DEFINE_TYPE (XfceDisplaySettingsWayland, xfce_display_settings_wayland, XFCE_TYPE_DISPLAY_SETTINGS);



static void
xfce_display_settings_wayland_class_init (XfceDisplaySettingsWaylandClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    XfceDisplaySettingsClass *settings_class = XFCE_DISPLAY_SETTINGS_CLASS (klass);

    gobject_class->finalize = xfce_display_settings_wayland_finalize;

    settings_class->get_n_outputs = xfce_display_settings_wayland_get_n_outputs;
    settings_class->get_n_active_outputs = xfce_display_settings_wayland_get_n_active_outputs;
    settings_class->get_display_infos = xfce_display_settings_wayland_get_display_infos;
    settings_class->get_monitor = xfce_display_settings_wayland_get_monitor;
    settings_class->get_friendly_name = xfce_display_settings_wayland_get_friendly_name;
    settings_class->get_geometry = xfce_display_settings_wayland_get_geometry;
    settings_class->get_rotation = xfce_display_settings_wayland_get_rotation;
    settings_class->set_rotation = xfce_display_settings_wayland_set_rotation;
    settings_class->get_rotations = xfce_display_settings_wayland_get_rotations;
    settings_class->get_scale = xfce_display_settings_wayland_get_scale;
    settings_class->set_scale = xfce_display_settings_wayland_set_scale;
    settings_class->set_mode = xfce_display_settings_wayland_set_mode;
    settings_class->update_output_mode = xfce_display_settings_wayland_update_output_mode;
    settings_class->set_position = xfce_display_settings_wayland_set_position;
    settings_class->get_output = xfce_display_settings_wayland_get_output;
    settings_class->is_active = xfce_display_settings_wayland_is_active;
    settings_class->set_active = xfce_display_settings_wayland_set_active;
    settings_class->update_output_active = xfce_display_settings_wayland_update_output_active;
    settings_class->is_primary = xfce_display_settings_wayland_is_primary;
    settings_class->set_primary = xfce_display_settings_wayland_set_primary;
    settings_class->is_clonable = xfce_display_settings_wayland_is_clonable;
    settings_class->save = xfce_display_settings_wayland_save;
    settings_class->mirror = xfce_display_settings_wayland_mirror;
    settings_class->unmirror = xfce_display_settings_wayland_unmirror;
    settings_class->update_output_mirror = xfce_display_settings_wayland_update_output_mirror;
    settings_class->extend = xfce_display_settings_wayland_extend;
}



static void
xfce_display_settings_wayland_init (XfceDisplaySettingsWayland *settings)
{
    /* use a dummy mode for drawing if an output has no modes: this also avoids having
     * sanity checks all over the code because of this */
    settings->dummy_mode = g_new0 (XfceWlrMode, 1);
    settings->dummy_mode->width = 640;
    settings->dummy_mode->height = 480;
}



static void
xfce_display_settings_wayland_finalize (GObject *object)
{
    XfceDisplaySettingsWayland *settings = XFCE_DISPLAY_SETTINGS_WAYLAND (object);

    g_object_unref (settings->manager);
    g_free (settings->dummy_mode);

    G_OBJECT_CLASS (xfce_display_settings_wayland_parent_class)->finalize (object);
}



static guint
xfce_display_settings_wayland_get_n_outputs (XfceDisplaySettings *settings)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    return outputs->len;
}



static guint
xfce_display_settings_wayland_get_n_active_outputs (XfceDisplaySettings *settings)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    guint count = 0;

    for (guint n = 0; n < outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (outputs, n);
        if (output->enabled)
            count++;
    }

    return count;
}



static gchar **
xfce_display_settings_wayland_get_display_infos (XfceDisplaySettings *settings)
{
    return xfce_wlr_output_manager_get_display_infos (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
}



static XfceWlrMode **
get_clonable_modes (GPtrArray *outputs)
{
    XfceWlrOutput *output = g_ptr_array_index (outputs, 0);
    XfceWlrMode *modes[outputs->len];
    modes[outputs->len - 1] = NULL;

    /* walk supported modes from the first output */
    for (GList *lp = output->modes; lp != NULL; lp = lp->next)
    {
        modes[0] = lp->data;

        /* walk other outputs */
        for (guint n = 1; n < outputs->len; n++)
        {
            output = g_ptr_array_index (outputs, n);
            modes[n] = NULL;

            /* walk supported modes from this output */
            for (GList *lq = output->modes; lq != NULL; lq = lq->next)
            {
                XfceWlrMode *mode = lq->data;
                if (mode->width == modes[0]->width && mode->height == modes[0]->height)
                {
                    modes[n] = mode;
                    break;
                }
            }

            /* modes[0] is not supported by nth output, forget it */
            if (modes[n] == NULL)
                break;
        }

        /* modes[0] is supported by all outputs: let's go with it */
        if (modes[outputs->len - 1] != NULL)
            break;
    }

    if (modes[outputs->len - 1] != NULL)
        return g_memdup2 (modes, sizeof (XfceWlrMode *) * outputs->len);

    return NULL;
}



static void
get_geometry_mm (XfceWlrOutput *output,
                 GdkRectangle *geometry)
{
    switch (output->transform)
    {
        case XFCE_WLR_TRANSFORM_90:
        case XFCE_WLR_TRANSFORM_270:
        case XFCE_WLR_TRANSFORM_FLIPPED_90:
        case XFCE_WLR_TRANSFORM_FLIPPED_270:
            geometry->width = output->height;
            geometry->height = output->width;
            break;

        default:
            geometry->width = output->width;
            geometry->height = output->height;
            break;
    }
}



static GdkMonitor *
xfce_display_settings_wayland_get_monitor (XfceDisplaySettings *settings,
                                           guint output_id)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    GdkDisplay *display = gdk_display_get_default ();
    gint n_monitors = gdk_display_get_n_monitors (display);
    GdkRectangle geom;

    /* try to find the right monitor based on info that the protocol ensures is the same
     * as wl-output, i.e. what GTK uses */
    get_geometry_mm (output, &geom);
    for (gint n = 0; n < n_monitors; n++)
    {
        GdkMonitor *monitor = gdk_display_get_monitor (display, n);
        if (g_strcmp0 (gdk_monitor_get_model (monitor), output->model) == 0
            && g_strcmp0 (gdk_monitor_get_manufacturer (monitor), output->manufacturer) == 0
            && gdk_monitor_get_width_mm (monitor) == geom.width
            && gdk_monitor_get_height_mm (monitor) == geom.height)
        {
            return monitor;
        }
    }

    return NULL;
}



static const gchar *
xfce_display_settings_wayland_get_friendly_name (XfceDisplaySettings *settings,
                                                 guint output_id)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    const gchar *fallback;

    /* special case, a laptop */
    if (display_name_is_laptop_name (output->name))
        return _("Laptop");

    /* normal case */
    if (output->description != NULL)
        return output->description;

    /* fallback */
    fallback = display_name_get_fallback (output->name);
    if (fallback != NULL)
        return fallback;

    return output->name;
}



static XfceWlrMode *
get_preferred_mode (XfceDisplaySettingsWayland *settings,
                    XfceWlrOutput *output)
{
    for (GList *lp = output->modes; lp != NULL; lp = lp->next)
    {
        XfceWlrMode *mode = lp->data;
        if (mode->preferred)
            return mode;
    }

    if (output->modes != NULL)
        return output->modes->data;

    return settings->dummy_mode;
}



static XfceWlrMode *
get_current_mode (XfceDisplaySettingsWayland *settings,
                  XfceWlrOutput *output)
{
    if (!output->enabled)
        return get_preferred_mode (settings, output);

    for (GList *lp = output->modes; lp != NULL; lp = lp->next)
    {
        XfceWlrMode *mode = lp->data;
        if (mode->wl_mode == output->wl_mode)
            return mode;
    }

    g_warn_if_reached ();
    return settings->dummy_mode;
}



static XfceWlrMode *
get_nth_mode (XfceDisplaySettingsWayland *settings,
              XfceWlrOutput *output,
              guint mode_id)
{
    if (mode_id == -1U)
        return get_preferred_mode (settings, output);

    GList *lp = g_list_nth (output->modes, mode_id);
    if (lp != NULL)
        return lp->data;

    return settings->dummy_mode;
}



static void
xfce_display_settings_wayland_get_geometry (XfceDisplaySettings *settings,
                                            guint output_id,
                                            GdkRectangle *geometry)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    XfceWlrMode *mode = get_current_mode (wsettings, output);
    gdouble scale = wl_fixed_to_double (output->scale);

    geometry->x = output->x;
    geometry->y = output->y;
    switch (output->transform)
    {
        case XFCE_WLR_TRANSFORM_90:
        case XFCE_WLR_TRANSFORM_270:
        case XFCE_WLR_TRANSFORM_FLIPPED_90:
        case XFCE_WLR_TRANSFORM_FLIPPED_270:
            geometry->width = round (mode->height / scale);
            geometry->height = round (mode->width / scale);
            break;

        default:
            geometry->width = round (mode->width / scale);
            geometry->height = round (mode->height / scale);
            break;
    }
}



static RotationFlags
convert_rotation_from_transform (int32_t transform)
{
    switch (transform)
    {
        case XFCE_WLR_TRANSFORM_90: return ROTATION_FLAGS_90;
        case XFCE_WLR_TRANSFORM_180: return ROTATION_FLAGS_180;
        case XFCE_WLR_TRANSFORM_270: return ROTATION_FLAGS_270;
        case XFCE_WLR_TRANSFORM_FLIPPED: return ROTATION_FLAGS_REFLECT_X;
        case XFCE_WLR_TRANSFORM_FLIPPED_90: return ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_90;
        case XFCE_WLR_TRANSFORM_FLIPPED_180: return ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_180;
        case XFCE_WLR_TRANSFORM_FLIPPED_270: return ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_270;
        default: return ROTATION_FLAGS_0;
    }
}



static int32_t
convert_rotation_to_transform (RotationFlags flags)
{
    switch (flags & REFLECTION_MASK)
    {
        case ROTATION_FLAGS_REFLECT_X:
            switch (flags & ROTATION_MASK)
            {
                case ROTATION_FLAGS_90: return XFCE_WLR_TRANSFORM_FLIPPED_90;
                case ROTATION_FLAGS_180: return XFCE_WLR_TRANSFORM_FLIPPED_180;
                case ROTATION_FLAGS_270: return XFCE_WLR_TRANSFORM_FLIPPED_270;
                default: return XFCE_WLR_TRANSFORM_FLIPPED;
            }
            break;
        case ROTATION_FLAGS_REFLECT_Y:
            switch (flags & ROTATION_MASK)
            {
                case ROTATION_FLAGS_90: return XFCE_WLR_TRANSFORM_FLIPPED_270;
                case ROTATION_FLAGS_180: return XFCE_WLR_TRANSFORM_FLIPPED;
                case ROTATION_FLAGS_270: return XFCE_WLR_TRANSFORM_FLIPPED_90;
                default: return XFCE_WLR_TRANSFORM_FLIPPED_180;
            }
            break;
        case ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_REFLECT_Y:
            switch (flags & ROTATION_MASK)
            {
                case ROTATION_FLAGS_90: return XFCE_WLR_TRANSFORM_270;
                case ROTATION_FLAGS_180: return XFCE_WLR_TRANSFORM_NORMAL;
                case ROTATION_FLAGS_270: return XFCE_WLR_TRANSFORM_90;
                default: return XFCE_WLR_TRANSFORM_180;
            }
            break;
        default:
            switch (flags & ROTATION_MASK)
            {
                case ROTATION_FLAGS_90: return XFCE_WLR_TRANSFORM_90;
                case ROTATION_FLAGS_180: return XFCE_WLR_TRANSFORM_180;
                case ROTATION_FLAGS_270: return XFCE_WLR_TRANSFORM_270;
                default: return XFCE_WLR_TRANSFORM_NORMAL;
            }
    }
}



static RotationFlags
xfce_display_settings_wayland_get_rotation (XfceDisplaySettings *settings,
                                            guint output_id)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    return convert_rotation_from_transform (output->transform);
}



static void
xfce_display_settings_wayland_set_rotation (XfceDisplaySettings *settings,
                                            guint output_id,
                                            RotationFlags rotation)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    output->transform = convert_rotation_to_transform (rotation);
}



static RotationFlags
xfce_display_settings_wayland_get_rotations (XfceDisplaySettings *settings,
                                             guint output_id)
{
    return ROTATION_FLAGS_ALL;
}



static gdouble
xfce_display_settings_wayland_get_scale (XfceDisplaySettings *settings,
                                         guint output_id)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    return wl_fixed_to_double (output->scale);
}



static void
xfce_display_settings_wayland_set_scale (XfceDisplaySettings *settings,
                                         guint output_id,
                                         gdouble scale)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    output->scale = wl_fixed_from_double (scale);
}



static void
xfce_display_settings_wayland_set_mode (XfceDisplaySettings *settings,
                                        guint output_id,
                                        guint mode_id)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    XfceWlrMode *mode = get_nth_mode (wsettings, output, mode_id);
    output->wl_mode = mode->wl_mode;
}



static void
output_set_mode_and_tranformation (XfceOutput *output,
                                   XfceWlrMode *mode,
                                   guint mode_id,
                                   XfceWlrOutput *xfwl_output)
{
    if (output->active)
    {
        output->mode->id = mode_id;
        output->mode->width = mode->width;
        output->mode->height = mode->height;
        output->mode->rate = (gdouble) mode->refresh / 1000;
        output->rotation = convert_rotation_from_transform (xfwl_output->transform);
        output->scale = wl_fixed_to_double (xfwl_output->scale);
    }
    else
    {
        output->mode->id = -1;
        output->mode->width = output->pref_width;
        output->mode->height = output->pref_height;
        output->mode->rate = 0;
        output->rotation = ROTATION_FLAGS_0;
        output->scale = 1.0;
    }
}



static void
xfce_display_settings_wayland_update_output_mode (XfceDisplaySettings *settings,
                                                  XfceOutput *output,
                                                  guint mode_id)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *xfwl_output = g_ptr_array_index (outputs, output->id);
    XfceWlrMode *mode = get_nth_mode (wsettings, xfwl_output, mode_id);
    output_set_mode_and_tranformation (output, mode, mode_id, xfwl_output);
}



static void
xfce_display_settings_wayland_set_position (XfceDisplaySettings *settings,
                                            guint output_id,
                                            gint x,
                                            gint y)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    output->x = x;
    output->y = y;
}



static XfceOutput *
xfce_display_settings_wayland_get_output (XfceDisplaySettings *settings,
                                          guint output_id)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *xfwl_output = g_ptr_array_index (outputs, output_id);
    XfceOutput *output = g_new0 (XfceOutput, 1);
    XfceWlrMode *xfwl_mode, *preferred;
    guint n = 0;

    output->id = output_id;
    output->friendly_name = xfce_display_settings_wayland_get_friendly_name (settings, output_id);

    output->x = xfwl_output->x;
    output->y = xfwl_output->y;
    output->active = xfwl_output->enabled;

    preferred = get_preferred_mode (wsettings, xfwl_output);
    output->pref_width = preferred->width;
    output->pref_height = preferred->height;

    xfwl_mode = get_current_mode (wsettings, xfwl_output);
    output->mode = g_new0 (XfceMode, 1);
    output_set_mode_and_tranformation (output, xfwl_mode, g_list_index (xfwl_output->modes, xfwl_mode), xfwl_output);

    output->n_modes = g_list_length (xfwl_output->modes);
    output->modes = g_new (XfceMode *, output->n_modes);
    for (GList *lp = xfwl_output->modes; lp != NULL; lp = lp->next, n++)
    {
        XfceMode *mode = g_new (XfceMode, 1);
        xfwl_mode = lp->data;
        mode->id = n;
        mode->width = xfwl_mode->width;
        mode->height = xfwl_mode->height;
        mode->rate = (gdouble) xfwl_mode->refresh / 1000;
        output->modes[n] = mode;
    }

    return output;
}



static gboolean
xfce_display_settings_wayland_is_active (XfceDisplaySettings *settings,
                                         guint output_id)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    return output->enabled;
}



static void
xfce_display_settings_wayland_set_active (XfceDisplaySettings *settings,
                                          guint output_id,
                                          gboolean active)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *output = g_ptr_array_index (outputs, output_id);
    output->enabled = active;
}



static void
xfce_display_settings_wayland_update_output_active (XfceDisplaySettings *settings,
                                                    XfceOutput *output,
                                                    gboolean active)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *xfwl_output = g_ptr_array_index (outputs, output->id);
    XfceWlrMode *mode = get_current_mode (wsettings, xfwl_output);
    output_set_mode_and_tranformation (output, mode, g_list_index (xfwl_output->modes, mode), xfwl_output);
}



static gboolean
xfce_display_settings_wayland_is_primary (XfceDisplaySettings *settings,
                                          guint output_id)
{
    return FALSE;
}



static void
xfce_display_settings_wayland_set_primary (XfceDisplaySettings *settings,
                                           guint output_id,
                                           gboolean primary)
{
}



static gboolean
xfce_display_settings_wayland_is_clonable (XfceDisplaySettings *settings)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrMode **clonable_modes = get_clonable_modes (outputs);
    gboolean clonable = clonable_modes != NULL;
    g_free (clonable_modes);
    return clonable;
}



static void
output_save (XfceWlrOutput *output,
             XfceWlrMode *mode,
             const gchar *friendly_name,
             XfconfChannel *channel,
             const gchar *scheme)
{
    RotationFlags rotation;
    gchar *property, *str_value;
    gdouble scale;
    gint degrees;

    /* save the device name */
    property = g_strdup_printf ("/%s/%s", scheme, output->name);
    xfconf_channel_set_string (channel, property, friendly_name);
    g_free (property);

    property = g_strdup_printf ("/%s/%s/EDID", scheme, output->name);
    xfconf_channel_set_string (channel, property, output->edid);
    g_free (property);

    /* stop here if output is disabled */
    property = g_strdup_printf ("/%s/%s/Active", scheme, output->name);
    xfconf_channel_set_bool (channel, property, output->enabled);
    g_free (property);
    if (!output->enabled)
        return;

    /* save the resolution */
    str_value = g_strdup_printf ("%dx%d", mode->width, mode->height);
    property = g_strdup_printf ("/%s/%s/Resolution", scheme, output->name);
    xfconf_channel_set_string (channel, property, str_value);
    g_free (property);
    g_free (str_value);

    /* save the refresh rate */
    property = g_strdup_printf ("/%s/%s/RefreshRate", scheme, output->name);
    xfconf_channel_set_double (channel, property, (gdouble) mode->refresh / 1000);
    g_free (property);

    /* convert the rotation into degrees */
    rotation = convert_rotation_from_transform (output->transform);
    switch (rotation & ROTATION_MASK)
    {
        case ROTATION_FLAGS_90: degrees = 90; break;
        case ROTATION_FLAGS_180: degrees = 180; break;
        case ROTATION_FLAGS_270: degrees = 270; break;
        default: degrees = 0; break;
    }

    /* save the rotation in degrees */
    property = g_strdup_printf ("/%s/%s/Rotation", scheme, output->name);
    xfconf_channel_set_int (channel, property, degrees);
    g_free (property);

    /* convert the reflection into a string */
    switch (rotation & REFLECTION_MASK)
    {
        case ROTATION_FLAGS_REFLECT_X: str_value = "X"; break;
        case ROTATION_FLAGS_REFLECT_Y: str_value = "Y"; break;
        case ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_REFLECT_Y: str_value = "XY"; break;
        default: str_value = "0"; break;
    }

    /* save the reflection string */
    property = g_strdup_printf ("/%s/%s/Reflection", scheme, output->name);
    xfconf_channel_set_string (channel, property, str_value);
    g_free (property);

    /* save the scale */
    scale = wl_fixed_to_double (output->scale);
    property = g_strdup_printf ("/%s/%s/Scale", scheme, output->name);
    xfconf_channel_set_double (channel, property, scale);
    g_free (property);

    /* save the position */
    property = g_strdup_printf ("/%s/%s/Position/X", scheme, output->name);
    xfconf_channel_set_int (channel, property, MAX (output->x, 0));
    g_free (property);
    property = g_strdup_printf ("/%s/%s/Position/Y", scheme, output->name);
    xfconf_channel_set_int (channel, property, MAX (output->y, 0));
    g_free (property);
}



static void
xfce_display_settings_wayland_save (XfceDisplaySettings *settings,
                                    const gchar *scheme)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfconfChannel *channel = xfce_display_settings_get_channel (settings);
    for (guint n = 0; n < outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (outputs, n);
        XfceWlrMode *mode = get_current_mode (wsettings, output);
        const gchar *friendly_name = xfce_display_settings_wayland_get_friendly_name (settings, n);
        output_save (output, mode, friendly_name, channel, scheme);
    }
}



static void
xfce_display_settings_wayland_mirror (XfceDisplaySettings *settings)
{
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (XFCE_DISPLAY_SETTINGS_WAYLAND (settings)->manager);
    XfceWlrMode **clonable_modes = get_clonable_modes (outputs);
    if (clonable_modes == NULL)
    {
        g_warn_if_reached ();
        return;
    }

    for (guint n = 0; n < outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (outputs, n);
        output->enabled = TRUE;
        output->wl_mode = clonable_modes[n]->wl_mode;
        output->transform = ROTATION_FLAGS_0;
        output->scale = wl_fixed_from_double (1.0);
        output->x = 0;
        output->y = 0;
    }

    g_free (clonable_modes);
}



static void
xfce_display_settings_wayland_unmirror (XfceDisplaySettings *settings)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    guint x = 0;

    for (guint n = 0; n < outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (outputs, n);
        XfceWlrMode *mode = get_preferred_mode (wsettings, output);
        output->wl_mode = mode->wl_mode;
        output->transform = ROTATION_FLAGS_0;
        output->scale = wl_fixed_from_double (1.0);
        output->x = x;
        output->y = 0;
        x += mode->width;
    }
}



static void
xfce_display_settings_wayland_update_output_mirror (XfceDisplaySettings *settings,
                                                    XfceOutput *output)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *xfwl_output = g_ptr_array_index (outputs, output->id);
    XfceWlrMode *mode = get_current_mode (wsettings, xfwl_output);
    output->x = xfwl_output->x;
    output->y = xfwl_output->y;
    output->active = xfwl_output->enabled;
    output_set_mode_and_tranformation (output, mode, g_list_index (xfwl_output->modes, mode), xfwl_output);
}



static void
xfce_display_settings_wayland_extend (XfceDisplaySettings *settings,
                                      guint output_id_1,
                                      guint output_id_2,
                                      ExtendedMode mode)
{
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (wsettings->manager);
    XfceWlrOutput *output_1 = g_ptr_array_index (outputs, output_id_1);
    XfceWlrOutput *output_2 = g_ptr_array_index (outputs, output_id_2);
    GdkRectangle geom_1, geom_2;

    xfce_display_settings_wayland_get_geometry (settings, output_id_1, &geom_1);
    xfce_display_settings_wayland_get_geometry (settings, output_id_2, &geom_2);
    output_1->x = 0;
    output_1->y = 0;
    output_2->x = 0;
    output_2->y = 0;
    switch (mode)
    {
        case EXTENDED_MODE_RIGHT:
            output_2->x = geom_1.width;
            break;
        case EXTENDED_MODE_LEFT:
            output_1->x = geom_2.width;
            break;
        case EXTENDED_MODE_UP:
            output_1->y = geom_2.height;
            break;
        case EXTENDED_MODE_DOWN:
            output_2->y = geom_1.height;
            break;
        default:
            break;
    }
}



static void
manager_listener (XfceWlrOutputManager *manager,
                  struct zwlr_output_manager_v1 *wl_manager,
                  uint32_t serial)
{
    XfceDisplaySettings *settings = xfce_wlr_output_manager_get_listener_data (manager);
    XfceDisplaySettingsWayland *wsettings = XFCE_DISPLAY_SETTINGS_WAYLAND (settings);
    GPtrArray *outputs = xfce_wlr_output_manager_get_outputs (manager);

    /* initialization: nothing to update */
    if (wsettings->serial == 0)
    {
        wsettings->serial = serial;
        return;
    }

    wsettings->serial = serial;

    /* disable outputs that have no modes: this really shouldn't happen, but the protocol
     * allows it and we can't do anything with it */
    for (guint n = 0; n < outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (outputs, n);
        if (output->modes == NULL)
        {
            output->enabled = FALSE;
            g_warning ("Output '%s' has no modes, disabling it", output->name);
        }
    }

    xfce_display_settings_reload (settings);
    g_signal_emit_by_name (G_OBJECT (settings), "outputs-changed");
}



XfceDisplaySettings *
xfce_display_settings_wayland_new (GError **error)
{
    XfceDisplaySettingsWayland *settings = g_object_new (XFCE_TYPE_DISPLAY_SETTINGS_WAYLAND, NULL);
    XfceWlrOutputManager *manager = xfce_wlr_output_manager_new (manager_listener, settings);
    settings->manager = manager;

    if (xfce_wlr_output_manager_get_outputs (manager) == NULL)
    {
        g_object_unref (settings);
        g_set_error (error, 0, 0, _("Your compositor does not seem to support the wlr-output-management protocol"));
        return NULL;
    }

    return XFCE_DISPLAY_SETTINGS (settings);
}
