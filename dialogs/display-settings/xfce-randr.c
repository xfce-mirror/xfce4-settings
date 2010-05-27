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

#include "xfce-randr.h"

#ifdef HAS_RANDR_ONE_POINT_TWO

XfceRandr *
xfce_randr_new (GdkDisplay  *display,
                GError     **error)
{
    XfceRandr              *randr;
    Display                *xdisplay;
    GdkWindow              *root_window;
    XRRScreenConfiguration *screen_config;
    XRRCrtcInfo            *crtc_info;
#ifdef HAS_RANDR_ONE_POINT_THREE
    gint                    has_1_3 = FALSE;
#endif
    gint                    n;
    gint                    major, minor;

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
#ifdef HAS_RANDR_ONE_POINT_THREE
    else if (major == 1 && minor >= 3)
        has_1_3 = TRUE;
#endif

    /* allocate the structure */
    randr = g_slice_new0 (XfceRandr);
    
    /* set display */
    randr->display = display;

    /* get the root window */
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
    randr->resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    /* set some layout */
    randr->layout = XFCE_DISPLAY_LAYOUT_SINGLE;

    /* allocate space for the settings */
    randr->mode = g_new0 (RRMode, randr->resources->noutput);
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
                randr->output_info[n] = NULL;

            /* cleanup */
            xfce_randr_free (randr);

            /* return nothing, then we'll fallback on screens (randr 1.1) code */
            return NULL;
        }

        /* load defaults */
        if (randr->output_info[n]->crtc != None)
        {
            crtc_info = XRRGetCrtcInfo (xdisplay, randr->resources, randr->output_info[n]->crtc);
            randr->mode[n] = crtc_info->mode;
            randr->rotation[n] = crtc_info->rotation;
            randr->rotations[n] = crtc_info->rotations;
            XRRFreeCrtcInfo (crtc_info);
        }
        else
        {
            randr->mode[n] = None;
            screen_config = XRRGetScreenInfo (xdisplay, GDK_WINDOW_XID (root_window));
            randr->rotations[n] = XRRConfigRotations (screen_config, &randr->rotation[n]);
            XRRFreeScreenConfigInfo (screen_config);
        }

        if (randr->output_info[n]->connection == RR_Connected)
        {
#ifdef HAS_RANDR_ONE_POINT_THREE
            if (has_1_3 && XRRGetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window)) == randr->resources->outputs[n])
            {
                randr->status[n] = XFCE_OUTPUT_STATUS_PRIMARY;
                continue;
            }
            else
#endif
            {
                /* for randr 1.2, no XRRGetOutputPrimary(), so use the first one */
                if (G_UNLIKELY (n == 0))
                {
                    randr->status[n] = XFCE_OUTPUT_STATUS_PRIMARY;
                    continue;
                }
            }

            randr->status[n] = XFCE_OUTPUT_STATUS_SECONDARY;
        }
    }

    return randr;
}



void
xfce_randr_free (XfceRandr *randr)
{
    gint n;

    /* free the output info cache */
    for (n = 0; n < randr->resources->noutput; n++)
        if (G_LIKELY (randr->output_info[n]))
            XRRFreeOutputInfo (randr->output_info[n]);

    /* free the screen resources */
    XRRFreeScreenResources (randr->resources);

    /* free the settings */
    g_free (randr->mode);
    g_free (randr->rotation);
    g_free (randr->rotations);
    g_free (randr->status);
    g_free (randr->position);
    g_free (randr->output_info);

    /* free the structure */
    g_slice_free (XfceRandr, randr);
}



void
xfce_randr_reload (XfceRandr *randr)
{
    gint       n;
    Display   *xdisplay;
    GdkWindow *root_window;
    
    /* free the screen resources */
    XRRFreeScreenResources (randr->resources);

    /* free the output info cache */
    for (n = 0; n < randr->resources->noutput; n++)
        if (G_LIKELY (randr->output_info[n]))
            XRRFreeOutputInfo (randr->output_info[n]);
    
    /* get the x display and the root window */
    xdisplay = gdk_x11_display_get_xdisplay (randr->display);
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
    randr->resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    /* cache the output info again */
    for (n = 0; n < randr->resources->noutput; n++)
    {
        /* get the output info */
        randr->output_info[n] = XRRGetOutputInfo (xdisplay, randr->resources, randr->resources->outputs[n]);
    }
}



