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
/* wrappers to avoid querying too often */
typedef struct _XfceRRCrtc XfceRRCrtc;
typedef struct _XfceRROutput XfceRROutput;

struct _XfceRRCrtc
{
    RRCrtc    id;
    RRMode    mode;
    Rotation  rotation;
    Rotation  rotations;
    gint      x;
    gint      y;
    gint      noutput;
    RROutput *outputs;
    gint      npossible;
    RROutput *possible;
    gint      processed;
};

struct _XfceRROutput
{
    RROutput       id;
    XRROutputInfo *info;
    XfceRRCrtc    *pending;
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

            /* remove any leftover apply property before setting the monitor */
            xfconf_channel_reset_property (helper->channel, "/Schemes/Apply", FALSE);

            /* monitor channel changes */
            g_signal_connect (G_OBJECT (helper->channel), "property-changed", 
                              G_CALLBACK (xfce_displays_helper_channel_property_changed), helper);

#ifdef HAS_RANDR_ONE_POINT_TWO
            if (major == 1 && minor >= 2)
            {
                helper->has_1_3 = (major == 1 && minor >= 3);
                /* restore the default scheme */
                xfce_displays_helper_channel_apply (helper, "Default");
            }
            else
#endif
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
xfce_displays_helper_process_screen_size (gint  mode_width,
                                          gint  mode_height,
                                          gint  crtc_pos_x,
                                          gint  crtc_pos_y,
                                          gint *width,
                                          gint *height,
                                          gint *mm_width,
                                          gint *mm_height)
{
    gdouble dpi = 0;

    *width = MAX (*width, crtc_pos_x + mode_width);
    *height = MAX (*height, crtc_pos_y + mode_height);

    dpi = 25.4 * gdk_screen_height ();
    dpi /= gdk_screen_height_mm ();

    if (dpi <= 0)
    {
        *mm_width = gdk_screen_width_mm ();
        *mm_height = gdk_screen_height_mm ();
    }
    else
    {
        *mm_width = 25.4 * (*width) / dpi;
        *mm_height = 25.4 * (*height) / dpi;
    }
}



static XfceRRCrtc *
xfce_displays_helper_list_crtcs (Display            *xdisplay,
                                 XRRScreenResources *resources)
{
    XfceRRCrtc  *crtcs;
    XRRCrtcInfo *crtc_info;
    gint         n, m;

    g_return_val_if_fail (xdisplay != NULL, NULL);
    g_return_val_if_fail (resources != NULL, NULL);

    /* get all existing CRTCs */
    crtcs = g_new0 (XfceRRCrtc, resources->ncrtc);
    for (n = 0; n < resources->ncrtc; ++n)
    {
        crtcs[n].id = resources->crtcs[n];
        crtc_info = XRRGetCrtcInfo (xdisplay, resources, resources->crtcs[n]);
        crtcs[n].mode = crtc_info->mode;
        crtcs[n].rotation = crtc_info->rotation;
        crtcs[n].rotations = crtc_info->rotations;
        crtcs[n].x = crtc_info->x;
        crtcs[n].y = crtc_info->y;
        crtcs[n].noutput = crtc_info->noutput;
        crtcs[n].outputs = NULL;
        if (crtc_info->noutput > 0)
        {
            crtcs[n].outputs = g_new0 (RROutput, crtc_info->noutput);
            for (m = 0; m < crtc_info->noutput; ++m)
                crtcs[n].outputs[m] = crtc_info->outputs[m];
        }
        crtcs[n].npossible = crtc_info->npossible;
        crtcs[n].possible = NULL;
        if (crtc_info->npossible > 0)
        {
            crtcs[n].possible = g_new0 (RROutput, crtc_info->npossible);
            for (m = 0; m < crtc_info->npossible; ++m)
                crtcs[n].possible[m] = crtc_info->possible[m];
        }
        crtcs[n].processed = FALSE;
        XRRFreeCrtcInfo (crtc_info);
    }

    return crtcs;
}



static void
xfce_displays_helper_cleanup_crtc (XfceRRCrtc *crtc)
{
    if (crtc == NULL)
        return;

    if (crtc->outputs != NULL)
        g_free (crtc->outputs);
    if (crtc->possible != NULL)
        g_free (crtc->possible);
}



static void
xfce_displays_helper_free_output (XfceRROutput *output)
{
    if (output == NULL)
        return;

    XRRFreeOutputInfo (output->info);
    xfce_displays_helper_cleanup_crtc (output->pending);
    g_free (output->pending);
    g_free (output);
}



static GHashTable *
xfce_displays_helper_list_outputs (Display            *xdisplay,
                                   XRRScreenResources *resources)
{
    GHashTable    *outputs;
    XRROutputInfo *info;
    XfceRROutput  *output;
    gint           n;

    g_return_val_if_fail (xdisplay != NULL, NULL);
    g_return_val_if_fail (resources != NULL, NULL);
    g_return_val_if_fail (resources->noutput > 0, NULL);

    /* keys (info->name) are owned by X, do not free them */
    outputs = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                     (GDestroyNotify) xfce_displays_helper_free_output);

