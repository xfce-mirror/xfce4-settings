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
 * Switches the Gtk theme, the preferred color scheme and more at fixed times
 * of the day. The rules live in the xsettings channel as parallel string
 * arrays, where a missing entry counts as "":
 *
 *   /Xfce/ThemeSchedule/Enabled       bool
 *   /Xfce/ThemeSchedule/Times         "HH:MM"
 *   /Xfce/ThemeSchedule/Themes        theme name, ":dark" or ":light" for that
 *                                     variant of the current theme, "" to keep it
 *   /Xfce/ThemeSchedule/ColorSchemes  "prefer-dark", "prefer-light", "default",
 *                                     "" to keep it; if missing, it follows the
 *                                     theme name like in the appearance dialog
 *   /Xfce/ThemeSchedule/Xfwm4         "true" or "false" to set the matching Xfwm4
 *                                     theme along with the Gtk theme, "" to
 *                                     follow /Xfce/SyncThemes
 *   /Xfce/ThemeSchedule/IconThemes    like Themes, for the icon theme
 *   /Xfce/ThemeSchedule/CursorThemes  cursor theme name, "" to keep it
 *   /Xfce/ThemeSchedule/Commands      shell command run after switching, with
 *                                     the new values in XFCE_SCHEDULE_GTK_THEME,
 *                                     XFCE_SCHEDULE_ICON_THEME and
 *                                     XFCE_SCHEDULE_COLOR_SCHEME
 *
 * A rule fires when the clock passes its time, so a theme picked by hand stays
 * until the next switch time and editing the rules switches nothing. The moment
 * up to which the rules were checked is kept in
 *
 *   /Xfce/ThemeSchedule/CheckedUntil  Unix time
 *
 * so that a switch missed while logged out is caught up at login.
 */

#include "theme-schedule.h"

#include "common/debug.h"
#include "common/theme-variant.h"

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#define SCHEDULE_ENABLED_PROP "/Xfce/ThemeSchedule/Enabled"
#define SCHEDULE_CHECKED_PROP "/Xfce/ThemeSchedule/CheckedUntil"
#define SCHEDULE_PROP_PREFIX "/Xfce/ThemeSchedule/"

/* the parallel arrays of the rules */
enum
{
    RULE_TIME,
    RULE_THEME,
    RULE_COLOR_SCHEME,
    RULE_XFWM4,
    RULE_ICON_THEME,
    RULE_CURSOR_THEME,
    RULE_COMMAND,
    N_RULE_FIELDS
};

static const gchar *rule_props[N_RULE_FIELDS] = {
    SCHEDULE_PROP_PREFIX "Times",
    SCHEDULE_PROP_PREFIX "Themes",
    SCHEDULE_PROP_PREFIX "ColorSchemes",
    SCHEDULE_PROP_PREFIX "Xfwm4",
    SCHEDULE_PROP_PREFIX "IconThemes",
    SCHEDULE_PROP_PREFIX "CursorThemes",
    SCHEDULE_PROP_PREFIX "Commands",
};

/* where to look up a theme of some kind */
typedef struct
{
    XfceResourceType type;
    const gchar *datadir_path; /* also searched, below DATADIR */
    const gchar *required_file; /* inside the theme's directory */
} ThemeKind;

static const ThemeKind gtk_theme_kind = { XFCE_RESOURCE_THEMES, "themes", "gtk-3.0" };
static const ThemeKind icon_theme_kind = { XFCE_RESOURCE_ICONS, "icons", "index.theme" };



static void
xfce_theme_schedule_helper_finalize (GObject *object);
static void
xfce_theme_schedule_helper_channel_property_changed (XfconfChannel *channel,
                                                     const gchar *property,
                                                     const GValue *value,
                                                     XfceThemeScheduleHelper *helper);
static void
xfce_theme_schedule_helper_update (XfceThemeScheduleHelper *helper,
                                   gboolean rules_changed);



struct _XfceThemeScheduleHelper
{
    GObject parent;

    XfconfChannel *channel;
    GSettings *desktop_interface_gsettings;

    guint timer_id;
};



G_DEFINE_FINAL_TYPE (XfceThemeScheduleHelper, xfce_theme_schedule_helper, G_TYPE_OBJECT)



static void
xfce_theme_schedule_helper_class_init (XfceThemeScheduleHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = xfce_theme_schedule_helper_finalize;
}



