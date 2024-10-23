/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010-2012 Lionel Le Folgoc <lionel@lefolgoc.net>
 *  Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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
#include "config.h"
#endif

#include "displays-x11.h"

#include "common/debug.h"
#include "common/display-profiles.h"
#include "common/edid.h"
#include "common/xfce-randr.h"

#include <X11/extensions/Xrandr.h>
#include <gdk/gdkx.h>
#include <libxfce4ui/libxfce4ui.h>

#ifdef HAVE_MATH_H
#include <math.h>
#endif



/* wrappers to avoid querying too often */
typedef struct _XfceRRCrtc XfceRRCrtc;
typedef struct _XfceRROutput XfceRROutput;



static void
xfce_displays_helper_x11_dispose (GObject *object);
static void
xfce_displays_helper_x11_finalize (GObject *object);
static GPtrArray *
xfce_displays_helper_x11_get_outputs (XfceDisplaysHelper *helper);
static void
xfce_displays_helper_x11_toggle_internal (gpointer *power,
                                          gboolean lid_is_closed,
                                          XfceDisplaysHelper *helper);
static gchar **
xfce_displays_helper_x11_get_display_infos (XfceDisplaysHelper *helper);
static void
xfce_displays_helper_x11_channel_apply (XfceDisplaysHelper *helper,
                                        const gchar *scheme);
static void
xfce_displays_helper_x11_reload (XfceDisplaysHelperX11 *helper);
static GdkFilterReturn
xfce_displays_helper_x11_screen_on_event (GdkXEvent *xevent,
                                          GdkEvent *event,
                                          gpointer data);
static void
xfce_displays_helper_x11_set_screen_size (XfceDisplaysHelperX11 *helper);
static gboolean
xfce_displays_helper_x11_load_from_xfconf (XfceDisplaysHelperX11 *helper,
                                           const gchar *scheme,
                                           GHashTable *saved_outputs,
                                           XfceRROutput *output);
static GPtrArray *
xfce_displays_helper_x11_list_outputs (XfceDisplaysHelperX11 *helper);
static void
xfce_displays_helper_x11_free_output (XfceRROutput *output);
static GPtrArray *
xfce_displays_helper_x11_list_crtcs (XfceDisplaysHelperX11 *helper);
static XfceRRCrtc *
xfce_displays_helper_x11_find_crtc_by_id (XfceDisplaysHelperX11 *helper,
                                          RRCrtc id);
static void
xfce_displays_helper_x11_free_crtc (XfceRRCrtc *crtc);
static XfceRRCrtc *
xfce_displays_helper_x11_find_usable_crtc (XfceDisplaysHelperX11 *helper,
                                           XfceRROutput *output);
static void
xfce_displays_helper_x11_get_topleftmost_pos (XfceRRCrtc *crtc,
                                              XfceDisplaysHelperX11 *helper);
static void
xfce_displays_helper_x11_normalize_crtc (XfceRRCrtc *crtc,
                                         XfceDisplaysHelperX11 *helper);
static Status
xfce_displays_helper_x11_disable_crtc (XfceDisplaysHelperX11 *helper,
                                       RRCrtc crtc);
static void
xfce_displays_helper_x11_workaround_crtc_size (XfceRRCrtc *crtc,
                                               XfceDisplaysHelperX11 *helper);
static void
xfce_displays_helper_x11_apply_crtc_transform (XfceRRCrtc *crtc,
                                               XfceDisplaysHelperX11 *helper);
static void
xfce_displays_helper_x11_apply_crtc (XfceRRCrtc *crtc,
                                     XfceDisplaysHelperX11 *helper);
static void
xfce_displays_helper_x11_set_outputs (XfceRRCrtc *crtc,
                                      XfceRROutput *output);
static void
xfce_displays_helper_x11_apply_all (XfceDisplaysHelperX11 *helper);



struct _XfceDisplaysHelperX11
{
    XfceDisplaysHelper __parent__;

    gint primary;

    GdkDisplay *display;
    GdkWindow *root_window;
    Display *xdisplay;
    gint event_base;
    guint screen_on_event_id;

    /* RandR cache */
    XfceRandr *randr;
    XRRScreenResources *resources;
    GPtrArray *crtcs;
    GPtrArray *outputs;

    /* screen size */
    gint width;
    gint height;
    gint mm_width;
    gint mm_height;

    /* used to normalize positions */
    gint min_x;
    gint min_y;
};

struct _XfceRRCrtc
{
    RRCrtc id;
    RRMode mode;
    Rotation rotation;
    Rotation rotations;
    gint width;
    gint height;
    gint x;
    gint y;
    gdouble scalex;
    gdouble scaley;
    gint noutput;
    RROutput *outputs;
    gint npossible;
    RROutput *possible;
    gint changed;
};

struct _XfceRROutput
{
    RROutput id;
    XRROutputInfo *info;
    RRMode preferred_mode;
    guint active : 1;
};


G_DEFINE_TYPE (XfceDisplaysHelperX11, xfce_displays_helper_x11, XFCE_TYPE_DISPLAYS_HELPER);