    /* get all connected outputs */
    for (n = 0; n < resources->noutput; ++n)
    {
        info = XRRGetOutputInfo (xdisplay, resources, resources->outputs[n]);

        if (info->connection != RR_Connected)
        {
            XRRFreeOutputInfo (info);
            continue;
        }

        output = g_new0 (XfceRROutput, 1);
        output->id = resources->outputs[n];
        output->info = info;
        /* this will contain the settings to apply (filled in later) */
        output->pending = NULL;

        /* enable quick lookup by name */
        g_hash_table_insert (outputs, info->name, output);
    }

    return outputs;
}



static XfceRRCrtc *
xfce_displays_helper_find_crtc_by_id (XRRScreenResources *resources,
                                      XfceRRCrtc         *crtcs,
                                      RRCrtc              id)
{
    gint n;

    g_return_val_if_fail (resources != NULL, NULL);
    g_return_val_if_fail (crtcs != NULL, NULL);

    for (n = 0; n < resources->ncrtc; ++n)
    {
        if (crtcs[n].id == id)
            return &crtcs[n];
    }

    return NULL;
}



static XfceRRCrtc *
xfce_displays_helper_find_clonable_crtc (XRRScreenResources *resources,
                                         XfceRRCrtc         *crtcs,
                                         XfceRROutput       *output)
{
    gint m, n, candidate;

    g_return_val_if_fail (resources != NULL, NULL);
    g_return_val_if_fail (crtcs != NULL, NULL);
    g_return_val_if_fail (output != NULL, NULL);

    for (n = 0; n < resources->ncrtc; ++n)
    {
        if (crtcs[n].processed && crtcs[n].x == output->pending->x
            && crtcs[n].y == output->pending->y
            && crtcs[n].mode == output->pending->mode
            && crtcs[n].rotation == output->pending->rotation)
        {
            /* we found a CRTC already enabled with the exact values
               => might be suitable for a clone, check that it can be
               connected to the new output */
            candidate = FALSE;
            for (m = 0; m < crtcs[n].npossible; ++m)
            {
                if (crtcs[n].possible[m] == output->id)
                {
                    candidate = TRUE;
                    break;
                }
            }

            /* definitely suitable for a clone */
            if (candidate)
                return &crtcs[n];
        }
    }

    return NULL;
}



static XfceRRCrtc *
xfce_displays_helper_find_usable_crtc (XRRScreenResources *resources,
                                       XfceRRCrtc         *crtcs,
                                       XfceRROutput       *output)
{
    gint m, n;

    g_return_val_if_fail (resources != NULL, NULL);
    g_return_val_if_fail (crtcs != NULL, NULL);
    g_return_val_if_fail (output != NULL, NULL);

    /* if there is one already active, return it */
    if (output->info->crtc != None)
        return xfce_displays_helper_find_crtc_by_id (resources, crtcs,
                                                     output->info->crtc);

    /* try to find one that is not already used by another output */
    for (n = 0; n < resources->ncrtc; ++n)
    {
        if (crtcs[n].noutput > 0)
            continue;

        for (m = 0; m < crtcs[n].npossible; ++m)
        {
            if (crtcs[n].possible[m] == output->id)
                return &crtcs[n];
        }
    }

    /* none available */
    g_warning ("No CRTC found for %s.", output->info->name);
    return NULL;
}



