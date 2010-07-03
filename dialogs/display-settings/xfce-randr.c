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
    gint    l, n, candidate, found;
    guint   m;

    clone_modes = g_array_new (TRUE, FALSE, sizeof (RRMode));

    /* walk all available modes */
    for (n = 0; n < randr->resources->nmode; ++n)
    {
        candidate = TRUE;
        /* walk all connected outputs */
        for (m = 0; m < randr->noutput; ++m)
        {
            found = FALSE;
            /* walk supported modes from this output */
            for (l = 0; l < randr->output_info[m]->nmode; ++l)
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
    randr->clone_modes = (RRMode *) g_array_free (clone_modes, FALSE);
}



static Rotation
xfce_randr_get_safe_rotations (XfceRandr *randr,
                               Display   *xdisplay,
                               guint      num_output)
{
    XRRCrtcInfo *crtc_info;
    Rotation     rot;
    gint         n;

    g_return_val_if_fail (num_output < randr->noutput, RR_Rotate_0);
    g_return_val_if_fail (randr->output_info[num_output]->ncrtc > 0, RR_Rotate_0);

    rot = XFCE_RANDR_ROTATIONS_MASK | XFCE_RANDR_REFLECTIONS_MASK;
    for (n = 0; n < randr->output_info[num_output]->ncrtc; ++n)
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
    GPtrArray     *outputs;
    XRROutputInfo *output_info;
    XRRCrtcInfo   *crtc_info;
    gint           n;
    guint          m;

    g_return_val_if_fail (randr != NULL, FALSE);
    g_return_val_if_fail (randr->resources != NULL, FALSE);

    /* prepare the temporary cache */
    outputs = g_ptr_array_new_with_free_func ((GDestroyNotify) XRRFreeOutputInfo);

    /* walk the outputs */
    for (n = 0; n < randr->resources->noutput; ++n)
    {
        /* get the output info */
        output_info = XRRGetOutputInfo (xdisplay, randr->resources,
                                        randr->resources->outputs[n]);

        /* forget about disconnected outputs */
        if (output_info->connection != RR_Connected)
        {
            XRRFreeOutputInfo (output_info);
            continue;
        }

        /* cache it */
        g_ptr_array_add (outputs, output_info);

        /* check if the device is really a randr 1.2 device */
        if (n == 0 && strcmp (output_info->name, "default") == 0)
        {
            /* free the cache */
            g_ptr_array_unref (outputs);
            return FALSE;
        }
    }

    /* migrate the temporary cache */
    randr->noutput = outputs->len;
    randr->output_info = (XRROutputInfo **) g_ptr_array_free (outputs, FALSE);

    /* allocate final space for the settings */
    randr->mode = g_new0 (RRMode, randr->noutput);
    randr->modes = g_new0 (XfceRRMode *, randr->noutput);
    randr->rotation = g_new0 (Rotation, randr->noutput);
    randr->rotations = g_new0 (Rotation, randr->noutput);
    randr->position = g_new0 (XfceOutputPosition, randr->noutput);
    randr->status = g_new0 (XfceOutputStatus, randr->noutput);

    /* walk the connected outputs */
    for (m = 0; m < randr->noutput; ++m)
    {
        /* fill in supported modes */
        randr->modes[m] = xfce_randr_list_supported_modes (randr->resources, randr->output_info[m]);

#ifdef HAS_RANDR_ONE_POINT_THREE
        /* find the primary screen if supported */
        if (randr->has_1_3 && XRRGetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window)) == randr->resources->outputs[m])
            randr->status[m] = XFCE_OUTPUT_STATUS_PRIMARY;
        else
#endif
            randr->status[m] = XFCE_OUTPUT_STATUS_SECONDARY;

        if (randr->output_info[m]->crtc != None)
        {
            crtc_info = XRRGetCrtcInfo (xdisplay, randr->resources,
                                        randr->output_info[m]->crtc);
            randr->mode[m] = crtc_info->mode;
            randr->rotation[m] = crtc_info->rotation;
            randr->rotations[m] = crtc_info->rotations;
            randr->position[m].x = crtc_info->x;
            randr->position[m].y = crtc_info->y;
            XRRFreeCrtcInfo (crtc_info);
        }
        else
        {
            /* output disabled */
            randr->mode[m] = None;
            randr->rotation[m] = RR_Rotate_0;
            randr->rotations[m] = xfce_randr_get_safe_rotations (randr, xdisplay, m);
        }
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
    guint n;

    /* free the output/mode info cache */
    for (n = 0; n < randr->noutput; ++n)
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



void
xfce_randr_save_output (XfceRandr     *randr,
                        const gchar   *scheme,
                        XfconfChannel *channel,
                        guint          output)
{
    gchar             property[512];
    gchar            *str_value;
    const XfceRRMode *mode;
    gint              degrees;

    g_return_if_fail (randr != NULL && scheme != NULL);
    g_return_if_fail (XFCONF_IS_CHANNEL (channel));
    g_return_if_fail (output < randr->noutput);

    /* save the device name */
    str_value = xfce_randr_friendly_name (randr, randr->resources->outputs[output],
                                          randr->output_info[output]->name);
    g_snprintf (property, sizeof (property), "/%s/%s", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_string (channel, property, str_value);
    g_free (str_value);

    /* find the resolution and refresh rate */
    mode = xfce_randr_find_mode_by_id (randr, output, randr->mode[output]);

    /* if no resolution was found, mark it as inactive and stop */
    g_snprintf (property, sizeof (property), "/%s/%s/Active", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_bool (channel, property, mode != NULL);

    if (mode == NULL)
        return;

    /* save the resolution */
    str_value = g_strdup_printf ("%dx%d", mode->width, mode->height);
    g_snprintf (property, sizeof (property), "/%s/%s/Resolution", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_string (channel, property, str_value);
    g_free (str_value);

    /* save the refresh rate */
    g_snprintf (property, sizeof (property), "/%s/%s/RefreshRate", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_double (channel, property, mode->rate);

    /* convert the rotation into degrees */
    switch (randr->rotation[output] & XFCE_RANDR_ROTATIONS_MASK)
    {
        case RR_Rotate_90:  degrees = 90;  break;
        case RR_Rotate_180: degrees = 180; break;
        case RR_Rotate_270: degrees = 270; break;
        default:            degrees = 0;   break;
    }

    /* save the rotation in degrees */
    g_snprintf (property, sizeof (property), "/%s/%s/Rotation", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_int (channel, property, degrees);

    /* convert the reflection into a string */
    switch (randr->rotation[output] & XFCE_RANDR_REFLECTIONS_MASK)
    {
        case RR_Reflect_X:              str_value = "X";  break;
        case RR_Reflect_Y:              str_value = "Y";  break;
        case RR_Reflect_X|RR_Reflect_Y: str_value = "XY"; break;
        default:                        str_value = "0";  break;
    }

    /* save the reflection string */
    g_snprintf (property, sizeof (property), "/%s/%s/Reflection", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_string (channel, property, str_value);

#ifdef HAS_RANDR_ONE_POINT_THREE
    /* is it the primary output? */
    g_snprintf (property, sizeof (property), "/%s/%s/Primary", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_bool (channel, property,
                             randr->status[output] == XFCE_OUTPUT_STATUS_PRIMARY);
#endif

    /* save the position */
    g_snprintf (property, sizeof (property), "/%s/%s/Position/X", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_int (channel, property, MAX (randr->position[output].x, 0));
    g_snprintf (property, sizeof (property), "/%s/%s/Position/Y", scheme,
                randr->output_info[output]->name);
    xfconf_channel_set_int (channel, property, MAX (randr->position[output].y, 0));
}



void
xfce_randr_save_all (XfceRandr     *randr,
                     const gchar   *scheme,
                     XfconfChannel *channel)
{
    guint        n;

    g_return_if_fail (randr != NULL && scheme != NULL);
    g_return_if_fail (XFCONF_IS_CHANNEL (channel));

    /* save connected outputs */
    for (n = 0; n < randr->noutput; ++n)
        xfce_randr_save_output (randr, scheme, channel, n);
}



void
xfce_randr_apply (XfceRandr     *randr,
                  const gchar   *scheme,
                  XfconfChannel *channel)
{
    g_return_if_fail (randr != NULL && scheme != NULL);
    g_return_if_fail (XFCONF_IS_CHANNEL (channel));

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

    g_return_val_if_fail (randr != NULL && output != None && name != NULL, g_strdup ("<null>"));

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
                            guint      output,
                            RRMode     id)
{
    gint n;

    g_return_val_if_fail (randr != NULL, NULL);
    g_return_val_if_fail (output < randr->noutput, NULL);

    if (id == None)
        return NULL;

    for (n = 0; n < randr->output_info[output]->nmode; ++n)
    {
        if (randr->modes[output][n].id == id)
            return &randr->modes[output][n];
    }

    return NULL;
}



RRMode
xfce_randr_preferred_mode (XfceRandr *randr,
                           guint      output)
{
    RRMode best_mode;
    gint   best_dist, dist, n;

    g_return_val_if_fail (randr != NULL, None);
    g_return_val_if_fail (output < randr->noutput, None);

    /* mimic xrandr's preferred_mode () */

    best_mode = None;
    best_dist = 0;
    for (n = 0; n < randr->output_info[output]->nmode; ++n)
    {
        if (n < randr->output_info[output]->npreferred)
            dist = 0;
        else if (randr->output_info[output]->mm_height != 0)
            dist = (1000 * gdk_screen_height () / gdk_screen_height_mm () -
                1000 * randr->modes[output][n].height /
                    randr->output_info[output]->mm_height);
        else
            dist = gdk_screen_height () - randr->modes[output][n].height;

        dist = ABS (dist);

        if (best_mode == None || dist < best_dist)
        {
            best_mode = randr->modes[output][n].id;
            best_dist = dist;
        }
    }
    return best_mode;
}

#endif /* !HAS_RANDR_ONE_POINT_TWO */
