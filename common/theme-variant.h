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
 * Shared by the appearance dialog and xfsettingsd: whether a theme is a dark one.
 */

#ifndef __THEME_VARIANT_H__
#define __THEME_VARIANT_H__

#include <glib.h>

G_BEGIN_DECLS

gboolean
theme_variant_is_dark (const gchar *theme_name);

G_END_DECLS

#endif /* !__THEME_VARIANT_H__ */
