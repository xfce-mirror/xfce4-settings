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

#include "displays.h"
#include "displays-iio.h"

#ifdef HAVE_UPOWERGLIB
#include "displays-upower.h"
#endif

#ifdef HAVE_XRANDR
#include "displays-x11.h"

#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include "displays-wayland.h"

#include <gdk/gdkwayland.h>
#endif

#include "common/debug.h"
#include "common/display-profiles.h"

#include <libxfce4ui/libxfce4ui.h>


#define get_instance_private(instance) \
    ((XfceDisplaysHelperPrivate *) xfce_displays_helper_get_instance_private (XFCE_DISPLAYS_HELPER (instance)))

static void
xfce_displays_helper_constructed (GObject *object);
static void
xfce_displays_helper_finalize (GObject *object);

static void
xfce_displays_helper_iio_set_enabled (XfceDisplaysHelper *helper,
                                      gboolean enabled);
static gboolean
xfce_displays_helper_auto_rotate_enabled (XfceDisplaysHelper *helper);
static void
xfce_displays_helper_apply_profile (XfceDisplaysHelper *helper,
                                    const gchar *profile,
                                    gint rotation);


typedef struct _XfceDisplaysHelperPrivate
{
    XfconfChannel *channel;
#ifdef HAVE_UPOWERGLIB
    XfceDisplaysUPower *power;
#endif
    gulong iio_handler_id;
    XfceDisplaysIIO *iio;

    /* auto-rotate apply state: see xfce_displays_helper_apply_profile() */
    gboolean applying;          /* a profile apply is currently in flight */
    gboolean pending_rotation;  /* an orientation change arrived while applying */
    gint     pending_rotation_value;     /* latest rotation requested during that apply */
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



static GHashTable *
xfce_displays_helper_active_profile_properties (XfceDisplaysHelper *helper,
                                                gchar **profile)
{
    XfceDisplaysHelperPrivate *priv = get_instance_private (helper);
    g_autofree gchar *name = NULL;
    g_autofree gchar *root = NULL;
    GHashTable *props;

    name = xfconf_channel_get_string (priv->channel, ACTIVE_PROFILE, DEFAULT_SCHEME_NAME);
    root = g_strdup_printf ("/%s", name);
    props = xfconf_channel_get_properties (priv->channel, root);

    if (profile != NULL)
        *profile = g_steal_pointer (&name);

    return props;
}



static void
xfce_displays_helper_channel_property_changed (XfconfChannel *channel,
                                               const gchar *property_name,
                                               const GValue *value,
                                               XfceDisplaysHelper *helper)
{
    if (G_UNLIKELY (G_VALUE_HOLDS_STRING (value) && g_strcmp0 (property_name, APPLY_SCHEME_PROP) == 0))
    {
        /* apply */
        XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, g_value_get_string (value));
        /* remove the apply property */
        xfconf_channel_reset_property (channel, APPLY_SCHEME_PROP, FALSE);
    }
    else if (g_str_has_suffix (property_name, AUTO_ROTATE_SUFFIX)
             || g_strcmp0 (property_name, ACTIVE_PROFILE) == 0)
    {
        /* an output of the active profile (or the profile itself) was toggled:
         * claim the accelerometer proxy if auto-rotate is enabled on any output */
        xfce_displays_helper_iio_set_enabled (helper, xfce_displays_helper_auto_rotate_enabled (helper));
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
        gchar *matching_profile;
        gint mode;

#ifdef HAVE_UPOWERGLIB
        priv->power = g_object_new (XFCE_TYPE_DISPLAYS_UPOWER, NULL);
        g_signal_connect (G_OBJECT (priv->power),
                          "lid-changed",
                          G_CALLBACK (XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->toggle_internal),
                          helper);
#endif

        /* open the channel */
        priv->channel = display_settings_profiles_channel_get ();

        /* remove any leftover apply property before setting the monitor */
        xfconf_channel_reset_property (priv->channel, APPLY_SCHEME_PROP, FALSE);

        /* monitor channel changes */
        g_signal_connect_object (G_OBJECT (priv->channel),
                                 "property-changed",
                                 G_CALLBACK (xfce_displays_helper_channel_property_changed),
                                 helper, G_CONNECT_DEFAULT);

        /*  check if we can auto-enable a profile */
        matching_profile = xfce_displays_helper_get_matching_profile (helper);
        mode = xfconf_channel_get_int (priv->channel, AUTO_ENABLE_PROFILES, AUTO_ENABLE_PROFILES_DEFAULT);
        if (matching_profile != NULL && (mode == AUTO_ENABLE_PROFILES_ON_CONNECT || mode == AUTO_ENABLE_PROFILES_ALWAYS))
        {
            XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, matching_profile);
        }
        else
        {
            XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, DEFAULT_SCHEME_NAME);
        }
        g_free (matching_profile);

        xfce_displays_helper_iio_set_enabled (helper, xfce_displays_helper_auto_rotate_enabled (helper));
    }

    G_OBJECT_CLASS (xfce_displays_helper_parent_class)->constructed (object);
}



