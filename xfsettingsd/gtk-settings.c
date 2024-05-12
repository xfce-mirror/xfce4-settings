/*
 *  Copyright (c) 2023 Gaël Bonithon <gael@xfce.org>
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

#include "gtk-settings-exported.h"
#include "gtk-settings.h"
#include "xsettings-properties.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#define WINDOWING_IS_WAYLAND() GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())
#else
#define WINDOWING_IS_WAYLAND() FALSE
#endif



static void
xfce_gtk_settings_helper_finalize (GObject *object);
static void
xfce_gtk_settings_helper_gsettings_changed (GSettings *gsettings,
                                            gchar *key,
                                            XfceGtkSettingsHelper *helper);
static void
xfce_gtk_settings_helper_channel_property_changed (XfconfChannel *channel,
                                                   const gchar *property,
                                                   const GValue *value,
                                                   XfceGtkSettingsHelper *helper);



struct _XfceGtkSettingsHelper
{
    GObject parent;

    XfceGtkSettingsHelperExported *skeleton;

    GHashTable *gsettings_objs;
    XfconfChannel *channel;
    GHashTable *gsettings_data, *xfconf_data;
};

typedef struct _GSettingsData
{
    const gchar *schema;
    const gchar *key;
    const gchar *gtksetting;
} GSettingsData;

typedef struct _XfconfData
{
    const gchar *property;
    const gchar *gtksetting;
} XfconfData;

/* from https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/gdk/wayland/gdkscreen-wayland.c
 * only types that are supposed to match are kept (.type != G_TYPE_NONE) and duplicates are
 * removed (org.gnome.desktop is preferred) */
static const GSettingsData translations[] = {
    { "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" },
    { "org.gnome.desktop.interface", "gtk-key-theme", "gtk-key-theme-name" },
    { "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name" },
    { "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name" },
    { "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size" },
    { "org.gnome.desktop.interface", "font-name", "gtk-font-name" },
    { "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink" },
    { "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time" },
    { "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout" },
    { "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module" },
    { "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations" },
    { "org.gnome.desktop.interface", "gtk-enable-primary-paste", "gtk-enable-primary-paste" },
    { "org.gnome.desktop.interface", "overlay-scrolling", "gtk-overlay-scrolling" },
    { "org.gnome.desktop.peripherals.mouse", "double-click", "gtk-double-click-time" },
    { "org.gnome.desktop.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold" },
    { "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name" },
    { "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds" },
    { "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds" },
    { "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age" },
    { "org.gnome.desktop.privacy", "remember-recent-files", "gtk-recent-files-enabled" },
    { "org.gnome.desktop.wm.preferences", "button-layout", "gtk-decoration-layout" },
    { "org.gnome.desktop.wm.preferences", "action-double-click-titlebar", "gtk-titlebar-double-click" },
    { "org.gnome.desktop.wm.preferences", "action-middle-click-titlebar", "gtk-titlebar-middle-click" },
    { "org.gnome.desktop.wm.preferences", "action-right-click-titlebar", "gtk-titlebar-right-click" },
    { "org.gnome.desktop.a11y", "always-show-text-caret", "gtk-keynav-use-caret" },
    { "org.gnome.fontconfig", "serial", "gtk-fontconfig-timestamp" },
};



G_DEFINE_TYPE (XfceGtkSettingsHelper, xfce_gtk_settings_helper, G_TYPE_OBJECT)



static void
xfce_gtk_settings_helper_class_init (XfceGtkSettingsHelperClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = xfce_gtk_settings_helper_finalize;
}



/*
 * gtk-xft-token1-token2 -> /Xft/Token1Token2
 * if Token1Token2 in xsettings_properties_Net[]: gtk-token1-token2 -> /Net/Token1Token2
 * else: gtk-token1-token2 -> /Gtk/Token1Token2
 */