static void
xfce_randr_save_device (XfceRandr     *randr,
                        const gchar   *scheme,
                        XfconfChannel *channel,
                        gint           output,
                        const gchar   *distinct)
{
    gchar        property[512];
    const gchar *resolution_name = NULL;
    gdouble      refresh_rate = 0.00;
    XRRModeInfo *mode;
    const gchar *position_name;
    gint         n;
    gint         degrees;

    /* find the resolution name and refresh rate (only for primary device */
    if (randr->layout != XFCE_DISPLAY_LAYOUT_CLONE
        || (randr->status[output] & XFCE_OUTPUT_STATUS_PRIMARY) != 0)
    {
        for (n = 0; n < randr->resources->nmode; n++)
        {
            if (randr->resources->modes[n].id == randr->mode[output])
            {
                mode = &randr->resources->modes[n];
                resolution_name = mode->name;
                refresh_rate = (gdouble) mode->dotClock / ((gdouble) mode->hTotal * (gdouble) mode->vTotal);

                break;
            }

            break;
        }
    }

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
    if (G_LIKELY (refresh_rate > 0.00))
        xfconf_channel_set_double (channel, property, refresh_rate);
    else
        xfconf_channel_reset_property (channel, property, FALSE);

    /* convert the rotation into degrees */
    switch (randr->rotation[output])
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

    /* save the position */
    g_snprintf (property, sizeof (property), "/%s/%s/Position", scheme, distinct);
    if (randr->layout == XFCE_DISPLAY_LAYOUT_EXTEND
        && randr->status[output] == XFCE_OUTPUT_STATUS_SECONDARY)
    {
        /* convert the position into a string */
        switch (randr->position[output])
        {
            case XFCE_OUTPUT_POSITION_LEFT:  position_name = "Left";   break;
            case XFCE_OUTPUT_POSITION_RIGHT: position_name = "Right";  break;
            case XFCE_OUTPUT_POSITION_TOP:   position_name = "Top";    break;
            default:                         position_name = "Bottom"; break;
        }

        /* save the position */
        xfconf_channel_set_string (channel, property, position_name);
    }
    else
    {
        /* remove an existing postion */
        xfconf_channel_reset_property (channel, property, FALSE);
    }
}



void
xfce_randr_save (XfceRandr     *randr,
                 const gchar   *scheme,
                 XfconfChannel *channel)
{
    gchar        property[512];
    const gchar *layout_name;
    gint         n;

    g_return_if_fail (XFCONF_IS_CHANNEL (channel));

    /* convert the layout into a string */
    switch (randr->layout)
    {
        case XFCE_DISPLAY_LAYOUT_CLONE:  layout_name = "Clone";  break;
        case XFCE_DISPLAY_LAYOUT_EXTEND: layout_name = "Extend"; break;
        default:                         layout_name = "Single"; break;
    }

    /* store the layout type */
    g_snprintf (property, sizeof (property), "/%s/Layout", scheme);
    xfconf_channel_set_string (channel, property, layout_name);

    /* find the primary layout */
    for (n = 0; n < randr->resources->noutput; n++)
    {
        switch (randr->status[n])
        {
            /* save the primary device */
            case XFCE_OUTPUT_STATUS_PRIMARY:
                xfce_randr_save_device (randr, scheme, channel, n, "Primary");
                break;
                
            /* save the secondary device */
            case XFCE_OUTPUT_STATUS_SECONDARY:
                if (randr->mode != XFCE_DISPLAY_LAYOUT_SINGLE)
                    xfce_randr_save_device (randr, scheme, channel, n, "Secondary");
                break;
            
            /* ignore */
            case XFCE_OUTPUT_STATUS_NONE:
                break;
        }
    }
    
    /* tell the helper to apply this theme */
    xfconf_channel_set_string (channel, "/Schemes/Apply", scheme);
}



void
xfce_randr_load (XfceRandr     *randr,
                 const gchar   *scheme,
                 XfconfChannel *channel)
{
    
}



const gchar *
xfce_randr_friendly_name (const gchar *name)
{
    g_return_val_if_fail (name != NULL, "<null>");

    /* try to find a translated user friendly name
     * for the output name */
    if (g_str_has_prefix (name, "LVDS")
        || strcmp (name, "PANEL") == 0)
        return _("Laptop");
    else if (g_str_has_prefix (name, "VGA")
             || g_str_has_prefix (name, "Analog"))
        return _("Monitor");
    else if (g_str_has_prefix (name, "TV")
             || strcmp (name, "S-video") == 0)
        return _("Television");
    else if (g_str_has_prefix (name, "TMDS")
             || g_str_has_prefix (name, "DVI")
             || g_str_has_prefix (name, "Digital"))
        return _("Digital display");

    return name;
}

#endif /* !HAS_RANDR_ONE_POINT_TWO */