static void
xfce_displays_helper_x11_class_init (XfceDisplaysHelperX11Class *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    XfceDisplaysHelperClass *helper_class = XFCE_DISPLAYS_HELPER_CLASS (klass);

    gobject_class->dispose = xfce_displays_helper_x11_dispose;
    gobject_class->finalize = xfce_displays_helper_x11_finalize;

    helper_class->get_outputs = xfce_displays_helper_x11_get_outputs;
    helper_class->toggle_internal = xfce_displays_helper_x11_toggle_internal;
    helper_class->get_display_infos = xfce_displays_helper_x11_get_display_infos;
    helper_class->channel_apply = xfce_displays_helper_x11_channel_apply;
}



static void
xfce_displays_helper_x11_init (XfceDisplaysHelperX11 *helper)
{
    gint error_base, err;

    helper->resources = NULL;
    helper->outputs = NULL;
    helper->crtcs = NULL;

    /* get the default display */
    helper->display = gdk_display_get_default ();
    helper->xdisplay = gdk_x11_display_get_xdisplay (helper->display);
    helper->root_window = gdk_get_default_root_window ();

    /* check if the randr extension is running */
    if (XRRQueryExtension (helper->xdisplay, &helper->event_base, &error_base))
    {
        GError *error = NULL;
        helper->randr = xfce_randr_new (helper->display, &error);
        if (helper->randr == NULL)
            g_critical ("%s", error->message);

        gdk_x11_display_error_trap_push (helper->display);
        /* get the screen resource */
        helper->resources = XRRGetScreenResources (helper->xdisplay,
                                                   GDK_WINDOW_XID (helper->root_window));
        gdk_display_flush (helper->display);
        err = gdk_x11_display_error_trap_pop (helper->display);
        if (err)
        {
            g_critical ("XRRGetScreenResources failed (err: %d). Display settings won't be applied.", err);
            return;
        }

        /* get all existing CRTCs and connected outputs */
        helper->crtcs = xfce_displays_helper_x11_list_crtcs (helper);
        helper->outputs = xfce_displays_helper_x11_list_outputs (helper);

        /* Set up RandR notifications */
        XRRSelectInput (helper->xdisplay,
                        GDK_WINDOW_XID (helper->root_window),
                        RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (helper->display,
                                              helper->event_base,
                                              RRNotify + 1);
        gdk_window_add_filter (helper->root_window,
                               xfce_displays_helper_x11_screen_on_event,
                               helper);
    }
    else
    {
        g_critical ("No RANDR extension found in display %s. Display settings won't be applied.",
                    gdk_display_get_name (helper->display));
    }
}



static void
xfce_displays_helper_x11_dispose (GObject *object)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (object);

    gdk_window_remove_filter (helper->root_window,
                              xfce_displays_helper_x11_screen_on_event,
                              helper);

    if (helper->randr)
    {
        xfce_randr_free (helper->randr);
        helper->randr = NULL;
    }

    if (helper->outputs)
    {
        g_ptr_array_unref (helper->outputs);
        helper->outputs = NULL;
    }

    if (helper->crtcs)
    {
        g_ptr_array_unref (helper->crtcs);
        helper->crtcs = NULL;
    }

    (*G_OBJECT_CLASS (xfce_displays_helper_x11_parent_class)->dispose) (object);
}



static void
xfce_displays_helper_x11_finalize (GObject *object)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (object);

    if (helper->screen_on_event_id != 0)
        g_source_remove (helper->screen_on_event_id);

    /* Free the screen resources */
    if (helper->resources)
    {
        gdk_x11_display_error_trap_push (helper->display);
        XRRFreeScreenResources (helper->resources);
        gdk_display_flush (helper->display);
        if (gdk_x11_display_error_trap_pop (helper->display) != 0)
        {
            g_critical ("Failed to free screen resources");
        }
        helper->resources = NULL;
    }

    (*G_OBJECT_CLASS (xfce_displays_helper_x11_parent_class)->finalize) (object);
}



static GPtrArray *
xfce_displays_helper_x11_get_outputs (XfceDisplaysHelper *helper)
{
    return XFCE_DISPLAYS_HELPER_X11 (helper)->outputs;
}



