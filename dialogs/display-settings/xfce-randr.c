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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h>

#include <X11/Xatom.h>

#include "xfce-randr.h"
#include "edid.h"

#ifdef HAS_RANDR_ONE_POINT_TWO

static void
xfce_randr_list_clone_modes (XfceRandr *randr)
{
    GArray *clone_modes;
    gint    l, m, n, candidate, found;
    guint   i;

    clone_modes = g_array_new (FALSE, FALSE, sizeof (RRMode));

    /* walk all available modes */
    for (n = 0; n < randr->resources->nmode; n++)
    {
        candidate = TRUE;
        /* walk all active outputs */
        for (m = 0; m < randr->resources->noutput; m++)
        {
            if (randr->status[m] == XFCE_OUTPUT_STATUS_NONE)
                continue;

            found = FALSE;
            /* walk supported modes from this output */
            for (l = 0; l < randr->output_info[m]->nmode; l++)
            {
                if (randr->resources->modes[n].id == randr->output_info[m]->modes[l])
                {
                    found = TRUE;
                    break;
                }
            }

            /* if it is not present in one output, forget it */
            candidate &= found;
        }

        /* common to all outputs, can be used for clone mode */
        if (candidate)
            clone_modes = g_array_append_val (clone_modes, randr->resources->modes[n].id);
    }

    /* return a "normal" array (last value -> None) */
    randr->clone_modes = g_new0 (RRMode, clone_modes->len + 1);

    for (i = 0; i < clone_modes->len; i++)
        randr->clone_modes[i] = g_array_index (clone_modes, RRMode, i);

    g_array_free (clone_modes, TRUE);
}



static Rotation
xfce_randr_get_safe_rotations (XfceRandr *randr,
                               Display   *xdisplay,
                               GdkWindow *root_window,
                               gint       num_output)
{
    XRRScreenConfiguration *screen_config;
    XRRCrtcInfo            *crtc_info;
    gint                    n;
    Rotation                dummy, rot;

    g_return_val_if_fail (num_output >= 0
                          && num_output < randr->resources->noutput,
                          RR_Rotate_0);

    if (randr->output_info[num_output]->ncrtc < 1)
    {
        screen_config = XRRGetScreenInfo (xdisplay, GDK_WINDOW_XID (root_window));
        rot = XRRConfigRotations (screen_config, &dummy);
        XRRFreeScreenConfigInfo (screen_config);
        return rot;
    }

    rot = XFCE_RANDR_ROTATIONS_MASK | XFCE_RANDR_REFLECTIONS_MASK;
    for (n = 0; n < randr->output_info[num_output]->ncrtc; n++)
    {
        crtc_info = XRRGetCrtcInfo (xdisplay, randr->resources,
                                    randr->output_info[num_output]->crtcs[n]);
        rot &= crtc_info->rotations;
        XRRFreeCrtcInfo (crtc_info);
    }

    return rot;
}



static XfceRRMode *
xfce_randr_list_supported_modes (XRRScreenResources *resources,
                                XRROutputInfo       *output_info)
{
    XfceRRMode *modes;
    gint m, n;

    g_return_val_if_fail (resources != NULL, NULL);
    g_return_val_if_fail (output_info != NULL, NULL);

    if (output_info->nmode == 0)
        return NULL;

    modes = g_new0 (XfceRRMode, output_info->nmode);

    for (n = 0; n < output_info->nmode; ++n)
    {
        modes[n].id = output_info->modes[n];

        /* we need to walk yet another list to get the mode info */
        for (m = 0; m < resources->nmode; ++m)
        {
            if (output_info->modes[n] == resources->modes[m].id)
            {
                modes[n].width = resources->modes[m].width;
                modes[n].height = resources->modes[m].height;
                modes[n].rate = (gdouble) resources->modes[m].dotClock /
                                ((gdouble) resources->modes[m].hTotal * (gdouble) resources->modes[m].vTotal);

                break;
            }
        }
    }

    return modes;
}



