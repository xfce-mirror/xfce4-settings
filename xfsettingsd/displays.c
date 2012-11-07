/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010-2012 Lionel Le Folgoc <lionel@lefolgoc.net>
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
#ifdef HAVE_UPOWERGLIB
#include "displays-upower.h"
#endif

/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#else
#undef HAS_RANDR_ONE_POINT_THREE
#endif



/* wrappers to avoid querying too often */
typedef struct _XfceRRCrtc XfceRRCrtc;



static void             xfce_displays_helper_dispose                        (GObject                 *object);
static void             xfce_displays_helper_finalize                       (GObject                 *object);
static void             xfce_displays_helper_reload                         (XfceDisplaysHelper      *helper);
static GdkFilterReturn  xfce_displays_helper_screen_on_event                (GdkXEvent               *xevent,
                                                                             GdkEvent                *event,
                                                                             gpointer                 data);
static void             xfce_displays_helper_set_screen_size                (XfceDisplaysHelper      *helper);
static gboolean         xfce_displays_helper_load_from_xfconf               (XfceDisplaysHelper      *helper,
                                                                             const gchar             *scheme,
                                                                             GHashTable              *saved_outputs,
                                                                             RROutput                 output);
static GPtrArray       *xfce_displays_helper_list_crtcs                     (XfceDisplaysHelper      *helper);
static XfceRRCrtc      *xfce_displays_helper_find_crtc_by_id                (XfceDisplaysHelper      *helper,
                                                                             RRCrtc                   id);
static void             xfce_displays_helper_free_crtc                      (XfceRRCrtc              *crtc);
static XfceRRCrtc      *xfce_displays_helper_find_usable_crtc               (XfceDisplaysHelper      *helper,
                                                                             RROutput                 output);
