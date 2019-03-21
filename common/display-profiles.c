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



GList*
display_settings_get_profiles (XfceRandr *xfce_randr, XfconfChannel *channel)
{
    GHashTable *properties;
    GList *channel_contents, *profiles = NULL, *current;
    guint                     m;
    gchar                    *edid, *output_info_name, **display_infos;

    properties = xfconf_channel_get_properties (channel, NULL);
    channel_contents = g_hash_table_get_keys (properties);
    display_infos = g_new0 (gchar *, xfce_randr->noutput);

    /* get all display connectors in combination with their respective edids */
    for (m = 0; m < xfce_randr->noutput; ++m)
    {
        edid = xfce_randr_get_edid (xfce_randr, m);
        output_info_name = xfce_randr_get_output_info_name (xfce_randr, m);
        display_infos[m] = g_strdup_printf ("%s/%s", output_info_name, edid);
    }

    /* get all profiles */
    current = g_list_first (channel_contents);
    while (current)
    {
        gchar* buf = strtok (current->data, "/");
        gboolean profile_match = TRUE;

        /* walk all connected displays and filter for edids matching the current profile */
        for (m = 0; m < xfce_randr->noutput; ++m)
        {
            gchar *property;
            gchar *current_edid, *output_edid;
            gchar **display_infos_tokens;

            display_infos_tokens = g_strsplit (display_infos[m], "/", 2);
            property = g_strdup_printf ("/%s/%s/EDID", buf, display_infos_tokens[0]);
            current_edid = xfconf_channel_get_string (channel, property, NULL);
            output_edid = g_strdup_printf ("%s/%s", display_infos_tokens[0], current_edid);
            if (current_edid)
            {
                if (g_strcmp0 (display_infos[m], output_edid) != 0)
                    profile_match = FALSE;
            }
            else
            {
                profile_match = FALSE;
            }
            g_free (property);
            g_free (current_edid);
            g_free (output_edid);
            g_strfreev (display_infos_tokens);
        }
        /* filter the content of the combobox to only matching profiles and exclude "Notify", "Default" and "Schemes" */
        if (!g_list_find_custom (profiles, (char*) buf, (GCompareFunc) strcmp) &&
            strcmp (buf, "Notify") &&
            strcmp (buf, "Default") &&
            strcmp (buf, "Schemes") &&
            profile_match)
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
