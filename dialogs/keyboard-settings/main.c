/* $Id$ */
/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#include <glade/glade.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfconf/xfconf.h>

#include "keyboard-dialog_glade.h"
#include "shortcut-dialog.h"
#include "command-dialog.h"



enum
{
  SHORTCUT_COLUMN,
  ACTION_COLUMN,
};


static XfconfChannel *xsettings_channel;
static XfconfChannel *keyboards_channel;
static XfconfChannel *kbd_channel;



static gboolean     opt_version = FALSE;
static GOptionEntry entries[] = {
  { "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
  { NULL }
};



struct TreeViewInfo
{
  GtkTreeView *view;
  GtkTreeIter *iter;
};



static void
keyboard_settings_box_sensitivity (GtkToggleButton *button,
                                   GtkWidget       *box)
{
    gtk_widget_set_sensitive (GTK_WIDGET (box), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}



static void
keyboard_settings_load_shortcut (const gchar  *key,
                                 const GValue *value,
                                 GtkListStore *list_store)
{
  const GPtrArray *array;
  const GValue    *type_value;
  const GValue    *action_value;
  const gchar     *type;
  const gchar     *action;
  GtkTreeIter      iter;

  /* MAke sure we only load shortcuts from string arrays */
  if (G_UNLIKELY (G_VALUE_TYPE (value) != dbus_g_type_get_collection ("GPtrArray", G_TYPE_VALUE)))
    return;

  /* Get the pointer array */
  array = g_value_get_boxed (value);

  /* Make sure the array has exactly two members */
  if (G_UNLIKELY (array->len != 2))
    return;

  /* Get GValues for the array members */
  type_value = g_ptr_array_index (array, 0);
  action_value = g_ptr_array_index (array, 1);

  /* Make sure both are string values */
  if (G_UNLIKELY (G_VALUE_TYPE (type_value) != G_TYPE_STRING || G_VALUE_TYPE (action_value) != G_TYPE_STRING))
    return;

  /* Get shortcut type and action */
  type = g_value_get_string (type_value);
  action = g_value_get_string (action_value);

  /* Only add shortcuts with type 'execute' */
  if (g_utf8_collate (type, "execute") == 0)
    {
      /* Add shortcut to the list store */
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, SHORTCUT_COLUMN, key+1, ACTION_COLUMN, action, -1);
    }
}



static void
keyboard_settings_load_shortcuts (GtkWidget    *kbd_shortcuts_view,
                                  GtkListStore *list_store)
{
  GHashTable *shortcuts;

  g_return_if_fail (GTK_IS_TREE_VIEW (kbd_shortcuts_view));
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  shortcuts = xfconf_channel_get_all (kbd_channel);

  if (G_LIKELY (shortcuts != NULL))
    {
      g_hash_table_foreach (shortcuts, (GHFunc) keyboard_settings_load_shortcut, list_store);
      g_hash_table_destroy (shortcuts);
    }
}



static gboolean
keyboard_settings_validate_shortcut (ShortcutDialog      *dialog,
                                     const gchar         *shortcut,
                                     struct TreeViewInfo *info)
{
  gboolean  shortcut_accepted = TRUE;
  gchar    *current_shortcut;
  gchar    *property;

  /* Ignore raw 'Return' and 'space' since that may have been used to activate the shortcut row */
  if (G_UNLIKELY (g_utf8_collate (shortcut, "Return") == 0 || g_utf8_collate (shortcut, "space") == 0))
    return FALSE;

  /* Build property name */
  property = g_strdup_printf ("/%s", shortcut);

  if (G_LIKELY (info->iter != NULL))
    {
      /* Get shortcut of the row we're currently editing */
      gtk_tree_model_get (gtk_tree_view_get_model (info->view), info->iter, SHORTCUT_COLUMN, &current_shortcut, -1);

      /* Don't accept the shortcut if it already is being used somewhere else (and not by the current row) */
      if (G_UNLIKELY (xfconf_channel_has_property (kbd_channel, property) && g_utf8_collate (current_shortcut, shortcut) != 0))
        {
          xfce_err (_("Keyboard shortcut '%s' is already being used for something else."), shortcut);
          shortcut_accepted = FALSE;
        }
  
      /* Free shortcut string */
      g_free (current_shortcut);
    }
  else
    {
      if (G_UNLIKELY (xfconf_channel_has_property (kbd_channel, property)))
        {
          xfce_err (_("Keyboard shortcut '%s' is already being used for something else."), shortcut);
          shortcut_accepted = FALSE;
        }
    }

  /* Free strings */
  g_free (property);

  return shortcut_accepted;
}



static void
keyboard_settings_add_shortcut (GtkTreeView *tree_view)
{
  struct TreeViewInfo info;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  GtkWidget          *shortcut_dialog;
  GtkWidget          *command_dialog;
  const gchar        *shortcut = NULL;
  gboolean            finished = FALSE;
  gchar              *command = NULL;
  gchar              *property;
  gint                response;

  /* Create command dialog */
  command_dialog = command_dialog_new (NULL, NULL);

  /* Run command dialog until a vaild (non-empty) command is entered or the dialog is cancelled */
  do
    {
      response = command_dialog_run (COMMAND_DIALOG (command_dialog), GTK_WIDGET (tree_view));

      if (G_UNLIKELY (response == GTK_RESPONSE_OK && g_utf8_strlen (command_dialog_get_command (COMMAND_DIALOG (command_dialog)), -1) == 0))
        xfce_err (_("Short command may not be empty."));
      else
        finished = TRUE;
    }
  while (!finished);

  /* Abort if the dialog was cancelled */
  if (G_UNLIKELY (response == GTK_RESPONSE_OK))
    {
      /* Get the command */
      command = g_strdup (command_dialog_get_command (COMMAND_DIALOG (command_dialog)));

      /* Hide the command dialog */
      gtk_widget_hide (command_dialog);

      /* Prepare tree view info */
      info.view = tree_view;
      info.iter = NULL;

      /* Create shortcut dialog */
      shortcut_dialog = shortcut_dialog_new (command);
      g_signal_connect (shortcut_dialog, "validate-shortcut", G_CALLBACK (keyboard_settings_validate_shortcut), &info);

      /* Run shortcut dialog until a valid shortcut is entered or the dialog is cancelled */
      response = shortcut_dialog_run (SHORTCUT_DIALOG (shortcut_dialog), GTK_WIDGET (tree_view));

      /* Only continue if the shortcut dialog succeeded */
      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get shortcut */
          shortcut = shortcut_dialog_get_shortcut (SHORTCUT_DIALOG (shortcut_dialog));

          /* Get tree view list store */
          model = gtk_tree_view_get_model (tree_view);

          /* Append new row to the list store */
          gtk_list_store_append (GTK_LIST_STORE (model), &iter);

          /* Set row values */
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, SHORTCUT_COLUMN, shortcut, ACTION_COLUMN, command, -1);

          /* Save the new shortcut to xfconf */
          property = g_strdup_printf ("/%s", shortcut);
          xfconf_channel_set_array (kbd_channel, property, G_TYPE_STRING, "execute", G_TYPE_STRING, command, G_TYPE_INVALID);
          g_free (property);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (shortcut_dialog);

      /* Free command string */
      g_free (command);
    }

  /* Destroy the shortcut dialog */
  gtk_widget_destroy (command_dialog);
}