static void
xfce_displays_helper_x11_toggle_internal (gpointer *power,
                                          gboolean lid_is_closed,
                                          XfceDisplaysHelper *_helper)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (_helper);
    GHashTable *saved_outputs;
    XfceRRCrtc *crtc = NULL;
    XfceRROutput *output, *lvds = NULL;
    gboolean active = FALSE;
    guint n;
    gint m;

    if (helper->outputs->len == 1)
    {
        /* If there's only one output left and we pass here, it's supposed to be the
         * internal display or an output we want to reactivate anyway */
        lvds = g_ptr_array_index (helper->outputs, 0);
    }
    else
    {
        for (n = 0; n < helper->outputs->len; ++n)
        {
            output = g_ptr_array_index (helper->outputs, n);
            g_assert (output);

            /* Try to find the internal display */
            if (display_name_is_laptop_name (output->info->name))
            {
                lvds = output;
                break;
            }
        }
    }

    if (!lvds)
        return;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_TOGGLING_INTERNAL, lvds->info->name);

    if (lvds->active && lid_is_closed)
    {
        /* if active and the lid is closed, deactivate it */
        crtc = xfce_displays_helper_x11_find_usable_crtc (helper, lvds);
        if (!crtc)
            return;
        crtc->mode = None;
        crtc->noutput = 0;
        crtc->changed = TRUE;
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_DISABLING_INTERNAL, lvds->info->name);
    }
    else if (!lvds->active && !lid_is_closed)
    {
        /* re-activate it because the user opened the lid */
        XfconfChannel *channel = xfce_displays_helper_get_channel (XFCE_DISPLAYS_HELPER (helper));
        saved_outputs = xfconf_channel_get_properties (channel, "/" DEFAULT_SCHEME_NAME);
        if (saved_outputs)
        {
            /* first, ensure the position of the other outputs is correct */
            for (n = 0; n < helper->outputs->len; ++n)
            {
                output = g_ptr_array_index (helper->outputs, n);
                g_assert (output);

                if (output->id == lvds->id)
                    continue;

                xfce_displays_helper_x11_load_from_xfconf (helper, DEFAULT_SCHEME_NAME,
                                                           saved_outputs, output);
            }

            /* try to load user saved settings for lvds */
            active = xfce_displays_helper_x11_load_from_xfconf (helper, DEFAULT_SCHEME_NAME,
                                                                saved_outputs, lvds);
            g_hash_table_destroy (saved_outputs);
        }
        if (!active)
        {
            /* autoset the preferred mode */
            crtc = xfce_displays_helper_x11_find_usable_crtc (helper, lvds);
            if (!crtc)
                return;
            crtc->mode = lvds->preferred_mode;
            crtc->rotation = RR_Rotate_0;
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            if ((crtc->x > gdk_screen_width () + 1) || (crtc->y > gdk_screen_height () + 1))
            {
                crtc->x = crtc->y = 0;
            } /* else - leave values from last time we saw the monitor */
            G_GNUC_END_IGNORE_DEPRECATIONS
            /* set width and height */
            for (m = 0; m < helper->resources->nmode; ++m)
            {
                if (helper->resources->modes[m].id == lvds->preferred_mode)
                {
                    crtc->width = helper->resources->modes[m].width;
                    crtc->height = helper->resources->modes[m].height;
                    break;
                }
            }
            xfce_displays_helper_x11_set_outputs (crtc, lvds);
            crtc->changed = TRUE;
        }
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_ENABLING_INTERNAL, lvds->info->name);
    }
    else
        return;

    /* apply settings */
    xfce_displays_helper_x11_apply_all (helper);
}



static gchar **
xfce_displays_helper_x11_get_display_infos (XfceDisplaysHelper *_helper)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (_helper);
    if (helper->randr != NULL)
        return xfce_randr_get_display_infos (helper->randr);

    return NULL;
}



static void
xfce_displays_helper_x11_channel_apply (XfceDisplaysHelper *_helper,
                                        const gchar *scheme)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (_helper);
    XfconfChannel *channel = xfce_displays_helper_get_channel (_helper);
    gchar property[512];
    guint n, nactive;
    GHashTable *saved_outputs = NULL;

    helper->primary = None;

    xfconf_channel_set_string (channel, ACTIVE_PROFILE, scheme);

    /* finally the list of saved outputs from xfconf */
    g_snprintf (property, sizeof (property), "/%s", scheme);
    saved_outputs = xfconf_channel_get_properties (channel, property);

    /* nothing saved, nothing to do */
    if (saved_outputs == NULL)
        goto err_cleanup;

    /* first loop, loads all the outputs, and gets the number of active ones */
    nactive = 0;
    for (n = 0; n < helper->outputs->len; ++n)
    {
        if (xfce_displays_helper_x11_load_from_xfconf (helper, scheme, saved_outputs,
                                                       g_ptr_array_index (helper->outputs, n)))
            ++nactive;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_TOTAL_ACTIVE, nactive);

    /* safety check */
    if (nactive == 0)
    {
        g_warning (WARNING_MESSAGE_ALL_DISABLED);
        goto err_cleanup;
    }

    /* apply settings */
    xfce_displays_helper_x11_apply_all (helper);

err_cleanup:
    /* Free the xfconf properties */
    if (saved_outputs)
        g_hash_table_destroy (saved_outputs);
}



static void
xfce_displays_helper_x11_reload (XfceDisplaysHelperX11 *helper)
{
    gint err;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Refreshing RandR cache.");

    if (helper->randr != NULL)
        xfce_randr_reload (helper->randr);

    /* Free the caches */
    g_ptr_array_unref (helper->outputs);
    g_ptr_array_unref (helper->crtcs);

    gdk_x11_display_error_trap_push (helper->display);

    /* Free the screen resources */
    XRRFreeScreenResources (helper->resources);

    /* get the screen resource */
    /* xfce_displays_helper_x11_reload () is usually called after a xrandr notification,
       which means that X is aware of the new hardware already. So, if possible,
       do not reprobe the hardware again. */
    helper->resources = XRRGetScreenResourcesCurrent (helper->xdisplay, GDK_WINDOW_XID (helper->root_window));

    gdk_display_flush (helper->display);
    err = gdk_x11_display_error_trap_pop (helper->display);
    if (err)
        g_critical ("Failed to reload the RandR cache (err: %d).", err);

    /* recreate the caches */
    helper->crtcs = xfce_displays_helper_x11_list_crtcs (helper);
    helper->outputs = xfce_displays_helper_x11_list_outputs (helper);
}

