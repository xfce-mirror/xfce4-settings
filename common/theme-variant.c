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

#include "theme-variant.h"

/* whether @theme_name is a dark theme: "-dark" at the end or in the middle, in any case */
gboolean
theme_variant_is_dark (const gchar *theme_name)
{
    gchar *strdown = g_ascii_strdown (theme_name, -1);
    gboolean dark = g_strstr_len (strdown, -1, "-dark-") != NULL || g_str_has_suffix (strdown, "-dark");

    g_free (strdown);

    return dark;
}
