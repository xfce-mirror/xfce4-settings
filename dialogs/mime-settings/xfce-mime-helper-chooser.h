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

#ifndef __EXO_HELPER_CHOOSER_H__
#define __EXO_HELPER_CHOOSER_H__

#include "exo-helper.h"

G_BEGIN_DECLS

#define EXO_TYPE_HELPER_CHOOSER            (exo_helper_chooser_get_type ())
#define EXO_HELPER_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_HELPER_CHOOSER, ExoHelperChooser))
#define EXO_HELPER_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_HELPER_CHOOSER, ExoHelperChooserClass))
#define EXO_IS_HELPER_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_HELPER_CHOOSER))
#define EXO_IS_HELPER_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_HELPER_CHOOSER))
#define EXO_HELPER_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_HELPER_CHOOSER, ExoHelperChooserClass))

typedef struct _ExoHelperChooserClass ExoHelperChooserClass;
typedef struct _ExoHelperChooser      ExoHelperChooser;

GType              exo_helper_chooser_get_type      (void) G_GNUC_CONST;

GtkWidget         *exo_helper_chooser_new           (ExoHelperCategory       category) G_GNUC_MALLOC;

ExoHelperCategory  exo_helper_chooser_get_category  (const ExoHelperChooser *chooser);
void               exo_helper_chooser_set_category  (ExoHelperChooser       *chooser,
                                                     ExoHelperCategory       category);

gboolean           exo_helper_chooser_get_is_valid  (const ExoHelperChooser *chooser);

G_END_DECLS

#endif /* !__EXO_HELPER_CHOOSER_H__ */
