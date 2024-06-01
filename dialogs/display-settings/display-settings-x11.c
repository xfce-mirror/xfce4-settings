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

#include "display-settings-x11.h"
#include "scrollarea.h"

#include "common/xfce-randr.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h>



static void
xfce_display_settings_x11_finalize (GObject *object);
static guint
xfce_display_settings_x11_get_n_outputs (XfceDisplaySettings *settings);
static guint
xfce_display_settings_x11_get_n_active_outputs (XfceDisplaySettings *settings);
static gchar **
xfce_display_settings_x11_get_display_infos (XfceDisplaySettings *settings);
static GdkMonitor *
xfce_display_settings_x11_get_monitor (XfceDisplaySettings *settings,
                                       guint output_id);
static const gchar *
xfce_display_settings_x11_get_friendly_name (XfceDisplaySettings *settings,
                                             guint output_id);
static void
xfce_display_settings_x11_get_geometry (XfceDisplaySettings *settings,
                                        guint output_id,
                                        GdkRectangle *geometry);
static RotationFlags
xfce_display_settings_x11_get_rotation (XfceDisplaySettings *settings,
                                        guint output_id);
static void
xfce_display_settings_x11_set_rotation (XfceDisplaySettings *settings,
                                        guint output_id,
                                        RotationFlags rotation);
static RotationFlags
xfce_display_settings_x11_get_rotations (XfceDisplaySettings *settings,
                                         guint output_id);
static gdouble
xfce_display_settings_x11_get_scale (XfceDisplaySettings *settings,
                                     guint output_id);
static void
xfce_display_settings_x11_set_scale (XfceDisplaySettings *settings,
                                     guint output_id,
                                     gdouble scale);
static void
xfce_display_settings_x11_set_mode (XfceDisplaySettings *settings,
                                    guint output_id,
                                    guint mode_id);
static void
xfce_display_settings_x11_update_output_mode (XfceDisplaySettings *settings,
                                              XfceOutput *output,
                                              guint mode_id);
static void
xfce_display_settings_x11_set_position (XfceDisplaySettings *settings,
                                        guint output_id,
                                        gint x,
                                        gint y);
static XfceOutput *
xfce_display_settings_x11_get_output (XfceDisplaySettings *settings,
                                      guint output_id);
static gboolean
xfce_display_settings_x11_is_active (XfceDisplaySettings *settings,
                                     guint output_id);
static void
xfce_display_settings_x11_set_active (XfceDisplaySettings *settings,
                                      guint output_id,
                                      gboolean active);
static void
xfce_display_settings_x11_update_output_active (XfceDisplaySettings *settings,
                                                XfceOutput *output,
                                                gboolean active);
static gboolean
xfce_display_settings_x11_is_primary (XfceDisplaySettings *settings,
                                      guint output_id);
static void
xfce_display_settings_x11_set_primary (XfceDisplaySettings *settings,
                                       guint output_id,
                                       gboolean primary);
static gboolean
xfce_display_settings_x11_is_clonable (XfceDisplaySettings *settings);
static void
xfce_display_settings_x11_save (XfceDisplaySettings *settings,
                                const gchar *scheme);
static void
xfce_display_settings_x11_mirror (XfceDisplaySettings *settings);
static void
xfce_display_settings_x11_unmirror (XfceDisplaySettings *settings);
static void
xfce_display_settings_x11_update_output_mirror (XfceDisplaySettings *settings,
                                                XfceOutput *output);
static void
xfce_display_settings_x11_extend (XfceDisplaySettings *settings,
                                  guint output_id_1,
                                  guint output_id_2,
                                  ExtendedMode mode);



struct _XfceDisplaySettingsX11
{
    XfceDisplaySettings __parent__;

    XfceRandr *randr;
    gint event_base;
};



G_DEFINE_TYPE (XfceDisplaySettingsX11, xfce_display_settings_x11, XFCE_TYPE_DISPLAY_SETTINGS);