static gchar *
gtk_setting_to_xfconf_prop (const gchar *setting,
                            GHashTable *net_props)
{
    gchar **tokens;
    gchar *prop, *suffix;
    gboolean xft = FALSE;

    if (!g_str_has_prefix (setting, "gtk-"))
        return g_strdup (setting);

    setting += 4;
    if (g_str_has_prefix (setting, "xft-"))
    {
        xft = TRUE;
        setting += 4;
    }

    tokens = g_strsplit (setting, "-", -1);
    for (gchar **token = tokens; *token != NULL; token++)
        **token = g_ascii_toupper (**token);

    suffix = g_strjoinv (NULL, tokens);
    if (xft)
        prop = g_strdup_printf ("/Xft/%s", suffix);
    else if (g_hash_table_contains (net_props, suffix))
        prop = g_strdup_printf ("/Net/%s", suffix);
    else
        prop = g_strdup_printf ("/Gtk/%s", suffix);

    g_free (suffix);
    g_strfreev (tokens);

    return prop;
}



static void
bus_acquired (GDBusConnection *connection,
              const gchar *name,
              gpointer user_data)
{
    XfceGtkSettingsHelper *helper = user_data;
    GError *error = NULL;

    helper->skeleton = xfce_gtk_settings_helper_exported_skeleton_new ();
    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (helper->skeleton),
                                           connection, "/org/gtk/Settings", &error))
    {
        g_warning ("Failed to export object at path '/org/gtk/Settings': %s", error->message);
        g_error_free (error);
    }
    else
        xfce_gtk_settings_helper_exported_set_modules (helper->skeleton, "xfsettingsd-gtk-settings-sync");

    g_object_unref (helper);
}



static void
name_lost (GDBusConnection *connection,
           const gchar *name,
           gpointer user_data)
{
    if (connection == NULL)
    {
        g_warning ("Failed to connect to session bus");
        g_object_unref (user_data);
    }
    else
        g_warning ("Name '%s' lost on session bus", name);
}



