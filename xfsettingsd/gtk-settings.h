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

#ifndef __XFCE_GTK_SETTINGS_HELPER_H__
#define __XFCE_GTK_SETTINGS_HELPER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_GTK_SETTINGS_HELPER xfce_gtk_settings_helper_get_type ()
G_DECLARE_FINAL_TYPE (XfceGtkSettingsHelper, xfce_gtk_settings_helper, XFCE, GTK_SETTINGS_HELPER, GObject)

G_END_DECLS

#endif /* !__XFCE_GTK_SETTINGS_HELPER_H__ */