static void
xfce_display_settings_x11_class_init (XfceDisplaySettingsX11Class *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    XfceDisplaySettingsClass *settings_class = XFCE_DISPLAY_SETTINGS_CLASS (klass);

    gobject_class->finalize = xfce_display_settings_x11_finalize;

    settings_class->get_n_outputs = xfce_display_settings_x11_get_n_outputs;
    settings_class->get_n_active_outputs = xfce_display_settings_x11_get_n_active_outputs;
    settings_class->get_display_infos = xfce_display_settings_x11_get_display_infos;
    settings_class->get_monitor = xfce_display_settings_x11_get_monitor;
    settings_class->get_friendly_name = xfce_display_settings_x11_get_friendly_name;
    settings_class->get_geometry = xfce_display_settings_x11_get_geometry;
    settings_class->get_rotation = xfce_display_settings_x11_get_rotation;
    settings_class->set_rotation = xfce_display_settings_x11_set_rotation;
    settings_class->get_rotations = xfce_display_settings_x11_get_rotations;
    settings_class->get_scale = xfce_display_settings_x11_get_scale;
    settings_class->set_scale = xfce_display_settings_x11_set_scale;
    settings_class->set_mode = xfce_display_settings_x11_set_mode;
    settings_class->update_output_mode = xfce_display_settings_x11_update_output_mode;
    settings_class->set_position = xfce_display_settings_x11_set_position;
    settings_class->get_output = xfce_display_settings_x11_get_output;
    settings_class->is_active = xfce_display_settings_x11_is_active;
    settings_class->set_active = xfce_display_settings_x11_set_active;
    settings_class->update_output_active = xfce_display_settings_x11_update_output_active;
    settings_class->is_primary = xfce_display_settings_x11_is_primary;
    settings_class->set_primary = xfce_display_settings_x11_set_primary;
    settings_class->is_clonable = xfce_display_settings_x11_is_clonable;
    settings_class->save = xfce_display_settings_x11_save;
    settings_class->mirror = xfce_display_settings_x11_mirror;
    settings_class->unmirror = xfce_display_settings_x11_unmirror;
    settings_class->update_output_mirror = xfce_display_settings_x11_update_output_mirror;
    settings_class->extend = xfce_display_settings_x11_extend;
}



static void
xfce_display_settings_x11_init (XfceDisplaySettingsX11 *settings)
{
    /* to prevent the settings dialog to be saved in the session */
    gdk_x11_set_sm_client_id ("FAKE ID");
}



static GdkFilterReturn
screen_on_event (GdkXEvent *gdk_xevent,
                 GdkEvent *event,
                 gpointer data)
{
    XfceDisplaySettingsX11 *xsettings = data;
    XfceDisplaySettings *settings = data;
    XEvent *xevent = gdk_xevent;

    if (xevent == NULL)
        return GDK_FILTER_CONTINUE;

    if (xevent->type - xsettings->event_base != RRScreenChangeNotify)
        return GDK_FILTER_CONTINUE;

    xfce_randr_reload (xsettings->randr);
    xfce_display_settings_reload (settings);
    g_signal_emit_by_name (G_OBJECT (settings), "outputs-changed");

    return GDK_FILTER_CONTINUE;
}



static void
xfce_display_settings_x11_finalize (GObject *object)
{
    XfceDisplaySettingsX11 *settings = XFCE_DISPLAY_SETTINGS_X11 (object);

    xfce_randr_free (settings->randr);
    gdk_window_remove_filter (gdk_get_default_root_window (), screen_on_event, settings);

    G_OBJECT_CLASS (xfce_display_settings_x11_parent_class)->finalize (object);
}



static guint
xfce_display_settings_x11_get_n_outputs (XfceDisplaySettings *settings)
{
    return XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->noutput;
}



static guint
xfce_display_settings_x11_get_n_active_outputs (XfceDisplaySettings *settings)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    guint count = 0;

    for (guint n = 0; n < randr->noutput; n++)
    {
        if (randr->mode[n] != None)
            count++;
    }

    return count;
}



static gchar **
xfce_display_settings_x11_get_display_infos (XfceDisplaySettings *settings)
{
    return xfce_randr_get_display_infos (XFCE_DISPLAY_SETTINGS_X11 (settings)->randr);
}



static GdkMonitor *
xfce_display_settings_x11_get_monitor (XfceDisplaySettings *settings,
                                       guint output_id)
{
    return NULL;
}



static const gchar *
xfce_display_settings_x11_get_friendly_name (XfceDisplaySettings *settings,
                                             guint output_id)
{
    return XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->friendly_name[output_id];
}



