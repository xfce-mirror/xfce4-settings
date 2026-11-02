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
 * Shared by the appearance dialog and xfsettingsd: finding the dark or light
 * variant of a theme. Theme authors spell the variant as "-dark", "-Dark", in
 * the middle of the name ("Orchis-Dark-Compact") or as a "-light" counterpart
 * ("Materia-light"), so names are compared case-insensitively, part by part.
 */

#ifndef __THEME_VARIANT_H__
#define __THEME_VARIANT_H__

#include <glib.h>

G_BEGIN_DECLS

/* returns the installed spelling of @name (case-insensitive match) or NULL */
typedef gchar *(*ThemeVariantLookupFunc) (const gchar *name,
                                          gpointer user_data);

/* whether @theme_name is a dark variant ("-dark" at the end or in the middle) */
gboolean
theme_variant_is_dark (const gchar *theme_name);

/*
 * The installed dark (@dark is TRUE) or light variant of @theme_name, @theme_name
 * itself if it is that variant already, or NULL if there is none.
 */
gchar *
theme_variant_find (const gchar *theme_name,
                    gboolean dark,
                    ThemeVariantLookupFunc lookup,
                    gpointer user_data);

G_END_DECLS

#endif /* !__THEME_VARIANT_H__ */
