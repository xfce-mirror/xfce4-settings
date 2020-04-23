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

#ifndef __EXO_HELPER_H__
#define __EXO_HELPER_H__

#include <exo/exo.h>

G_BEGIN_DECLS

typedef enum /*< enum,prefix=EXO_HELPER >*/
{
  EXO_HELPER_WEBBROWSER,        /*< nick=WebBrowser >*/
  EXO_HELPER_MAILREADER,        /*< nick=MailReader >*/
  EXO_HELPER_FILEMANAGER,       /*< nick=FileManager >*/
  EXO_HELPER_TERMINALEMULATOR,  /*< nick=TerminalEmulator >*/
  EXO_HELPER_N_CATEGORIES,      /*< skip >*/
} ExoHelperCategory;

typedef struct _ExoHelperClass ExoHelperClass;
typedef struct _ExoHelper      ExoHelper;

#define EXO_TYPE_HELPER            (exo_helper_get_type ())
#define EXO_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_HELPER, ExoHelper))
#define EXO_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_HELPER, ExoHelperClass))
#define EXO_IS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_HELPER))
#define EXO_IS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_HELPER))
#define EXO_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_HELPER, ExoHelperClass))

GType              exo_helper_get_type      (void) G_GNUC_CONST;
ExoHelperCategory  exo_helper_get_category  (const ExoHelper   *helper);
const gchar       *exo_helper_get_id        (const ExoHelper   *helper);
const gchar       *exo_helper_get_name      (const ExoHelper   *helper);
const gchar       *exo_helper_get_icon      (const ExoHelper   *helper);
const gchar       *exo_helper_get_command   (const ExoHelper   *helper);
gboolean           exo_helper_execute       (ExoHelper         *helper,
                                             GdkScreen         *screen,
                                             const gchar       *parameter,
                                             GError           **error);


#define EXO_TYPE_HELPER_DATABASE             (exo_helper_database_get_type ())
#define EXO_HELPER_DATABASE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_HELPER_DATABASE, ExoHelperDatabase))
#define EXO_HELPER_DATABASE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_HELPER_DATABASE, ExoHelperDatabaseClass))
#define EXO_IS_HELPER_DATABASE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_HELPER_DATABASE))
#define EXO_IS_HELPER_DATABASE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_HELPER_DATABASE))
#define EXO_HELPER_DATABASE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_HELPER_DATABASE, ExoHelperDatabaseClass))

typedef struct _ExoHelperDatabaseClass ExoHelperDatabaseClass;
typedef struct _ExoHelperDatabase      ExoHelperDatabase;

GType               exo_helper_database_get_type        (void) G_GNUC_CONST;
ExoHelperDatabase  *exo_helper_database_get             (void);
ExoHelper          *exo_helper_database_get_default     (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category);
gboolean            exo_helper_database_set_default     (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category,
                                                         ExoHelper         *helper,
                                                         GError           **error);
gboolean            exo_helper_database_clear_default   (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category,
                                                         GError           **error);
GList              *exo_helper_database_get_all         (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category);
ExoHelper          *exo_helper_database_get_custom      (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category);
void                exo_helper_database_set_custom      (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category,
                                                         const gchar       *command);
gboolean            exo_helper_database_get_dismissed   (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category);
gboolean            exo_helper_database_set_dismissed   (ExoHelperDatabase *database,
                                                         ExoHelperCategory  category,
                                                         gboolean           dismissed);

G_END_DECLS

#endif /* !__EXO_HELPER_H__ */