static void
xfce_theme_schedule_helper_init (XfceThemeScheduleHelper *helper)
{
    GSettingsSchemaSource *source;

    /* to keep the color-scheme in sync for GTK4 and the settings portal */
    source = g_settings_schema_source_get_default ();
    if (source != NULL)
    {
        GSettingsSchema *schema = g_settings_schema_source_lookup (source, "org.gnome.desktop.interface", TRUE);
        if (schema != NULL)
        {
            if (g_settings_schema_has_key (schema, "color-scheme"))
                helper->desktop_interface_gsettings = g_settings_new ("org.gnome.desktop.interface");
            g_settings_schema_unref (schema);
        }
    }

    helper->channel = xfconf_channel_get ("xsettings");
    g_signal_connect (helper->channel, "property-changed",
                      G_CALLBACK (xfce_theme_schedule_helper_channel_property_changed), helper);

    xfce_theme_schedule_helper_update (helper, FALSE);
}



static void
xfce_theme_schedule_helper_finalize (GObject *object)
{
    XfceThemeScheduleHelper *helper = XFCE_THEME_SCHEDULE_HELPER (object);

    g_signal_handlers_disconnect_by_func (helper->channel,
                                          xfce_theme_schedule_helper_channel_property_changed, helper);

    if (helper->timer_id != 0)
        g_source_remove (helper->timer_id);

    if (helper->desktop_interface_gsettings != NULL)
        g_object_unref (helper->desktop_interface_gsettings);

    G_OBJECT_CLASS (xfce_theme_schedule_helper_parent_class)->finalize (object);
}



/* "HH:MM" -> minutes since midnight, or -1 if invalid */
static gint
xfce_theme_schedule_parse_time (const gchar *str)
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



static gchar **
xfce_theme_schedule_theme_dirs (const ThemeKind *kind)
{
    gchar **theme_dirs, *path;

    /* same lookup as the appearance dialog */
    path = g_build_filename (DATADIR, kind->datadir_path, NULL);
    xfce_resource_push_path (kind->type, path);
    theme_dirs = xfce_resource_dirs (kind->type);
    xfce_resource_pop_path (kind->type);
    g_free (path);

    return theme_dirs;
}



/* whether @theme_name (with @relative_path inside it, if non-NULL) is installed */
static gboolean
xfce_theme_schedule_find_theme_file (const gchar *theme_name,
                                     const gchar *relative_path)
{
    gchar **theme_dirs = xfce_theme_schedule_theme_dirs (&gtk_theme_kind);
    gboolean found = FALSE;

    for (guint i = 0; theme_dirs[i] != NULL && !found; i++)
    {
        gchar *path = g_build_filename (theme_dirs[i], theme_name, relative_path, NULL);
        found = g_file_test (path, G_FILE_TEST_EXISTS);
        g_free (path);
    }

    g_strfreev (theme_dirs);

    return found;
}



/* the installed theme of the ThemeKind @user_data whose name matches @name
 * case-insensitively, or NULL */
static gchar *
xfce_theme_schedule_lookup_theme (const gchar *name,
                                  gpointer user_data)
{
    const ThemeKind *kind = user_data;
    gchar **theme_dirs = xfce_theme_schedule_theme_dirs (kind);
    gchar *found = NULL;

    for (guint i = 0; theme_dirs[i] != NULL && found == NULL; i++)
    {
        GDir *dir = g_dir_open (theme_dirs[i], 0, NULL);
        const gchar *entry;

        if (dir == NULL)
            continue;

        while (found == NULL && (entry = g_dir_read_name (dir)) != NULL)
        {
            gchar *path;

            if (g_ascii_strcasecmp (entry, name) != 0)
                continue;

            path = g_build_filename (theme_dirs[i], entry, kind->required_file, NULL);
            if (g_file_test (path, G_FILE_TEST_EXISTS))
                found = g_strdup (entry);
            g_free (path);
        }

        g_dir_close (dir);
    }

    g_strfreev (theme_dirs);

    return found;
}



/* the theme a rule's @value (name, ":dark" or ":light") resolves to with
 * @current in use, or NULL to keep @current */
static gchar *
xfce_theme_schedule_resolve_theme (const gchar *value,
                                   const gchar *current,
                                   const ThemeKind *kind)
{
    gchar *theme_name = NULL;

    if (xfce_str_is_empty (value))
        return NULL;

    if (g_strcmp0 (value, ":dark") == 0 || g_strcmp0 (value, ":light") == 0)
    {
        theme_name = theme_variant_find (current, value[1] == 'd', xfce_theme_schedule_lookup_theme, (gpointer) kind);
        if (theme_name == NULL)
            xfsettings_dbg (XFSD_DEBUG_THEME_SCHEDULE, "Theme '%s' has no %s variant, keeping it", current, value + 1);
    }
    else
    {
        theme_name = xfce_theme_schedule_lookup_theme (value, (gpointer) kind);
        if (theme_name == NULL)
            g_warning ("Scheduled theme '%s' is not installed", value);
    }

    return theme_name;
}



