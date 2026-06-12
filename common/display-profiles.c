/*
 *  Copyright (c) 2019 Simon Steinbeiß <simon@xfce.org>
 *  Copyright (c) 2026 Brian Tarricone <brian@tarricone.org>
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

#include <gdk/gdk.h>
#include <string.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "display-profiles.h"

#define DISPLAYS_CHANNEL_X11 "displays"

#ifdef ENABLE_WAYLAND
#define DISPLAYS_CHANNEL_WAYLAND "displays-wl"
#define PROP_X11_MIGRATION_DONE "/X11MigrationDone"
#endif

static gboolean
is_user_profile (const gchar *property,
                 XfconfChannel *channel)
{
    GHashTable *props = xfconf_channel_get_properties (channel, property);
    gboolean is_user_profile = FALSE;

    if (g_hash_table_size (props) > 1 && g_strrstr_len (property, -1, "/") == property)
    {
        GHashTableIter iter;
        gpointer key;
        g_hash_table_iter_init (&iter, props);
        while (g_hash_table_iter_next (&iter, &key, NULL))
        {
            gchar **tokens = g_strsplit (key, "/", -1);
            gboolean maybe_output_name = g_strv_length (tokens) == 3;
            g_strfreev (tokens);
            if (maybe_output_name)
            {
                gchar *prop = g_strdup_printf ("%s/EDID", (gchar *) key);
                gchar *edid = xfconf_channel_get_string (channel, prop, NULL);
                g_free (prop);
                if (edid != NULL)
                {
                    is_user_profile = TRUE;
                    g_free (edid);
                    break;
                }
            }
        }
    }
    g_hash_table_destroy (props);

    return is_user_profile;
}

XfconfChannel *
display_settings_profiles_channel_get (void)
{
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        return xfconf_channel_get (DISPLAYS_CHANNEL_X11);
    }
#endif

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
        return xfconf_channel_get (DISPLAYS_CHANNEL_WAYLAND);
    }
#endif

    g_error ("No supported windowing system for current environment");
}

gboolean
display_settings_profile_name_exists (XfconfChannel *channel,
                                      const gchar *new_profile_name)
{
    GHashTable *props = xfconf_channel_get_properties (channel, NULL);
    GHashTableIter iter;
    gpointer key;
    gboolean exists = FALSE;

    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, &key, NULL))
    {
        if (is_user_profile (key, channel))
        {
            gchar *old_profile_name = xfconf_channel_get_string (channel, key, NULL);
            exists = g_strcmp0 (new_profile_name, old_profile_name) == 0;
            g_free (old_profile_name);
            if (exists)
                break;
        }
    }
    g_hash_table_destroy (props);

    return exists;
}

GList *
display_settings_get_profiles (gchar **display_infos,
                               XfconfChannel *channel,
                               gboolean matching_only)
{
    GHashTable *props = xfconf_channel_get_properties (channel, NULL);
    GList *profiles = NULL;
    GHashTableIter iter;
    gpointer key;

    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, &key, NULL))
    {
        if (is_user_profile (key, channel))
        {
            const gchar *profile = (gchar *) key + 1; /* remove leading '/' */
            if (!matching_only || display_settings_profile_matches (profile, display_infos, channel))
            {
                profiles = g_list_prepend (profiles, g_strdup (profile));
            }
        }
    }

    g_hash_table_destroy (props);

    return profiles;
}

gboolean
display_settings_profile_matches (const gchar *profile,
                                  gchar **display_infos,
                                  XfconfChannel *channel)
{
    /* Walk through the profile and check if every EDID referenced there is also currently available */
    gchar *profile_prop = g_strdup_printf ("/%s", profile);
    GHashTable *props = xfconf_channel_get_properties (channel, profile_prop);
    GHashTableIter iter;
    gpointer key;
    guint n_infos = g_strv_length (display_infos);
    guint n_outputs = 0;
    gboolean all_match = FALSE;

    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, &key, NULL))
    {
        gchar **tokens = g_strsplit (key, "/", -1);
        gboolean is_output_name = g_strv_length (tokens) == 3;
        g_strfreev (tokens);
        if (is_output_name)
        {
            gchar *property;
            gchar *edid;
            if (++n_outputs > n_infos)
                break;

            property = g_strdup_printf ("%s/EDID", (gchar *) key);
            edid = xfconf_channel_get_string (channel, property, NULL);
            all_match = g_strv_contains ((const gchar *const *) display_infos, edid);
            g_free (edid);
            g_free (property);
            if (!all_match)
                break;
        }
    }
    g_hash_table_destroy (props);
    g_free (profile_prop);

    return all_match && n_outputs == n_infos;
}

#ifdef ENABLE_WAYLAND

