/*
 *  Copyright (c) 2019 Simon Steinbeiß <simon@xfce.org>
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

#include "display-profiles.h"

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