static void
xfce_theme_schedule_helper_set_xfwm4_theme (XfceThemeScheduleHelper *helper,
                                            const gchar *theme_name)
{
    /* same rules as the "Set matching Xfwm4 theme" switch of the appearance dialog */
    if (xfce_theme_schedule_find_theme_file (theme_name, "xfwm4" G_DIR_SEPARATOR_S "themerc"))
        xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", theme_name);
    else if (g_strcmp0 (theme_name, "Adwaita") == 0 || g_strcmp0 (theme_name, "Adwaita-dark") == 0)
        xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", "Default");
}



static void
xfce_theme_schedule_helper_run_command (XfceThemeScheduleHelper *helper,
                                        const gchar *command)
{
    gchar *argv[] = { "/bin/sh", "-c", (gchar *) command, NULL };
    gchar **envp, *value;
    GError *error = NULL;

    envp = g_get_environ ();

    value = xfconf_channel_get_string (helper->channel, "/Net/ThemeName", "");
    envp = g_environ_setenv (envp, "XFCE_SCHEDULE_GTK_THEME", value, TRUE);
    g_free (value);

    value = xfconf_channel_get_string (helper->channel, "/Net/IconThemeName", "");
    envp = g_environ_setenv (envp, "XFCE_SCHEDULE_ICON_THEME", value, TRUE);
    g_free (value);

    value = helper->desktop_interface_gsettings != NULL
                ? g_settings_get_string (helper->desktop_interface_gsettings, "color-scheme")
                : g_strdup ("");
    envp = g_environ_setenv (envp, "XFCE_SCHEDULE_COLOR_SCHEME", value, TRUE);
    g_free (value);

    if (!g_spawn_async (NULL, argv, envp, G_SPAWN_DEFAULT, NULL, NULL, NULL, &error))
    {
        g_warning ("Failed to run the scheduled command '%s': %s", command, error->message);
        g_error_free (error);
    }

    g_strfreev (envp);
}



/* @rule holds the fields of a rule, NULL where missing */
static void
xfce_theme_schedule_helper_apply (XfceThemeScheduleHelper *helper,
                                  const gchar **rule)
{
    gchar *current, *theme_name, *icon_theme_name;
    gboolean xfwm4;

    /* Gtk theme */
    current = xfconf_channel_get_string (helper->channel, "/Net/ThemeName", "Default");
    theme_name = xfce_theme_schedule_resolve_theme (rule[RULE_THEME], current, &gtk_theme_kind);
    if (theme_name != NULL && g_strcmp0 (theme_name, current) != 0)
        xfconf_channel_set_string (helper->channel, "/Net/ThemeName", theme_name);
    g_free (current);

    /* Xfwm4 theme */
    if (xfce_str_is_empty (rule[RULE_XFWM4]))
        xfwm4 = xfconf_channel_get_bool (helper->channel, "/Xfce/SyncThemes", FALSE);
    else
        xfwm4 = g_strcmp0 (rule[RULE_XFWM4], "true") == 0;
    if (xfwm4 && theme_name != NULL)
        xfce_theme_schedule_helper_set_xfwm4_theme (helper, theme_name);

    /* the preferred color scheme for GTK4, libadwaita and the settings portal */
    if (helper->desktop_interface_gsettings != NULL)
    {
        const gchar *color_scheme = rule[RULE_COLOR_SCHEME];

        if (color_scheme == NULL)
        {
            /* rules saved before color schemes were configurable */
            if (theme_name != NULL && theme_variant_is_dark (theme_name))
                g_settings_set_string (helper->desktop_interface_gsettings, "color-scheme", "prefer-dark");
            else if (theme_name != NULL)
                g_settings_reset (helper->desktop_interface_gsettings, "color-scheme");
        }
        else if (g_strcmp0 (color_scheme, "default") == 0)
            g_settings_reset (helper->desktop_interface_gsettings, "color-scheme");
        else if (!xfce_str_is_empty (color_scheme))
            g_settings_set_string (helper->desktop_interface_gsettings, "color-scheme", color_scheme);
    }

    /* icon theme */
    current = xfconf_channel_get_string (helper->channel, "/Net/IconThemeName", "Rodent");
    icon_theme_name = xfce_theme_schedule_resolve_theme (rule[RULE_ICON_THEME], current, &icon_theme_kind);
    if (icon_theme_name != NULL && g_strcmp0 (icon_theme_name, current) != 0)
        xfconf_channel_set_string (helper->channel, "/Net/IconThemeName", icon_theme_name);
    g_free (current);

    /* cursor theme */
    if (!xfce_str_is_empty (rule[RULE_CURSOR_THEME]))
        xfconf_channel_set_string (helper->channel, "/Gtk/CursorThemeName", rule[RULE_CURSOR_THEME]);

    if (!xfce_str_is_empty (rule[RULE_COMMAND]))
        xfce_theme_schedule_helper_run_command (helper, rule[RULE_COMMAND]);

    g_free (theme_name);
    g_free (icon_theme_name);
}



