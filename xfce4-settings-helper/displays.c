/* $Id$ */
/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010 Lionel Le Folgoc <lionel@lefolgoc.net>
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
#include <libxfce4ui/libxfce4ui.h>

#include <X11/extensions/Xrandr.h>

#include "displays.h"

/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#else
#undef HAS_RANDR_ONE_POINT_THREE
#endif

static void            xfce_displays_helper_finalize                       (GObject                 *object);
static void            xfce_displays_helper_channel_apply                  (XfceDisplaysHelper      *helper,
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
            && (major > 1 || (major == 1 && minor >= 2)))
        {
            /* open the channel */
            helper->channel = xfconf_channel_new ("displays");

            /* remove any leftover apply property before setting the monitor */
            xfconf_channel_reset_property (helper->channel, "/Schemes/Apply", FALSE);

            /* monitor channel changes */
            g_signal_connect (G_OBJECT (helper->channel), "property-changed",
                              G_CALLBACK (xfce_displays_helper_channel_property_changed), helper);

            helper->has_1_3 = (major > 1 || (major == 1 && minor >= 3));
            /* restore the default scheme */
            xfce_displays_helper_channel_apply (helper, "Default");
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

    g_assert (width && height && mm_width && mm_height);

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
    gint         n;

    g_assert (xdisplay && resources);

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
            crtcs[n].outputs = g_memdup (crtc_info->outputs,
                                         crtc_info->noutput * sizeof (RROutput));

        crtcs[n].npossible = crtc_info->npossible;
        crtcs[n].possible = NULL;
        if (crtc_info->npossible > 0)
            crtcs[n].possible = g_memdup (crtc_info->possible,
                                          crtc_info->npossible * sizeof (RROutput));

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



static GPtrArray *
xfce_displays_helper_list_outputs (Display            *xdisplay,
                                   XRRScreenResources *resources,
                                   gint               *nactive)
{
    GPtrArray     *outputs;
    XRROutputInfo *info;
    XfceRROutput  *output;
    gint           n;

    g_assert (xdisplay && resources && nactive);

    outputs = g_ptr_array_new_with_free_func (
        (GDestroyNotify) xfce_displays_helper_free_output);

    /* get all connected outputs */
    *nactive = 0;
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

        /* cache it */
        g_ptr_array_add (outputs, output);

        /* return the number of active outputs */
        if (info->crtc != None)
            ++(*nactive);
    }

    return outputs;
}



static XfceRRCrtc *
xfce_displays_helper_find_crtc_by_id (XRRScreenResources *resources,
                                      XfceRRCrtc         *crtcs,
                                      RRCrtc              id)
{
    gint n;

    g_assert (resources && crtcs);

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

    g_assert (resources && crtcs && output);

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

    g_assert (resources && crtcs && output);

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

    g_assert (xdisplay && resources && crtc && pending);

    ret = XRRSetCrtcConfig (xdisplay, resources, crtc->id, CurrentTime,
                            pending->x, pending->y, pending->mode,
                            pending->rotation, pending->outputs,
                            pending->noutput);

    /* update our view */
    if (ret == RRSetConfigSuccess)
    {
        g_free (crtc->outputs);
        crtc->outputs = NULL;
        crtc->mode = pending->mode;
        crtc->rotation = pending->rotation;
        crtc->x = pending->x;
        crtc->y = pending->y;
        crtc->noutput = pending->noutput;
        crtc->outputs = g_memdup (pending->outputs,
                                  pending->noutput * sizeof (RROutput));
        crtc->processed = TRUE;
    }

    return ret;
}



static void
xfce_displays_helper_set_outputs (XfceRRCrtc   *crtc,
                                  XfceRROutput *output)
{
    gint n, found;

    g_assert (crtc && output);

    /* nothing to do */
    if (output->pending->mode == None)
        return;

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
    gint                mm_width, mm_height, width, height, mode_height, mode_width;
    gint                l, m, output_rot, nactive;
    guint               n;
    GValue             *value;
    const gchar        *str_value;
    gdouble             output_rate, rate;
    XRRModeInfo        *mode_info;
    GPtrArray          *connected_outputs;
    GHashTable         *saved_outputs;
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
        g_critical ("Unable to get the range of screen sizes, aborting.");
        goto err_cleanup;
    }

    /* get all existing CRTCs */
    crtcs = xfce_displays_helper_list_crtcs (xdisplay, resources);

    /* then all connected outputs */
    connected_outputs = xfce_displays_helper_list_outputs (xdisplay, resources, &nactive);

    /* finally the list of saved outputs from xfconf */
    g_snprintf (property, sizeof (property), "/%s", scheme);
    saved_outputs = xfconf_channel_get_properties (helper->channel, property);

    /* nothing saved, nothing to do */
    if (saved_outputs == NULL)
        goto err_cleanup;

    /* first loop, loads all the outputs, and gets the number of active ones */
    for (n = 0; n < connected_outputs->len; ++n)
    {
        output = g_ptr_array_index (connected_outputs, n);

        /* does this output exist in xfconf? */
        g_snprintf (property, sizeof (property), "/%s/%s", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);

        if (value == NULL || !G_VALUE_HOLDS_STRING (value))
            continue;

#ifdef HAS_RANDR_ONE_POINT_THREE
        if (helper->has_1_3)
        {
            /* is it the primary output? */
            g_snprintf (property, sizeof (property), "/%s/%s/Primary", scheme,
                        output->info->name);
            value = g_hash_table_lookup (saved_outputs, property);
            if (G_VALUE_HOLDS_BOOLEAN (value) && g_value_get_boolean (value))
                XRRSetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window), output->id);
        }