static void
keyboard_settings_delete_shortcut (GtkTreeView *tree_view)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  GList            *rows;
  GList            *row_iter;
  GList            *row_references = NULL;
  gchar            *shortcut;
  gchar            *property_name;

  /* Determine selected rows */
  selection = gtk_tree_view_get_selection (tree_view);
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (row_iter = g_list_first (rows); row_iter != NULL; row_iter = g_list_next (row_iter))
    row_references = g_list_append (row_references, gtk_tree_row_reference_new (model, (GtkTreePath *) (row_iter->data)));

  for (row_iter = g_list_first (row_references); row_iter != NULL; row_iter = g_list_next (row_iter))
    {
      path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *) (row_iter->data));

      /* Conver tree path to tree iter */
      if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
        {
          /* Read row values */
          gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &shortcut, -1);

          /* Delete row from the list store */
          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

          /* Build property name */
          property_name = g_strdup_printf ("/%s", shortcut);

          /* Remove keyboard shortcut via xfconf */
          xfconf_channel_remove_property (kbd_channel, property_name);

          /* Free strings */
          g_free (property_name);
          g_free (shortcut);
        }

      gtk_tree_path_free (path);
    }

  /* Free row reference list */
  g_list_foreach (row_references, (GFunc) gtk_tree_row_reference_free, NULL);
  g_list_free (row_references);

  /* Free row list */
  g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (rows);
}