static Status
xfce_displays_helper_apply_crtc (Display            *xdisplay,
                                 XRRScreenResources *resources,
                                 XfceRRCrtc         *crtc,
                                 XfceRRCrtc         *pending)
{
    Status ret;

    g_return_val_if_fail (xdisplay != NULL, RRSetConfigSuccess);
    g_return_val_if_fail (resources != NULL, RRSetConfigSuccess);
    g_return_val_if_fail (crtc != NULL, RRSetConfigSuccess);

    if (G_LIKELY (pending != NULL))
        ret = XRRSetCrtcConfig (xdisplay, resources, crtc->id, CurrentTime,
                                pending->x, pending->y, pending->mode,
                                pending->rotation, pending->outputs,
                                pending->noutput);
    else
        ret = XRRSetCrtcConfig (xdisplay, resources, crtc->id, CurrentTime,
                                0, 0, None, RR_Rotate_0, NULL, 0);

    /* update our view */
    if (ret == RRSetConfigSuccess)
    {
        g_free (crtc->outputs);
        crtc->outputs = NULL;
        if (G_LIKELY (pending != NULL))
        {
            crtc->mode = pending->mode;
            crtc->rotation = pending->rotation;
            crtc->x = pending->x;
            crtc->y = pending->y;
            crtc->noutput = pending->noutput;
            crtc->outputs = g_memdup (pending->outputs,
                                      pending->noutput * sizeof (RROutput));
        }
        else
        {
            crtc->mode = None;
            crtc->rotation = RR_Rotate_0;
            crtc->noutput = crtc->x = crtc->y = 0;
        }
        crtc->processed = TRUE;
    }

    return ret;
}



static Status
xfce_displays_helper_disable_crtc (Display            *xdisplay,
                                   XRRScreenResources *resources,
                                   XfceRRCrtc         *crtc)
{
    g_return_val_if_fail (xdisplay != NULL, RRSetConfigSuccess);
    g_return_val_if_fail (resources != NULL, RRSetConfigSuccess);

    /* already disabled */
    if (!crtc)
        return RRSetConfigSuccess;

    return xfce_displays_helper_apply_crtc (xdisplay, resources, crtc, NULL);
}


static void
xfce_displays_helper_set_outputs (XfceRRCrtc   *crtc,
                                  XfceRROutput *output)
{
    gint n, found;

    g_return_if_fail (crtc != NULL);
    g_return_if_fail (output != NULL);

    if (crtc->noutput == 0)
    {
        /* no output connected, easy, put the current one */
        output->pending->noutput = 1;
        output->pending->outputs = g_new0 (RROutput, 1);
        *(output->pending->outputs) = output->id;
        return;
    }

    found = FALSE;
    /* some outputs are already connected, check if the current one is present */
    for (n = 0; n < crtc->noutput; ++n)
    {
        if (crtc->outputs[n] == output->id)
        {
            found = TRUE;
            break;
        }
    }

    output->pending->noutput = found ? crtc->noutput : crtc->noutput + 1;
    output->pending->outputs = g_new0 (RROutput, output->pending->noutput);
    /* readd the existing ones */
    for (n = 0; n < crtc->noutput; ++n)
        output->pending->outputs[n] = crtc->outputs[n];
    /* add the current one if needed */
    if (!found)
        output->pending->outputs[++n] = output->id;
}