#endif

        /* status */
        g_snprintf (property, sizeof (property), "/%s/%s/Active", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);

        /* prepare pending settings */
        pending = g_new0 (XfceRRCrtc, 1);
        pending->mode = None;
        pending->rotation = RR_Rotate_0;
        output->pending = pending;

        /* disable inactive outputs  */
        if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value)
            || !g_value_get_boolean (value))
        {
            --nactive;
            continue;
        }

        /* resolution */
        g_snprintf (property, sizeof (property), "/%s/%s/Resolution",
                    scheme, output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (value == NULL || !G_VALUE_HOLDS_STRING (value))
            continue;
        else
            str_value = g_value_get_string (value);

        /* refresh rate */
        g_snprintf (property, sizeof (property), "/%s/%s/RefreshRate", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_DOUBLE (value))
            output_rate = g_value_get_double (value);
        else
            output_rate = 0.0;

        /* does this mode exist for the output? */
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
                    && (g_strcmp0 (mode_info->name, str_value) == 0))
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
                       str_value, output_rate, output->info->name);
            g_free (pending);
            output->pending = NULL;
            continue;
        }
        else
            ++nactive;

        /* rotation */
        g_snprintf (property, sizeof (property), "/%s/%s/Rotation", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_INT (value))
            output_rot = g_value_get_int (value);
        else
            output_rot = 0;

        /* convert to a Rotation */
        switch (output_rot)
        {
            case 90:  pending->rotation = RR_Rotate_90;  break;
            case 180: pending->rotation = RR_Rotate_180; break;
            case 270: pending->rotation = RR_Rotate_270; break;
            default:  pending->rotation = RR_Rotate_0;   break;
        }

        /* reflection */
        g_snprintf (property, sizeof (property), "/%s/%s/Reflection", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_STRING (value))
            str_value = g_value_get_string (value);
        else
            str_value = "0";

        /* convert to a Rotation */
        if (g_strcmp0 (str_value, "X") == 0)
            pending->rotation |= RR_Reflect_X;
        else if (g_strcmp0 (str_value, "Y") == 0)
            pending->rotation |= RR_Reflect_Y;
        else if (g_strcmp0 (str_value, "XY") == 0)
            pending->rotation |= (RR_Reflect_X|RR_Reflect_Y);

        /* position, x */
        g_snprintf (property, sizeof (property), "/%s/%s/Position/X", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_INT (value))
            pending->x = g_value_get_int (value);
        else
            pending->x = 0;

        /* position, y */
        g_snprintf (property, sizeof (property), "/%s/%s/Position/Y", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_INT (value))
            pending->y = g_value_get_int (value);
        else
            pending->y = 0;
    }

    /* safety check */
    if (nactive < 1)
    {
        g_critical ("Stored Xfconf properties disable all outputs, aborting.");
        goto err_cleanup;
    }

    /* second loop, applies the settings */
    for (n = 0; n < connected_outputs->len; ++n)
    {
        output = g_ptr_array_index (connected_outputs, n);

        /* nothing to apply */
        if (output->pending == NULL)
            continue;

        crtc = NULL;
        /* outputs to disable */
        if (output->pending->mode == None)
            crtc = xfce_displays_helper_find_crtc_by_id (resources, crtcs,
                                                         output->info->crtc);
        else
        {
            /* else, search for a possible clone */
            crtc = xfce_displays_helper_find_clonable_crtc (resources, crtcs, output);

            /* if it failed, forget about it and pick a free one */
            if (!crtc)
                crtc = xfce_displays_helper_find_usable_crtc (resources, crtcs, output);
        }

        if (crtc)
        {
            /* rotation support */
            if ((crtc->rotations & output->pending->rotation) == 0)
            {
                g_warning ("Unsupported rotation for %s. Fallback to RR_Rotate_0.",
                           output->info->name);
                output->pending->rotation = RR_Rotate_0;
            }

            xfce_displays_helper_set_outputs (crtc, output);

            mode_height = mode_width = 0;
            /* get the sizes of the mode to enforce */
            for (m = 0; m < resources->nmode; ++m)
            {
                /* get the mode info */
                mode_info = &resources->modes[m];

                /* does the mode info match the mode we seek? */
                if (mode_info->id != output->pending->mode)
                    continue;

                /* store the dimensions */
                mode_height = resources->modes[m].height;
                mode_width = resources->modes[m].width;
                break;
            }


            if ((output->pending->rotation & (RR_Rotate_90|RR_Rotate_270)) != 0)
                xfce_displays_helper_process_screen_size (mode_height, mode_width,
                                                          output->pending->x,
                                                          output->pending->y, &width,
                                                          &height, &mm_width, &mm_height);
            else
                xfce_displays_helper_process_screen_size (mode_width, mode_height,
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
        }
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

err_cleanup:
    /* Free the xfconf properties */
    if (saved_outputs)
        g_hash_table_destroy (saved_outputs);

    /* Free our output cache */
    g_ptr_array_unref (connected_outputs);

    /* cleanup our CRTC cache */
    for (m = 0; m < resources->ncrtc; ++m)
    {
        xfce_displays_helper_cleanup_crtc (&crtcs[m]);
    }
    g_free (crtcs);

    /* free the screen resources */
    XRRFreeScreenResources (resources);

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
    if (G_UNLIKELY (G_VALUE_HOLDS_STRING (value) &&
        g_strcmp0 (property_name, "/Schemes/Apply") == 0))
    {
        /* apply */
        xfce_displays_helper_channel_apply (helper, g_value_get_string (value));
        /* remove the apply property */
        xfconf_channel_reset_property (channel, "/Schemes/Apply", FALSE);
    }
}