static gboolean
screen_on_event (gpointer data)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (data);
    XfconfChannel *channel = xfce_displays_helper_get_channel (XFCE_DISPLAYS_HELPER (helper));
    GPtrArray *old_outputs;

    helper->screen_on_event_id = 0;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "RRScreenChangeNotify event received.");

    old_outputs = g_ptr_array_ref (helper->outputs);
    xfce_displays_helper_x11_reload (helper);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_DIFF_N_OUTPUTS,
                    old_outputs->len, helper->outputs->len);

    /* Check if we have different amount of outputs and a matching profile and
       apply it if there's only one */
    if (helper->outputs->len != old_outputs->len)
    {
        gint mode = xfconf_channel_get_int (channel, AUTO_ENABLE_PROFILES, AUTO_ENABLE_PROFILES_DEFAULT);
        if (mode == AUTO_ENABLE_PROFILES_ALWAYS
            || (mode == AUTO_ENABLE_PROFILES_ON_CONNECT && helper->outputs->len > old_outputs->len)
            || (mode == AUTO_ENABLE_PROFILES_ON_DISCONNECT && helper->outputs->len < old_outputs->len))
        {
            gchar *matching_profile = xfce_displays_helper_get_matching_profile (XFCE_DISPLAYS_HELPER (helper));
            if (matching_profile != NULL)
            {
                xfce_displays_helper_x11_channel_apply (XFCE_DISPLAYS_HELPER (helper), matching_profile);
                g_free (matching_profile);
                return FALSE;
            }
        }
        xfconf_channel_set_string (channel, ACTIVE_PROFILE, DEFAULT_SCHEME_NAME);
    }

    if (old_outputs->len > helper->outputs->len)
    {
        gboolean changed = FALSE;
        guint nactive = 0;

        /* Diff the new and old output list to find removed outputs */
        for (guint n = 0; n < old_outputs->len; ++n)
        {
            gboolean found = FALSE;
            XfceRROutput *output = g_ptr_array_index (old_outputs, n);
            for (guint m = 0; m < helper->outputs->len && !found; ++m)
            {
                XfceRROutput *o = g_ptr_array_index (helper->outputs, m);
                found = o->id == output->id;
            }
            if (!found)
            {
                XfceRRCrtc *crtc = NULL;

                xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Output disconnected: %s",
                                output->info->name);
                /* force deconfiguring the crtc for the removed output */
                if (output->info->crtc != None)
                    crtc = xfce_displays_helper_x11_find_crtc_by_id (helper,
                                                                     output->info->crtc);
                if (crtc != NULL)
                {
                    crtc->mode = None;
                    xfce_displays_helper_x11_disable_crtc (helper, crtc->id);
                }
                /* if the output was active, we must recalculate the screen size */
                changed |= output->active;
            }
        }

        /* Basically, this means the external output was disconnected,
           so reenable the internal one if needed. */
        for (guint n = 0; n < helper->outputs->len; ++n)
        {
            XfceRROutput *output = g_ptr_array_index (helper->outputs, n);
            if (output->active)
                ++nactive;
        }
        if (nactive == 0)
        {
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_ALL_DISABLED);
            xfce_displays_helper_x11_toggle_internal (NULL, FALSE, XFCE_DISPLAYS_HELPER (helper));
        }
        else if (changed)
            xfce_displays_helper_x11_apply_all (helper);
    }
    else
    {
        gint action = xfconf_channel_get_int (channel, NOTIFY_PROP, ACTION_ON_NEW_OUTPUT_DEFAULT);
        if (action != ACTION_ON_NEW_OUTPUT_DO_NOTHING)
        {
            gboolean changed = FALSE;

            /* Diff the new and old output list to find new outputs */
            for (guint n = 0; n < helper->outputs->len; ++n)
            {
                gboolean found = FALSE;
                XfceRROutput *output = g_ptr_array_index (helper->outputs, n);
                for (guint m = 0; m < old_outputs->len && !found; ++m)
                {
                    XfceRROutput *o = g_ptr_array_index (old_outputs, m);
                    found = o->id == output->id;
                }
                if (!found)
                {
                    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, DEBUG_MESSAGE_NEW_OUTPUT, output->info->name);
                    /* need to enable crtc for output ? */
                    if (output->info->crtc == None)
                    {
                        XfceRRCrtc *crtc = xfce_displays_helper_x11_find_usable_crtc (helper, output);

                        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "enabling crtc for %s", output->info->name);

                        if (crtc != NULL)
                        {
                            crtc->mode = output->preferred_mode;
                            crtc->rotation = RR_Rotate_0;
                            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                            if ((crtc->x > gdk_screen_width () + 1) || (crtc->y > gdk_screen_height () + 1)
                                || action == ACTION_ON_NEW_OUTPUT_MIRROR)
                            {
                                crtc->x = crtc->y = 0;
                            }
                            /* Extend to the right if configured */
                            else if (action == ACTION_ON_NEW_OUTPUT_EXTEND)
                            {
                                crtc->x = helper->width - crtc->width;
                                crtc->y = 0;
                            } /* else - leave values from last time we saw the monitor */
                            G_GNUC_END_IGNORE_DEPRECATIONS
                            /* set width and height */
                            for (gint j = 0; j < helper->resources->nmode; ++j)
                            {
                                if (helper->resources->modes[j].id == output->preferred_mode)
                                {
                                    crtc->width = helper->resources->modes[j].width;
                                    crtc->height = helper->resources->modes[j].height;
                                    break;
                                }
                            }
                            xfce_displays_helper_x11_set_outputs (crtc, output);
                            crtc->changed = TRUE;
                        }
                    }

                    changed = TRUE;
                }
            }

            if (changed)
            {
                xfce_displays_helper_x11_apply_all (helper);

                /* Start the display dialog according to the user preferences */
                if (action == ACTION_ON_NEW_OUTPUT_SHOW_DIALOG)
                {
                    const gchar *cmd = helper->outputs->len <= 2 ? "xfce4-display-settings -m" : "xfce4-display-settings";
                    xfce_spawn_command_line (NULL, cmd, FALSE, FALSE, TRUE, NULL);
                }
            }
        }
    }
    g_ptr_array_unref (old_outputs);

    return FALSE;
}

