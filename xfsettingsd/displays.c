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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include <X11/extensions/Xrandr.h>

#include "debug.h"
#include "displays.h"

/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#else
#undef HAS_RANDR_ONE_POINT_THREE
#endif



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
    gint      width;
    gint      height;
    gint      x;
    gint      y;
    gint      noutput;
    RROutput *outputs;
    gint      npossible;
    RROutput *possible;
    gint      changed;
};

struct _XfceRROutput
{
    RROutput       id;
    XRROutputInfo *info;
};



G_DEFINE_TYPE (XfceDisplaysHelper, xfce_displays_helper, G_TYPE_OBJECT);



static void
xfce_displays_helper_class_init (XfceDisplaysHelperClass *klass)
{

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
            helper->channel = xfconf_channel_get ("displays");

            /* remove any leftover apply property before setting the monitor */
            xfconf_channel_reset_property (helper->channel, "/Schemes/Apply", FALSE);

            /* monitor channel changes */
            g_signal_connect (G_OBJECT (helper->channel), "property-changed",
                              G_CALLBACK (xfce_displays_helper_channel_property_changed), helper);

#ifdef HAS_RANDR_ONE_POINT_THREE
            helper->has_1_3 = (major > 1 || (major == 1 && minor >= 3));
#endif
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
xfce_displays_helper_process_screen_size (gint  mode_width,
                                          gint  mode_height,
                                          gint  crtc_pos_x,
                                          gint  crtc_pos_y,
                                          gint *width,
                                          gint *height,
                                          gint *mm_width,
                                          gint *mm_height)
{
    g_assert (width && height && mm_width && mm_height);

    *width = MAX (*width, crtc_pos_x + mode_width);
    *height = MAX (*height, crtc_pos_y + mode_height);

    /* The 'physical size' of an X screen is meaningless if that screen
     * can consist of many monitors. So just pick a size that make the
     * dpi 96.
     *
     * Firefox and Evince apparently believe what X tells them.
     */
    *mm_width = (*width / 96.0) * 25.4 + 0.5;
    *mm_height = (*height / 96.0) * 25.4 + 0.5;
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
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected CRTC %lu.", resources->crtcs[n]);

        crtcs[n].id = resources->crtcs[n];
        crtc_info = XRRGetCrtcInfo (xdisplay, resources, resources->crtcs[n]);
        crtcs[n].mode = crtc_info->mode;
        crtcs[n].rotation = crtc_info->rotation;
        crtcs[n].rotations = crtc_info->rotations;
        crtcs[n].width = crtc_info->width;
        crtcs[n].height = crtc_info->height;
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

        crtcs[n].changed = FALSE;
        XRRFreeCrtcInfo (crtc_info);
    }

    return crtcs;
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
    g_free (output);
}



static GPtrArray *
xfce_displays_helper_list_outputs (Display            *xdisplay,
                                   XRRScreenResources *resources,
                                   XfceRRCrtc         *crtcs,
                                   gint               *nactive)
{
    GPtrArray     *outputs;
    XRROutputInfo *info;
    XfceRROutput  *output;
    XfceRRCrtc    *crtc;
    gint           n;

    g_assert (xdisplay && resources && nactive);

    outputs = g_ptr_array_new ();

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

        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected output %lu %s.", output->id,
                        output->info->name);

        /* track active outputs */
        crtc = xfce_displays_helper_find_crtc_by_id (resources, crtcs,
                                                     output->info->crtc);
        if (crtc && crtc->mode != None)
        {
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s is active.", output->info->name);
            ++(*nactive);
        }

        /* cache it */
        g_ptr_array_add (outputs, output);
    }

    return outputs;
}



static XfceRRCrtc *
xfce_displays_helper_find_usable_crtc (XRRScreenResources *resources,
                                       XfceRRCrtc         *crtcs,
                                       XfceRROutput       *output)
{
    gint m, n;

    g_assert (resources && crtcs && output);

    /* if there is one already assigned, return it */
    if (output->info->crtc != None)
        return xfce_displays_helper_find_crtc_by_id (resources, crtcs,
                                                     output->info->crtc);

    /* try to find one that is not already used by another output */
    for (n = 0; n < resources->ncrtc; ++n)
    {
        if (crtcs[n].noutput > 0 || crtcs[n].changed)
            continue;

        for (m = 0; m < crtcs[n].npossible; ++m)
        {
            if (crtcs[n].possible[m] == output->id)
                return &crtcs[n];
        }
    }

    /* none available */
    return NULL;
}



