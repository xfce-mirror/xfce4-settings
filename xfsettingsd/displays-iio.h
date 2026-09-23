/*
 *  Copyright (C) 2026 John Williams <codermuffin@gmail.com>
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

#ifndef __DISPLAYS_IIO_H__
#define __DISPLAYS_IIO_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _XfceDisplaysIIOClass XfceDisplaysIIOClass;
typedef struct _XfceDisplaysIIO XfceDisplaysIIO;

#define XFCE_TYPE_DISPLAYS_IIO (xfce_displays_iio_get_type ())
#define XFCE_DISPLAYS_IIO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_DISPLAYS_IIO, XfceDisplaysIIO))
#define XFCE_DISPLAYS_IIO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_DISPLAYS_IIO, XfceDisplaysIIOClass))
#define XFCE_IS_DISPLAYS_IIO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_DISPLAYS_IIO))
#define XFCE_IS_DISPLAYS_IIO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_DISPLAYS_IIO))
#define XFCE_DISPLAYS_IIO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_DISPLAYS_IIO, XfceDisplaysIIOClass))
G_DEFINE_AUTOPTR_CLEANUP_FUNC (XfceDisplaysIIO, g_object_unref)

GType
xfce_displays_iio_get_type (void);

G_END_DECLS

#endif /* !__DISPLAYS_IIO_H__ */
