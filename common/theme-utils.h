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

/*
 * Shared by the appearance dialog and xfsettingsd: where the themes are
 * installed, the matching xfwm4 theme, whether a theme is a dark one and
 * the installed cursor themes.
 */

#ifndef __THEME_UTILS_H__
#define __THEME_UTILS_H__

#include <libxfce4util/libxfce4util.h>

G_BEGIN_DECLS

gchar **
theme_utils_get_theme_dirs (XfceResourceType type,
                            const gchar *themes_directory);

gboolean
theme_utils_theme_has_file (XfceResourceType type,
                            const gchar *themes_directory,
                            const gchar *theme_name,
                            const gchar *file_in_theme);

void
theme_utils_set_matching_xfwm4_theme (const gchar *theme_name);

gboolean
theme_utils_theme_is_dark (const gchar *theme_name);

GPtrArray *
theme_utils_list_cursor_themes (void);

/* The theme schedule of the Style tab, applied by xfsettingsd. The rules are
 * stored in the xsettings channel as one string array per field, see
 * xfsettingsd/theme-schedule.c */
#define THEME_SCHEDULE_PROP_PREFIX "/Xfce/ThemeSchedule/"
#define THEME_SCHEDULE_ENABLED_PROP THEME_SCHEDULE_PROP_PREFIX "Enabled"
#define THEME_SCHEDULE_CHECKED_PROP THEME_SCHEDULE_PROP_PREFIX "CheckedUntil"

typedef enum
{
    THEME_SCHEDULE_TIME,
    THEME_SCHEDULE_THEME,
    THEME_SCHEDULE_COLOR_SCHEME,
    THEME_SCHEDULE_XFWM4,
    THEME_SCHEDULE_ICON_THEME,
    THEME_SCHEDULE_CURSOR_THEME,
    THEME_SCHEDULE_COMMAND,
    N_THEME_SCHEDULE_FIELDS
} ThemeScheduleField;

extern const gchar *const theme_schedule_properties[N_THEME_SCHEDULE_FIELDS];

gint
theme_utils_parse_schedule_time (const gchar *str);

G_END_DECLS

#endif /* !__THEME_UTILS_H__ */
