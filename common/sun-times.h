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

#ifndef __SUN_TIMES_H__
#define __SUN_TIMES_H__

#include <glib.h>

G_BEGIN_DECLS

/* the largest offset of a rule from sunrise or sunset, in minutes */
#define SUN_TIMES_MAX_OFFSET_MINUTES (3 * 60)

typedef enum
{
    SUN_TIMES_SUNRISE,
    SUN_TIMES_SUNSET,
} SunTimesEvent;

gboolean
sun_times_parse (const gchar *str,
                 SunTimesEvent *event,
                 gint *offset_minutes);

gchar *
sun_times_to_string (SunTimesEvent event,
                     gint offset_minutes);

gboolean
sun_times_has_location (void);

gint
sun_times_get_minutes_since_midnight (GDateTime *day,
                                      SunTimesEvent event,
                                      gint offset_minutes);

G_END_DECLS

#endif /* !__SUN_TIMES_H__ */