static gboolean
xfce_randr_populate (XfceRandr *randr,
                     Display   *xdisplay,
                     GdkWindow *root_window)
{
    XRRCrtcInfo            *crtc_info;
    gint                    n;

    g_return_val_if_fail (randr != NULL, FALSE);
    g_return_val_if_fail (randr->resources != NULL, FALSE);

    /* allocate space for the settings */
    randr->mode = g_new0 (RRMode, randr->resources->noutput);
    randr->preferred_mode = g_new0 (RRMode, randr->resources->noutput);
    randr->modes = g_new0 (XfceRRMode *, randr->resources->noutput);
    randr->rotation = g_new0 (Rotation, randr->resources->noutput);
    randr->rotations = g_new0 (Rotation, randr->resources->noutput);
    randr->position = g_new0 (XfceOutputPosition, randr->resources->noutput);
    randr->status = g_new0 (XfceOutputStatus, randr->resources->noutput);
    randr->output_info = g_new0 (XRROutputInfo *, randr->resources->noutput);

    /* walk the outputs */
    for (n = 0; n < randr->resources->noutput; n++)
    {
        /* reset */
        randr->status[n] = XFCE_OUTPUT_STATUS_NONE;

        /* get the output info */
        randr->output_info[n] = XRRGetOutputInfo (xdisplay, randr->resources, randr->resources->outputs[n]);

        /* check if the device is really a randr 1.2 device */
        if (n == 0 && strcmp (randr->output_info[n]->name, "default") == 0)
        {
            /* make sure we don't free the not yet allocated outputs */
            for (n++; n < randr->resources->noutput; n++)
            {
                randr->output_info[n] = NULL;
                randr->modes[n] = NULL;
            }

            return FALSE;
        }

        /* fill in supported modes */
        randr->modes[n] = xfce_randr_list_supported_modes (randr->resources, randr->output_info[n]);

        /* do not query disconnected outputs */
        if (randr->output_info[n]->connection == RR_Connected)
        {
            /* load defaults */
            randr->preferred_mode[n] = randr->output_info[n]->modes[randr->output_info[n]->npreferred];

#ifdef HAS_RANDR_ONE_POINT_THREE
            /* find the primary screen if supported */
            if (randr->has_1_3 && XRRGetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window)) == randr->resources->outputs[n])
                randr->status[n] = XFCE_OUTPUT_STATUS_PRIMARY;
            else
#endif
                randr->status[n] = XFCE_OUTPUT_STATUS_SECONDARY;

            if (randr->output_info[n]->crtc != None)
            {
                crtc_info = XRRGetCrtcInfo (xdisplay, randr->resources,
                                            randr->output_info[n]->crtc);
                randr->mode[n] = crtc_info->mode;
                randr->rotation[n] = crtc_info->rotation;
                randr->rotations[n] = crtc_info->rotations;
                randr->position[n].x = crtc_info->x;
                randr->position[n].y = crtc_info->y;
                XRRFreeCrtcInfo (crtc_info);
                continue;
            }
        }

        /* output either disabled or disconnected */
        randr->mode[n] = None;
        randr->rotation[n] = RR_Rotate_0;
        randr->rotations[n] = xfce_randr_get_safe_rotations (randr, xdisplay,
                                                             root_window, n);
    }

    /* clone modes: same RRModes present for all outputs */
    xfce_randr_list_clone_modes (randr);

    return TRUE;
}



XfceRandr *
xfce_randr_new (GdkDisplay  *display,
                GError     **error)
{
    XfceRandr *randr;
    Display   *xdisplay;
    GdkWindow *root_window;
    gint       major, minor;

    g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    /* get the x display */
    xdisplay = gdk_x11_display_get_xdisplay (display);

    /* check if the randr extension is available */
    if (XRRQueryVersion (xdisplay, &major, &minor) == FALSE)
    {
        g_set_error (error, 0, 0, _("Unable to query the version of the RandR extension being used"));
        return NULL;
    }

    /* we need atleast randr 1.2, 2.0 will probably break the api */
    if (major < 1 || (major == 1 && minor < 2))
    {
        /* 1.1 (not 1.2) is required because of the legacy code in xfce-randr-legacy.c */
        g_set_error (error, 0, 0, _("This system is using RandR %d.%d. For the display settings to work "
                                    "version 1.1 is required at least"), major, minor);
        return NULL;
    }

    /* allocate the structure */
    randr = g_slice_new0 (XfceRandr);

    randr->has_1_3 = (major > 1 || (major == 1 && minor >= 3));

    /* set display */
    randr->display = display;

    /* get the root window */
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
    randr->resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    if (!xfce_randr_populate (randr, xdisplay, root_window))
    {
        /* cleanup */
        xfce_randr_free (randr);

        /* return nothing, then we'll fallback on screens (randr 1.1) code */
        return NULL;
    }

    return randr;
}



static void
xfce_randr_cleanup (XfceRandr *randr)
{
    gint n;

    /* free the output/mode info cache */
    for (n = 0; n < randr->resources->noutput; n++)
    {
        if (G_LIKELY (randr->output_info[n]))
            XRRFreeOutputInfo (randr->output_info[n]);
        if (G_LIKELY (randr->modes[n]))
            g_free (randr->modes[n]);
    }

    /* free the screen resources */
    XRRFreeScreenResources (randr->resources);

    /* free the settings */
    g_free (randr->clone_modes);
    g_free (randr->mode);
    g_free (randr->modes);
    g_free (randr->preferred_mode);
    g_free (randr->rotation);
    g_free (randr->rotations);
    g_free (randr->status);
    g_free (randr->position);
    g_free (randr->output_info);
}



