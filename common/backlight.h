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
 * The screen backlight, shared by the appearance dialog and xfsettingsd.
 *
 * Linux: the devices in /sys/class/backlight. Everyone may read them, but
 * writing needs root, so the brightness is set through logind, which allows
 * it for the user of the session.
 *
 * FreeBSD: the backlight(9) devices in /dev/backlight. They belong to
 * root:video with mode 0660, so members of the video group may read and set
 * the brightness directly.
 *
 * On other systems no backlight is found, and the dialog does not offer it.
 */

#ifndef __COMMON_BACKLIGHT_H__
#define __COMMON_BACKLIGHT_H__

#include <glib.h>

G_BEGIN_DECLS

/* the name of the backlight device to use, or NULL if there is none */
gchar *
backlight_find_device (void);

/* the brightness of @device in percent, rounded, or -1 on error */
gint
backlight_get_percent (const gchar *device);

/* sets the brightness of @device to @percent (1-100); errors are logged */
void
backlight_set_percent (const gchar *device,
                       guint percent);

G_END_DECLS

#endif /* !__COMMON_BACKLIGHT_H__ */
