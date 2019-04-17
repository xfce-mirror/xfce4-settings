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

        if (get_size (current_elements) != 2)
        {
            g_strfreev (current_elements);
            current = g_list_next (current);
            continue;
        }

        if (g_strcmp0 (new_profile_name, xfconf_channel_get_string (channel, current->data, NULL)) == 0)
            return FALSE;

        current = g_list_next (current);
    }
    g_hash_table_destroy (properties);
    return TRUE;
}

GList*
display_settings_get_profiles (XfceRandr *xfce_randr, XfconfChannel *channel)
{
    GHashTable *properties;
    GList *channel_contents, *profiles = NULL, *current;
    guint                     m;
    gchar                   **display_infos;

    properties = xfconf_channel_get_properties (channel, NULL);
    channel_contents = g_hash_table_get_keys (properties);
    display_infos = g_new0 (gchar *, xfce_randr->noutput);

    /* get all display edids, to only query randr once */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        display_infos[m] = g_strdup_printf ("%s", xfce_randr_get_edid (xfce_randr, m));
    }

    /* get all profiles */
    current = g_list_first (channel_contents);
    while (current)
    {
        GHashTable     *props;
        gchar          *property_profile;
        GHashTableIter  iter;
        gpointer        key, value;
        guint           profile_match = 0;
        guint           monitors = 0;
        gchar          *buf;
        gchar         **current_elements = g_strsplit (current->data, "/", -1);

        /* Only process the profiles and skip all other xfconf properties */
        /* If xfconf ever supports just getting the first-level children of a property
           we could replace this */
        if (get_size (current_elements) != 2)
        {
            g_strfreev (current_elements);
            current = g_list_next (current);
            continue;
        }
        g_strfreev (current_elements);
        buf = strtok (current->data, "/");

        /* Walk through the profile and check if every EDID referenced there is also currently available */
        property_profile = g_strdup_printf ("/%s", buf);
        props = xfconf_channel_get_properties (channel, property_profile);
        g_hash_table_iter_init (&iter, props);

        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            gchar *property;
            gchar *current_edid, *output_edid;

            gchar ** property_elements = g_strsplit (key, "/", -1);
            if (get_size (property_elements) == 3) {
                monitors++;

                property = g_strdup_printf ("%s/EDID", (gchar*)key);
                current_edid = xfconf_channel_get_string (channel, property, NULL);
                output_edid = g_strdup_printf ("%s", current_edid);

                if (current_edid) {

                    for (m = 0; m < xfce_randr->noutput; ++m)
                    {
                        if (g_strcmp0 (display_infos[m], output_edid) == 0)
                        {
                            profile_match ++;
                        }
                    }
                }
                g_free (property);
                g_free (current_edid);
                g_free (output_edid);
            }

            g_strfreev (property_elements);
        }
        g_free (property_profile);
        g_hash_table_destroy (props);

        /* filter the content of the combobox to only matching profiles and exclude "Notify", "Default" and "Schemes" */
        if (!g_list_find_custom (profiles, (char*) buf, (GCompareFunc) strcmp) &&
            strcmp (buf, "Notify") &&
            strcmp (buf, "Default") &&
            strcmp (buf, "Schemes") &&
            profile_match == monitors &&
            xfce_randr->noutput == profile_match)
        {
            profiles = g_list_prepend (profiles, buf);
        }
        /* else don't add the profile to the list */
        current = g_list_next (current);
    }

    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        g_free (display_infos[m]);
    }
    g_free (display_infos);
    g_list_free (channel_contents);

    return profiles;
}