static void
xfce_display_settings_x11_get_geometry (XfceDisplaySettings *settings,
                                        guint output_id,
                                        GdkRectangle *geometry)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    const XfceRRMode *mode = xfce_randr_find_mode_by_id (randr, output_id, randr->mode[output_id]);
    if (mode == NULL)
        mode = xfce_randr_find_mode_by_id (randr, output_id, xfce_randr_preferred_mode (randr, output_id));

    if (!xfce_randr_get_positions (randr, output_id, &geometry->x, &geometry->y))
    {
        geometry->x = 0;
        geometry->y = 0;
    }

    if (mode == NULL)
    {
        g_warn_if_reached ();
        geometry->width = 1;
        geometry->height = 1;
    }
    else
    {
        geometry->width = xfce_randr_mode_width (randr, output_id, mode);
        geometry->height = xfce_randr_mode_height (randr, output_id, mode);
    }
}



static RotationFlags
convert_rotation_from_randr (Rotation rot)
{
    RotationFlags flags = ROTATION_FLAGS_0;
    if (rot & RR_Rotate_90)
        flags |= ROTATION_FLAGS_90;
    if (rot & RR_Rotate_180)
        flags |= ROTATION_FLAGS_180;
    if (rot & RR_Rotate_270)
        flags |= ROTATION_FLAGS_270;
    if (rot & RR_Reflect_X)
        flags |= ROTATION_FLAGS_REFLECT_X;
    if (rot & RR_Reflect_Y)
        flags |= ROTATION_FLAGS_REFLECT_Y;
    return flags;
}



static Rotation
convert_rotation_to_randr (RotationFlags flags)
{
    Rotation rot = 0;
    if (!(flags & ROTATION_MASK))
        rot |= RR_Rotate_0;
    if (flags & ROTATION_FLAGS_90)
        rot |= RR_Rotate_90;
    if (flags & ROTATION_FLAGS_180)
        rot |= RR_Rotate_180;
    if (flags & ROTATION_FLAGS_270)
        rot |= RR_Rotate_270;
    if (flags & ROTATION_FLAGS_REFLECT_X)
        rot |= RR_Reflect_X;
    if (flags & ROTATION_FLAGS_REFLECT_Y)
        rot |= RR_Reflect_Y;
    return rot;
}



static RotationFlags
xfce_display_settings_x11_get_rotation (XfceDisplaySettings *settings,
                                        guint output_id)
{
    return convert_rotation_from_randr (XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->rotation[output_id]);
}



static void
xfce_display_settings_x11_set_rotation (XfceDisplaySettings *settings,
                                        guint output_id,
                                        RotationFlags rotation)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    randr->rotation[output_id] = convert_rotation_to_randr (rotation);
}



static RotationFlags
xfce_display_settings_x11_get_rotations (XfceDisplaySettings *settings,
                                         guint output_id)
{
    return convert_rotation_from_randr (XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->rotations[output_id]);
}



static gdouble
xfce_display_settings_x11_get_scale (XfceDisplaySettings *settings,
                                     guint output_id)
{
    return 1.0 / XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->scalex[output_id];
}



static void
xfce_display_settings_x11_set_scale (XfceDisplaySettings *settings,
                                     guint output_id,
                                     gdouble scale)
{
    XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->scalex[output_id] = 1.0 / scale;
    XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->scaley[output_id] = 1.0 / scale;
}



static void
xfce_display_settings_x11_set_mode (XfceDisplaySettings *settings,
                                    guint output_id,
                                    guint mode_id)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    randr->mode[output_id] = (mode_id == -1U ? xfce_randr_preferred_mode (randr, output_id) : mode_id);
}



static void
output_set_mode_and_tranformation (XfceOutput *output,
                                   const XfceRRMode *mode,
                                   XfceRandr *randr)
{
    if (mode != NULL)
    {
        output->mode->id = mode->id;
        output->mode->width = mode->width;
        output->mode->height = mode->height;
        output->mode->rate = mode->rate;
        output->rotation = convert_rotation_from_randr (randr->rotation[output->id]);
        output->scale = 1.0 / randr->scalex[output->id];
    }
    else
    {
        output->mode->id = 0;
        output->mode->width = output->pref_width;
        output->mode->height = output->pref_height;
        output->mode->rate = 0;
        output->rotation = ROTATION_FLAGS_0;
        output->scale = 1.0;
    }
}