static void             xfce_displays_helper_get_topleftmost_pos            (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_normalize_crtc                 (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static Status           xfce_displays_helper_disable_crtc                   (XfceDisplaysHelper      *helper,
                                                                             RRCrtc                   crtc);
static void             xfce_displays_helper_workaround_crtc_size           (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_apply_crtc                     (XfceRRCrtc              *crtc,
                                                                             XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_set_outputs                    (XfceRRCrtc              *crtc,
                                                                             RROutput                 output);
static void             xfce_displays_helper_apply_all                      (XfceDisplaysHelper      *helper);
static void             xfce_displays_helper_channel_apply                  (XfceDisplaysHelper      *helper,
                                                                             const gchar             *scheme);
static void             xfce_displays_helper_channel_property_changed       (XfconfChannel           *channel,
                                                                             const gchar             *property_name,
                                                                             const GValue            *value,
                                                                             XfceDisplaysHelper      *helper);
#ifdef HAVE_UPOWERGLIB
static void             xfce_displays_helper_toggle_internal                (XfceDisplaysUPower      *power,
                                                                             gboolean                 lid_is_closed,
                                                                             XfceDisplaysHelper      *helper);
#endif



struct _XfceDisplaysHelperClass
{
    GObjectClass __parent__;
};

struct _XfceDisplaysHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel      *channel;
    guint               handler;

#ifdef HAS_RANDR_ONE_POINT_THREE
    gint                has_1_3;
    gint                primary;
#endif

#ifdef HAVE_UPOWERGLIB
    XfceDisplaysUPower *power;
    gint                phandler;
#endif

    GdkDisplay         *display;
    GdkWindow          *root_window;
    Display            *xdisplay;
    gint                event_base;

    /* RandR cache */
    XRRScreenResources *resources;
    GPtrArray          *crtcs;

    /* screen size */
    gint                width;
    gint                height;
    gint                mm_width;
    gint                mm_height;

    /* used to normalize positions */
    gint                min_x;
    gint                min_y;
};

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



G_DEFINE_TYPE (XfceDisplaysHelper, xfce_displays_helper, G_TYPE_OBJECT);



static void
xfce_displays_helper_class_init (XfceDisplaysHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = xfce_displays_helper_dispose;
    gobject_class->finalize = xfce_displays_helper_finalize;
}



static void
xfce_displays_helper_init (XfceDisplaysHelper *helper)
{
    gint major = 0, minor = 0;
    gint error_base, err;

#ifdef HAVE_UPOWERGLIB
    helper->power = NULL;
    helper->phandler = 0;
#endif
    helper->resources = NULL;
    helper->crtcs = NULL;
    helper->handler = 0;

    /* get the default display */
    helper->display = gdk_display_get_default ();
    helper->xdisplay = gdk_x11_display_get_xdisplay (helper->display);
    helper->root_window = gdk_get_default_root_window ();

    /* check if the randr extension is running */
    if (XRRQueryExtension (helper->xdisplay, &helper->event_base, &error_base))
    {
        /* query the version */
        if (XRRQueryVersion (helper->xdisplay, &major, &minor)
            && (major > 1 || (major == 1 && minor >= 2)))
        {
            gdk_error_trap_push ();
            /* get the screen resource */
            helper->resources = XRRGetScreenResources (helper->xdisplay,
                                                       GDK_WINDOW_XID (helper->root_window));
            gdk_flush ();
            err = gdk_error_trap_pop ();
            if (err)
            {
                g_critical ("XRRGetScreenResources failed (err: %d). "
                            "Display settings won't be applied.", err);
                return;
            }

            /* get all existing CRTCs */
            helper->crtcs = xfce_displays_helper_list_crtcs (helper);

            /* Set up RandR notifications */
            XRRSelectInput (helper->xdisplay,
                            GDK_WINDOW_XID (helper->root_window),
                            RRScreenChangeNotifyMask);
            gdk_x11_register_standard_event_type (helper->display,
                                                  helper->event_base,
                                                  RRNotify + 1);
            gdk_window_add_filter (helper->root_window,
                                   xfce_displays_helper_screen_on_event,
                                   helper);

#ifdef HAVE_UPOWERGLIB
            helper->power = g_object_new (XFCE_TYPE_DISPLAYS_UPOWER, NULL);
            helper->phandler = g_signal_connect (G_OBJECT (helper->power),
                                                 "lid-changed",
                                                 G_CALLBACK (xfce_displays_helper_toggle_internal),
                                                 helper);
#endif

            /* open the channel */
            helper->channel = xfconf_channel_get ("displays");

            /* remove any leftover apply property before setting the monitor */
            xfconf_channel_reset_property (helper->channel, "/Schemes/Apply", FALSE);

            /* monitor channel changes */
            helper->handler = g_signal_connect (G_OBJECT (helper->channel),
                                                "property-changed",
                                                G_CALLBACK (xfce_displays_helper_channel_property_changed),
                                                helper);

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
                    gdk_display_get_name (helper->display));
    }
}



static void
xfce_displays_helper_dispose (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);

    if (helper->handler > 0)
    {
        g_signal_handler_disconnect (G_OBJECT (helper->channel),
                                     helper->handler);
        helper->handler = 0;
    }

#ifdef HAVE_UPOWERGLIB
    if (helper->phandler > 0)
    {
        g_signal_handler_disconnect (G_OBJECT (helper->power),
                                     helper->phandler);
        g_object_unref (helper->power);
        helper->phandler = 0;
    }
#endif

    gdk_window_remove_filter (helper->root_window,
                              xfce_displays_helper_screen_on_event,
                              helper);

    (*G_OBJECT_CLASS (xfce_displays_helper_parent_class)->dispose) (object);
}



static void
xfce_displays_helper_finalize (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);

    /* Free the CRTC cache */
    if (helper->crtcs)
    {
        g_ptr_array_free (helper->crtcs, TRUE);
        helper->crtcs = NULL;
    }

    /* Free the screen resources */
    if (helper->resources)
    {
        gdk_error_trap_push ();
        XRRFreeScreenResources (helper->resources);
        gdk_flush ();
        gdk_error_trap_pop ();
        helper->resources = NULL;
    }

    (*G_OBJECT_CLASS (xfce_displays_helper_parent_class)->finalize) (object);
}



static void
xfce_displays_helper_reload (XfceDisplaysHelper *helper)
{
    gint err;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Refreshing RandR cache.");

    /* Free the CRTC cache */
    g_ptr_array_free (helper->crtcs, TRUE);

    gdk_error_trap_push ();

    /* Free the screen resources */
    XRRFreeScreenResources (helper->resources);

    /* get the screen resource */
#ifdef HAS_RANDR_ONE_POINT_THREE
    /* xfce_displays_helper_reload () is usually called after a xrandr notification,
       which means that X is aware of the new hardware already. So, if possible,
       do not reprobe the hardware again. */
    if (helper->has_1_3)
        helper->resources = XRRGetScreenResourcesCurrent (helper->xdisplay,
                                                          GDK_WINDOW_XID (helper->root_window));
    else
#endif
    helper->resources = XRRGetScreenResources (helper->xdisplay,
                                               GDK_WINDOW_XID (helper->root_window));

    gdk_flush ();
    err = gdk_error_trap_pop ();
    if (err)
        g_critical ("Failed to reload the RandR cache (err: %d).", err);

    /* get all existing CRTCs */
    helper->crtcs = xfce_displays_helper_list_crtcs (helper);
}



static GdkFilterReturn
xfce_displays_helper_screen_on_event (GdkXEvent *xevent,
                                      GdkEvent  *event,
                                      gpointer   data)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (data);
    XEvent             *e = xevent;
    gint                event_num;

    if (!e)
        return GDK_FILTER_CONTINUE;

    event_num = e->type - helper->event_base;

    if (event_num == RRScreenChangeNotify)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "RRScreenChangeNotify event received.");

        xfce_displays_helper_reload (helper);

        /*TODO: check that there is still one output enabled */
        /*TODO: e.g. reenable LVDS1 when VGA1 is diconnected. */
    }

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}