static GdkFilterReturn
xfce_displays_helper_x11_screen_on_event (GdkXEvent *xevent,
                                          GdkEvent *event,
                                          gpointer data)
{
    XfceDisplaysHelperX11 *helper = XFCE_DISPLAYS_HELPER_X11 (data);
    XEvent *e = xevent;

    if (e == NULL || e->type - helper->event_base != RRScreenChangeNotify)
        return GDK_FILTER_CONTINUE;

    if (helper->screen_on_event_id != 0)
        g_source_remove (helper->screen_on_event_id);
    helper->screen_on_event_id = g_timeout_add_seconds (1, screen_on_event, helper);

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}



static void
xfce_displays_helper_x11_set_screen_size (XfceDisplaysHelperX11 *helper)
{
    gint min_width, min_height, max_width, max_height;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && helper->resources);

    /* get the screen size extremums */
    if (!XRRGetScreenSizeRange (helper->xdisplay, GDK_WINDOW_XID (helper->root_window),
                                &min_width, &min_height, &max_width, &max_height))
    {
        g_warning ("Unable to get the range of screen sizes. "
                   "Display settings may fail to apply.");
        return;
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS,
                    "min_h = %d, min_w = %d, max_h = %d, max_w = %d, "
                    "prev_h = %d, prev_w = %d, prev_hmm = %d, prev_wmm = %d, "
                    "h = %d, w = %d, hmm = %d, wmm = %d.",
                    min_height, min_width, max_height, max_width,
                    gdk_screen_height (), gdk_screen_width (), gdk_screen_height_mm (), gdk_screen_width_mm (), helper->height, helper->width, helper->mm_height, helper->mm_width);
    if (helper->width > max_width || helper->height > max_height)
    {
        g_warning ("Your screen can't handle the requested size. "
                   "%dx%d exceeds the maximum: %dx%d",
                   helper->width, helper->height, max_width, max_height);
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
    G_GNUC_END_IGNORE_DEPRECATIONS
}



