/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __EXO_HELPER_UTILS_H__
#define __EXO_HELPER_UTILS_H__

#include "exo-helper.h"

G_BEGIN_DECLS

gboolean exo_helper_category_from_string (const gchar       *string,
                                          ExoHelperCategory *category_return);
gchar   *exo_helper_category_to_string   (ExoHelperCategory  category) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__EXO_HELPER_UTILS_H__ */
