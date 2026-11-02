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
 * Shared by the appearance dialog and xfsettingsd: finding the screen
 * backlight in /sys/class/backlight. Like in GNOME, firmware interfaces are
 * preferred over platform ones and those over raw ones. Writing the
 * brightness needs root, so xfsettingsd sets it through logind.
 */

#ifndef __BACKLIGHT_H__
#define __BACKLIGHT_H__

#include <glib.h>

G_BEGIN_DECLS

#define BACKLIGHT_SYSFS_DIR "/sys/class/backlight"

guint64
backlight_read_value (const gchar *device,
                      const gchar *file);

/* the name of the backlight device to use, or NULL if there is none */
gchar *
backlight_find_device (void);

/* the brightness of @device in percent, rounded */
gint
backlight_get_percent (const gchar *device);

G_END_DECLS

#endif /* !__BACKLIGHT_H__ */
