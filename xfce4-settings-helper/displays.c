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
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include <X11/extensions/Xrandr.h>

#include "displays.h"

/* check for randr 1.2 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 2)
#define HAS_RANDR_ONE_POINT_TWO
/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#endif
#else
#undef HAS_RANDR_ONE_POINT_TWO
#undef HAS_RANDR_ONE_POINT_THREE
#endif

static void            xfce_displays_helper_finalize                       (GObject                 *object);
static void            xfce_displays_helper_channel_apply                  (XfceDisplaysHelper      *helper,
                                                                            const gchar             *scheme);
static void            xfce_displays_helper_channel_apply_legacy           (XfceDisplaysHelper      *helper,
                                                                            const gchar             *scheme);
static void            xfce_displays_helper_channel_property_changed       (XfconfChannel           *channel,
                                                                            const gchar             *property_name,
                                                                            const GValue            *value,
                                                                            XfceDisplaysHelper      *helper);



struct _XfceDisplaysHelperClass
{
    GObjectClass __parent__;
};

struct _XfceDisplaysHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel *channel;

#ifdef HAS_RANDR_ONE_POINT_THREE
    gint           has_1_3;
#endif
};

#ifdef HAS_RANDR_ONE_POINT_TWO
typedef enum _XfceDisplayLayout XfceDisplayLayout;
enum _XfceDisplayLayout
{
    XFCE_DISPLAY_LAYOUT_SINGLE,
    XFCE_DISPLAY_LAYOUT_CLONE,
    XFCE_DISPLAY_LAYOUT_EXTEND
};
#endif



G_DEFINE_TYPE (XfceDisplaysHelper, xfce_displays_helper, G_TYPE_OBJECT);



static void
xfce_displays_helper_class_init (XfceDisplaysHelperClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_displays_helper_finalize;
}



static void
xfce_displays_helper_init (XfceDisplaysHelper *helper)
{
    gint major = 0, minor = 0;
    gint event_base, error_base;

    /* check if the randr extension is running */
    if (XRRQueryExtension (GDK_DISPLAY (), &event_base, &error_base))
    {
        /* query the version */
        if (XRRQueryVersion (GDK_DISPLAY (), &major, &minor)
            && major == 1 && minor >= 1)
        {
            /* open the channel */
            helper->channel = xfconf_channel_new ("displays");
        
            /* monitor channel changes */
            g_signal_connect (G_OBJECT (helper->channel), "property-changed", 
                              G_CALLBACK (xfce_displays_helper_channel_property_changed), helper);

            if (major == 1 && minor >= 2)
            {
                helper->has_1_3 = (major == 1 && minor >= 3);
                /* restore the default scheme */
                xfce_displays_helper_channel_apply (helper, "Default");
            }
            else
            {
                /* restore the default scheme */
                xfce_displays_helper_channel_apply_legacy (helper, "Default");
            }
        }
        else
        {
             g_critical ("RANDR extension is too old, version %d.%d. "
                         "Display settings won't be applied.",
                         major, minor);
        }
    }
    else
    {
        g_critical ("No RANDR extension found in display %s. Display settings won't be applied.",
                    gdk_display_get_name (gdk_display_get_default ()));
    }
}



static void
xfce_displays_helper_finalize (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);

    /* release the channel */
    if (G_LIKELY (helper->channel))
        g_object_unref (G_OBJECT (helper->channel));

    (*G_OBJECT_CLASS (xfce_displays_helper_parent_class)->finalize) (object);
}



