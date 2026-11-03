/*
 *  Copyright (c) 2026 Grigory Tuboltsev <gtubolcev@gmail.com>
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

#include "backlight.h"

guint64
backlight_read_value (const gchar *device,
                      const gchar *file)
{
    gchar *path = g_build_filename (BACKLIGHT_SYSFS_DIR, device, file, NULL);
    gchar *contents = NULL;
    guint64 value = 0;

    if (g_file_get_contents (path, &contents, NULL, NULL))
        value = g_ascii_strtoull (contents, NULL, 10);

    g_free (path);
    g_free (contents);

    return value;
}

/* the name of the backlight device to use, or NULL if there is none */
gchar *
backlight_find_device (void)
{
    static const gchar *types[] = { "firmware", "platform", "raw" };
    GDir *dir = g_dir_open (BACKLIGHT_SYSFS_DIR, 0, NULL);
    const gchar *entry;
    gchar *best = NULL;
    guint best_rank = G_N_ELEMENTS (types);

    if (dir == NULL)
        return NULL;

    while ((entry = g_dir_read_name (dir)) != NULL)
    {
        gchar *path = g_build_filename (BACKLIGHT_SYSFS_DIR, entry, "type", NULL);
        gchar *type = NULL;
        guint rank = G_N_ELEMENTS (types);

        if (g_file_get_contents (path, &type, NULL, NULL))
        {
            g_strstrip (type);
            for (guint i = 0; i < G_N_ELEMENTS (types); i++)
                if (g_strcmp0 (type, types[i]) == 0)
                    rank = i;
        }

        if (rank < best_rank && backlight_read_value (entry, "max_brightness") > 0)
        {
            g_set_str (&best, entry);
            best_rank = rank;
        }

        g_free (path);
        g_free (type);
    }

    g_dir_close (dir);

    return best;
}

/* the brightness of @device in percent, rounded */
gint
backlight_get_percent (const gchar *device)
{
    guint64 max = backlight_read_value (device, "max_brightness");

    if (max == 0)
        return 0;

    return (backlight_read_value (device, "brightness") * 100 + max / 2) / max;
}