static void
xfce_displays_helper_set_screen_size (XfceDisplaysHelper *helper)
{
    gint min_width, min_height, max_width, max_height;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    /* get the screen size extremums */
    if (!XRRGetScreenSizeRange (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                                &min_width, &min_height, &max_width, &max_height))
    {
        g_warning ("Unable to get the range of screen sizes. "
                   "Display settings may fail to apply.");
        return;
    }

    /* set the screen size only if it's really needed and valid */
    if (helper->width >= min_width && helper->width <= max_width
        && helper->height >= min_height && helper->height <= max_height
        && (helper->width != gdk_screen_width ()
            || helper->height != gdk_screen_height ()
            || helper->mm_width != gdk_screen_width_mm ()
            || helper->mm_height != gdk_screen_height_mm ()))
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying desktop dimensions: %dx%d (px), %dx%d (mm).",
                        helper->width, helper->height, helper->mm_width, helper->mm_height);
        XRRSetScreenSize (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                          helper->width, helper->height, helper->mm_width, helper->mm_height);
    }
}



static gboolean
xfce_displays_helper_load_from_xfconf (XfceDisplaysHelper *helper,
                                       const gchar        *scheme,
                                       GHashTable         *saved_outputs,
                                       RROutput            output)
{
    XfceRRCrtc    *crtc = NULL;
    XRROutputInfo *info;
    GValue        *value;
    const gchar   *str_value;
    gchar          property[512];
    gdouble        output_rate, rate;
    RRMode         valid_mode;
    Rotation       rot;
    gint           x, y, n, m, int_value, err;
    gboolean       active = FALSE;

    gdk_error_trap_push ();
    info = XRRGetOutputInfo (helper->xdisplay, helper->resources, output);
    gdk_flush ();
    err = gdk_error_trap_pop ();
    if (err || !info)
    {
        g_warning ("Failed to load info for output %lu (err: %d). Skipping.",
                   output, err);
        return FALSE;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected output %lu %s.", output,
                    info->name);

    /* ignore disconnected outputs */
    if (info->connection != RR_Connected)
        goto next_output;

    /* Get the associated CRTC */
    if (info->crtc != None)
        crtc = xfce_displays_helper_find_crtc_by_id (helper, info->crtc);

    /* track active outputs */
    if (crtc && crtc->mode != None)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s is active.", info->name);
        active = TRUE;
    }

    /* does this output exist in xfconf? */
    g_snprintf (property, sizeof (property), "/%s/%s", scheme, info->name);
    value = g_hash_table_lookup (saved_outputs, property);

    if (value == NULL || !G_VALUE_HOLDS_STRING (value))
        goto next_output;

#ifdef HAS_RANDR_ONE_POINT_THREE
    if (helper->has_1_3)
    {
        /* is it the primary output? */
        g_snprintf (property, sizeof (property), "/%s/%s/Primary", scheme, info->name);
        value = g_hash_table_lookup (saved_outputs, property);
        if (G_VALUE_HOLDS_BOOLEAN (value) && g_value_get_boolean (value))
            helper->primary = output;
    }
