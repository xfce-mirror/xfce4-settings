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

#include "common/debug.h"

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>



G_MODULE_EXPORT void
gtk_module_init (gint *argc,
                 gchar ***argv);
G_MODULE_EXPORT const gchar *
g_module_check_init (GModule *module);

static const gchar *sync_properties[] = {
    "/Gtk/ButtonImages",
    "/Gtk/CanChangeAccels",
    "/Gtk/ColorPalette",
    "/Gtk/CursorThemeName",
    "/Gtk/CursorThemeSize",
    "/Gtk/DecorationLayout",
    "/Gtk/DialogsUseHeader",
    "/Gtk/FontName",
    "/Gtk/IconSizes",
    "/Gtk/KeyThemeName",
    "/Gtk/MenuBarAccel",
    "/Gtk/MenuImages",
    "/Gtk/Modules",
    "/Gtk/TitlebarMiddleClick",
    "/Net/CursorBlink",
    "/Net/CursorBlinkTime",
    "/Net/DndDragThreshold",
    "/Net/DoubleClickDistance",
    "/Net/DoubleClickTime",
    "/Net/EnableEventSounds",
    "/Net/EnableInputFeedbackSounds",
    "/Net/IconThemeName",
    "/Net/SoundThemeName",
    "/Net/ThemeName",
    "/Xft/Antialias",
    "/Xft/HintStyle",
    "/Xft/Hinting",
    "/Xft/RGBA",
    NULL, // g_strv_contains() requires NULL-termination
};
static const struct
{
    const gchar *transformed;
    const gchar *corrected;
} property_name_fixups[] = {
    { "gtk-xft-hint-style", "gtk-xft-hintstyle" },
};



/*
 * /Gtk/Token1Token2 -> gtk-token1-token2
 * /Net/Token1Token2 -> gtk-token1-token2
 * /Xft/Token1Token2 -> gtk-xft-token1-token2
 */
static gchar *
xfconf_prop_to_gtk_setting (const gchar *prop)
{
    GString *setting;
    const gchar *suffix;

    suffix = g_strrstr (prop, "/");
    if (suffix == NULL)
        return g_strdup (prop);

    setting = g_string_sized_new (strlen (prop));
    g_string_append (setting, "gtk");
    if (g_str_has_prefix (prop, "/Xft/"))
        g_string_append (setting, "-xft");

    for (const gchar *p = ++suffix; *p != '\0'; p++)
    {
        gboolean is_upper = g_ascii_isupper (*p);
        gboolean add_separator = is_upper && (p == suffix || g_ascii_islower (p[-1]) || g_ascii_islower (p[1]));

        if (add_separator)
            g_string_append_c (setting, '-');

        if (is_upper)
            g_string_append_c (setting, g_ascii_tolower (*p));
        else
            g_string_append_c (setting, *p);
    }

    for (gsize i = 0; i < G_N_ELEMENTS (property_name_fixups); ++i)
        if (g_strcmp0 (setting->str, property_name_fixups[i].transformed) == 0)
        {
            g_string_free (setting, TRUE);
            return g_strdup (property_name_fixups[i].corrected);
        }

    return g_string_free (setting, FALSE);
}



static void
property_changed (XfconfChannel *channel,
                  const gchar *property,
                  const GValue *value,
                  gpointer data)
{
    if (g_strv_contains (sync_properties, property))
    {
        GtkSettings *settings = gtk_settings_get_default ();
        gchar *setting = xfconf_prop_to_gtk_setting (property);
        GParamSpec *pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), setting);

        if (pspec != NULL)
        {
            const GValue *default_value = g_param_spec_get_default_value (pspec);

            xfsettings_dbg (XFSD_DEBUG_GTK_SETTINGS,
                            "Xfconf property '%s' changed, syncing with GtkSettings property '%s'",
                            property, g_param_spec_get_name (pspec));

            if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
                g_object_set_property (G_OBJECT (settings), setting, default_value);
            else
            {
                GValue trans_value = G_VALUE_INIT;
                g_value_init (&trans_value, G_VALUE_TYPE (default_value));
                if (g_value_transform (value, &trans_value))
                    g_object_set_property (G_OBJECT (settings), setting, &trans_value);
                else
                    g_object_set_property (G_OBJECT (settings), setting, default_value);
                g_value_unset (&trans_value);
            }
        }

        g_free (setting);
    }
}



G_MODULE_EXPORT void
gtk_module_init (gint *argc,
                 gchar ***argv)
{
    XfconfChannel *channel;
    GHashTable *props;
    GHashTableIter iter;
    gchar *prop;
    GValue *value;

    channel = xfconf_channel_get ("xsettings");
    g_signal_connect (channel, "property-changed", G_CALLBACK (property_changed), NULL);

    props = xfconf_channel_get_properties (channel, NULL);
    g_hash_table_iter_init (&iter, props);
    while (g_hash_table_iter_next (&iter, (gpointer *) &prop, (gpointer *) &value))
        if (g_strv_contains (sync_properties, prop))
            property_changed (channel, prop, value, NULL);

    g_hash_table_destroy (props);
}



G_MODULE_EXPORT const gchar *
g_module_check_init (GModule *module)
{
    GError *error = NULL;
    if (!xfconf_init (&error))
    {
        static gchar message[1024];
        g_strlcpy (message, error->message, 1024);
        g_error_free (error);
        return message;
    }

    g_module_make_resident (module);
    return NULL;
}