static void
xfce_displays_helper_finalize (GObject *object)
{
    XfceDisplaysHelperPrivate *priv = get_instance_private (object);

#ifdef HAVE_UPOWERGLIB
    if (priv->power != NULL)
        g_object_unref (priv->power);
#endif

    if (priv->iio != NULL)
        g_object_unref (priv->iio);

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



gchar *
xfce_displays_helper_get_matching_profile (XfceDisplaysHelper *helper)
{
    XfceDisplaysHelperPrivate *priv = get_instance_private (helper);
    GList *profiles = NULL;
    gchar **display_infos = XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->get_display_infos (helper);
    gchar *profile = NULL;
    gboolean default_matches = FALSE;

    if (display_infos != NULL)
    {
        profiles = display_settings_get_profiles (display_infos, priv->channel, TRUE);
        default_matches = display_settings_profile_matches ("Default", display_infos, priv->channel);
        if (profiles == NULL && default_matches)
        {
            /* if user profile matching failed, use Default if possible */
            profiles = g_list_prepend (profiles, g_strdup ("Default"));
        }
        g_strfreev (display_infos);
    }

    if (profiles == NULL)
    {
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "No matching display profiles found.");
    }
    else if (g_list_length (profiles) == 1)
    {
        profile = g_strdup (profiles->data);
        xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Applying the only matching display profile: %s", profile);
    }
    else
    {
        if (default_matches)
        {
            profile = g_strdup ("Default");
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Found %d matching display profiles, applying %s",
                            g_list_length (profiles) + 1, profile);
        }
        else
        {
            xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Found %d matching display profiles, unable to choose",
                            g_list_length (profiles));
        }
    }

    g_list_free_full (profiles, g_free);

    return profile;
}



XfconfChannel *
xfce_displays_helper_get_channel (XfceDisplaysHelper *helper)
{
    return get_instance_private (helper)->channel;
}



static gboolean
xfce_displays_helper_auto_rotate_enabled (XfceDisplaysHelper *helper)
{
    GHashTable *props = xfce_displays_helper_active_profile_properties (helper, NULL);
    GHashTableIter iter;
    gpointer key, value;
    gboolean enabled = FALSE;

    if (props == NULL)
        return FALSE;

    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        const gchar *prop_key = key;
        const GValue *val = value;

        if (g_str_has_suffix (prop_key, AUTO_ROTATE_SUFFIX)
            && G_VALUE_HOLDS_BOOLEAN (val)
            && g_value_get_boolean (val))
        {
            enabled = TRUE;
            break;
        }
    }

    g_hash_table_destroy (props);

    return enabled;
}



static void
xfce_displays_helper_apply_profile (XfceDisplaysHelper *helper,
                                    const gchar *profile,
                                    gint rotation)
{
    XfceDisplaysHelperPrivate *priv = get_instance_private (helper);

    if (priv->applying)
    {
        priv->pending_rotation = TRUE;
        priv->pending_rotation_value = rotation;
        return;
    }

    priv->applying = TRUE;
    XFCE_DISPLAYS_HELPER_GET_CLASS (helper)->channel_apply (helper, profile);

    if (priv->pending_rotation)
    {
        priv->pending_rotation = FALSE;
        xfce_displays_helper_apply_profile (helper, profile, priv->pending_rotation_value);
        return;
    }

    priv->applying = FALSE;
}



static void
xfce_displays_helper_iio_set_orientation_cb (XfceDisplaysIIO *iio,
                                             gint rotation,
                                             gpointer user_data)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (user_data);
    XfceDisplaysHelperPrivate *priv = get_instance_private (helper);
    g_autofree gchar *profile = NULL;
    GHashTable *props;
    GHashTableIter iter;
    gpointer key, value;
    gboolean need_apply = FALSE;

    g_debug ("Accelerometer reported orientation rotation: %d degrees", rotation);

    props = xfce_displays_helper_active_profile_properties (helper, &profile);
    if (props == NULL)
        return;

    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        const gchar *prop_key = key;
        const GValue *val = value;
        g_autofree gchar *output_root = NULL;
        g_autofree gchar *rotation_prop = NULL;
        const GValue *rotation_val;

        /* only outputs that opted in to auto-rotate are rotated */
        if (!g_str_has_suffix (prop_key, AUTO_ROTATE_SUFFIX)
            || !G_VALUE_HOLDS_BOOLEAN (val)
            || !g_value_get_boolean (val))
            continue;

        output_root = g_strndup (prop_key, strlen (prop_key) - strlen (AUTO_ROTATE_SUFFIX));
        rotation_prop = g_strconcat (output_root, ROTATION_SUFFIX, NULL);
        rotation_val = g_hash_table_lookup (props, rotation_prop);
        if (rotation_val == NULL || !G_VALUE_HOLDS_INT (rotation_val))
            continue;

        if (g_value_get_int (rotation_val) != rotation)
        {
            xfconf_channel_set_int (priv->channel, rotation_prop, rotation);
            need_apply = TRUE;
        }
    }

    g_hash_table_destroy (props);

    if (need_apply)
        xfce_displays_helper_apply_profile (helper, profile, rotation);
}



static void
xfce_displays_helper_iio_set_enabled (XfceDisplaysHelper *helper,
                                      gboolean enabled)
{
    XfceDisplaysHelperPrivate *priv = get_instance_private (helper);
    if (enabled && priv->iio == NULL)
    {
        priv->iio = g_object_new (XFCE_TYPE_DISPLAYS_IIO, NULL);

        priv->iio_handler_id = g_signal_connect (
            priv->iio,
            "orientation-changed",
            G_CALLBACK (xfce_displays_helper_iio_set_orientation_cb),
            helper);
    }
    else if (!enabled && priv->iio != NULL)
    {
        if (priv->iio_handler_id > 0)
        {
            g_signal_handler_disconnect (priv->iio, priv->iio_handler_id);
            priv->iio_handler_id = 0;
        }

        g_clear_object (&priv->iio);
    }
}