#endif

    /* status */
    g_snprintf (property, sizeof (property), "/%s/%s/Active", scheme, info->name);
    value = g_hash_table_lookup (saved_outputs, property);

    if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value))
        goto next_output;

    /* No existing CRTC, try to find a free one */
    if (info->crtc == None)
        crtc = xfce_displays_helper_find_usable_crtc (helper, output);

    if (!crtc)
    {
        g_warning ("No available CRTC for %s, aborting.", info->name);
        goto next_output;
    }
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu assigned to %s.", crtc->id, info->name);

    /* disable inactive outputs */
    if (!g_value_get_boolean (value))
    {
        if (crtc->mode != None)
        {
            active = FALSE;
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be disabled by configuration.", info->name);

            crtc->mode = None;
            crtc->noutput = 0;
            crtc->changed = TRUE;
        }

        goto next_output;
    }

    /* rotation */
    g_snprintf (property, sizeof (property), "/%s/%s/Rotation", scheme, info->name);
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
    g_snprintf (property, sizeof (property), "/%s/%s/Reflection", scheme, info->name);
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
        g_warning ("Unsupported rotation for %s. Fallback to RR_Rotate_0.", info->name);
        rot = RR_Rotate_0;
    }

    /* update CRTC rotation */
    if (crtc->rotation != rot)
    {
        crtc->rotation = rot;
        crtc->changed = TRUE;
    }

    /* resolution */
    g_snprintf (property, sizeof (property), "/%s/%s/Resolution", scheme, info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (value == NULL || !G_VALUE_HOLDS_STRING (value))
        str_value = "";
    else
        str_value = g_value_get_string (value);

    /* refresh rate */
    g_snprintf (property, sizeof (property), "/%s/%s/RefreshRate", scheme, info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        output_rate = g_value_get_double (value);
    else
        output_rate = 0.0;

    /* check mode validity */
    valid_mode = None;
    for (n = 0; n < info->nmode; ++n)
    {
        /* walk all modes */
        for (m = 0; m < helper->resources->nmode; ++m)
        {
            /* does the mode info match the mode we seek? */
            if (helper->resources->modes[m].id != info->modes[n])
                continue;

            /* calculate the refresh rate */
            rate = (gdouble) helper->resources->modes[m].dotClock /
                    ((gdouble) helper->resources->modes[m].hTotal * (gdouble) helper->resources->modes[m].vTotal);

            /* find the mode corresponding to the saved values */
            if (rint (rate) == rint (output_rate)
                && (g_strcmp0 (helper->resources->modes[m].name, str_value) == 0))
            {
                valid_mode = helper->resources->modes[m].id;
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
                   str_value, output_rate, info->name);
        goto next_output;
    }
    else if (crtc->mode != valid_mode)
    {
        if (crtc->mode == None)
            active = TRUE;

        /* update CRTC mode */
        crtc->mode = valid_mode;
        crtc->changed = TRUE;
    }

    /* recompute dimensions according to the selected rotation */
    if ((crtc->rotation & (RR_Rotate_90|RR_Rotate_270)) != 0)
    {
        crtc->width = helper->resources->modes[m].height;
        crtc->height = helper->resources->modes[m].width;
    }
    else
    {
        crtc->width = helper->resources->modes[m].width;
        crtc->height = helper->resources->modes[m].height;
    }

    /* position, x */
    g_snprintf (property, sizeof (property), "/%s/%s/Position/X", scheme, info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_INT (value))
        x = g_value_get_int (value);
    else
        x = 0;

    /* position, y */
    g_snprintf (property, sizeof (property), "/%s/%s/Position/Y", scheme,
                info->name);
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

    xfce_displays_helper_set_outputs (crtc, output);

next_output:
    XRRFreeOutputInfo (info);
    return active;
}



static GPtrArray *
xfce_displays_helper_list_crtcs (XfceDisplaysHelper *helper)
{
    GPtrArray   *crtcs;
    XRRCrtcInfo *crtc_info;
    XfceRRCrtc  *crtc;
    gint         n, err;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    /* get all existing CRTCs */
    crtcs = g_ptr_array_new_with_free_func ((GDestroyNotify) xfce_displays_helper_free_crtc);
    for (n = 0; n < helper->resources->ncrtc; ++n)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected CRTC %lu.", helper->resources->crtcs[n]);

        gdk_error_trap_push ();
        crtc_info = XRRGetCrtcInfo (helper->xdisplay, helper->resources, helper->resources->crtcs[n]);
        gdk_flush ();
        err = gdk_error_trap_pop ();
        if (err || !crtc_info)
        {
            g_warning ("Failed to load info for CRTC %lu (err: %d). Skipping.",
                       helper->resources->crtcs[n], err);
            continue;
        }

        crtc = g_new0 (XfceRRCrtc, 1);
        crtc->id = helper->resources->crtcs[n];
        crtc->mode = crtc_info->mode;
        crtc->rotation = crtc_info->rotation;
        crtc->rotations = crtc_info->rotations;
        crtc->width = crtc_info->width;
        crtc->height = crtc_info->height;
        crtc->x = crtc_info->x;
        crtc->y = crtc_info->y;

        crtc->noutput = crtc_info->noutput;
        crtc->outputs = NULL;
        if (crtc_info->noutput > 0)
            crtc->outputs = g_memdup (crtc_info->outputs,
                                      crtc_info->noutput * sizeof (RROutput));

        crtc->npossible = crtc_info->npossible;
        crtc->possible = NULL;
        if (crtc_info->npossible > 0)
            crtc->possible = g_memdup (crtc_info->possible,
                                       crtc_info->npossible * sizeof (RROutput));

        crtc->changed = FALSE;
        XRRFreeCrtcInfo (crtc_info);

        /* cache it */
        g_ptr_array_add (crtcs, crtc);
    }

    return crtcs;
}



static XfceRRCrtc *
xfce_displays_helper_find_crtc_by_id (XfceDisplaysHelper *helper,
                                      RRCrtc              id)
{
    XfceRRCrtc *crtc;
    guint       n;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs);

    for (n = 0; n < helper->crtcs->len; ++n)
    {
        crtc = g_ptr_array_index (helper->crtcs, n);
        if (crtc->id == id)
            return crtc;
    }

    return NULL;
}