static void
xfce_gtk_settings_helper_init (XfceGtkSettingsHelper *helper)
{
    GHashTable *net_properties;
    GSettingsSchemaSource *source;
    GHashTableIter iter;
    XfconfData *data;

    /* synchronization via gtk-modules: requires GTK 3.24.38 to work properly (see MR !104) */
    if (WINDOWING_IS_WAYLAND ())
        g_bus_own_name (G_BUS_TYPE_SESSION, "org.gtk.Settings", G_BUS_NAME_OWNER_FLAGS_NONE,
                        bus_acquired, NULL, name_lost, g_object_ref (helper), NULL);

    source = g_settings_schema_source_get_default ();
    if (source == NULL)
    {
        g_warning ("No GSettings schema could be found");
        return;
    }

    net_properties = g_hash_table_new (g_str_hash, g_str_equal);
    for (guint i = 0; i < G_N_ELEMENTS (xsettings_properties_Net); i++)
        g_hash_table_add (net_properties, (gpointer) xsettings_properties_Net[i]);

    helper->gsettings_objs = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
    helper->gsettings_data = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    helper->xfconf_data = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    /* properties synchronized with GSettings */
    for (guint i = 0; i < G_N_ELEMENTS (translations); i++)
    {
        /* some schemas in translations may not be installed */
        GSettingsSchema *schema = g_settings_schema_source_lookup (source, translations[i].schema, TRUE);
        if (schema == NULL)
            continue;

        if (!g_hash_table_contains (helper->gsettings_objs, translations[i].schema))
        {
            GSettings *gsettings = g_settings_new (translations[i].schema);
            g_signal_connect (gsettings, "changed", G_CALLBACK (xfce_gtk_settings_helper_gsettings_changed), helper);
            g_hash_table_insert (helper->gsettings_objs, (gpointer) translations[i].schema, gsettings);
        }

        /* be careful: translations may not be up to date and there is no API to retrieve it */
        if (!g_settings_schema_has_key (schema, translations[i].key))
            g_warning ("Key '%s' not in schema '%s': skipped", translations[i].key, translations[i].schema);
        else
        {
            GObjectClass *class = G_OBJECT_GET_CLASS (gtk_settings_get_default ());
            GParamSpec *pspec = g_object_class_find_property (class, translations[i].gtksetting);
            if (pspec == NULL)
                g_warning ("Property '%s' not found in GtkSettings: skipped", translations[i].gtksetting);
            else
            {
                GSettings *gsettings = g_hash_table_lookup (helper->gsettings_objs, translations[i].schema);
                GVariant *variant = g_settings_get_value (gsettings, translations[i].key);
                const GValue *gtksettings_value = g_param_spec_get_default_value (pspec);
                GValue gsettings_value = G_VALUE_INIT;
                gboolean type_match;

                g_dbus_gvariant_to_gvalue (variant, &gsettings_value);
                type_match = G_VALUE_TYPE (gtksettings_value) == G_VALUE_TYPE (&gsettings_value);
                g_value_unset (&gsettings_value);
                g_variant_unref (variant);

                /* for GSettings, unlike Xfconf, this check can take place here only once */
                if (!type_match)
                    g_warning ("Types of GSettings id '%s.%s' and GtkSettings property '%s' mismatch",
                               translations[i].schema, translations[i].key, translations[i].gtksetting);
                else
                {
                    gchar *xfconf_prop = gtk_setting_to_xfconf_prop (translations[i].gtksetting, net_properties);
                    gchar *gsettings_id = g_strdup_printf ("%s.%s", translations[i].schema, translations[i].key);

                    if (g_hash_table_contains (helper->gsettings_data, xfconf_prop))
                    {
                        g_warning ("Duplicate gtksetting '%s': the first wins", translations[i].gtksetting);
                        g_free (xfconf_prop);
                        g_free (gsettings_id);
                    }
                    else if (g_hash_table_contains (helper->xfconf_data, gsettings_id))
                    {
                        g_warning ("Duplicate gsettings id '%s': the first wins", gsettings_id);
                        g_free (xfconf_prop);
                        g_free (gsettings_id);
                    }
                    else
                    {
                        GSettingsData *gsettings_data = g_new (GSettingsData, 1);
                        XfconfData *xfconf_data = g_new (XfconfData, 1);

                        gsettings_data->schema = translations[i].schema;
                        gsettings_data->key = translations[i].key;
                        gsettings_data->gtksetting = translations[i].gtksetting;
                        g_hash_table_insert (helper->gsettings_data, xfconf_prop, gsettings_data);

                        xfconf_data->property = xfconf_prop;
                        xfconf_data->gtksetting = translations[i].gtksetting;
                        g_hash_table_insert (helper->xfconf_data, gsettings_id, xfconf_data);
                    }
                }
            }
        }

        g_settings_schema_unref (schema);
    }

    helper->channel = xfconf_channel_get ("xsettings");
    g_signal_connect (helper->channel, "property-changed", G_CALLBACK (xfce_gtk_settings_helper_channel_property_changed), helper);

    /*
     * Initialize GSettings with Xfconf. Since we have no natural way to decide who was
     * last modified, a choice must be made. We might as well choose to give precedence
     * to our settings manager, this allows for example a restoration of Xfce settings.
     */
    g_hash_table_iter_init (&iter, helper->xfconf_data);
    while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
        GValue value = G_VALUE_INIT;
        xfconf_channel_get_property (helper->channel, data->property, &value);
        xfce_gtk_settings_helper_channel_property_changed (helper->channel, data->property, &value, helper);
        g_value_unset (&value);
    }

    g_hash_table_destroy (net_properties);
}



static void
xfce_gtk_settings_helper_finalize (GObject *object)
{
    XfceGtkSettingsHelper *helper = XFCE_GTK_SETTINGS_HELPER (object);

    if (helper->skeleton != NULL)
        g_object_unref (helper->skeleton);

    if (helper->gsettings_objs != NULL)
    {
        g_hash_table_destroy (helper->gsettings_objs);
        g_hash_table_destroy (helper->gsettings_data);
        g_hash_table_destroy (helper->xfconf_data);
    }

    G_OBJECT_CLASS (xfce_gtk_settings_helper_parent_class)->finalize (object);
}



