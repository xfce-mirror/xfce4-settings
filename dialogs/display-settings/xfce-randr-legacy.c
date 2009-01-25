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

#include <glib.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h>

#include "xfce-randr-legacy.h"

/** CLAMP gives a warning with comparing unsigned values */
#define UCLAMP(x, low, high) \
  (((x) > (high)) ? (high) : \
   (((low) > 0) ? (((low) < (x)) ? (low) : (x)) : 0))



XfceRandrLegacy *
xfce_randr_legacy_new (GdkDisplay  *display,
                       GError     **error)
{
    XfceRandrLegacy *legacy;
    gint             n, num_screens;
    GdkScreen       *screen;
    GdkWindow       *root_window;
    Display         *xdisplay;
    Rotation         rotation;
    gint             major, minor;

    g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    /* get the number of screens on this diaply */
    num_screens = gdk_display_get_n_screens (display);
    if (G_UNLIKELY (num_screens < 1))
        return NULL;

    /* get the x display */
    xdisplay = gdk_x11_display_get_xdisplay (display);

    /* check if the randr extension is available */
    if (XRRQueryVersion (xdisplay, &major, &minor) == FALSE)
    {
        g_set_error (error, 0, 0, _("Unable to query the version of the RandR extension being used"));
        return NULL;
    }

    /* we need atleast randr 1.1, 2.0 will probably break the api */
    if (major < 1 || (major == 1 && minor < 1))
    {
        g_set_error (error, 0, 0, _("This system is using RandR %d.%d. For the display settings to work "
                                    "version 1.1 is required at least"), major, minor);
        return NULL;
    }

    /* allocate a slice for the structure */
    legacy = g_slice_new0 (XfceRandrLegacy);

    /* set the display */
    legacy->display = display;

    /* allocate space for the difference screens */
    legacy->config = g_new0 (XRRScreenConfiguration *, num_screens);
    legacy->resolution = g_new0 (SizeID, num_screens);
    legacy->rate = g_new0 (gshort, num_screens);
    legacy->rotation = g_new0 (Rotation, num_screens);

    legacy->num_screens = num_screens;
    legacy->active_screen = 0;

    /* walk the screens */
    for (n = 0; n < num_screens; n++)
    {
        /* get the root window of this screen */
        screen = gdk_display_get_screen (display, n);
        root_window = gdk_screen_get_root_window (screen);

        /* get the screen config */
        legacy->config[n] = XRRGetScreenInfo (xdisplay, GDK_WINDOW_XID (root_window));

        /* get the active settings */
        legacy->resolution[n] = XRRConfigCurrentConfiguration (legacy->config[n], &rotation);
        legacy->rate[n] = XRRConfigCurrentRate (legacy->config[n]);
        legacy->rotation[n] = rotation;
    }

    return legacy;
}



void
xfce_randr_legacy_free (XfceRandrLegacy *legacy)
{
    gint n;

    /* free all the screen data */
    for (n = 0; n < legacy->num_screens; n++)
        XRRFreeScreenConfigInfo (legacy->config[n]);

    /* cleanup everything else we've allocated */
    g_free (legacy->config);
    g_free (legacy->resolution);
    g_free (legacy->rate);
    g_free (legacy->rotation);

    /* free slice */
    g_slice_free (XfceRandrLegacy, legacy);
}



void
xfce_randr_legacy_reload (XfceRandrLegacy *legacy)
{
    gint       n;
    GdkScreen *screen;
    GdkWindow *root_window;
    Display   *xdisplay;

    /* free all the screen data */
    for (n = 0; n < legacy->num_screens; n++)
        XRRFreeScreenConfigInfo (legacy->config[n]);

    /* get the x display */
    xdisplay = gdk_x11_display_get_xdisplay (legacy->display);

    /* walk the screens */
    for (n = 0; n < legacy->num_screens; n++)
    {
        /* get the root window of this screen */
        screen = gdk_display_get_screen (legacy->display, n);
        root_window = gdk_screen_get_root_window (screen);

        /* get the screen config */
        legacy->config[n] = XRRGetScreenInfo (xdisplay, GDK_WINDOW_XID (root_window));
    }
}



void
xfce_randr_legacy_save (XfceRandrLegacy *legacy,
                        const gchar     *scheme,
                        XfconfChannel   *channel)
{
    gchar          property[512];
    gint           n;
    XRRScreenSize *sizes;
    gint           nsizes, size;
    gchar         *resolution;
    gint           degrees;

    g_return_if_fail (XFCONF_IS_CHANNEL (channel));

    /* store the layout type */
    g_snprintf (property, sizeof (property), "/%s/Layout", scheme);
    xfconf_channel_set_string (channel, property, "Screens");

    /* store the number of screens in this scheme */
    g_snprintf (property, sizeof (property), "/%s/NumScreens", scheme);
    xfconf_channel_set_int (channel, property, legacy->num_screens);

    for (n = 0; n < legacy->num_screens; n++)
    {
        /* find the resolution and save it as a string */
        sizes = XRRConfigSizes (legacy->config[n], &nsizes);
        size = UCLAMP (legacy->resolution[n], 0, nsizes - 1);
        resolution = g_strdup_printf ("%dx%d", sizes[size].width, sizes[size].height);
        g_snprintf (property, sizeof (property), "/%s/Screen_%d/Resolution", scheme, n);
        xfconf_channel_set_string (channel, property, resolution);
        g_free (resolution);

        /* save the refresh rate */
        g_snprintf (property, sizeof (property), "/%s/Screen_%d/RefreshRate", scheme, n);
        xfconf_channel_set_int (channel, property, legacy->rate[n]);

        /* convert the rotation into degrees */
        switch (legacy->rotation[n])
        {
            case RR_Rotate_90:  degrees = 90;  break;
            case RR_Rotate_180: degrees = 180; break;
            case RR_Rotate_270: degrees = 270; break;
            default:            degrees = 0;   break;
        }

        /* save the rotation in degrees */
        g_snprintf (property, sizeof (property), "/%s/Screen_%d/Rotation", scheme, n);
        xfconf_channel_set_int (channel, property, degrees);
    }
    
    /* tell the helper to apply this theme */
    xfconf_channel_set_string (channel, "/Schemes/Apply", scheme);
}



void
xfce_randr_legacy_load (XfceRandrLegacy *legacy,
                        const gchar     *scheme,
                        XfconfChannel   *channel)
{
    
}