static gboolean
xfce_displays_helper_x11_load_from_xfconf (XfceDisplaysHelperX11 *helper,
                                           const gchar *scheme,
                                           GHashTable *saved_outputs,
                                           XfceRROutput *output)
{
    XfceRRCrtc *crtc = NULL;
    GValue *value;
    const gchar *str_value;
    gchar property[512];
    gdouble output_rate, rate;
    gdouble scale;
    RRMode valid_mode;
    Rotation rot;
    gint x, y, n, m, int_value;
    gboolean active;

    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->resources && output);

    active = output->active;

    /* does this output exist in xfconf? */
    g_snprintf (property, sizeof (property), OUTPUT_FMT, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);

    if (value == NULL || !G_VALUE_HOLDS_STRING (value))
        return active;

    /* is it the primary output? */
    g_snprintf (property, sizeof (property), PRIMARY_PROP, scheme,
                output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_BOOLEAN (value) && g_value_get_boolean (value))
        helper->primary = output->id;

    /* status */
    g_snprintf (property, sizeof (property), ACTIVE_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);

    if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value))
        return active;

    /* Get the associated CRTC */
    crtc = xfce_displays_helper_x11_find_usable_crtc (helper, output);
    if (!crtc)
        return active;

    /* disable inactive outputs */
    if (!g_value_get_boolean (value))
    {
        if (crtc->mode != None)
        {
            active = FALSE;
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "%s will be disabled by configuration.",
                            output->info->name);

            crtc->mode = None;
            crtc->noutput = 0;
            crtc->changed = TRUE;
        }
        return active;
    }

    /* rotation */
    g_snprintf (property, sizeof (property), ROTATION_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_INT (value))
        int_value = g_value_get_int (value);
    else
        int_value = 0;

    /* convert to a Rotation */
    switch (int_value)
    {
        case 90: rot = RR_Rotate_90; break;
        case 180: rot = RR_Rotate_180; break;
        case 270: rot = RR_Rotate_270; break;
        default: rot = RR_Rotate_0; break;
    }

    /* reflection */
    g_snprintf (property, sizeof (property), REFLECTION_PROP, scheme, output->info->name);
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
        rot |= (RR_Reflect_X | RR_Reflect_Y);

    /* check rotation support */
    if ((crtc->rotations & rot) == 0)
    {
        g_warning ("Unsupported rotation for %s. Fallback to RR_Rotate_0.", output->info->name);
        rot = RR_Rotate_0;
    }

    /* update CRTC rotation */
    if (crtc->rotation != rot)
    {
        crtc->rotation = rot;
        crtc->changed = TRUE;
    }

    /* resolution */
    g_snprintf (property, sizeof (property), RESOLUTION_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (value == NULL || !G_VALUE_HOLDS_STRING (value))
        str_value = "";
    else
        str_value = g_value_get_string (value);

    /* refresh rate */
    g_snprintf (property, sizeof (property), RRATE_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        output_rate = g_value_get_double (value);
    else
        output_rate = 0.0;

    /* scaling */
    g_snprintf (property, sizeof (property), SCALE_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        scale = g_value_get_double (value);
    else
        scale = 1.0;

    /* backward compatibility; old properties are reset in xfce-randr.c when saving  */
    g_snprintf (property, sizeof (property), SCALEX_PROP, scheme, output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_DOUBLE (value))
        scale = g_value_get_double (value);

    if (scale <= 0.0)
        scale = 1.0;

    if (crtc->scalex != scale || crtc->scaley != scale)
    {
        crtc->scalex = scale;
        crtc->scaley = scale;
        crtc->changed = TRUE;
    }

    /* check mode validity */
    valid_mode = None;
    for (n = 0; n < output->info->nmode; ++n)
    {
        /* walk all modes */
        for (m = 0; m < helper->resources->nmode; ++m)
        {
            /* does the mode info match the mode we seek? */
            if (helper->resources->modes[m].id != output->info->modes[n])
                continue;

            /* calculate the refresh rate */
            rate = (gdouble) helper->resources->modes[m].dotClock
                   / ((gdouble) helper->resources->modes[m].hTotal * (gdouble) helper->resources->modes[m].vTotal);

            /* construct a string equivalent to the mode generated in displays */
            /* property is the resources mode translated into display panel name */
            g_snprintf (property, sizeof (property), "%dx%d", helper->resources->modes[m].width,
                        helper->resources->modes[m].height);

            /* find the mode corresponding to the saved values */
            if (rint (rate * 100) == rint (output_rate * 100)
                && (g_strcmp0 (property, str_value) == 0))
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
        g_warning (WARNING_MESSAGE_UNKNOWN_MODE, str_value, output_rate, output->info->name);
        return active;
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
    if ((crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) != 0)
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
    g_snprintf (property, sizeof (property), POSX_PROP, scheme,
                output->info->name);
    value = g_hash_table_lookup (saved_outputs, property);
    if (G_VALUE_HOLDS_INT (value))
        x = g_value_get_int (value);
    else
        x = 0;

    /* position, y */
    g_snprintf (property, sizeof (property), POSY_PROP, scheme,
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

    xfce_displays_helper_x11_set_outputs (crtc, output);

    return active;
}



static GPtrArray *
xfce_displays_helper_x11_list_outputs (XfceDisplaysHelperX11 *helper)
{
    GPtrArray *outputs;
    XRROutputInfo *output_info;
    XfceRROutput *output;
    XfceRRCrtc *crtc;
    gint best_dist, dist, n, m, l, err;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && helper->resources);

    /* get all connected outputs */
    outputs = g_ptr_array_new_with_free_func ((GDestroyNotify) xfce_displays_helper_x11_free_output);
    for (n = 0; n < helper->resources->noutput; ++n)
    {
        gdk_x11_display_error_trap_push (helper->display);
        output_info = XRRGetOutputInfo (helper->xdisplay, helper->resources, helper->resources->outputs[n]);
        gdk_display_flush (helper->display);
        err = gdk_x11_display_error_trap_pop (helper->display);
        if (err || !output_info)
        {
            g_warning ("Failed to load info for output %lu (err: %d). Skipping.",
                       helper->resources->outputs[n], err);
            continue;
        }

        if (output_info->connection != RR_Connected)
        {
            XRRFreeOutputInfo (output_info);
            continue;
        }

        output = g_new0 (XfceRROutput, 1);
        output->id = helper->resources->outputs[n];
        output->info = output_info;

        /* find the preferred mode */
        output->preferred_mode = None;
        best_dist = 0;
        for (l = 0; l < output->info->nmode; ++l)
        {
            /* walk all modes */
            for (m = 0; m < helper->resources->nmode; ++m)
            {
                /* does the mode info match the mode we seek? */
                if (helper->resources->modes[m].id != output->info->modes[l])
                    continue;

                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                if (l < output->info->npreferred)
                    dist = 0;
                else if ((output->info->mm_height != 0) && (gdk_screen_height_mm () != 0))
                    dist = 1000 * gdk_screen_height () / gdk_screen_height_mm ()
                           - 1000 * helper->resources->modes[m].height / output->info->mm_height;
                else
                    dist = gdk_screen_height () - helper->resources->modes[m].height;
                G_GNUC_END_IGNORE_DEPRECATIONS

                dist = ABS (dist);

                if (output->preferred_mode == None || dist < best_dist)
                {
                    output->preferred_mode = helper->resources->modes[m].id;
                    best_dist = dist;
                }
            }
        }

        /* track active outputs */
        crtc = xfce_displays_helper_x11_find_crtc_by_id (helper, output->info->crtc);
        output->active = crtc && crtc->mode != None;

        /* Translate output->name into xfconf compatible format in place */
        g_strcanon (output->info->name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_<>", '_');

        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected output %lu %s.", output->id,
                        output->info->name);

        /* cache it */
        g_ptr_array_add (outputs, output);
    }

    return outputs;
}



static void
xfce_displays_helper_x11_free_output (XfceRROutput *output)
{
    if (output == NULL)
        return;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    XRRFreeOutputInfo (output->info);
    gdk_display_flush (gdk_display_get_default ());
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) != 0)
    {
        g_critical ("Failed to free output info");
    }
    g_free (output);
}



static GPtrArray *
xfce_displays_helper_x11_list_crtcs (XfceDisplaysHelperX11 *helper)
{
    GPtrArray *crtcs;
    XRRCrtcInfo *crtc_info;
    XfceRRCrtc *crtc;
    gint n, err;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && helper->resources);

    /* get all existing CRTCs */
    crtcs = g_ptr_array_new_with_free_func ((GDestroyNotify) xfce_displays_helper_x11_free_crtc);
    for (n = 0; n < helper->resources->ncrtc; ++n)
    {
        XRRCrtcTransformAttributes *attr;
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Detected CRTC %lu.", helper->resources->crtcs[n]);

        gdk_x11_display_error_trap_push (helper->display);
        crtc_info = XRRGetCrtcInfo (helper->xdisplay, helper->resources, helper->resources->crtcs[n]);
        gdk_display_flush (helper->display);
        err = gdk_x11_display_error_trap_pop (helper->display);
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
        if (XRRGetCrtcTransform (helper->xdisplay, helper->resources->crtcs[n], &attr) && attr)
        {
            crtc->scalex = XFixedToDouble (attr->currentTransform.matrix[0][0]);
            crtc->scaley = XFixedToDouble (attr->currentTransform.matrix[1][1]);
            XFree (attr);
        }
        else
        {
            crtc->scalex = 1.0;
            crtc->scaley = 1.0;
        }

        crtc->noutput = crtc_info->noutput;
        crtc->outputs = NULL;
        if (crtc_info->noutput > 0)
            crtc->outputs = g_memdup2 (crtc_info->outputs,
                                       crtc_info->noutput * sizeof (RROutput));

        crtc->npossible = crtc_info->npossible;
        crtc->possible = NULL;
        if (crtc_info->npossible > 0)
            crtc->possible = g_memdup2 (crtc_info->possible,
                                        crtc_info->npossible * sizeof (RROutput));

        crtc->changed = FALSE;
        XRRFreeCrtcInfo (crtc_info);

        /* cache it */
        g_ptr_array_add (crtcs, crtc);
    }

    return crtcs;
}



static XfceRRCrtc *
xfce_displays_helper_x11_find_crtc_by_id (XfceDisplaysHelperX11 *helper,
                                          RRCrtc id)
{
    XfceRRCrtc *crtc;
    guint n;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->crtcs);

    for (n = 0; n < helper->crtcs->len; ++n)
    {
        crtc = g_ptr_array_index (helper->crtcs, n);
        if (crtc->id == id)
            return crtc;
    }

    return NULL;
}



