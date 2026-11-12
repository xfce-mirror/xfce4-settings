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

#include "theme-utils.h"

#include <xfconf/xfconf.h>

const gchar *const theme_schedule_properties[N_THEME_SCHEDULE_FIELDS] = {
    THEME_SCHEDULE_PROP_PREFIX "Times",
    THEME_SCHEDULE_PROP_PREFIX "Themes",
    THEME_SCHEDULE_PROP_PREFIX "ColorSchemes",
    THEME_SCHEDULE_PROP_PREFIX "Xfwm4",
    THEME_SCHEDULE_PROP_PREFIX "IconThemes",
    THEME_SCHEDULE_PROP_PREFIX "CursorThemes",
    THEME_SCHEDULE_PROP_PREFIX "Commands",
};

/* the directories with themes of @type, as the Style and Icons tabs search
 * them: those of xfce_resource_dirs() and DATADIR/@themes_directory, where
 * @themes_directory is "themes" or "icons"; free with g_strfreev() */
gchar **
theme_utils_get_theme_dirs (XfceResourceType type,
                            const gchar *themes_directory)
{
    gchar **theme_dirs, *path;

    path = g_build_filename (DATADIR, themes_directory, NULL);
    xfce_resource_push_path (type, path);
    theme_dirs = xfce_resource_dirs (type);
    xfce_resource_pop_path (type);
    g_free (path);

    return theme_dirs;
}



/* whether the theme @theme_name of @type is installed with @file_in_theme
 * inside its directory, e.g. "gtk-3.0" for a Gtk theme */
gboolean
theme_utils_theme_has_file (XfceResourceType type,
                            const gchar *themes_directory,
                            const gchar *theme_name,
                            const gchar *file_in_theme)
{
    gchar **theme_dirs = theme_utils_get_theme_dirs (type, themes_directory);
    gboolean found = FALSE;

    for (guint i = 0; theme_dirs[i] != NULL && !found; i++)
    {
        gchar *path = g_build_filename (theme_dirs[i], theme_name, file_in_theme, NULL);
        found = g_file_test (path, G_FILE_TEST_EXISTS);
        g_free (path);
    }

    g_strfreev (theme_dirs);

    return found;
}



/* sets the xfwm4 theme that matches the Gtk theme @theme_name: the one of
 * the same name if @theme_name has one, "Default" for Adwaita */
void
theme_utils_set_matching_xfwm4_theme (const gchar *theme_name)
{
    if (theme_utils_theme_has_file (XFCE_RESOURCE_THEMES, "themes", theme_name, "xfwm4" G_DIR_SEPARATOR_S "themerc"))
        xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", theme_name);
    else if (g_strcmp0 (theme_name, "Adwaita") == 0 || g_strcmp0 (theme_name, "Adwaita-dark") == 0)
        xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", "Default");
}



/* whether @theme_name is a dark theme: "-dark" at the end or in the middle, in any case */
gboolean
theme_utils_theme_is_dark (const gchar *theme_name)
{
    gchar *strdown = g_ascii_strdown (theme_name, -1);
    gboolean dark = g_strstr_len (strdown, -1, "-dark-") != NULL || g_str_has_suffix (strdown, "-dark");

    g_free (strdown);

    return dark;
}



static gint
theme_utils_compare_names (gconstpointer a,
                           gconstpointer b)
{
    return g_utf8_collate (*(const gchar **) a, *(const gchar **) b);
}



/* the names of the installed cursor themes, sorted; free with g_ptr_array_unref() */
GPtrArray *
theme_utils_list_cursor_themes (void)
{
    GPtrArray *themes = g_ptr_array_new_with_free_func (g_free);
    gchar **dirs;

    dirs = theme_utils_get_theme_dirs (XFCE_RESOURCE_ICONS, "icons");

    for (guint i = 0; dirs[i] != NULL; i++)
    {
        GDir *dir = g_dir_open (dirs[i], 0, NULL);
        const gchar *entry;

        if (dir == NULL)
            continue;

        while ((entry = g_dir_read_name (dir)) != NULL)
        {
            gchar *path = g_build_filename (dirs[i], entry, "cursors", NULL);

            if (g_file_test (path, G_FILE_TEST_IS_DIR)
                && !g_ptr_array_find_with_equal_func (themes, entry, g_str_equal, NULL))
                g_ptr_array_add (themes, g_strdup (entry));
            g_free (path);
        }

        g_dir_close (dir);
    }

    g_strfreev (dirs);
    g_ptr_array_sort (themes, theme_utils_compare_names);

    return themes;
}



/* the time of a schedule rule, "HH:MM", in minutes since midnight, or -1 if invalid */
gint
theme_utils_parse_schedule_time (const gchar *str)
{
    guint64 hours, minutes;
    gchar *end;

    if (str == NULL || !g_ascii_isdigit (*str))
        return -1;

    hours = g_ascii_strtoull (str, &end, 10);
    if (*end != ':' || !g_ascii_isdigit (end[1]))
        return -1;

    minutes = g_ascii_strtoull (end + 1, &end, 10);
    if (*end != '\0' || hours > 23 || minutes > 59)
        return -1;

    return hours * 60 + minutes;
}
