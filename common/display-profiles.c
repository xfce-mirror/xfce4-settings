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
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <display-profiles.h>

gint get_size (gchar **i);

gint
get_size (gchar **i) {
    gint num = 0;
    while (*i != NULL) {
        num++;
        i++;
    }
    return num;
}

gboolean
display_settings_profile_name_exists (XfconfChannel *channel, const gchar *new_profile_name)
{
    GHashTable *properties;
    GList *channel_contents, *current;

    properties = xfconf_channel_get_properties (channel, NULL);
    channel_contents = g_hash_table_get_keys (properties);

    /* get all profiles */
    current = g_list_first (channel_contents);
    while (current)
    {
        gchar **current_elements = g_strsplit (current->data, "/", -1);
        gchar *old_profile_name;

        if (get_size (current_elements) != 2)
        {
            g_strfreev (current_elements);
            current = g_list_next (current);
            continue;
        }

        old_profile_name = xfconf_channel_get_string (channel, current->data, NULL);
        if (g_strcmp0 (new_profile_name, old_profile_name) == 0)
        {
            g_free (old_profile_name);
            return FALSE;
        }
        g_free (old_profile_name);

        current = g_list_next (current);
    }
    g_list_free (channel_contents);
    g_hash_table_destroy (properties);
    return TRUE;
}

GList*
display_settings_get_profiles (gchar **display_infos, XfconfChannel *channel)
{
    GHashTable *properties;
    GList      *channel_contents;
    GList      *profiles = NULL;
    GList      *current;

    properties = xfconf_channel_get_properties (channel, NULL);
    channel_contents = g_hash_table_get_keys (properties);

    /* get all profiles */
    current = g_list_first (channel_contents);
    while (current)
    {
        gchar **current_elements = g_strsplit (current->data, "/", -1);
        gchar *profile_name;

        /* Only process the profiles and skip all other xfconf properties */
        /* If xfconf ever supports just getting the first-level children of a property
           we could replace this */
        if (get_size (current_elements) != 2)
        {
            g_strfreev (current_elements);
            current = g_list_next (current);
            continue;
        }

        profile_name = g_strdup_printf ("%s", *(current_elements + 1));
        g_strfreev (current_elements);

        /* filter the content of the combobox to only matching profiles and exclude "Notify", "Default" and "Schemes" */
        if (!g_list_find_custom (profiles, profile_name, (GCompareFunc) strcmp) &&
            strcmp (profile_name, "Notify") &&
            strcmp (profile_name, "Default") &&
            strcmp (profile_name, "Schemes") &&
            display_settings_profile_matches (current->data, display_infos, channel))
        {
            profiles = g_list_prepend (profiles, g_strdup (profile_name));
        }
        /* else don't add the profile to the list */
        current = g_list_next (current);
        g_free (profile_name);
    }

    g_list_free (channel_contents);
    g_hash_table_destroy (properties);

    return profiles;
}

gboolean
display_settings_profile_matches (const gchar *profile,
                                  gchar **display_infos,
                                  XfconfChannel *channel)
{
    /* Walk through the profile and check if every EDID referenced there is also currently available */
    GHashTable *props = xfconf_channel_get_properties (channel, profile);
    GHashTableIter iter;
    gpointer key, value;
    guint n_infos = g_strv_length (display_infos);
    guint n_outputs = 0;
    gboolean all_match = FALSE;

    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        gchar **tokens = g_strsplit (key, "/", -1);
        guint n_tokens = g_strv_length (tokens);
        g_strfreev (tokens);
        if (n_tokens == 3)
        {
            gchar *property;
            gchar *edid;
            if (++n_outputs > n_infos)
                break;

            property = g_strdup_printf ("%s/EDID", (gchar*) key);
            edid = xfconf_channel_get_string (channel, property, NULL);
            all_match = g_strv_contains ((const gchar *const *) display_infos, edid);
            g_free (edid);
            g_free (property);
            if (!all_match)
                break;
        }
    }
    g_hash_table_destroy (props);

    return all_match && n_outputs == n_infos;
}