static void
keyboard_settings_edit_shortcut (GtkTreeView *tree_view,
                                 GtkTreePath *path)
{
  struct TreeViewInfo info;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  GtkWidget          *dialog;
  const gchar        *new_shortcut;
  gchar              *current_shortcut;
  gchar              *action;
  gchar              *old_property;
  gchar              *new_property;
  gint                response;

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read current shortcut from the activated row */
      gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &current_shortcut, ACTION_COLUMN, &action, -1);

      /* Prepare tree view info */
      info.view = tree_view;
      info.iter = &iter;

      /* Request a new shortcut from the user */
      dialog = shortcut_dialog_new (action);
      g_signal_connect (dialog, "validate-shortcut", G_CALLBACK (keyboard_settings_validate_shortcut), &info);
      response = shortcut_dialog_run (SHORTCUT_DIALOG (dialog), GTK_WIDGET (tree_view));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Build property name */
          old_property = g_strdup_printf ("/%s", current_shortcut);

          /* Remove old shortcut from the settings */
          xfconf_channel_remove_property (kbd_channel, old_property);

          /* Get the shortcut entered by the user */
          new_shortcut = shortcut_dialog_get_shortcut (SHORTCUT_DIALOG (dialog));

          /* Save the new shortcut */
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, SHORTCUT_COLUMN, new_shortcut, -1);

          /* Only save new shortcut if it's not empty */
          if (G_LIKELY (response == GTK_RESPONSE_OK && strlen (new_shortcut) > 0))
            {
              /* Build property name */
              new_property = g_strdup_printf ("/%s", new_shortcut);

              /* Save new shortcut to the settings */
              xfconf_channel_set_array (kbd_channel, new_property, G_TYPE_STRING, "execute", G_TYPE_STRING, action, G_TYPE_INVALID);

              /* Free property names */
              g_free (new_property);
            }

          /* Free strings */
          g_free (old_property);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (action);
      g_free (current_shortcut);
    }
}



static void
keyboard_settings_edit_action (GtkTreeView *tree_view,
                               GtkTreePath *path)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkWidget    *dialog;
  gchar        *shortcut;
  gchar        *current_action;
  const gchar  *new_action;
  gchar        *property;
  gint          response;

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read shortcut and current action from the activated row */
      gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &shortcut, ACTION_COLUMN, &current_action, -1);

      /* Build property name */
      property = g_strdup_printf ("/%s", shortcut);

      /* Request a new action from the user */
      dialog = command_dialog_new (shortcut, current_action);
      response = command_dialog_run (COMMAND_DIALOG (dialog), GTK_WIDGET (tree_view));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get the action entered by the user */
          new_action = command_dialog_get_command (COMMAND_DIALOG (dialog));

          /* Replace the old action in the tree view */
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, ACTION_COLUMN, new_action, -1);

          /* Save new action to the settings */
          xfconf_channel_set_array (kbd_channel, property, G_TYPE_STRING, "execute", G_TYPE_STRING, new_action, G_TYPE_INVALID);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (property);
      g_free (shortcut);
      g_free (current_action);
    }
}



static void
keyboard_settings_row_activated (GtkTreeView       *tree_view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column)
{
  if (column == gtk_tree_view_get_column (tree_view, SHORTCUT_COLUMN))
    keyboard_settings_edit_shortcut (tree_view, path);
  else
    keyboard_settings_edit_action (tree_view, path);

  return;
}