void
xfce_randr_free (XfceRandr *randr)
{
    xfce_randr_cleanup (randr);

    /* free the structure */
    g_slice_free (XfceRandr, randr);
}



void
xfce_randr_reload (XfceRandr *randr)
{
    Display   *xdisplay;
    GdkWindow *root_window;

    xfce_randr_cleanup (randr);

    /* get the x display */
    xdisplay = gdk_x11_display_get_xdisplay (randr->display);

    /* get the root window */
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
#ifdef HAS_RANDR_ONE_POINT_THREE
    /* xfce_randr_reload() is only called after a xrandr notification, which
       means that X is aware of the new hardware already. So, if possible,
       do not reprobe the hardware again. */
    if (randr->has_1_3)
        randr->resources = XRRGetScreenResourcesCurrent (xdisplay, GDK_WINDOW_XID (root_window));
    else
#endif
    randr->resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    /* repopulate */
    xfce_randr_populate (randr, xdisplay, root_window);
}



static void
xfce_randr_save_device (XfceRandr     *randr,
                        const gchar   *scheme,
                        XfconfChannel *channel,
                        gint           output,
                        const gchar   *distinct)
{
    gchar        property[512];
    gchar       *resolution_name = NULL;
    const gchar *reflection_name = NULL;
    XfceRRMode  *mode;
    gint         degrees;

    /* find the resolution and refresh rate */
    mode = xfce_randr_find_mode_by_id (randr, output, randr->mode[output]);
    if (mode)
        resolution_name = g_strdup_printf ("%dx%d", mode->width, mode->height);

    /* save the device name */
    g_snprintf (property, sizeof (property), "/%s/%s", scheme, distinct);
    xfconf_channel_set_string (channel, property, randr->output_info[output]->name);

    /* save (or remove) the resolution */
    g_snprintf (property, sizeof (property), "/%s/%s/Resolution", scheme, distinct);
    if (G_LIKELY (resolution_name != NULL))
        xfconf_channel_set_string (channel, property, resolution_name);
    else
        xfconf_channel_reset_property (channel, property, FALSE);

    /* save the refresh rate */
    g_snprintf (property, sizeof (property), "/%s/%s/RefreshRate", scheme, distinct);
    if (G_LIKELY (resolution_name != NULL))
        xfconf_channel_set_double (channel, property, mode->rate);
    else
        xfconf_channel_reset_property (channel, property, FALSE);

    /* convert the rotation into degrees */
    switch (randr->rotation[output] & XFCE_RANDR_ROTATIONS_MASK)
    {
        case RR_Rotate_90:  degrees = 90;  break;
        case RR_Rotate_180: degrees = 180; break;
        case RR_Rotate_270: degrees = 270; break;
        default:            degrees = 0;   break;
    }

    /* save the rotation in degrees */
    g_snprintf (property, sizeof (property), "/%s/%s/Rotation", scheme, distinct);
    /* resolution name NULL means output disabled */
    if (G_LIKELY (resolution_name != NULL))
        xfconf_channel_set_int (channel, property, degrees);
    else
        xfconf_channel_reset_property (channel, property, FALSE);

    /* convert the reflection into a string */
    switch (randr->rotation[output] & XFCE_RANDR_REFLECTIONS_MASK)
    {
        case RR_Reflect_X:              reflection_name = "X";  break;
        case RR_Reflect_Y:              reflection_name = "Y";  break;
        case RR_Reflect_X|RR_Reflect_Y: reflection_name = "XY"; break;
        default:                        reflection_name = "0";  break;
    }

    /* save the reflection string */
    g_snprintf (property, sizeof (property), "/%s/%s/Reflection", scheme, distinct);
    /* resolution name NULL means output disabled */
    if (G_LIKELY (resolution_name != NULL))
        xfconf_channel_set_string (channel, property, reflection_name);
    else
        xfconf_channel_reset_property (channel, property, FALSE);

#ifdef HAS_RANDR_ONE_POINT_THREE
    /* is it the primary output? */
    g_snprintf (property, sizeof (property), "/%s/%s/Primary", scheme, distinct);
    if (randr->status[output] == XFCE_OUTPUT_STATUS_PRIMARY)
        xfconf_channel_set_bool (channel, property, TRUE);
    else
        xfconf_channel_reset_property (channel, property, FALSE);
#endif

    /* first, remove any existing position */
    g_snprintf (property, sizeof (property), "/%s/%s/Position", scheme, distinct);
    xfconf_channel_reset_property (channel, property, TRUE);
    /* then save the new one */
    if (G_LIKELY (resolution_name != NULL)
        && randr->position[output].x >= 0
        && randr->position[output].y >= 0)
    {
        g_snprintf (property, sizeof (property), "/%s/%s/Position/X", scheme, distinct);
        xfconf_channel_set_int (channel, property, randr->position[output].x);
        g_snprintf (property, sizeof (property), "/%s/%s/Position/Y", scheme, distinct);
        xfconf_channel_set_int (channel, property, randr->position[output].y);
    }

    g_free (resolution_name);
}