#ifdef HAS_RANDR_ONE_POINT_TWO
static void
xfce_displays_helper_channel_apply (XfceDisplaysHelper *helper,
                                    const gchar        *scheme)
{
    GdkDisplay         *display;
    Display            *xdisplay;
    GdkWindow          *root_window;
    XRRScreenResources *resources;
    gchar               property[512];
    gint                l, m, n, num_outputs, output_rot;
#ifdef HAS_RANDR_ONE_POINT_THREE
    gint                is_primary;
#endif
    gchar              *output_name, *output_res;
    gdouble             output_rate;
    XRROutputInfo      *output_info;
    XRRCrtcInfo        *crtc_info;
    XRRModeInfo        *mode_info;
    gdouble             rate;
    RRMode              mode;
    Rotation            rot;

    /* flush x and trap errors */
    gdk_flush ();
    gdk_error_trap_push ();

    /* get the default display */
    display = gdk_display_get_default ();
    xdisplay = gdk_x11_display_get_xdisplay (display);
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
    resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    /* get the number of outputs */
    g_snprintf (property, sizeof (property), "/%s/NumOutputs", scheme);
    num_outputs = xfconf_channel_get_int (helper->channel, property, 0);

    for (n = 0; n < num_outputs; ++n)
    {
        /* get the output name */
        g_snprintf (property, sizeof (property), "/%s/Output%d", scheme, n);
        output_name = xfconf_channel_get_string (helper->channel, property, NULL);

        if (output_name == NULL)
            continue;

        g_snprintf (property, sizeof (property), "/%s/Output%d/Resolution", scheme, n);
        output_res = xfconf_channel_get_string (helper->channel, property, NULL);

        g_snprintf (property, sizeof (property), "/%s/Output%d/RefreshRate", scheme, n);
        output_rate = xfconf_channel_get_double (helper->channel, property, 0.0);

        g_snprintf (property, sizeof (property), "/%s/Output%d/Rotation", scheme, n);
        output_rot = xfconf_channel_get_int (helper->channel, property, 0);
        /* convert to a Rotation */
        switch (output_rot)
        {
            case 90:
                rot = RR_Rotate_90;
                break;
            case 180:
                rot = RR_Rotate_180;
                break;
            case 270:
                rot = RR_Rotate_270;
                break;
            default:
                rot = RR_Rotate_0;
                break;
        }

#ifdef HAS_RANDR_ONE_POINT_THREE
        g_snprintf (property, sizeof (property), "/%s/Output%d/Primary", scheme, n);
        is_primary = xfconf_channel_get_bool (helper->channel, property, FALSE);
#endif

        /* walk the existing outputs */
        for (m = 0; m < resources->noutput; ++m)
        {
            output_info = XRRGetOutputInfo (xdisplay, resources, resources->outputs[m]);

            if (g_strcmp0 (output_info->name, output_name) != 0)
            {
                XRRFreeOutputInfo (output_info);
                continue;
            }

            /* walk all supported modes */
            mode = None;
            for (l = 0; l < output_info->nmode; ++l)
            {
                /* get the mode info */
                mode_info = &resources->modes[m];

                /* calculate the refresh rate */
                rate = (gfloat) mode_info->dotClock / ((gfloat) mode_info->hTotal * (gfloat) mode_info->vTotal);

                /* find the mode corresponding to the saved values */
                if (((int) rate == (int) output_rate)
                    && (g_strcmp0 (mode_info->name, output_res) == 0))
                {
                    mode = mode_info->id;
                    break;
                }
            }

            /* unsupported mode, abort for this output */
            if (mode == None && output_res != NULL)
            {
                XRRFreeOutputInfo (output_info);
                break;
            }

            if (output_info->crtc != None)
            {
                crtc_info = XRRGetCrtcInfo (xdisplay, resources, output_info->crtc);

                /* unsupported rotation, abort for this output */
                if ((crtc_info->rotations & rot) == 0)
                {
                    XRRFreeCrtcInfo (crtc_info);
                    XRRFreeOutputInfo (output_info);
                    break;
                }

                /* check if we really need to do something */
                if (crtc_info->mode != mode || crtc_info->rotation != rot)
                {
                    if (XRRSetCrtcConfig (xdisplay, resources, output_info->crtc,
                                          crtc_info->timestamp, crtc_info->x, crtc_info->y,
                                          mode, rot, crtc_info->outputs, crtc_info->noutput) != Success)
                        g_warning ("Failed to configure %s.", output_info->name);
                }

                XRRFreeCrtcInfo (crtc_info);
            }
            else
                g_warning ("No CRTC found for %s.", output_info->name);

            XRRFreeOutputInfo (output_info);

#ifdef HAS_RANDR_ONE_POINT_THREE
            if (helper->has_1_3 && is_primary)
                XRRSetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window), resources->outputs[m]);
#endif
            /* done with this output, go to the next one */
            break;
        }

        g_free (output_res);
        g_free (output_name);
    }

    /* free the screen resources */
    XRRFreeScreenResources (resources);

    /* flush and remove the x error trap */
    gdk_flush ();
    gdk_error_trap_pop ();
}
#endif