static void
xfce_displays_helper_channel_apply (XfceDisplaysHelper *helper,
                                    const gchar        *scheme)
{
    GdkDisplay         *display;
    Display            *xdisplay;
    GdkWindow          *root_window;
    XRRScreenResources *resources;
    XfceRRCrtc         *crtcs, *crtc, *pending;
    gchar               property[512];
    gint                min_width, min_height, max_width, max_height;
    gint                mm_width, mm_height, width, height;
    gint                l, m, n, num_outputs, output_rot;
#ifdef HAS_RANDR_ONE_POINT_THREE
    gint                is_primary;
#endif
    gchar              *value;
    gdouble             output_rate, rate;
    XRRModeInfo        *mode_info;
    GHashTable         *connected_outputs;
    XfceRROutput       *output;

    /* flush x and trap errors */
    gdk_flush ();
    gdk_error_trap_push ();

    /* get the default display */
    display = gdk_display_get_default ();
    xdisplay = gdk_x11_display_get_xdisplay (display);
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
    resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    /* get the range of screen sizes */
    mm_width = mm_height = width = height = 0;
    if (!XRRGetScreenSizeRange (xdisplay, GDK_WINDOW_XID (root_window),
                                &min_width, &min_height, &max_width, &max_height))
    {
        g_warning ("Unable to get the range of screen sizes, aborting.");
        XRRFreeScreenResources (resources);
        gdk_flush ();
        gdk_error_trap_pop ();
        return;
    }

    /* get all existing CRTCs */
    crtcs = xfce_displays_helper_list_crtcs (xdisplay, resources);

    /* then all connected outputs */
    connected_outputs = xfce_displays_helper_list_outputs (xdisplay, resources);

    /* get the number of saved outputs */
    g_snprintf (property, sizeof (property), "/%s/NumOutputs", scheme);
    num_outputs = xfconf_channel_get_int (helper->channel, property, 0);

    for (n = 0; n < num_outputs; ++n)
    {
        /* get the output name */
        g_snprintf (property, sizeof (property), "/%s/Output%d", scheme, n);
        value = xfconf_channel_get_string (helper->channel, property, NULL);

        /* does this output exist? */
        output = g_hash_table_lookup (connected_outputs, value);
        g_free (value);

        if (output == NULL)
            continue;

        g_snprintf (property, sizeof (property), "/%s/Output%d/Resolution", scheme, n);
        value = xfconf_channel_get_string (helper->channel, property, NULL);

        /* outputs that have to be disabled are stored without resolution */
        if (value == NULL)
        {
            crtc = xfce_displays_helper_find_crtc_by_id (resources, crtcs,
                                                         output->info->crtc);
            if (xfce_displays_helper_disable_crtc (xdisplay, resources, crtc) != RRSetConfigSuccess)
                g_warning ("Failed to disable CRTC for output %s.", output->info->name);

            continue;
        }

        g_snprintf (property, sizeof (property), "/%s/Output%d/RefreshRate", scheme, n);
        output_rate = xfconf_channel_get_double (helper->channel, property, 0.0);

        /* prepare pending settings */
        pending = g_new0 (XfceRRCrtc, 1);

        /* does this mode exist for the output? */
        pending->mode = None;
        for (m = 0; m < output->info->nmode; ++m)
        {
            /* walk all modes */
            for (l = 0; l < resources->nmode; ++l)
            {
                /* get the mode info */
                mode_info = &resources->modes[l];

                /* does the mode info match the mode we seek? */
                if (mode_info->id != output->info->modes[m])
                    continue;

                /* calculate the refresh rate */
                rate = (gfloat) mode_info->dotClock / ((gfloat) mode_info->hTotal * (gfloat) mode_info->vTotal);

                /* find the mode corresponding to the saved values */
                if (((int) rate == (int) output_rate)
                    && (g_strcmp0 (mode_info->name, value) == 0))
                {
                    pending->mode = mode_info->id;
                    break;
                }
            }

            /* found it */
            if (pending->mode != None)
                break;
        }

        /* unsupported mode, abort for this output */
        if (pending->mode == None)
        {
            g_warning ("Unknown mode '%s @ %.1f' for output %s.\n",
                       value, output_rate, output->info->name);
            g_free (pending);
            g_free (value);

            continue;
        }
        g_free (value);

        g_snprintf (property, sizeof (property), "/%s/Output%d/Rotation", scheme, n);
        output_rot = xfconf_channel_get_int (helper->channel, property, 0);
        /* convert to a Rotation */
        switch (output_rot)
        {
            case 90:  pending->rotation = RR_Rotate_90;  break;
            case 180: pending->rotation = RR_Rotate_180; break;
            case 270: pending->rotation = RR_Rotate_270; break;
            default:  pending->rotation = RR_Rotate_0;   break;
        }

        g_snprintf (property, sizeof (property), "/%s/Output%d/Reflection", scheme, n);
        value = xfconf_channel_get_string (helper->channel, property, "0");
        /* convert to a Rotation */
        if (g_strcmp0 (value, "X") == 0)
            pending->rotation |= RR_Reflect_X;
        else if (g_strcmp0 (value, "Y") == 0)
            pending->rotation |= RR_Reflect_Y;
        else if (g_strcmp0 (value, "XY") == 0)
            pending->rotation |= (RR_Reflect_X|RR_Reflect_Y);

        g_free (value);

        g_snprintf (property, sizeof (property), "/%s/Output%d/Position/X", scheme, n);
        pending->x = xfconf_channel_get_int (helper->channel, property, 0);

        g_snprintf (property, sizeof (property), "/%s/Output%d/Position/Y", scheme, n);
        pending->y = xfconf_channel_get_int (helper->channel, property, 0);

        /* done */
        output->pending = pending;

#ifdef HAS_RANDR_ONE_POINT_THREE
        g_snprintf (property, sizeof (property), "/%s/Output%d/Primary", scheme, n);
        is_primary = xfconf_channel_get_bool (helper->channel, property, FALSE);
#endif

        /* first, search for a possible clone */
        crtc = xfce_displays_helper_find_clonable_crtc (resources, crtcs, output);

        /* if it failed, forget about it and pick a free one */
        if (!crtc)
            crtc = xfce_displays_helper_find_usable_crtc (resources, crtcs, output);

        if (crtc)
        {
            /* unsupported rotation, abort for this output */
            if ((crtc->rotations & output->pending->rotation) == 0)
            {
                g_warning ("Unsupported rotation for %s.\n", output->info->name);
                continue;
            }

            xfce_displays_helper_set_outputs (crtc, output);

            /* get the sizes of the mode to enforce */
            if ((output->pending->rotation & (RR_Rotate_90|RR_Rotate_270)) != 0)
                xfce_displays_helper_process_screen_size (resources->modes[l].height,
                                                          resources->modes[l].width,
                                                          output->pending->x,
                                                          output->pending->y, &width,
                                                          &height, &mm_width, &mm_height);
            else
                xfce_displays_helper_process_screen_size (resources->modes[l].width,
                                                          resources->modes[l].height,
                                                          output->pending->x,
                                                          output->pending->y, &width,
                                                          &height, &mm_width, &mm_height);

            /* check if we really need to do something */
            if (crtc->mode != output->pending->mode
                || crtc->rotation != output->pending->rotation
                || crtc->x != output->pending->x
                || crtc->y != output->pending->y
                || crtc->noutput != output->pending->noutput)
            {
                if (xfce_displays_helper_apply_crtc (xdisplay, resources, crtc,
                                                     output->pending) != RRSetConfigSuccess)
                    g_warning ("Failed to configure %s.", output->info->name);
            }
            else
                g_debug ("Nothing to do for %s.\n", output->info->name);
        }

#ifdef HAS_RANDR_ONE_POINT_THREE
        if (helper->has_1_3 && is_primary)
            XRRSetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window), output->id);