static gboolean
xfce_theme_schedule_helper_timeout (gpointer user_data)
{
    XfceThemeScheduleHelper *helper = user_data;

    helper->timer_id = 0;
    xfce_theme_schedule_helper_update (helper, FALSE);

    return G_SOURCE_REMOVE;
}



/* Unix time of the last occurrence of @minutes (since midnight) not after @now */
static gint64
xfce_theme_schedule_last_occurrence (GDateTime *now,
                                     gint minutes)
{
    GDateTime *date, *at;
    gint64 result;

    date = g_date_time_ref (now);
    for (gint days = 0; days < 2; days++)
    {
        at = g_date_time_new_local (g_date_time_get_year (date), g_date_time_get_month (date),
                                    g_date_time_get_day_of_month (date), minutes / 60, minutes % 60, 0);
        if (at != NULL && g_date_time_compare (at, now) <= 0)
        {
            result = g_date_time_to_unix (at);
            g_date_time_unref (at);
            g_date_time_unref (date);
            return result;
        }

        if (at != NULL)
            g_date_time_unref (at);

        at = g_date_time_add_days (date, -1);
        g_date_time_unref (date);
        date = at;
    }

    g_date_time_unref (date);

    return 0;
}



static void
xfce_theme_schedule_helper_update (XfceThemeScheduleHelper *helper,
                                   gboolean rules_changed)
{
    gchar **fields[N_RULE_FIELDS];
    guint n_fields[N_RULE_FIELDS];
    const gchar *rule[N_RULE_FIELDS];
    GDateTime *now;
    gint64 now_unix, checked, best_unix;
    gint best = -1;

    g_clear_handle_id (&helper->timer_id, g_source_remove);

    if (!xfconf_channel_get_bool (helper->channel, SCHEDULE_ENABLED_PROP, FALSE))
        return;

    now = g_date_time_new_now_local ();
    now_unix = g_date_time_to_unix (now);
    checked = xfconf_channel_get_uint64 (helper->channel, SCHEDULE_CHECKED_PROP, 0);

    /* edited rules only count from now on, and so does a clock set back */
    if (rules_changed || checked == 0 || checked > now_unix)
    {
        checked = now_unix;
        xfconf_channel_set_uint64 (helper->channel, SCHEDULE_CHECKED_PROP, checked);
    }

    for (guint f = 0; f < N_RULE_FIELDS; f++)
    {
        fields[f] = xfconf_channel_get_string_list (helper->channel, rule_props[f]);
        n_fields[f] = fields[f] != NULL ? g_strv_length (fields[f]) : 0;
    }

    /* the latest rule time passed since the last check */
    best_unix = checked;
    for (guint i = 0; i < n_fields[RULE_TIME]; i++)
    {
        gint minutes = xfce_theme_schedule_parse_time (fields[RULE_TIME][i]);
        gboolean does_something = FALSE;
        gint64 at;

        for (guint f = RULE_THEME; f < N_RULE_FIELDS; f++)
            if (f != RULE_XFWM4 && i < n_fields[f] && !xfce_str_is_empty (fields[f][i]))
                does_something = TRUE;

        if (minutes < 0 || !does_something)
            continue;

        at = xfce_theme_schedule_last_occurrence (now, minutes);
        if (at > best_unix)
        {
            best = i;
            best_unix = at;
        }
    }

    if (best >= 0)
    {
        for (guint f = 0; f < N_RULE_FIELDS; f++)
            rule[f] = (guint) best < n_fields[f] ? fields[f][best] : NULL;

        xfce_theme_schedule_helper_apply (helper, rule);
        xfconf_channel_set_uint64 (helper->channel, SCHEDULE_CHECKED_PROP, now_unix);
    }

    /* re-check at the start of the next minute; this also copes with clock
     * changes, DST and suspend without tracking them explicitly */
    helper->timer_id = g_timeout_add_seconds (60 - g_date_time_get_second (now),
                                              xfce_theme_schedule_helper_timeout, helper);

    g_date_time_unref (now);
    for (guint f = 0; f < N_RULE_FIELDS; f++)
        g_strfreev (fields[f]);
}



static void
xfce_theme_schedule_helper_channel_property_changed (XfconfChannel *channel,
                                                     const gchar *property,
                                                     const GValue *value,
                                                     XfceThemeScheduleHelper *helper)
{
    if (g_str_has_prefix (property, SCHEDULE_PROP_PREFIX)
        && g_strcmp0 (property, SCHEDULE_CHECKED_PROP) != 0)
        xfce_theme_schedule_helper_update (helper, TRUE);
}
