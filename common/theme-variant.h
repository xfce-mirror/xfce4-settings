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
 * Shared by the appearance dialog and xfsettingsd (header only, the common
 * library is not always built): finding the dark or light variant of a theme.
 * Theme authors spell the variant as "-dark", "-Dark", in the middle of the
 * name ("Orchis-Dark-Compact") or as a "-light" counterpart ("Materia-light"),
 * so names are compared case-insensitively, part by part.
 */

#ifndef __THEME_VARIANT_H__
#define __THEME_VARIANT_H__

#include <glib.h>

G_BEGIN_DECLS

/* returns the installed spelling of @name (case-insensitive match) or NULL */
typedef gchar *(*ThemeVariantLookupFunc) (const gchar *name,
                                          gpointer user_data);

/* @parts joined with '-', without the part at @skip and with @insert before
 * the part at @insert_at (both may be -1) */
static inline gchar *
theme_variant_join (gchar **parts,
                    gint skip,
                    gint insert_at,
                    const gchar *insert)
{
    GString *name = g_string_new (NULL);

    for (gint i = 0; parts[i] != NULL; i++)
    {
        if (i == insert_at)
            g_string_append_printf (name, "%s%s", name->len > 0 ? "-" : "", insert);
        if (i != skip)
            g_string_append_printf (name, "%s%s", name->len > 0 ? "-" : "", parts[i]);
    }

    if (insert_at == (gint) g_strv_length (parts))
        g_string_append_printf (name, "-%s", insert);

    return g_string_free (name, FALSE);
}

/* whether @theme_name is a dark variant, the same rule as for the color scheme */
static inline gboolean
theme_variant_is_dark (const gchar *theme_name)
{
    gchar *strdown = g_ascii_strdown (theme_name, -1);
    gboolean dark = g_strstr_len (strdown, -1, "-dark-") != NULL || g_str_has_suffix (strdown, "-dark");

    g_free (strdown);

    return dark;
}

/*
 * The installed dark (@dark is TRUE) or light variant of @theme_name, @theme_name
 * itself if it is that variant already, or NULL if there is none.
 */
static inline gchar *
theme_variant_find (const gchar *theme_name,
                    gboolean dark,
                    ThemeVariantLookupFunc lookup,
                    gpointer user_data)
{
    gchar **parts = g_strsplit (theme_name, "-", -1);
    gint n_parts = g_strv_length (parts);
    gint dark_part = -1, light_part = -1;
    GPtrArray *candidates = g_ptr_array_new_with_free_func (g_free);
    gchar *found = NULL;

    /* the first part is the theme's own name, never a variant */
    for (gint i = 1; i < n_parts; i++)
    {
        if (dark_part < 0 && g_ascii_strcasecmp (parts[i], "dark") == 0)
            dark_part = i;
        else if (light_part < 0 && g_ascii_strcasecmp (parts[i], "light") == 0)
            light_part = i;
    }

    if (dark ? dark_part > 0 : dark_part < 0)
    {
        g_ptr_array_unref (candidates);
        g_strfreev (parts);
        return g_strdup (theme_name);
    }

    if (dark)
    {
        gchar **base;
        gint n_base;

        /* Materia-light -> Materia-dark */
        if (light_part > 0)
            g_ptr_array_add (candidates, theme_variant_join (parts, light_part, light_part, "dark"));

        /* Yaru-blue -> Yaru-blue-dark, Orchis-Compact -> Orchis-Dark-Compact */
        if (light_part > 0)
        {
            gchar *name = theme_variant_join (parts, light_part, -1, NULL);
            base = g_strsplit (name, "-", -1);
            g_free (name);
        }
        else
            base = g_strdupv (parts);

        n_base = g_strv_length (base);
        g_ptr_array_add (candidates, theme_variant_join (base, -1, n_base, "dark"));
        for (gint i = 1; i < n_base; i++)
            g_ptr_array_add (candidates, theme_variant_join (base, -1, i, "dark"));

        g_strfreev (base);
    }
    else
    {
        /* Greybird-dark -> Greybird, Materia-dark -> Materia-light */
        g_ptr_array_add (candidates, theme_variant_join (parts, dark_part, -1, NULL));
        g_ptr_array_add (candidates, theme_variant_join (parts, dark_part, dark_part, "light"));
    }

    for (guint i = 0; i < candidates->len && found == NULL; i++)
        found = lookup (g_ptr_array_index (candidates, i), user_data);

    g_ptr_array_unref (candidates);
    g_strfreev (parts);

    return found;
}

G_END_DECLS

#endif /* !__THEME_VARIANT_H__ */