#endif
    }

    /* set the screen size only if it's really needed and valid */
    if (width >= min_width && width <= max_width
        && height >= min_height && height <= max_height
        && (width != gdk_screen_width ()
            || height != gdk_screen_height ()
            || mm_width != gdk_screen_width_mm ()
            || mm_height != gdk_screen_height_mm ()))
        XRRSetScreenSize (xdisplay, GDK_WINDOW_XID (root_window),
                          width, height, mm_width, mm_height);

    /* Free our output cache */
    g_hash_table_unref (connected_outputs);

    /* cleanup our CRTC cache */
    for (n = 0; n < resources->ncrtc; ++n)
    {
        xfce_displays_helper_cleanup_crtc (&crtcs[n]);
    }
    g_free (crtcs);

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

    if (G_UNLIKELY (G_VALUE_HOLDS_STRING(value) && strcmp (property_name, "/Schemes/Apply") == 0))
    {
        /* get the layout of the scheme */
        property = g_strdup_printf ("/%s/Layout", g_value_get_string (value));
        layout_name = xfconf_channel_get_string (channel, property, NULL);
        g_free (property);
        
        if (G_LIKELY (layout_name))
        {
#ifdef HAS_RANDR_ONE_POINT_TWO
            if (strcmp (layout_name, "Outputs") == 0)
                xfce_displays_helper_channel_apply (helper, g_value_get_string (value));
            else
#endif
            {
                if (strcmp (layout_name, "Screens") == 0)
                    xfce_displays_helper_channel_apply_legacy (helper, g_value_get_string (value));
                else
                    g_warning ("Unknown layout: %s\n", layout_name);
            }

            /* cleanup */
            g_free (layout_name);
        }
        
        /* remove the apply property */
        xfconf_channel_reset_property (channel, "/Schemes/Apply", FALSE);
    }
}
