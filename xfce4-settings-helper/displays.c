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
#else
#undef HAS_RANDR_ONE_POINT_TWO
#endif

#undef HAS_RANDR_ONE_POINT_TWO

static void            xfce_displays_helper_class_init                     (XfceDisplaysHelperClass *klass);
static void            xfce_displays_helper_init                           (XfceDisplaysHelper      *helper);
static void            xfce_displays_helper_finalize                       (GObject                 *object);
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
    /* open the channel */
    helper->channel = xfconf_channel_new ("displays");
    
    /* monitor channel changes */
    g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_displays_helper_channel_property_changed), helper);
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
                                    const gchar        *scheme,
                                    XfceDisplayLayout   layout)
{
    g_message ("Apply randr 1.2 scheme '%s'", scheme);
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
                xfce_displays_helper_channel_apply (helper, g_value_get_string (value), layout);
            }
            
            unknow_scheme_layout:
#endif

            /* cleanup */
            g_free (layout_name);
        }
        
        /* remove the apply property */
        xfconf_channel_remove_property (channel, "/Schemes/Apply");
    }
}
