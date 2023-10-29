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
#include <config.h>
#endif

#include <xfconf/xfconf.h>
#include <libxfce4ui/libxfce4ui.h>

#include "common/display-profiles.h"

#include "common/debug.h"
#include "displays.h"
#ifdef HAVE_UPOWERGLIB
#include "displays-upower.h"
#endif

#ifdef HAVE_XRANDR
#include <gdk/gdkx.h>
#include "displays-x11.h"
#endif
#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#include "displays-wayland.h"
#endif



#define get_instance_private(instance) ((XfceDisplaysHelperPrivate *) \
    xfce_displays_helper_get_instance_private (XFCE_DISPLAYS_HELPER (instance)))

static void             xfce_displays_helper_constructed                    (GObject                 *object);
static void             xfce_displays_helper_finalize                       (GObject                 *object);



typedef struct _XfceDisplaysHelperPrivate
{
    XfconfChannel *channel;
#ifdef HAVE_UPOWERGLIB
    XfceDisplaysUPower *power;
#endif
} XfceDisplaysHelperPrivate;



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfceDisplaysHelper, xfce_displays_helper, G_TYPE_OBJECT);



static void
xfce_displays_helper_class_init (XfceDisplaysHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = xfce_displays_helper_constructed;
    gobject_class->finalize = xfce_displays_helper_finalize;
}



static void
xfce_displays_helper_init (XfceDisplaysHelper *helper)
{
}



static void
xfce_displays_helper_channel_property_changed (XfconfChannel      *channel,
                                               const gchar        *property_name,
                                               const GValue       *value,
                                               XfceDisplaysHelper *helper)
{
    if (G_UNLIKELY (G_VALUE_HOLDS_STRING (value) &&
        g_strcmp0 (property_name, APPLY_SCHEME_PROP) == 0))
    {
        /* apply */
        XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, g_value_get_string (value));
        /* remove the apply property */
        xfconf_channel_reset_property (channel, APPLY_SCHEME_PROP, FALSE);
    }
}



static void
xfce_displays_helper_constructed (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);
    XfceDisplaysHelperPrivate *priv = get_instance_private (object);

    /* if X11/Wayland impl init suceeded */
    if (XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->get_outputs (helper) != NULL)
    {
        const gchar *matching_profile;

#ifdef HAVE_UPOWERGLIB
        priv->power = g_object_new (XFCE_TYPE_DISPLAYS_UPOWER, NULL);
        g_signal_connect (G_OBJECT (priv->power),
                          "lid-changed",
                          G_CALLBACK (XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->toggle_internal),
                          helper);
#endif

        /* open the channel */
        priv->channel = xfconf_channel_get ("displays");

        /* remove any leftover apply property before setting the monitor */
        xfconf_channel_reset_property (priv->channel, APPLY_SCHEME_PROP, FALSE);
        xfconf_channel_set_string (priv->channel, ACTIVE_PROFILE, DEFAULT_SCHEME_NAME);

        /* monitor channel changes */
        g_signal_connect_object (G_OBJECT (priv->channel),
                                 "property-changed",
                                 G_CALLBACK (xfce_displays_helper_channel_property_changed),
                                 helper, 0);

        /*  check if we can auto-enable a profile */
        matching_profile = xfce_displays_helper_get_matching_profile (helper);
        if (matching_profile != NULL)
        {
            XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, matching_profile);
        }
        else
        {
            XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, DEFAULT_SCHEME_NAME);
        }
    }

    G_OBJECT_CLASS (xfce_displays_helper_parent_class)->constructed (object);
}



static void
xfce_displays_helper_finalize (GObject *object)
{
#ifdef HAVE_UPOWERGLIB
    XfceDisplaysHelperPrivate *priv = get_instance_private (object);
    if (priv->power != NULL)
        g_object_unref (priv->power);
#endif

    G_OBJECT_CLASS (xfce_displays_helper_parent_class)->finalize (object);
}



GObject *
xfce_displays_helper_new (void)
{
#ifdef HAVE_XRANDR
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        return g_object_new (XFCE_TYPE_DISPLAYS_HELPER_X11, NULL);
#endif
#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
        return g_object_new (XFCE_TYPE_DISPLAYS_HELPER_WAYLAND, NULL);
#endif

    g_critical ("Display settings are not supported on this windowing environment");

    return NULL;
}



const gchar *
xfce_displays_helper_get_matching_profile (XfceDisplaysHelper *helper)
{
    XfceDisplaysHelperPrivate *priv = get_instance_private (helper);
    GList *profiles = NULL;
    gchar **display_infos;

    if (xfconf_channel_get_bool (priv->channel, AUTO_ENABLE_PROFILES, FALSE)
        || xfconf_channel_get_int (priv->channel, NOTIFY_PROP, 1) == 0)
        return NULL;

    display_infos = XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->get_display_infos (helper);
    if (display_infos != NULL)
    {
        profiles = display_settings_get_profiles (display_infos, priv->channel);
        g_strfreev (display_infos);
    }

    if (profiles == NULL)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "No matching display profiles found.");
    }
    else if (g_list_length (profiles) == 1)
    {
        gchar *property = g_strdup_printf ("/%s", (gchar *) profiles->data);
        gchar *profile_name = xfconf_channel_get_string (priv->channel, property, NULL);
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applied the only matching display profile: %s", profile_name);
        g_free (profile_name);
        g_free (property);
        return profiles->data;
    }
    else
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Found %d matching display profiles.", g_list_length (profiles));
    }

    return NULL;
}



XfconfChannel *
xfce_displays_helper_get_channel (XfceDisplaysHelper *helper)
{
    return get_instance_private (helper)->channel;
}