static void
xfce_displays_helper_free_crtc (XfceRRCrtc *crtc)
{
    if (crtc == NULL)
        return;

    if (crtc->outputs != NULL)
        g_free (crtc->outputs);
    if (crtc->possible != NULL)
        g_free (crtc->possible);
    g_free (crtc);
}



static XfceRRCrtc *
xfce_displays_helper_find_usable_crtc (XfceDisplaysHelper *helper,
                                       RROutput            output)
{
    XfceRRCrtc *crtc;
    guint       n;
    gint        m;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs);

    /* try to find one that is not already used by another output */
    for (n = 0; n < helper->crtcs->len; ++n)
    {
        crtc = g_ptr_array_index (helper->crtcs, n);
        if (crtc->noutput > 0 || crtc->changed)
            continue;

        for (m = 0; m < crtc->npossible; ++m)
        {
            if (crtc->possible[m] == output)
                return crtc;
        }
    }

    /* none available */
    return NULL;
}



static void
xfce_displays_helper_get_topleftmost_pos (XfceRRCrtc         *crtc,
                                          XfceDisplaysHelper *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && crtc);

    /* used to normalize positions later */
    helper->min_x = MIN (helper->min_x, crtc->x);
    helper->min_y = MIN (helper->min_y, crtc->y);
}



static void
xfce_displays_helper_normalize_crtc (XfceRRCrtc         *crtc,
                                     XfceDisplaysHelper *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && crtc);

    /* ignore disabled outputs for size computations */
    if (crtc->mode == None)
        return;

    /* normalize positions to ensure the upper left corner is at (0,0) */
    if (helper->min_x || helper->min_y)
    {
        crtc->x -= helper->min_x;
        crtc->y -= helper->min_y;
        crtc->changed = TRUE;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Normalized CRTC %lu: size=%dx%d, pos=%dx%d.",
                    crtc->id, crtc->width, crtc->height, crtc->x, crtc->y);

    /* calculate the total screen size */
    helper->width = MAX (helper->width, crtc->x + crtc->width);
    helper->height = MAX (helper->height, crtc->y + crtc->height);

    /* The 'physical size' of an X screen is meaningless if that screen
     * can consist of many monitors. So just pick a size that make the
     * dpi 96.
     *
     * Firefox and Evince apparently believe what X tells them.
     */
    helper->mm_width = (helper->width / 96.0) * 25.4 + 0.5;
    helper->mm_height = (helper->height / 96.0) * 25.4 + 0.5;
}



static Status
xfce_displays_helper_disable_crtc (XfceDisplaysHelper *helper,
                                   RRCrtc              crtc)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Disabling CRTC %lu.", crtc);

    return XRRSetCrtcConfig (helper->xdisplay, helper->resources, crtc,
                             CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
}