static void
xfce_displays_helper_x11_free_crtc (XfceRRCrtc *crtc)
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
xfce_displays_helper_x11_find_usable_crtc (XfceDisplaysHelperX11 *helper,
                                           XfceRROutput *output)
{
    XfceRRCrtc *crtc = NULL;
    guint n;
    gint m;
    gboolean found = FALSE;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->crtcs && output);

    if (output->info->crtc != None)
    {
        crtc = xfce_displays_helper_x11_find_crtc_by_id (helper, output->info->crtc);
        found = crtc != NULL;
    }

    /* try to find one that is not already used by another output */
    for (n = 0; n < helper->crtcs->len && !found; ++n)
    {
        crtc = g_ptr_array_index (helper->crtcs, n);
        if (crtc->noutput > 0 || crtc->changed)
            continue;

        for (m = 0; m < crtc->npossible; ++m)
        {
            if (crtc->possible[m] == output->id)
            {
                found = TRUE;
                break;
            }
        }
    }

    if (found)
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu assigned to %s.", crtc->id,
                        output->info->name);
    else
        g_warning ("No available CRTC for %s.", output->info->name);

    return crtc;
}



static void
xfce_displays_helper_x11_get_topleftmost_pos (XfceRRCrtc *crtc,
                                              XfceDisplaysHelperX11 *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && crtc);

    if (crtc->mode == None)
        return;

    /* used to normalize positions later */
    helper->min_x = MIN (helper->min_x, crtc->x);
    helper->min_y = MIN (helper->min_y, crtc->y);
}



static void
xfce_displays_helper_x11_normalize_crtc (XfceRRCrtc *crtc,
                                         XfceDisplaysHelperX11 *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && crtc);

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
    helper->width = MAX (helper->width, crtc->x + round (crtc->width * crtc->scalex));
    helper->height = MAX (helper->height, crtc->y + round (crtc->height * crtc->scaley));

    /* The 'physical size' of an X screen is meaningless if that screen
     * can consist of many monitors. So just pick a size that make the
     * dpi 96.
     *
     * Firefox and Evince apparently believe what X tells them.
     */
    helper->mm_width = round ((helper->width / 96.0) * 25.4 + 0.5);
    helper->mm_height = round ((helper->height / 96.0) * 25.4 + 0.5);
}



static Status
xfce_displays_helper_x11_disable_crtc (XfceDisplaysHelperX11 *helper,
                                       RRCrtc crtc)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && helper->resources);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Disabling CRTC %lu.", crtc);

    return XRRSetCrtcConfig (helper->xdisplay, helper->resources, crtc,
                             CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
}