static Status
xfce_displays_helper_disable_crtc (Display            *xdisplay,
                                   XRRScreenResources *resources,
                                   RRCrtc              crtc)
{
    g_assert (xdisplay && resources);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Disabling CRTC %lu.", crtc);

    return XRRSetCrtcConfig (xdisplay, resources, crtc, CurrentTime,
                             0, 0, None, RR_Rotate_0, NULL, 0);
}



static Status
xfce_displays_helper_apply_crtc (Display            *xdisplay,
                                 XRRScreenResources *resources,
                                 XfceRRCrtc         *crtc)
{
    Status ret;

    g_assert (xdisplay && resources && crtc);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Configuring CRTC %lu.", crtc->id);

    if (crtc->mode == None)
        ret = xfce_displays_helper_disable_crtc (xdisplay, resources, crtc->id);
    else
        ret = XRRSetCrtcConfig (xdisplay, resources, crtc->id, CurrentTime,
                                crtc->x, crtc->y, crtc->mode, crtc->rotation,
                                crtc->outputs, crtc->noutput);

    if (ret == RRSetConfigSuccess)
        crtc->changed = FALSE;

    return ret;
}



static void
xfce_displays_helper_set_outputs (XfceRRCrtc *crtc,
                                  RROutput    output)
{
    gint n;

    g_assert (crtc);

    for (n = 0; n < crtc->noutput; ++n)
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu, output list[%d] -> %lu.", crtc->id, n,
                        crtc->outputs[n]);

    /* check if the output is already present */
    for (n = 0; n < crtc->noutput; ++n)
    {
        if (crtc->outputs[n] == output)
            return;
    }


    if (crtc->outputs)
        crtc->outputs = g_realloc (crtc->outputs, (crtc->noutput + 1) * sizeof (RROutput));
    else
        crtc->outputs = g_new0 (RROutput, 1);

    g_assert (crtc->outputs);

    crtc->outputs [crtc->noutput++] = output;
    crtc->changed = TRUE;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu, output list[%d] -> %lu.", crtc->id,
                    crtc->noutput - 1, crtc->outputs[crtc->noutput - 1]);
}



static void
xfce_displays_helper_channel_apply (XfceDisplaysHelper *helper,
                                    const gchar        *scheme)
{
    GdkDisplay         *display;
    Display            *xdisplay;
    GdkWindow          *root_window;
    XRRScreenResources *resources;
    XfceRRCrtc         *crtcs, *crtc;
    gchar               property[512];
    gint                min_width, min_height, max_width, max_height;
    gint                mm_width, mm_height, width, height;
    gint                x, y, min_x, min_y;
    gint                l, m, int_value, nactive;
    guint               n;
    GValue             *value;
    const gchar        *str_value;
    gdouble             output_rate, rate;
    RRMode              valid_mode;
    Rotation            rot;
    GPtrArray          *connected_outputs;
    GHashTable         *saved_outputs;
    XfceRROutput       *output;
#ifdef HAS_RANDR_ONE_POINT_THREE
    RROutput            primary = None;
#endif

    gdk_error_trap_push ();

    saved_outputs = NULL;

    /* get the default display */
    display = gdk_display_get_default ();
    xdisplay = gdk_x11_display_get_xdisplay (display);
    root_window = gdk_get_default_root_window ();

    /* get the screen resource */
    resources = XRRGetScreenResources (xdisplay, GDK_WINDOW_XID (root_window));

    /* get the range of screen sizes */
    mm_width = mm_height = width = height = 0;
    min_x = min_y = 32768;
    if (!XRRGetScreenSizeRange (xdisplay, GDK_WINDOW_XID (root_window),
                                &min_width, &min_height, &max_width, &max_height))
    {
        g_critical ("Unable to get the range of screen sizes, aborting.");
        goto err_cleanup;
    }

    /* get all existing CRTCs */
    crtcs = xfce_displays_helper_list_crtcs (xdisplay, resources);

    /* then all connected outputs */
    connected_outputs = xfce_displays_helper_list_outputs (xdisplay, resources,
                                                           crtcs, &nactive);

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
                primary = output->id;
        }