static void
xfce_displays_helper_workaround_crtc_size (XfceRRCrtc         *crtc,
                                           XfceDisplaysHelper *helper)
{
    XRRCrtcInfo *crtc_info;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources && crtc);

    /* The CRTC needs to be disabled if its previous mode won't fit in the new screen.
       It will be reenabled with its new mode (known to fit) after the screen size is
       changed, unless the user disabled it (no need to reenable it then). */
    crtc_info = XRRGetCrtcInfo (helper->xdisplay, helper->resources, crtc->id);
    if ((crtc_info->x + crtc_info->width > (guint) helper->width) ||
        (crtc_info->y + crtc_info->height > (guint) helper->height))
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu must be temporarily disabled.", crtc->id);
        if (xfce_displays_helper_disable_crtc (helper, crtc->id) == RRSetConfigSuccess)
            crtc->changed = (crtc->mode != None);
        else
            g_warning ("Failed to temporarily disable CRTC %lu.", crtc->id);
    }
    XRRFreeCrtcInfo (crtc_info);
}



static void
xfce_displays_helper_apply_crtc (XfceRRCrtc         *crtc,
                                 XfceDisplaysHelper *helper)
{
    Status ret;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->xdisplay && helper->resources && crtc);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Configuring CRTC %lu.", crtc->id);

    /* check if we really need to do something */
    if (crtc->changed)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying changes to CRTC %lu.", crtc->id);

        if (crtc->mode == None)
            ret = xfce_displays_helper_disable_crtc (helper, crtc->id);
        else
            ret = XRRSetCrtcConfig (helper->xdisplay, helper->resources, crtc->id,
                                    CurrentTime, crtc->x, crtc->y, crtc->mode,
                                    crtc->rotation, crtc->outputs, crtc->noutput);

        if (ret == RRSetConfigSuccess)
            crtc->changed = FALSE;
        else
            g_warning ("Failed to configure CRTC %lu.", crtc->id);
    }
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
xfce_displays_helper_apply_all (XfceDisplaysHelper *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs);

    /* normalization and screen size calculation */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_get_topleftmost_pos, helper);
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_normalize_crtc, helper);

    gdk_error_trap_push ();

    /* grab server to prevent clients from thinking no output is enabled */
    gdk_x11_display_grab (helper->display);

    /* disable CRTCs that won't fit in the new screen */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_workaround_crtc_size, helper);

    /* set the screen size only if it's really needed and valid */
    xfce_displays_helper_set_screen_size (helper);

    /* final loop, apply crtc changes */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_apply_crtc, helper);

#ifdef HAS_RANDR_ONE_POINT_THREE
        if (helper->has_1_3)
            XRRSetOutputPrimary (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                                 helper->primary);
#endif

    /* release the grab, changes are done */
    gdk_x11_display_ungrab (helper->display);
    gdk_flush ();
    gdk_error_trap_pop ();
}