static void
xfce_display_settings_x11_update_output_mode (XfceDisplaySettings *settings,
                                              XfceOutput *output,
                                              guint mode_id)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    const XfceRRMode *mode = xfce_randr_find_mode_by_id (randr, output->id, mode_id);
    output_set_mode_and_tranformation (output, mode, randr);
}



static void
xfce_display_settings_x11_set_position (XfceDisplaySettings *settings,
                                        guint output_id,
                                        gint x,
                                        gint y)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    randr->position[output_id].x = x;
    randr->position[output_id].y = y;
}



static XfceOutput *
xfce_display_settings_x11_get_output (XfceDisplaySettings *settings,
                                      guint output_id)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    XfceOutput *output = g_new0 (XfceOutput, 1);
    const XfceRRMode *xfrr_modes, *xfrr_mode, *preferred;
    gint n_modes;

    output->id = output_id;
    output->friendly_name = randr->friendly_name[output_id];

    xfce_randr_get_positions (randr, output_id, &output->x, &output->y);
    output->active = randr->mode[output_id] != None;

    preferred = xfce_randr_find_mode_by_id (randr, output_id, xfce_randr_preferred_mode (randr, output_id));
    if (preferred != NULL)
    {
        output->pref_width = preferred->width;
        output->pref_height = preferred->height;
    }
    else
    {
        // Fallback on 640x480 if randr detection fails (Xfce #12580)
        output->pref_width = 640;
        output->pref_height = 480;
    }

    xfrr_mode = xfce_randr_find_mode_by_id (randr, output_id, randr->mode[output_id]);
    output->mode = g_new0 (XfceMode, 1);
    output_set_mode_and_tranformation (output, xfrr_mode, randr);

    xfrr_modes = xfce_randr_get_modes (randr, output_id, &n_modes);
    output->n_modes = n_modes;
    output->modes = g_new (XfceMode *, n_modes);
    for (gint n = 0; n < n_modes; n++)
    {
        XfceMode *mode = g_new (XfceMode, 1);
        mode->id = xfrr_modes[n].id;
        mode->width = xfrr_modes[n].width;
        mode->height = xfrr_modes[n].height;
        mode->rate = xfrr_modes[n].rate;
        output->modes[n] = mode;
    }

    return output;
}



static gboolean
xfce_display_settings_x11_is_active (XfceDisplaySettings *settings,
                                     guint output_id)
{
    return XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->mode[output_id] != None;
}



static void
xfce_display_settings_x11_set_active (XfceDisplaySettings *settings,
                                      guint output_id,
                                      gboolean active)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    randr->mode[output_id] = active ? xfce_randr_preferred_mode (randr, output_id) : None;
}



static void
xfce_display_settings_x11_update_output_active (XfceDisplaySettings *settings,
                                                XfceOutput *output,
                                                gboolean active)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    const XfceRRMode *mode = xfce_randr_find_mode_by_id (randr, output->id, randr->mode[output->id]);
    output_set_mode_and_tranformation (output, mode, randr);
}



static gboolean
xfce_display_settings_x11_is_primary (XfceDisplaySettings *settings,
                                      guint output_id)
{
    return XFCE_DISPLAY_SETTINGS_X11 (settings)->randr->status[output_id] == XFCE_OUTPUT_STATUS_PRIMARY;
}



static void
xfce_display_settings_x11_set_primary (XfceDisplaySettings *settings,
                                       guint output_id,
                                       gboolean primary)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    if (primary)
        randr->status[output_id] = XFCE_OUTPUT_STATUS_PRIMARY;
    else
        randr->status[output_id] = XFCE_OUTPUT_STATUS_SECONDARY;
}



static gboolean
xfce_display_settings_x11_is_clonable (XfceDisplaySettings *settings)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    RRMode *clonable_modes = xfce_randr_clonable_modes (randr);
    gboolean clonable = clonable_modes != NULL;
    g_free (clonable_modes);
    return clonable;
}



static void
xfce_display_settings_x11_save (XfceDisplaySettings *settings,
                                const gchar *scheme)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    XfconfChannel *channel = xfce_display_settings_get_channel (settings);
    for (guint n = 0; n < randr->noutput; n++)
    {
        xfce_randr_save_output (randr, scheme, channel, n);
    }
}