GtkWidget*
keyboard_settings_dialog_new_from_xml (GladeXML *gxml)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkAdjustment     *net_cursor_blink_time_scale;
  GtkAdjustment     *xkb_key_repeat_delay_scale;
  GtkAdjustment     *xkb_key_repeat_rate_scale;
  GtkListStore      *list_store;
  GtkWidget         *kbd_shortcuts_view;
  GtkWidget         *net_cursor_blink_check;
  GtkWidget         *xkb_key_repeat_check;
  GtkWidget         *add_shortcut_button;
  GtkWidget         *delete_shortcut_button;
  GtkWidget         *dialog;
  GtkWidget         *box;

  /* XKB Settings */
  xkb_key_repeat_check = glade_xml_get_widget (gxml, "xkb_key_repeat_check");
  box = glade_xml_get_widget (gxml, "xkb_key_repeat_box");
  g_signal_connect (G_OBJECT (xkb_key_repeat_check), "toggled", G_CALLBACK (keyboard_settings_box_sensitivity), box);
  xfconf_g_property_bind (keyboards_channel, "/Default/KeyRepeat", G_TYPE_BOOLEAN, G_OBJECT (xkb_key_repeat_check), "active");
  
  xkb_key_repeat_rate_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "xkb_key_repeat_rate_scale")));
  xfconf_g_property_bind (keyboards_channel, "/Default/KeyRepeat/Rate", G_TYPE_INT, G_OBJECT (xkb_key_repeat_rate_scale), "value");
  
  xkb_key_repeat_delay_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "xkb_key_repeat_delay_scale")));
  xfconf_g_property_bind (keyboards_channel, "/Default/KeyRepeat/Delay", G_TYPE_INT, G_OBJECT (xkb_key_repeat_delay_scale), "value");

  /* XSETTINGS */
  net_cursor_blink_check = glade_xml_get_widget (gxml, "net_cursor_blink_check");
  box = glade_xml_get_widget (gxml, "net_cursor_blink_box");
  g_signal_connect (G_OBJECT (net_cursor_blink_check), "toggled", G_CALLBACK (keyboard_settings_box_sensitivity), box);
  xfconf_g_property_bind (xsettings_channel, "/Net/CursorBlink", G_TYPE_BOOLEAN, G_OBJECT (net_cursor_blink_check), "active");
  
  net_cursor_blink_time_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "net_cursor_blink_time_scale")));
  xfconf_g_property_bind (xsettings_channel, "/Net/CursorBlinkTime", G_TYPE_INT, G_OBJECT (net_cursor_blink_time_scale), "value");

  /* Configure shortcuts tree view */
  kbd_shortcuts_view = glade_xml_get_widget (gxml, "kbd_shortcuts_view");
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (kbd_shortcuts_view), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (kbd_shortcuts_view)), GTK_SELECTION_MULTIPLE);
  g_signal_connect (kbd_shortcuts_view, "row-activated", G_CALLBACK (keyboard_settings_row_activated), NULL);

  /* Create list store for keyboard shortcuts */
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), SHORTCUT_COLUMN, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (kbd_shortcuts_view), GTK_TREE_MODEL (list_store));

  /* Create shortcut column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, "text", SHORTCUT_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Create renderer for the action columns */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Command"), renderer, "text", ACTION_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Load keyboard shortcuts */
  keyboard_settings_load_shortcuts (kbd_shortcuts_view, list_store);

  /* Connect to add/delete button signals */
  add_shortcut_button = glade_xml_get_widget (gxml, "add_shortcut_button");
  g_signal_connect_swapped (G_OBJECT (add_shortcut_button), "clicked", G_CALLBACK (keyboard_settings_add_shortcut), GTK_TREE_VIEW (kbd_shortcuts_view));
  
  delete_shortcut_button = glade_xml_get_widget (gxml, "delete_shortcut_button");
  g_signal_connect_swapped (G_OBJECT (delete_shortcut_button), "clicked", G_CALLBACK (keyboard_settings_delete_shortcut), GTK_TREE_VIEW (kbd_shortcuts_view));

  /* Get dialog widget */
  dialog = glade_xml_get_widget (gxml, "keyboard-settings-dialog");
  gtk_widget_show_all(dialog);
  gtk_widget_hide(dialog);

  return dialog;
}

int
main(int argc, char **argv)
{
  GtkWidget *dialog;
  GladeXML  *gxml;
  GError    *error = NULL;

  /* Set translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  /* Initialize GTK+ and parse command line options */
  if(G_UNLIKELY (!gtk_init_with_args (&argc, &argv, "", entries, PACKAGE, &error)))
    {
      /* Print error if that failed */
      if (G_LIKELY (error != NULL))
        {
          /* print error */
          g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
          g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
          g_print ("\n");

          /* cleanup */
          g_error_free (error);
        }
      else
        g_error ("Unable to open display.");

        return EXIT_FAILURE;
    }

  /* Print version information and quit when the user entered --version or -v */
  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2008");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  /* Initialize xfconf */
  if (G_UNLIKELY (!xfconf_init (&error)))
    {
      /* print error and exit */
      g_error ("Failed to connect to xfconf daemon: %s.", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  /* load channels */
  xsettings_channel = xfconf_channel_new ("xsettings");
  keyboards_channel = xfconf_channel_new ("keyboards");
  kbd_channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  /* Parse Glade XML */
  gxml = glade_xml_new_from_buffer (keyboard_dialog_glade, keyboard_dialog_glade_length, NULL, NULL);

  /* Create settings dialog and run it */
  dialog = keyboard_settings_dialog_new_from_xml (gxml);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* Free Glade XML */
  g_object_unref (G_OBJECT (gxml));

  /* Unload channels */
  g_object_unref (G_OBJECT (xsettings_channel));
  g_object_unref (G_OBJECT (keyboards_channel));
  g_object_unref (G_OBJECT (kbd_channel));

  xfconf_shutdown ();

  return EXIT_SUCCESS;
}