static void
xfce_displays_helper_channel_apply (XfceDisplaysHelper *helper,
                                    const gchar        *scheme)
{
    gchar       property[512];
    gint        n, nactive;
    GHashTable *saved_outputs;

    saved_outputs = NULL;
    helper->mm_width = helper->mm_height = helper->width = helper->height = 0;
    helper->min_x = helper->min_y = 32768;
#ifdef HAS_RANDR_ONE_POINT_THREE
    helper->primary = None;
#endif

    /* finally the list of saved outputs from xfconf */
    g_snprintf (property, sizeof (property), "/%s", scheme);
    saved_outputs = xfconf_channel_get_properties (helper->channel, property);

    /* nothing saved, nothing to do */
    if (saved_outputs == NULL)
        goto err_cleanup;

    /* first loop, loads all the outputs, and gets the number of active ones */
    nactive = 0;
    for (n = 0; n < helper->resources->noutput; ++n)
    {
        if (xfce_displays_helper_load_from_xfconf (helper, scheme, saved_outputs,
                                                   helper->resources->outputs[n]))
            ++nactive;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Total %d active output(s).", nactive);

    /* safety check */
    if (nactive < 1)
    {
        g_critical ("Stored Xfconf properties disable all outputs, aborting.");
        goto err_cleanup;
    }

    /* apply settings */
    xfce_displays_helper_apply_all (helper);

err_cleanup:
    /* Free the xfconf properties */
    if (saved_outputs)
        g_hash_table_destroy (saved_outputs);
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



#ifdef HAVE_UPOWERGLIB
static void
xfce_displays_helper_toggle_internal (XfceDisplaysUPower *power,
                                      gboolean            lid_is_closed,
                                      XfceDisplaysHelper *helper)
{
    GHashTable    *saved_outputs;
    XfceRRCrtc    *crtc = NULL;
    XRROutputInfo *info;
    RROutput       lvds = None;
    gboolean       active = FALSE;
    RRMode         best_mode;
    gint           best_dist, dist, n, m;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Lid is %s, toggling internal output.",
                    XFSD_LID_STR (lid_is_closed));

    for (n = 0; n < helper->resources->noutput; ++n)
    {
        gdk_error_trap_push ();
        info = XRRGetOutputInfo (helper->xdisplay, helper->resources,
                                 helper->resources->outputs[n]);

        /* Try to find the internal display */
        if (info && (info->connection == RR_Connected)
            && (g_str_has_prefix (info->name, "LVDS")
                || strcmp (info->name, "PANEL") == 0))
        {
            lvds = helper->resources->outputs[n];
            break;
        }

        XRRFreeOutputInfo (info);
        gdk_flush ();
        gdk_error_trap_pop ();
    }

    if (lvds == None)
        return;

    /* Get the associated CRTC */
    if (info->crtc != None)
        crtc = xfce_displays_helper_find_crtc_by_id (helper, info->crtc);

    /* Is LVDS active? */
    active = crtc && crtc->mode != None;
    helper->mm_width = helper->mm_height = helper->width = helper->height = 0;
    helper->min_x = helper->min_y = 32768;

    if (active && lid_is_closed)
    {
        /* if active and the lid is closed, deactivate it */
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be disabled (lid closed event).", info->name);
        crtc->mode = None;
        crtc->noutput = 0;
        crtc->changed = TRUE;

    }
    else if (!active && !lid_is_closed)
    {
        /* re-activate it because the user opened the lid */
        saved_outputs = xfconf_channel_get_properties (helper->channel, "/Default");
        if (saved_outputs)
        {
            /* try to load user saved settings */
            active = xfce_displays_helper_load_from_xfconf (helper, "Default",
                                                            saved_outputs, lvds);
            g_hash_table_destroy (saved_outputs);
            if (!active)
            {
                /* inactive, because of invalid or inexistent settings,
                 * so set up a mode manually */
                if (info->crtc == None)
                    crtc = xfce_displays_helper_find_usable_crtc (helper, lvds);

                if (!crtc)
                {
                    g_warning ("No available CRTC for %s (lid opened event).", info->name);
                    goto lid_abort;
                }
                xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu assigned to %s (lid opened event).",
                                crtc->id, info->name);

                /* find the preferred mode */
                best_mode = None;
                best_dist = 0;
                for (n = 0; n < info->nmode; ++n)
                {
                    /* walk all modes */
                    for (m = 0; m < helper->resources->nmode; ++m)
                    {
                        /* does the mode info match the mode we seek? */
                        if (helper->resources->modes[m].id != info->modes[n])
                            continue;

                        if (n < info->npreferred)
                            dist = 0;
                        else if (info->mm_height != 0)
                            dist = (1000 * gdk_screen_height () / gdk_screen_height_mm () -
                                    1000 * helper->resources->modes[m].height / info->mm_height);
                        else
                            dist = gdk_screen_height () - helper->resources->modes[m].height;

                        dist = ABS (dist);

                        if (best_mode == None || dist < best_dist)
                        {
                            best_mode = helper->resources->modes[m].id;
                            best_dist = dist;
                        }
                    }
                }
                /* bad luck */
                if (best_mode == None)
                {
                    g_warning ("No available mode for %s (lid opened event).", info->name);
                    goto lid_abort;
                }
                /* set the mode found */
                crtc->mode = best_mode;
                xfce_displays_helper_set_outputs (crtc, lvds);
                crtc->changed = TRUE;
            }
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be re-enabled (lid opened event).", info->name);
        }
    }
    else
        goto lid_abort;

    /* apply settings */
    xfce_displays_helper_apply_all (helper);

lid_abort:
    /* wasn't freed because of the break */
    XRRFreeOutputInfo (info);
    gdk_flush ();
    gdk_error_trap_pop ();
}
#endif