static void
xfce_displays_helper_x11_workaround_crtc_size (XfceRRCrtc *crtc,
                                               XfceDisplaysHelperX11 *helper)
{
    XRRCrtcInfo *crtc_info;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && helper->resources && crtc);

    /* The CRTC needs to be disabled if its previous mode won't fit in the new screen.
       It will be reenabled with its new mode (known to fit) after the screen size is
       changed, unless the user disabled it (no need to reenable it then). */
    crtc_info = XRRGetCrtcInfo (helper->xdisplay, helper->resources, crtc->id);
    if (crtc_info->x + crtc_info->width > (guint) helper->width
        || crtc_info->y + crtc_info->height > (guint) helper->height)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu must be temporarily disabled.", crtc->id);
        if (xfce_displays_helper_x11_disable_crtc (helper, crtc->id) == RRSetConfigSuccess)
            crtc->changed = (crtc->mode != None);
        else
            g_warning ("Failed to temporarily disable CRTC %lu.", crtc->id);
    }
    XRRFreeCrtcInfo (crtc_info);
}



static void
xfce_displays_helper_x11_apply_crtc_transform (XfceRRCrtc *crtc,
                                               XfceDisplaysHelperX11 *helper)
{
    XTransform transform;
    gchar *filter;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && crtc);

    if (!crtc->changed)
        return;

    if (crtc->scalex == 1 && crtc->scaley == 1)
        filter = "nearest";
    else
        filter = "bilinear";

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying CRTC %lu Transform: x=%lf y=%lf, filter=%s.", crtc->id,
                    crtc->scalex, crtc->scaley, filter);

    memset (&transform, '\0', sizeof (transform));

    transform.matrix[0][0] = XDoubleToFixed (crtc->scalex);
    transform.matrix[1][1] = XDoubleToFixed (crtc->scaley);
    transform.matrix[2][2] = XDoubleToFixed (1.0);

    gdk_x11_display_error_trap_push (helper->display);
    XRRSetCrtcTransform (helper->xdisplay, crtc->id, &transform, filter, NULL, 0);
    if (gdk_x11_display_error_trap_pop (helper->display) != 0)
    {
        g_warning ("Failed to apply the scale, maybe the CRTC does not support transforms");
    }
}



static void
xfce_displays_helper_x11_apply_crtc (XfceRRCrtc *crtc,
                                     XfceDisplaysHelperX11 *helper)
{
    Status ret;

    g_assert (XFCE_IS_DISPLAYS_HELPER_X11 (helper) && helper->xdisplay && helper->resources && crtc);

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Configuring CRTC %lu.", crtc->id);

    /* check if we really need to do something */
    if (crtc->changed)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying changes to CRTC %lu.", crtc->id);

        if (crtc->mode == None)
        {
            ret = xfce_displays_helper_x11_disable_crtc (helper, crtc->id);
        }
        else
        {
            xfce_displays_helper_x11_apply_crtc_transform (crtc, helper);

            ret = XRRSetCrtcConfig (helper->xdisplay, helper->resources, crtc->id,
                                    CurrentTime, crtc->x, crtc->y, crtc->mode,
                                    crtc->rotation, crtc->outputs, crtc->noutput);
        }

        if (ret == RRSetConfigSuccess)
            crtc->changed = FALSE;
        else
            g_warning ("Failed to configure CRTC %lu.", crtc->id);
    }
}



static void
xfce_displays_helper_x11_set_outputs (XfceRRCrtc *crtc,
                                      XfceRROutput *output)
{
    gint n;

    g_assert (crtc && output);

    for (n = 0; n < crtc->noutput; ++n)
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu, output list[%d] -> %lu.", crtc->id, n,
                        crtc->outputs[n]);

    /* check if the output is already present */
    for (n = 0; n < crtc->noutput; ++n)
    {
        if (crtc->outputs[n] == output->id)
            return;
    }


    if (crtc->outputs)
        crtc->outputs = g_realloc (crtc->outputs, (crtc->noutput + 1) * sizeof (RROutput));
    else
        crtc->outputs = g_new0 (RROutput, 1);

    g_assert (crtc->outputs);

    crtc->outputs[crtc->noutput++] = output->id;
    crtc->changed = TRUE;

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "CRTC %lu, output list[%d] -> %lu.", crtc->id,
                    crtc->noutput - 1, crtc->outputs[crtc->noutput - 1]);
}



static void
xfce_displays_helper_x11_apply_all (XfceDisplaysHelperX11 *helper)
{
    g_assert (XFCE_IS_DISPLAYS_HELPER (helper) && helper->crtcs);

    helper->mm_width = helper->mm_height = helper->width = helper->height = 0;
    helper->min_x = helper->min_y = 32768;

    /* normalization and screen size calculation */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_x11_get_topleftmost_pos, helper);
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_x11_normalize_crtc, helper);

    gdk_x11_display_error_trap_push (helper->display);

    /* grab server to prevent clients from thinking no output is enabled */
    gdk_x11_display_grab (helper->display);

    /* disable CRTCs that won't fit in the new screen */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_x11_workaround_crtc_size, helper);

    /* set the screen size only if it's really needed and valid */
    xfce_displays_helper_x11_set_screen_size (helper);

    /* final loop, apply crtc changes */
    g_ptr_array_foreach (helper->crtcs, (GFunc) xfce_displays_helper_x11_apply_crtc, helper);

    XRRSetOutputPrimary (helper->xdisplay, GDK_WINDOW_XID (helper->root_window), helper->primary);

    /* release the grab, changes are done */
    gdk_display_sync (helper->display);
    gdk_x11_display_ungrab (helper->display);
    if (gdk_x11_display_error_trap_pop (helper->display) != 0)
    {
        g_warning ("Failed to apply display settings");
    }
}