static void
migrate_profile (XfconfChannel *x11_channel,
                 XfconfChannel *wl_channel,
                 const gchar *profile_root_prop,
                 gint ui_scale_factor)
{
    GHashTable *properties = xfconf_channel_get_properties (x11_channel, profile_root_prop);

    const GValue *profile_name_value = g_hash_table_lookup (properties, profile_root_prop);
    if (profile_name_value != NULL)
    {
        xfconf_channel_set_property (wl_channel, profile_root_prop, profile_name_value);
    }

    GHashTableIter iter;
    g_hash_table_iter_init (&iter, properties);

    GList *monitors = NULL;
    gchar *property_name = NULL;
    while (g_hash_table_iter_next (&iter, (gpointer) &property_name, NULL))
    {
        const gchar *last = g_strrstr (property_name, "/Active");
        if (last != NULL && (last + strlen ("/Active"))[0] == '\0' && last > property_name)
        {
            const gchar *connector_name_slash = g_strrstr_len (property_name, last - property_name, "/");
            if (connector_name_slash != NULL && connector_name_slash > property_name)
            {
                gchar *connector_name = g_strndup (connector_name_slash + 1, last - connector_name_slash - 1);
                monitors = g_list_prepend (monitors, connector_name);
            }
        }
    }

    static const gchar *profile_copy_verbatim[] = {
        "Active",
        "DuplicateEDID",
        "EDID",
        "ModeFlags",
        "Primary",
        "Reflection",
        "RefreshRate",
        "Resolution",
        "Rotation",
    };

    for (GList *mp = monitors; mp != NULL; mp = mp->next)
    {
        const gchar *connector_name = mp->data;
        gchar *monitor_root_prop = g_strconcat (profile_root_prop, "/", connector_name, NULL);

        const GValue *monitor_name_value = g_hash_table_lookup (properties, monitor_root_prop);
        if (monitor_name_value != NULL)
        {
            xfconf_channel_set_property (wl_channel, monitor_root_prop, monitor_name_value);
        }

        for (gsize i = 0; i < G_N_ELEMENTS (profile_copy_verbatim); i++)
        {
            gchar *prop = g_strconcat (monitor_root_prop, "/", profile_copy_verbatim[i], NULL);
            const GValue *value = g_hash_table_lookup (properties, prop);
            if (value != NULL)
            {
                xfconf_channel_set_property (wl_channel, prop, value);
            }

            g_free (prop);
        }

        gchar *scale_prop = g_strconcat (monitor_root_prop, "/Scale", NULL);
        const GValue *scale_value = g_hash_table_lookup (properties, scale_prop);
        if (scale_value == NULL)
        {
            gchar *scale_x_prop = g_strconcat (monitor_root_prop, "/Scale/X", NULL);
            scale_value = g_hash_table_lookup (properties, scale_x_prop);
            g_free (scale_x_prop);
        }
        gdouble x11_scale = MAX (0.01, G_VALUE_HOLDS_DOUBLE (scale_value) ? g_value_get_double (scale_value) : 1.0);
        gdouble wl_scale = (gdouble) ui_scale_factor / x11_scale;
        xfconf_channel_set_double (wl_channel, scale_prop, MAX (wl_scale, 0.01));
        g_free (scale_prop);

        gchar *posx_prop = g_strconcat (monitor_root_prop, "/Position/X", NULL);
        const GValue *posx_value = g_hash_table_lookup (properties, posx_prop);
        gint x11_posx = G_VALUE_HOLDS_INT (posx_value) ? g_value_get_int (posx_value) : 0;
        gint wl_posx = x11_posx / ui_scale_factor;
        xfconf_channel_set_int (wl_channel, posx_prop, wl_posx);
        g_free (posx_prop);

        gchar *posy_prop = g_strconcat (monitor_root_prop, "/Position/Y", NULL);
        const GValue *posy_value = g_hash_table_lookup (properties, posy_prop);
        gint x11_posy = G_VALUE_HOLDS_INT (posy_value) ? g_value_get_int (posy_value) : 0;
        gint wl_posy = x11_posy / ui_scale_factor;
        xfconf_channel_set_int (wl_channel, posy_prop, wl_posy);
        g_free (posy_prop);

        g_free (monitor_root_prop);
    }

    g_list_free_full (monitors, g_free);
    g_hash_table_destroy (properties);
}

void
display_settings_wayland_migrate_profiles (void)
{
    XfconfChannel *wl_channel = xfconf_channel_get (DISPLAYS_CHANNEL_WAYLAND);
    if (xfconf_channel_get_bool (wl_channel, PROP_X11_MIGRATION_DONE, FALSE))
    {
        return;
    }

    xfconf_channel_set_bool (wl_channel, PROP_X11_MIGRATION_DONE, TRUE);

    XfconfChannel *xsettings = xfconf_channel_get ("xsettings");
    gint ui_scale_factor = MAX (1, xfconf_channel_get_int (xsettings, "/Gdk/WindowScalingFactor", 1));

    XfconfChannel *x11_channel = xfconf_channel_get (DISPLAYS_CHANNEL_X11);
    GList *profiles = display_settings_get_profiles (NULL, x11_channel, FALSE);

    for (GList *pp = profiles; pp != NULL; pp = pp->next)
    {
        const gchar *profile_id = pp->data;
        gchar *profile_root_prop = g_strconcat ("/", profile_id, NULL);
        migrate_profile (x11_channel, wl_channel, profile_root_prop, ui_scale_factor);
        g_free (profile_root_prop);
    }
    g_list_free_full (profiles, g_free);

    migrate_profile (x11_channel, wl_channel, "/Default", ui_scale_factor);

    static const gchar *root_copy_verbatim[] = {
        "/ActiveProfile",
        "/AutoEnableProfiles",
        "/IdentityPopups",
        "/Notify",
    };
    for (gsize i = 0; i < G_N_ELEMENTS (root_copy_verbatim); i++)
    {
        GValue value = G_VALUE_INIT;
        if (xfconf_channel_get_property (x11_channel, root_copy_verbatim[i], &value))
        {
            xfconf_channel_set_property (wl_channel, root_copy_verbatim[i], &value);
            g_value_unset (&value);
        }
    }
}

#endif