static void
xfce_gtk_settings_helper_gsettings_changed (GSettings *gsettings,
                                            gchar *key,
                                            XfceGtkSettingsHelper *helper)
{
    gchar *schema, *id;
    XfconfData *data;
    GVariant *variant;
    GValue value = G_VALUE_INIT;

    g_object_get (gsettings, "schema-id", &schema, NULL);
    id = g_strdup_printf ("%s.%s", schema, key);
    data = g_hash_table_lookup (helper->xfconf_data, id);
    g_free (id);
    g_free (schema);

    /* not a synchronized id */
    if (data == NULL)
        return;

    variant = g_settings_get_value (gsettings, key);
    g_dbus_gvariant_to_gvalue (variant, &value);

    /* avoid cycling */
    g_signal_handlers_block_by_func (helper->channel, xfce_gtk_settings_helper_channel_property_changed, helper);

    if (!xfconf_channel_set_property (helper->channel, data->property, &value))
        g_warning ("Failed to set Xfconf property '%s'", data->property);

    g_signal_handlers_unblock_by_func (helper->channel, xfce_gtk_settings_helper_channel_property_changed, helper);

    g_value_unset (&value);
    g_variant_unref (variant);
}



static void
xfce_gtk_settings_helper_channel_property_changed (XfconfChannel *channel,
                                                   const gchar *property,
                                                   const GValue *value,
                                                   XfceGtkSettingsHelper *helper)
{
    GSettingsData *data;
    GObjectClass *class;
    GParamSpec *pspec;
    const GValue *default_value;
    GSettings *gsettings;

    data = g_hash_table_lookup (helper->gsettings_data, property);

    /* not a synchronized property */
    if (data == NULL)
        return;

    class = G_OBJECT_GET_CLASS (gtk_settings_get_default ());
    pspec = g_object_class_find_property (class, data->gtksetting);
    default_value = g_param_spec_get_default_value (pspec);

    /* unlike GSettings the type of Xfconf properties can change or they can be newly created */
    if (G_VALUE_TYPE (value) != G_TYPE_INVALID
        && !g_value_type_transformable (G_VALUE_TYPE (value), G_VALUE_TYPE (default_value)))
    {
        g_warning ("Xfconf property '%s' and GtkSettings property '%s' are of incompatible types", property, data->gtksetting);
        return;
    }

    gsettings = g_hash_table_lookup (helper->gsettings_objs, data->schema);

    /* avoid cycling */
    g_signal_handlers_block_by_func (gsettings, xfce_gtk_settings_helper_gsettings_changed, helper);

    /* Xfconf property removed => GtkSettings property reset, but better not to use GtkSettings
     * default value, see https://gitlab.gnome.org/GNOME/gtk/-/issues/5700 */
    if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
        g_settings_reset (gsettings, data->key);
    else
    {
        GVariant *old_variant;
        GVariant *new_variant;
        GValue new_value = G_VALUE_INIT;

        /* conversion rules of g_dbus_gvalue_to_gvariant() do not correspond to those of
         * g_value_transform() so this intermediate conversion is necessary in general */
        old_variant = g_settings_get_value (gsettings, data->key);
        g_dbus_gvariant_to_gvalue (old_variant, &new_value);
        g_value_reset (&new_value);
        g_value_transform (value, &new_value);
        new_variant = g_dbus_gvalue_to_gvariant (&new_value, g_variant_get_type (old_variant));

        /* we checked what we could but the value could be out of range for example */
        if (!g_settings_set_value (gsettings, data->key, new_variant))
            g_warning ("Failed to set GSettings id '%s.%s'", data->schema, data->key);

        g_variant_unref (new_variant);
        g_value_unset (&new_value);
        g_variant_unref (old_variant);
    }

    g_signal_handlers_unblock_by_func (gsettings, xfce_gtk_settings_helper_gsettings_changed, helper);
}
