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
 * of the day or at sunrise and sunset. The rules live in the xsettings
 * channel as parallel string arrays, where a missing entry counts as "":
 *
 *   /Xfce/ThemeSchedule/Enabled       bool
 *   /Xfce/ThemeSchedule/Times         "HH:MM", or "sunrise" or "sunset" with
 *                                     an optional offset in minutes, e.g.
 *                                     "sunset-30"
 *   /Xfce/ThemeSchedule/Themes        Gtk theme name, "" to keep it
 *   /Xfce/ThemeSchedule/ColorSchemes  "prefer-dark", "prefer-light", "default",
 *                                     "" to keep it; if missing, it follows the
 *                                     theme name like in the appearance dialog
 *   /Xfce/ThemeSchedule/Xfwm4         "true" or "false" to set the matching Xfwm4
 *                                     theme along with the Gtk theme, "" to
 *                                     follow /Xfce/SyncThemes
 *   /Xfce/ThemeSchedule/IconThemes    icon theme name, "" to keep it
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
 * so that the switches missed while logged out are caught up at login, as
 * one switch to what the missed rules set in the order of their times.
 */

#include "theme-schedule.h"

#include "common/debug.h"
#include "common/theme-utils.h"

#include <gio/gio.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

/* the dialog writes the rules as several properties, so wait for the last */
#define RULES_CHANGED_DELAY_MS 250



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

    /* org.gnome.desktop.interface and org.x.apps.portal, as far as installed
     * with a color-scheme key, like in the appearance dialog */
    GSettings *color_scheme_gsettings[2];

    guint timer_id;
    guint rules_changed_id;
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
    const gchar *schema_ids[] = { "org.gnome.desktop.interface", "org.x.apps.portal" };
    GSettingsSchemaSource *source;

    G_STATIC_ASSERT (G_N_ELEMENTS (schema_ids) == G_N_ELEMENTS (helper->color_scheme_gsettings));

    /* to keep the color-scheme in sync for GTK4 and the settings portals */
    source = g_settings_schema_source_get_default ();
    for (guint i = 0; source != NULL && i < G_N_ELEMENTS (schema_ids); i++)
    {
        GSettingsSchema *schema = g_settings_schema_source_lookup (source, schema_ids[i], TRUE);
        if (schema != NULL)
        {
            if (g_settings_schema_has_key (schema, "color-scheme"))
                helper->color_scheme_gsettings[i] = g_settings_new (schema_ids[i]);
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

    g_clear_handle_id (&helper->timer_id, g_source_remove);
    g_clear_handle_id (&helper->rules_changed_id, g_source_remove);

    for (guint i = 0; i < G_N_ELEMENTS (helper->color_scheme_gsettings); i++)
        g_clear_object (&helper->color_scheme_gsettings[i]);

    G_OBJECT_CLASS (xfce_theme_schedule_helper_parent_class)->finalize (object);
}



/* sets the preferred color scheme, or resets it if @color_scheme is NULL */
static void
xfce_theme_schedule_helper_set_color_scheme (XfceThemeScheduleHelper *helper,
                                             const gchar *color_scheme)
{
    for (guint i = 0; i < G_N_ELEMENTS (helper->color_scheme_gsettings); i++)
    {
        if (helper->color_scheme_gsettings[i] == NULL)
            continue;
        if (color_scheme != NULL)
            g_settings_set_string (helper->color_scheme_gsettings[i], "color-scheme", color_scheme);
        else
            g_settings_reset (helper->color_scheme_gsettings[i], "color-scheme");
    }
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

    value = helper->color_scheme_gsettings[0] != NULL
                ? g_settings_get_string (helper->color_scheme_gsettings[0], "color-scheme")
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
    const gchar *theme_name = NULL;
    const gchar *color_scheme;
    gboolean sync_xfwm4_theme;

    /* Gtk theme */
    if (!xfce_str_is_empty (rule[THEME_SCHEDULE_THEME]))
    {
        if (theme_utils_theme_has_file (XFCE_RESOURCE_THEMES, "themes", rule[THEME_SCHEDULE_THEME], "gtk-3.0"))
        {
            theme_name = rule[THEME_SCHEDULE_THEME];
            xfconf_channel_set_string (helper->channel, "/Net/ThemeName", theme_name);
        }
        else
            g_warning ("Scheduled theme '%s' is not installed", rule[THEME_SCHEDULE_THEME]);
    }

    /* Xfwm4 theme */
    if (xfce_str_is_empty (rule[THEME_SCHEDULE_XFWM4]))
        sync_xfwm4_theme = xfconf_channel_get_bool (helper->channel, "/Xfce/SyncThemes", FALSE);
    else
        sync_xfwm4_theme = g_strcmp0 (rule[THEME_SCHEDULE_XFWM4], "true") == 0;
    if (sync_xfwm4_theme && theme_name != NULL)
        theme_utils_set_matching_xfwm4_theme (theme_name);

    /* the preferred color scheme for GTK4, libadwaita and the settings portals */
    color_scheme = rule[THEME_SCHEDULE_COLOR_SCHEME];
    if (color_scheme == NULL)
    {
        /* rules saved before color schemes were configurable */
        if (theme_name != NULL)
            xfce_theme_schedule_helper_set_color_scheme (helper, theme_utils_theme_is_dark (theme_name) ? "prefer-dark" : NULL);
    }
    else if (g_strcmp0 (color_scheme, "default") == 0)
        xfce_theme_schedule_helper_set_color_scheme (helper, NULL);
    else if (!xfce_str_is_empty (color_scheme))
        xfce_theme_schedule_helper_set_color_scheme (helper, color_scheme);

    /* icon theme */
    if (!xfce_str_is_empty (rule[THEME_SCHEDULE_ICON_THEME]))
    {
        if (theme_utils_theme_has_file (XFCE_RESOURCE_ICONS, "icons", rule[THEME_SCHEDULE_ICON_THEME], "index.theme"))
            xfconf_channel_set_string (helper->channel, "/Net/IconThemeName", rule[THEME_SCHEDULE_ICON_THEME]);
        else
            g_warning ("Scheduled icon theme '%s' is not installed", rule[THEME_SCHEDULE_ICON_THEME]);
    }

    /* cursor theme */
    if (!xfce_str_is_empty (rule[THEME_SCHEDULE_CURSOR_THEME]))
        xfconf_channel_set_string (helper->channel, "/Gtk/CursorThemeName", rule[THEME_SCHEDULE_CURSOR_THEME]);

    if (!xfce_str_is_empty (rule[THEME_SCHEDULE_COMMAND]))
        xfce_theme_schedule_helper_run_command (helper, rule[THEME_SCHEDULE_COMMAND]);
}



static void
xfce_theme_schedule_helper_timeout (gpointer user_data)
{
    XfceThemeScheduleHelper *helper = user_data;

    helper->timer_id = 0;
    xfce_theme_schedule_helper_update (helper, FALSE);
}



/*
 * The last time the clock showed the rule time @time_str up to @now, as
 * Unix time: today at that time if it is not later than @now, else
 * yesterday at that time. E.g. for a rule at 21:00, at 22:30 it is 21:00
 * today and at 08:00 it is 21:00 yesterday. A rule at sunrise or sunset
 * has the time of that day. Returns 0 if the rule has no such time on
 * either day: the time falls into a DST gap, the sun does not rise or set
 * that day, or the time zone has no location.
 */
static gint64
xfce_theme_schedule_last_occurrence (GDateTime *now,
                                     const gchar *time_str)
{
    GDateTime *date, *at;
    gint64 result;

    date = g_date_time_ref (now);
    for (gint days = 0; days < 2; days++)
    {
        gint minutes_since_midnight = theme_utils_get_schedule_time (time_str, date);

        at = NULL;
        if (minutes_since_midnight >= 0)
            at = g_date_time_new_local (g_date_time_get_year (date), g_date_time_get_month (date),
                                        g_date_time_get_day_of_month (date),
                                        minutes_since_midnight / 60, minutes_since_midnight % 60, 0);
        else
            xfsettings_dbg (XFSD_DEBUG_THEME_SCHEDULE, "Rule at \"%s\" has no time on %d-%02d-%02d",
                            time_str, g_date_time_get_year (date), g_date_time_get_month (date),
                            g_date_time_get_day_of_month (date));
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



typedef struct
{
    gint64 at;
    guint index;
} PassedRule;

static gint
xfce_theme_schedule_compare_passed_rules (gconstpointer a,
                                          gconstpointer b)
{
    const PassedRule *rule_a = a, *rule_b = b;

    return rule_a->at < rule_b->at ? -1 : rule_a->at > rule_b->at;
}



/*
 * Merges @rule into @merged as if it was applied after it: what @rule sets
 * replaces the value in @merged, and the Xfwm4 setting and a missing color
 * scheme go with the Gtk theme.
 */
static void
xfce_theme_schedule_merge_rule (const gchar **merged,
                                const gchar **rule)
{
    if (!xfce_str_is_empty (rule[THEME_SCHEDULE_THEME]))
    {
        merged[THEME_SCHEDULE_THEME] = rule[THEME_SCHEDULE_THEME];
        merged[THEME_SCHEDULE_XFWM4] = rule[THEME_SCHEDULE_XFWM4];
        if (rule[THEME_SCHEDULE_COLOR_SCHEME] == NULL)
            merged[THEME_SCHEDULE_COLOR_SCHEME] = NULL;
    }

    for (guint f = THEME_SCHEDULE_COLOR_SCHEME; f < N_THEME_SCHEDULE_FIELDS; f++)
        if (f != THEME_SCHEDULE_XFWM4 && !xfce_str_is_empty (rule[f]))
            merged[f] = rule[f];
}



static void
xfce_theme_schedule_helper_update (XfceThemeScheduleHelper *helper,
                                   gboolean rules_changed)
{
    gchar **fields[N_THEME_SCHEDULE_FIELDS];
    guint n_fields[N_THEME_SCHEDULE_FIELDS];
    const gchar *rule[N_THEME_SCHEDULE_FIELDS];
    const gchar *merged[N_THEME_SCHEDULE_FIELDS] = { NULL };
    g_autoptr (GArray) passed = NULL;
    GDateTime *now;
    gint64 now_unix, checked;

    g_clear_handle_id (&helper->timer_id, g_source_remove);

    if (!xfconf_channel_get_bool (helper->channel, THEME_SCHEDULE_ENABLED_PROP, FALSE))
        return;

    now = g_date_time_new_now_local ();
    now_unix = g_date_time_to_unix (now);
    checked = xfconf_channel_get_uint64 (helper->channel, THEME_SCHEDULE_CHECKED_PROP, 0);

    /* edited rules only count from now on, and so does a clock set back */
    if (rules_changed || checked == 0 || checked > now_unix)
    {
        checked = now_unix;
        xfconf_channel_set_uint64 (helper->channel, THEME_SCHEDULE_CHECKED_PROP, checked);
    }

    for (guint f = 0; f < N_THEME_SCHEDULE_FIELDS; f++)
    {
        fields[f] = xfconf_channel_get_string_list (helper->channel, theme_schedule_properties[f]);
        n_fields[f] = fields[f] != NULL ? g_strv_length (fields[f]) : 0;
    }

    /* the rules whose time passed since the last check, e.g. several while
     * logged out, are merged in the order of their times and applied once,
     * so that the theme does not flicker and only the last command runs */
    passed = g_array_new (FALSE, FALSE, sizeof (PassedRule));
    for (guint i = 0; i < n_fields[THEME_SCHEDULE_TIME]; i++)
    {
        gboolean does_something = FALSE;
        gint64 at;

        for (guint f = THEME_SCHEDULE_THEME; f < N_THEME_SCHEDULE_FIELDS; f++)
            if (f != THEME_SCHEDULE_XFWM4 && i < n_fields[f] && !xfce_str_is_empty (fields[f][i]))
                does_something = TRUE;

        if (!does_something)
            continue;

        at = xfce_theme_schedule_last_occurrence (now, fields[THEME_SCHEDULE_TIME][i]);
        if (at > checked)
        {
            PassedRule passed_rule = { at, i };
            g_array_append_val (passed, passed_rule);
        }
    }

    if (passed->len > 0)
    {
        /* a color scheme of "" keeps the current one */
        merged[THEME_SCHEDULE_COLOR_SCHEME] = "";

        g_array_sort (passed, xfce_theme_schedule_compare_passed_rules);
        for (guint p = 0; p < passed->len; p++)
        {
            guint index = g_array_index (passed, PassedRule, p).index;

            for (guint f = 0; f < N_THEME_SCHEDULE_FIELDS; f++)
                rule[f] = index < n_fields[f] ? fields[f][index] : NULL;
            xfce_theme_schedule_merge_rule (merged, rule);
        }

        xfce_theme_schedule_helper_apply (helper, merged);
        xfconf_channel_set_uint64 (helper->channel, THEME_SCHEDULE_CHECKED_PROP, now_unix);
    }

    /* re-check at the start of the next minute; this also copes with clock
     * changes, DST and suspend without tracking them explicitly */
    helper->timer_id = g_timeout_add_seconds_once (60 - g_date_time_get_second (now),
                                                   xfce_theme_schedule_helper_timeout, helper);

    g_date_time_unref (now);
    for (guint f = 0; f < N_THEME_SCHEDULE_FIELDS; f++)
        g_strfreev (fields[f]);
}



static void
xfce_theme_schedule_helper_rules_changed (gpointer user_data)
{
    XfceThemeScheduleHelper *helper = user_data;

    helper->rules_changed_id = 0;
    xfce_theme_schedule_helper_update (helper, TRUE);
}



static void
xfce_theme_schedule_helper_channel_property_changed (XfconfChannel *channel,
                                                     const gchar *property,
                                                     const GValue *value,
                                                     XfceThemeScheduleHelper *helper)
{
    if (g_str_has_prefix (property, THEME_SCHEDULE_PROP_PREFIX)
        && g_strcmp0 (property, THEME_SCHEDULE_CHECKED_PROP) != 0)
    {
        g_clear_handle_id (&helper->rules_changed_id, g_source_remove);
        helper->rules_changed_id = g_timeout_add_once (RULES_CHANGED_DELAY_MS, xfce_theme_schedule_helper_rules_changed, helper);
    }
}