static void
xfce_displays_helper_channel_apply_legacy (XfceDisplaysHelper *helper,
                                           const gchar        *scheme)
{
    GdkDisplay             *display;
    Display                *xdisplay;
    GdkScreen              *screen;
    XRRScreenConfiguration *config;
    gint                    n, num_screens, s;
    gchar                   property[512];
    GdkWindow              *root_window;
    gchar                  *resolution_name;
    gint                    loaded_rate;
    Rotation                rotation, current_rotation, rotations;
    gint                    size_id, nsizes, nrates;
    XRRScreenSize          *sizes;
    gshort                 *rates, rate = -1;
    
    /* flush x and trap errors */
    gdk_flush ();
    gdk_error_trap_push ();
    
    /* get the default display */
    display = gdk_display_get_default ();
    xdisplay = gdk_x11_display_get_xdisplay (display);
    
    /* get the number of screens */
    g_snprintf (property, sizeof (property), "/%s/NumScreens", scheme);
    num_screens = MIN (gdk_display_get_n_screens (display),
                       xfconf_channel_get_int (helper->channel, property, 0));
    
    for (n = 0; n < num_screens; n++)
    {
        /* get the screen's root window */
        screen = gdk_display_get_screen (display, n);
        root_window = gdk_screen_get_root_window (screen);
        
        /* get the screen config */
        config = XRRGetScreenInfo (xdisplay, GDK_WINDOW_XID (root_window));
        
        /* get the resolution */
        g_snprintf (property, sizeof (property), "/%s/Screen_%d/Resolution", scheme, n);
        resolution_name = xfconf_channel_get_string (helper->channel, property, "");
        
        /* get all the config sizes */
        sizes = XRRConfigSizes (config, &nsizes);
            
        /* find the resolution in the list */
        for (size_id = s = 0; s < nsizes; s++)
        {
             g_snprintf (property, sizeof (property), "%dx%d", sizes[s].width, sizes[s].height);
             if (strcmp (property, resolution_name) == 0)
             {
                 size_id = s;
                 break;
             }
        }
            
        /* cleanup */
        g_free (resolution_name);
               
        /* get the refresh rate */
        g_snprintf (property, sizeof (property), "/%s/Screen_%d/RefreshRate", scheme, n);
        loaded_rate = xfconf_channel_get_int (helper->channel, property, -1);
        rates = XRRConfigRates (config, size_id, &nrates);
        
        /* make sure the rates exists */
        for (s = 0; s < nrates; s++)
        {
            if (rates[s] == loaded_rate)
            {
                rate = rates[s];
                break;
            }
        }
        
        /* get the first refresh rate if no valid rate was found */
        if (G_UNLIKELY (rate == -1 && nrates > 0))
            rate = rates[0];
        
        /* get the rotation */
        g_snprintf (property, sizeof (property), "/%s/Screen_%d/Rotation", scheme, n);
        switch (xfconf_channel_get_int (helper->channel, property, 0))
        {
            case 90:  rotation = RR_Rotate_90;  break;
            case 180: rotation = RR_Rotate_180; break;
            case 270: rotation = RR_Rotate_270; break;
            default:  rotation = RR_Rotate_0;   break;
        }
        
        /* check if the rotation is supported, fallback to no rotation */
        rotations = XRRConfigRotations(config, &current_rotation);
        if (G_UNLIKELY ((rotations & rotation) == 0))
            rotation = RR_Rotate_0;
                
        /* check if we really need to do something */
        if (rate != XRRConfigCurrentRate (config)
            || size_id != XRRConfigCurrentConfiguration (config, &current_rotation)
            || rotation != current_rotation)
        {
            /* set the new configutation */
            XRRSetScreenConfigAndRate (xdisplay, config, GDK_WINDOW_XID (root_window),
                                       size_id, rotation, rate, CurrentTime);
        }
        
        /* free the screen config */
        XRRFreeScreenConfigInfo (config);
    }
    
    /* flush and remove the x error trap */
    gdk_flush ();
    gdk_error_trap_pop ();
}



static void            
xfce_displays_helper_channel_property_changed (XfconfChannel      *channel,
                                               const gchar        *property_name,
                                               const GValue       *value,
                                               XfceDisplaysHelper *helper)
{
    gchar             *property;
    gchar             *layout_name;
#ifdef HAS_RANDR_ONE_POINT_TWO
    XfceDisplayLayout  layout;
#endif

    if (G_UNLIKELY (value && strcmp (property_name, "/Schemes/Apply") == 0))
    {
        /* get the layout of the scheme */
        property = g_strdup_printf ("/%s/Layout", g_value_get_string (value));
        layout_name = xfconf_channel_get_string (channel, property, NULL);
        g_free (property);
        
        if (G_LIKELY (layout_name))
        {
            if (strcmp (layout_name, "Screens") == 0)
            {
                xfce_displays_helper_channel_apply_legacy (helper, g_value_get_string (value));
            }
#ifdef HAS_RANDR_ONE_POINT_TWO
            else
            {
                /* detect the layout */
                if (strcmp (layout_name, "Single") == 0)
                    layout = XFCE_DISPLAY_LAYOUT_SINGLE;
                else if (strcmp (layout_name, "Clone") == 0)
                    layout = XFCE_DISPLAY_LAYOUT_CLONE;
                else if (strcmp (layout_name, "Extend") == 0)
                    layout = XFCE_DISPLAY_LAYOUT_EXTEND;
                else
                    goto unknow_scheme_layout;
                
                /* apply the layout */
                xfce_displays_helper_channel_apply (helper, g_value_get_string (value));
            }
            
            unknow_scheme_layout:
#endif

            /* cleanup */
            g_free (layout_name);
        }
        
        /* remove the apply property */
        xfconf_channel_reset_property (channel, "/Schemes/Apply", FALSE);
    }
}