#endif

        /* status */
        g_snprintf (property, sizeof (property), "/%s/%s/Active", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);

        if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value))
            continue;

        /* Pick a CRTC for this output */
        crtc = xfce_displays_helper_find_usable_crtc (resources, crtcs, output);
        if (!crtc)
        {
            g_warning ("No available CRTC for %s, aborting.", output->info->name);
            continue;
        }
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu assigned to %s.", crtc->id,
                        output->info->name);

        /* disable inactive outputs */
        if (!g_value_get_boolean (value))
        {
            if (crtc->mode != None)
            {
                --nactive;
                xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be disabled by configuration.",
                                output->info->name);

                crtc->mode = None;
                crtc->noutput = 0;
                crtc->changed = TRUE;
            }

            continue;
        }

        /* rotation */
        g_snprintf (property, sizeof (property), "/%s/%s/Rotation", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_INT (value))
            int_value = g_value_get_int (value);
        else
            int_value = 0;

        /* convert to a Rotation */
        switch (int_value)
        {
            case 90:  rot = RR_Rotate_90;  break;
            case 180: rot = RR_Rotate_180; break;
            case 270: rot = RR_Rotate_270; break;
            default:  rot = RR_Rotate_0;   break;
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
            rot |= RR_Reflect_X;
        else if (g_strcmp0 (str_value, "Y") == 0)
            rot |= RR_Reflect_Y;
        else if (g_strcmp0 (str_value, "XY") == 0)
            rot |= (RR_Reflect_X|RR_Reflect_Y);

        /* check rotation support */
        if ((crtc->rotations & rot) == 0)
        {
            g_warning ("Unsupported rotation for %s. Fallback to RR_Rotate_0.",
                       output->info->name);
            rot = RR_Rotate_0;
        }

        /* update CRTC rotation */
        if (crtc->rotation != rot)
        {
            crtc->rotation = rot;
            crtc->changed = TRUE;
        }

        /* resolution */
        g_snprintf (property, sizeof (property), "/%s/%s/Resolution",
                    scheme, output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (value == NULL || !G_VALUE_HOLDS_STRING (value))
            str_value = "";
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

        /* check mode validity */
        valid_mode = None;
        for (m = 0; m < output->info->nmode; ++m)
        {
            /* walk all modes */
            for (l = 0; l < resources->nmode; ++l)
            {
                /* does the mode info match the mode we seek? */
                if (resources->modes[l].id != output->info->modes[m])
                    continue;

                /* calculate the refresh rate */
                rate = (gdouble) resources->modes[l].dotClock /
                        ((gdouble) resources->modes[l].hTotal * (gdouble) resources->modes[l].vTotal);

                /* find the mode corresponding to the saved values */
                if (rint (rate) == rint (output_rate)
                    && (g_strcmp0 (resources->modes[l].name, str_value) == 0))
                {
                    valid_mode = resources->modes[l].id;
                    break;
                }
            }
            /* found it */
            if (valid_mode != None)
                break;
        }

        if (valid_mode == None)
        {
            /* unsupported mode, abort for this output */
            g_warning ("Unknown mode '%s @ %.1f' for output %s, aborting.",
                       str_value, output_rate, output->info->name);
            continue;
        }
        else if (crtc->mode != valid_mode)
        {
            if (crtc->mode == None)
                ++nactive;

            /* update CRTC mode */
            crtc->mode = valid_mode;
            crtc->changed = TRUE;
        }

        /* recompute dimensions according to the selected rotation */
        if ((crtc->rotation & (RR_Rotate_90|RR_Rotate_270)) != 0)
        {
            crtc->width = resources->modes[l].height;
            crtc->height = resources->modes[l].width;
        }
        else
        {
            crtc->width = resources->modes[l].width;
            crtc->height = resources->modes[l].height;
        }

        /* position, x */
        g_snprintf (property, sizeof (property), "/%s/%s/Position/X", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_INT (value))
            x = g_value_get_int (value);
        else
            x = 0;

        /* position, y */
        g_snprintf (property, sizeof (property), "/%s/%s/Position/Y", scheme,
                    output->info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_INT (value))
            y = g_value_get_int (value);
        else
            y = 0;

        /* update CRTC position */
        if (crtc->x != x || crtc->y != y)
        {
            crtc->x = x;
            crtc->y = y;
            crtc->changed = TRUE;
        }

        /* used to normalize positions later */
        min_x = MIN (min_x, crtc->x);
        min_y = MIN (min_y, crtc->y);

        xfce_displays_helper_set_outputs (crtc, output->id);
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Total %d active output(s).", nactive);

    /* safety check */
    if (nactive < 1)
    {
        g_critical ("Stored Xfconf properties disable all outputs, aborting.");
        goto err_cleanup;
    }

    /* grab server to prevent clients from thinking no output is enabled */
    gdk_x11_display_grab (display);

    /* second loop, normalization and global settings */
    for (m = 0; m < resources->ncrtc; ++m)
    {
        /* ignore disabled outputs for size computations */
        if (crtcs[m].mode != None)
        {
            /* normalize positions to ensure the upper left corner is at (0,0) */
            if (min_x || min_y)
            {
                crtcs[m].x -= min_x;
                crtcs[m].y -= min_y;
                crtcs[m].changed = TRUE;
            }

            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Normalized CRTC %lu: size=%dx%d, pos=%dx%d.",
                            crtcs[m].id, crtcs[m].width, crtcs[m].height, crtcs[m].x, crtcs[m].y);

            /* calculate the total screen size */
            xfce_displays_helper_process_screen_size (crtcs[m].width, crtcs[m].height,
                                                      crtcs[m].x, crtcs[m].y, &width,
                                                      &height, &mm_width, &mm_height);
        }

        /* disable the CRTC, it will be reenabled after size calculation, unless the user disabled it */
        if (xfce_displays_helper_disable_crtc (xdisplay, resources, crtcs[m].id) == RRSetConfigSuccess)
            crtcs[m].changed = (crtcs[m].mode != None);
        else
            g_warning ("Failed to disable CRTC %lu.", crtc->id);

    }

    /* set the screen size only if it's really needed and valid */
    if (width >= min_width && width <= max_width
        && height >= min_height && height <= max_height
        && (width != gdk_screen_width ()
            || height != gdk_screen_height ()
            || mm_width != gdk_screen_width_mm ()
            || mm_height != gdk_screen_height_mm ()))
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying desktop dimensions: %dx%d (px), %dx%d (mm).",
                        width, height, mm_width, mm_height);
        XRRSetScreenSize (xdisplay, GDK_WINDOW_XID (root_window),
                          width, height, mm_width, mm_height);
    }

    /* final loop, apply crtc changes */
    for (m = 0; m < resources->ncrtc; ++m)
    {
        /* check if we really need to do something */
        if (crtcs[m].changed)
        {
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying changes to CRTC %lu.", crtcs[m].id);

            if (xfce_displays_helper_apply_crtc (xdisplay, resources, &crtcs[m]) != RRSetConfigSuccess)
                g_warning ("Failed to configure CRTC %lu.", crtcs[m].id);
        }
    }

#ifdef HAS_RANDR_ONE_POINT_THREE
        if (helper->has_1_3)
            XRRSetOutputPrimary (xdisplay, GDK_WINDOW_XID (root_window), primary);
#endif

    /* release the grab, changes are done */
    gdk_x11_display_ungrab (display);

err_cleanup:
    /* Free the xfconf properties */
    if (saved_outputs)
        g_hash_table_destroy (saved_outputs);

    /* Free our output cache */
    g_ptr_array_foreach (connected_outputs, (GFunc) xfce_displays_helper_free_output, NULL);
    g_ptr_array_free (connected_outputs, TRUE);

    /* cleanup our CRTC cache */
    for (m = 0; m < resources->ncrtc; ++m)
    {
        xfce_displays_helper_cleanup_crtc (&crtcs[m]);
    }
    g_free (crtcs);

    /* free the screen resources */
    XRRFreeScreenResources (resources);

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