void
xfce_randr_save (XfceRandr     *randr,
                 const gchar   *scheme,
                 XfconfChannel *channel)
{
    gchar        property[512];
    gint         n, num_outputs = 0;

    g_return_if_fail (XFCONF_IS_CHANNEL (channel));

    /* store the layout type */
    g_snprintf (property, sizeof (property), "/%s/Layout", scheme);
    xfconf_channel_set_string (channel, property, "Outputs");

    /* parse all outputs */
    for (n = 0; n < randr->resources->noutput; n++)
    {
        /* do not save disconnected ones */
        if (randr->status[n] != XFCE_OUTPUT_STATUS_NONE)
        {
            g_snprintf (property, sizeof (property), "Output%d", num_outputs++);
            xfce_randr_save_device (randr, scheme, channel, n, property);
        }
    }

    /* store the number of outputs saved */
    g_snprintf (property, sizeof (property), "/%s/NumOutputs", scheme);
    xfconf_channel_set_int (channel, property, num_outputs);

    /* tell the helper to apply this theme */
    xfconf_channel_set_string (channel, "/Schemes/Apply", scheme);
}



void
xfce_randr_load (XfceRandr     *randr,
                 const gchar   *scheme,
                 XfconfChannel *channel)
{
    
}



static guint8 *
xfce_randr_read_edid_data (Display  *xdisplay,
                           RROutput  output)
{
    unsigned char *prop;
    int            actual_format;
    unsigned long  nitems, bytes_after;
    Atom           actual_type;
    Atom           edid_atom;
    guint8        *result = NULL;

    edid_atom = gdk_x11_get_xatom_by_name (RR_PROPERTY_RANDR_EDID);

    if (edid_atom != None)
    {
        if (XRRGetOutputProperty (xdisplay, output, edid_atom, 0, 100,
                                  False, False, AnyPropertyType,
                                  &actual_type, &actual_format, &nitems,
                                  &bytes_after, &prop) == Success)
        {
            if (actual_type == XA_INTEGER && actual_format == 8)
                result = g_memdup (prop, nitems);
        }

        XFree (prop);
    }

    return result;
}



gchar *
xfce_randr_friendly_name (XfceRandr   *randr,
                          RROutput     output,
                          const gchar *name)
{
    Display     *xdisplay;
    MonitorInfo *info = NULL;
    guint8      *edid_data;
    gchar       *friendly_name = NULL;

    g_return_val_if_fail (randr != NULL && output != None && name != NULL, "<null>");

    /* special case, a laptop */
    if (g_str_has_prefix (name, "LVDS")
        || strcmp (name, "PANEL") == 0)
        return g_strdup (_("Laptop"));

    /* otherwise, get the vendor & size */
    xdisplay = gdk_x11_display_get_xdisplay (randr->display);
    edid_data = xfce_randr_read_edid_data (xdisplay, output);

    if (edid_data)
        info = decode_edid (edid_data);

    if (info)
        friendly_name = make_display_name (info);

    g_free (info);
    g_free (edid_data);

    if (friendly_name)
        return friendly_name;

    /* last attempt to return a better name */
    if (g_str_has_prefix (name, "VGA")
             || g_str_has_prefix (name, "Analog"))
        return g_strdup (_("Monitor"));
    else if (g_str_has_prefix (name, "TV")
             || strcmp (name, "S-video") == 0)
        return g_strdup (_("Television"));
    else if (g_str_has_prefix (name, "TMDS")
             || g_str_has_prefix (name, "DVI")
             || g_str_has_prefix (name, "Digital"))
        return g_strdup (_("Digital display"));

    /* everything failed, fallback */
    return g_strdup (name);
}



XfceRRMode *
xfce_randr_find_mode_by_id (XfceRandr *randr,
                            gint       output,
                            RRMode     id)
{
    gint n;

    g_return_val_if_fail (randr != NULL, NULL);
    g_return_val_if_fail (output >= 0 && output < randr->resources->noutput,
                          NULL);

    if (id == None)
        return NULL;

    for (n = 0; n < randr->output_info[output]->nmode; ++n)
    {
        if (randr->modes[output][n].id == id)
            return &randr->modes[output][n];
    }

    return NULL;
}

#endif /* !HAS_RANDR_ONE_POINT_TWO */
