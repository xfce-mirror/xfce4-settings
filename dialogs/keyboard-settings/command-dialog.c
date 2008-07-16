/* $Id$ */
/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at 
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "command-dialog.h"



static void command_dialog_class_init      (CommandDialogClass *klass);
static void command_dialog_init            (CommandDialog      *dialog);
static void command_dialog_dispose         (GObject             *object);
static void command_dialog_finalize        (GObject             *object);
static void command_dialog_create_contents (CommandDialog      *dialog,
                                            const gchar        *shortcut,
                                            const gchar        *action);
static void command_dialog_button_clicked  (CommandDialog      *dialog);



struct _CommandDialogClass
{
  XfceTitledDialogClass __parent__;
};

struct _CommandDialog
{
  XfceTitledDialog __parent__;

  GtkWidget *entry;
  GtkWidget *button;
};



static GObjectClass *command_dialog_parent_class = NULL;



GType
command_dialog_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info = 
        {
          sizeof (CommandDialogClass),
          NULL,
          NULL,
          (GClassInitFunc) command_dialog_class_init,
          NULL,
          NULL,
          sizeof (CommandDialog),
          0,
          (GInstanceInitFunc) command_dialog_init,
          NULL,
        };

      type = g_type_register_static (XFCE_TYPE_TITLED_DIALOG, "CommandDialog", &info, 0);
    }
  
  return type;
}



static void
command_dialog_class_init (CommandDialogClass *klass)
{
  GObjectClass *gobject_class;

  /* Determine parent type class */
  command_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = command_dialog_dispose;
  gobject_class->finalize = command_dialog_finalize;
}



static void
command_dialog_init (CommandDialog *dialog)
{
  dialog->entry = NULL;
  dialog->button = NULL;
}



static void
command_dialog_dispose (GObject *object)
{
  (*G_OBJECT_CLASS (command_dialog_parent_class)->dispose) (object);
}



static void
command_dialog_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (command_dialog_parent_class)->finalize) (object);
}



GtkWidget*
command_dialog_new (const gchar *shortcut,
                    const gchar *action)
{
  CommandDialog *dialog;
  
  dialog = COMMAND_DIALOG (g_object_new (TYPE_COMMAND_DIALOG, NULL));

  command_dialog_create_contents (dialog, shortcut, action);

  return GTK_WIDGET (dialog);
}



static void 
command_dialog_create_contents (CommandDialog *dialog,
                                const gchar   *shortcut,
                                const gchar   *action)
{
  GtkWidget *button;
  GtkWidget *hbox;
  gchar     *text;

  /* Set dialog title and icon */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Select shortcut command"));
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "application-x-executable");

  /* Set subtitle */
  text = g_strdup_printf (_("Shortcut: %s"), shortcut != NULL ? shortcut : _("Undefined"));
  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), text);
  g_free (text);

  /* Configure dialog */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  /* Create cancel button */
  button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
  gtk_widget_show (button);

  button = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
  gtk_widget_show (button);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
  gtk_widget_show (hbox);

  dialog->entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (dialog->entry), action != NULL ? action : "");
  gtk_box_pack_start (GTK_BOX (hbox), dialog->entry, TRUE, TRUE, 0);
  gtk_widget_show (dialog->entry);

  dialog->button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
  g_signal_connect_swapped (dialog->button, "clicked", G_CALLBACK (command_dialog_button_clicked), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->button, FALSE, TRUE, 0);
  gtk_widget_show (dialog->button);
}



const char*
command_dialog_get_command (CommandDialog *dialog)
{
  g_return_val_if_fail (IS_COMMAND_DIALOG (dialog), NULL);
  return gtk_entry_get_text (GTK_ENTRY (dialog->entry));
}



gint
command_dialog_run (CommandDialog *dialog,
                    GtkWidget     *parent)
{
  gint     response = GTK_RESPONSE_CANCEL;
  gboolean finished = FALSE;

  g_return_val_if_fail (IS_COMMAND_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (parent)));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  do 
    {
      response = gtk_dialog_run (GTK_DIALOG (dialog));

      if (G_UNLIKELY (response != GTK_RESPONSE_CANCEL && g_utf8_strlen (command_dialog_get_command (dialog), -1) == 0))
        xfce_err (_("The command may not be empty."));
      else
        finished = TRUE;
    }
  while (!finished);

  return response;
}



static void 
command_dialog_button_clicked (CommandDialog *dialog)
{
  GtkWidget     *chooser;
  GtkFileFilter *filter;
  gchar         *filename;

  g_return_if_fail (IS_COMMAND_DIALOG (dialog));

  chooser = gtk_file_chooser_dialog_new (_("Select command"), 
                                         GTK_WINDOW (dialog), 
                                         GTK_FILE_CHOOSER_ACTION_OPEN, 
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);

  /* Add file chooser filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* Use bindir as default folder */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* Run the file chooser */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK))
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      gtk_entry_set_text (GTK_ENTRY (dialog->entry), filename);
      g_free (filename);
    }

  /* Destroy the dialog */
  gtk_widget_destroy (chooser);
}
