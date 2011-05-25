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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "command-dialog.h"



static void command_dialog_create_contents (CommandDialog      *dialog,
                                            const gchar        *shortcut,
                                            const gchar        *action,
                                            gboolean            snotify);
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
  GtkWidget *sn_option;
};



G_DEFINE_TYPE (CommandDialog, command_dialog, XFCE_TYPE_TITLED_DIALOG)



static void
command_dialog_class_init (CommandDialogClass *klass)
{

}



static void
command_dialog_init (CommandDialog *dialog)
{
  dialog->entry = NULL;
  dialog->button = NULL;
}



GtkWidget*
command_dialog_new (const gchar *shortcut,
                    const gchar *action,
                    gboolean     snotify)
{
  CommandDialog *dialog;

  dialog = COMMAND_DIALOG (g_object_new (TYPE_COMMAND_DIALOG, NULL));

  command_dialog_create_contents (dialog, shortcut, action, snotify);

  return GTK_WIDGET (dialog);
}



static void
command_dialog_create_contents (CommandDialog *dialog,
                                const gchar   *shortcut,
                                const gchar   *action,
                                gboolean       snotify)
{
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *hbox;

  /* Set dialog title and icon */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Shortcut Command"));
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "application-x-executable");

  /* Configure dialog */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  /* Create cancel button */
  button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
  gtk_widget_show (button);

  button = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), table);
  gtk_widget_show (table);

  label = gtk_label_new (_("Shortcut:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (shortcut);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Command:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_widget_show (hbox);

  dialog->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (dialog->entry), TRUE);
  gtk_entry_set_text (GTK_ENTRY (dialog->entry), action != NULL ? action : "");
  gtk_container_add (GTK_CONTAINER (hbox), dialog->entry);
  gtk_widget_show (dialog->entry);

  dialog->button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
  g_signal_connect_swapped (dialog->button, "clicked", G_CALLBACK (command_dialog_button_clicked), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->button, FALSE, TRUE, 0);
  gtk_widget_show (dialog->button);

  dialog->sn_option = gtk_check_button_new_with_mnemonic (_("Use _startup notification"));
  gtk_table_attach (GTK_TABLE (table), dialog->sn_option, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->sn_option), snotify);
  gtk_widget_show (dialog->sn_option);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}



const char*
command_dialog_get_command (CommandDialog *dialog)
{
  g_return_val_if_fail (IS_COMMAND_DIALOG (dialog), NULL);
  return gtk_entry_get_text (GTK_ENTRY (dialog->entry));
}




gboolean
command_dialog_get_snotify (CommandDialog *dialog)
{
  g_return_val_if_fail (IS_COMMAND_DIALOG (dialog), FALSE);
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->sn_option));
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
        xfce_dialog_show_error (GTK_WINDOW (dialog), NULL, _("The command may not be empty."));
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