static void
xfce_display_settings_x11_mirror (XfceDisplaySettings *settings)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    RRMode *clonable_modes = xfce_randr_clonable_modes (randr);
    if (clonable_modes == NULL)
    {
        g_warn_if_reached ();
        return;
    }

    for (guint n = 0; n < randr->noutput; n++)
    {
        randr->mode[n] = clonable_modes[n];
        randr->mirrored[n] = TRUE;
        randr->rotation[n] = RR_Rotate_0;
        randr->scalex[n] = 1.0;
        randr->scaley[n] = 1.0;
        randr->position[n].x = 0;
        randr->position[n].y = 0;
    }

    g_free (clonable_modes);
}



static void
xfce_display_settings_x11_unmirror (XfceDisplaySettings *settings)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    const XfceRRMode *mode;
    guint x = 0;

    for (guint n = 0; n < randr->noutput; n++)
    {
        randr->mode[n] = xfce_randr_preferred_mode (randr, n);
        randr->mirrored[n] = FALSE;
        randr->rotation[n] = RR_Rotate_0;
        randr->scalex[n] = 1.0;
        randr->scaley[n] = 1.0;
        randr->position[n].x = x;
        randr->position[n].y = 0;
        mode = xfce_randr_find_mode_by_id (randr, n, randr->mode[n]);
        if (mode != NULL)
            x += mode->width;
    }
}



static void
xfce_display_settings_x11_update_output_mirror (XfceDisplaySettings *settings,
                                                XfceOutput *output)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    const XfceRRMode *mode = xfce_randr_find_mode_by_id (randr, output->id, randr->mode[output->id]);

    output->x = randr->position[output->id].x;
    output->y = randr->position[output->id].y;
    output_set_mode_and_tranformation (output, mode, randr);
    output->active = (mode != NULL);
}



static void
xfce_display_settings_x11_extend (XfceDisplaySettings *settings,
                                  guint output_id_1,
                                  guint output_id_2,
                                  ExtendedMode mode)
{
    XfceRandr *randr = XFCE_DISPLAY_SETTINGS_X11 (settings)->randr;
    const XfceRRMode *mode_1 = xfce_randr_find_mode_by_id (randr, output_id_1, randr->mode[output_id_1]);
    const XfceRRMode *mode_2 = xfce_randr_find_mode_by_id (randr, output_id_2, randr->mode[output_id_2]);
    if (mode_1 == NULL || mode_2 == NULL)
    {
        g_warn_if_reached ();
        return;
    }

    randr->position[output_id_1].x = 0;
    randr->position[output_id_1].y = 0;
    randr->position[output_id_2].x = 0;
    randr->position[output_id_2].y = 0;
    switch (mode)
    {
        case EXTENDED_MODE_RIGHT:
            randr->position[output_id_2].x = xfce_randr_mode_width (randr, output_id_1, mode_1);
            break;
        case EXTENDED_MODE_LEFT:
            randr->position[output_id_1].x = xfce_randr_mode_width (randr, output_id_2, mode_2);
            break;
        case EXTENDED_MODE_UP:
            randr->position[output_id_1].y = xfce_randr_mode_height (randr, output_id_2, mode_2);
            break;
        case EXTENDED_MODE_DOWN:
            randr->position[output_id_2].y = xfce_randr_mode_height (randr, output_id_1, mode_1);
            break;
        default:
            break;
    }
}



XfceDisplaySettings *
xfce_display_settings_x11_new (GError **error)
{
    XfceDisplaySettingsX11 *settings;
    XfceRandr *randr;
    gint event_base, error_base;

    if (!XRRQueryExtension (gdk_x11_get_default_xdisplay (), &event_base, &error_base))
    {
        g_set_error (error, 0, 0, _("Unable to query the version of the RandR extension being used"));
        return NULL;
    }

    randr = xfce_randr_new (gdk_display_get_default (), error);
    if (randr == NULL)
        return NULL;

    settings = g_object_new (XFCE_TYPE_DISPLAY_SETTINGS_X11, NULL);
    settings->randr = randr;
    settings->event_base = event_base;

    /* set up notifications */
    XRRSelectInput (gdk_x11_get_default_xdisplay (),
                    GDK_WINDOW_XID (gdk_get_default_root_window ()),
                    RRScreenChangeNotifyMask);
    gdk_x11_register_standard_event_type (gdk_display_get_default (), event_base, RRNotify + 1);
    gdk_window_add_filter (gdk_get_default_root_window (), screen_on_event, settings);

    return XFCE_DISPLAY_SETTINGS (settings);
}
