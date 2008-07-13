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



enum
{
  SHORTCUT_COLUMN,
  ACTION_COLUMN,
};


static XfconfChannel *xsettings_channel;
static XfconfChannel *xkb_channel;
static XfconfChannel *kbd_channel;



gboolean opt_version = FALSE;



static GOptionEntry entries[] = {
  { "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
  { NULL }
};



static void
xkb_key_repeat_toggled (GtkToggleButton *button, 
                        GladeXML        *gxml)
{
    GtkWidget *box;
    
    box = glade_xml_get_widget (gxml, "xkb_key_repeat_box");
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active (button));
}



static void
net_cursor_blink_toggled (GtkToggleButton *button, 
                          GladeXML        *gxml)
{
    GtkWidget *box;
    
    box = glade_xml_get_widget (gxml, "net_cursor_blink_box");
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active (button));
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
keyboard_settings_load_shortcuts (XfconfChannel *channel, 
                                  GtkWidget     *kbd_shortcuts_view, 
                                  GtkListStore  *list_store)
{
  GHashTable *shortcuts;

  g_return_if_fail (GTK_IS_TREE_VIEW (kbd_shortcuts_view));
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  shortcuts = xfconf_channel_get_all (channel);

  if (G_LIKELY (shortcuts != NULL))
    {
      g_hash_table_foreach (shortcuts, (GHFunc) keyboard_settings_load_shortcut, list_store);
      g_hash_table_destroy (shortcuts);
    }
}



static void
keyboard_settings_add_shortcut (GtkTreeView *tree_view)
{
}



static void
keyboard_settings_delete_shortcut (GtkTreeView *tree_view)
{
  GtkTreeSelection *selection;
  XfconfChannel    *channel;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  GList            *rows;
  GList            *row_iter;
  GList            *row_references = NULL;
  gchar            *shortcut;
  gchar            *property_name;

  /* Get reference on the keyboard shortcuts channel */
  channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

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
          xfconf_channel_remove_property (channel, property_name);

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

  /* Release reference on the channel */
  g_object_unref (channel);
}



static void
keyboard_settings_shortcut_action_edited (GtkTreeView *tree_view,
                                          gchar       *path,
                                          gchar       *new_text)
                                          
{
  XfconfChannel *channel;
  GtkTreeModel  *model;
  GtkTreeIter    iter;
  gchar         *shortcut;
  gchar         *old_text;
  gchar         *property_name;

  /* Get reference on the keyboard shortcuts channel */
  channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  /* Get tree model */
  model = gtk_tree_view_get_model (tree_view);

  /* Get iter for the edited row */
  if (G_LIKELY (gtk_tree_model_get_iter_from_string (model, &iter, path)))
    {
      /* Read row values */
      gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &shortcut, ACTION_COLUMN, &old_text, -1);

      /* Check whether anything has changed at all */
      if (G_LIKELY (g_utf8_collate (old_text, new_text) != 0))
        {
          /* Upate row data with the new text */
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, ACTION_COLUMN, new_text, -1);

          /* Build xfconf property name */
          property_name = g_strdup_printf ("/%s", shortcut);

          /* Save new shortcut settings */
          xfconf_channel_set_array (channel, property_name, G_TYPE_STRING, "execute", G_TYPE_STRING, new_text, G_TYPE_INVALID);

          /* Free property name */
          g_free (property_name);
        }

      /* Free strings */
      g_free (shortcut);
      g_free (old_text);
    }

  /* Release reference on the channel */
  g_object_unref (channel);
}



static gboolean
keyboard_settings_validate_shortcut (ShortcutDialog *dialog,
                                     const gchar    *shortcut,
                                     GtkTreeView    *tree_view)
{
  GtkTreeSelection *selection;
  XfconfChannel    *channel;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gboolean          shortcut_accepted = TRUE;
  gchar            *current_shortcut;
  gchar            *property;

#if 1
  /* Ignore raw 'Return' since that may have been used to activate the shortcut row */
  if (G_UNLIKELY (g_utf8_collate (shortcut, "Return") == 0 
                  || g_utf8_collate (shortcut, "space") == 0))
    return FALSE;
#endif

  selection = gtk_tree_view_get_selection (tree_view);

  if (G_LIKELY (gtk_tree_selection_get_selected (selection, &model, &iter)))
    {
      gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &current_shortcut, -1);

      channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");
      property = g_strdup_printf ("/%s", shortcut);
      
      if (G_UNLIKELY (xfconf_channel_has_property (channel, property) && g_utf8_collate (current_shortcut, shortcut) != 0))
        {
          xfce_err (_("Keyboard shortcut '%s' is already being used for something else."), shortcut);
          shortcut_accepted = FALSE;
        }

      g_free (property);
      g_free (current_shortcut);

      g_object_unref (channel);
    }

  return shortcut_accepted;
}



static void
keyboard_settings_row_activated (GtkTreeView       *tree_view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column)
{
  XfconfChannel *channel;
  GtkTreeModel  *model;
  GtkTreeIter    iter;
  GtkWidget     *dialog;
  const gchar   *new_shortcut;
  gchar         *current_shortcut;
  gchar         *action;
  gchar         *old_property;
  gchar         *new_property;
  gint           response;

  if (G_UNLIKELY (column != gtk_tree_view_get_column (tree_view, SHORTCUT_COLUMN)))
    return;

  /* Get reference on the keyboard shortcuts channel */
  channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read current shortcut from the activated row */
      gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &current_shortcut, ACTION_COLUMN, &action, -1);

      /* Request a new shortcut from the user */
      dialog = shortcut_dialog_new (action);
      g_signal_connect (dialog, "validate-shortcut", G_CALLBACK (keyboard_settings_validate_shortcut), tree_view);
      response = shortcut_dialog_run (SHORTCUT_DIALOG (dialog), GTK_WIDGET (tree_view));

      if (G_LIKELY (response != GTK_RESPONSE_CANCEL))
        {
          /* Build property name */
          old_property = g_strdup_printf ("/%s", current_shortcut);

          /* Remove old shortcut from the settings */
          xfconf_channel_remove_property (channel, old_property);

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
              xfconf_channel_set_array (channel, new_property, G_TYPE_STRING, "execute", G_TYPE_STRING, action, G_TYPE_INVALID);

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

  /* Release xfconf channel */
  g_object_unref (channel);
}



GtkWidget*
keyboard_settings_dialog_new_from_xml (GladeXML *gxml)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkAdjustment   *net_cursor_blink_time_scale;
  GtkAdjustment   *xkb_key_repeat_delay_scale;
  GtkAdjustment   *xkb_key_repeat_rate_scale;
  GtkListStore    *list_store;
  GtkWidget       *kbd_shortcuts_view;
  GtkWidget       *net_cursor_blink_check;
  GtkWidget       *xkb_key_repeat_check;
  GtkWidget       *add_shortcut_button;
  GtkWidget       *delete_shortcut_button;
  GtkWidget       *dialog;

  net_cursor_blink_check = glade_xml_get_widget (gxml, "net_cursor_blink_check");
  net_cursor_blink_time_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "net_cursor_blink_time_scale")));
  xkb_key_repeat_check = glade_xml_get_widget (gxml, "xkb_key_repeat_check");
  xkb_key_repeat_delay_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "xkb_key_repeat_delay_scale")));
  xkb_key_repeat_rate_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "xkb_key_repeat_rate_scale")));
  kbd_shortcuts_view = glade_xml_get_widget (gxml, "kbd_shortcuts_view");
  add_shortcut_button = glade_xml_get_widget (gxml, "add_shortcut_button");
  delete_shortcut_button = glade_xml_get_widget (gxml, "delete_shortcut_button");

  g_signal_connect (net_cursor_blink_check, "toggled", G_CALLBACK (net_cursor_blink_toggled), gxml);
  g_signal_connect (xkb_key_repeat_check, "toggled", G_CALLBACK (xkb_key_repeat_toggled), gxml);

  /* XKB Settings */
  xfconf_g_property_bind (xkb_channel, "/Xkb/KeyRepeat", G_TYPE_BOOLEAN, G_OBJECT (xkb_key_repeat_check), "active");
  xfconf_g_property_bind (xkb_channel, "/Xkb/KeyRepeat/Rate", G_TYPE_INT, G_OBJECT (xkb_key_repeat_rate_scale), "value");
  xfconf_g_property_bind (xkb_channel, "/Xkb/KeyRepeat/Delay", G_TYPE_INT, G_OBJECT (xkb_key_repeat_delay_scale), "value");

  /* XSETTINGS */
  xfconf_g_property_bind (xsettings_channel, "/Net/CursorBlink", G_TYPE_BOOLEAN, G_OBJECT (net_cursor_blink_check), "active");
  xfconf_g_property_bind (xsettings_channel, "/Net/CursorBlinkTime", G_TYPE_INT, G_OBJECT (net_cursor_blink_time_scale), "value");

  /* Configure shortcuts tree view */
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
  g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
  g_signal_connect_swapped (renderer, "edited", G_CALLBACK (keyboard_settings_shortcut_action_edited), GTK_TREE_VIEW (kbd_shortcuts_view));
  column = gtk_tree_view_column_new_with_attributes (_("Command"), renderer, "text", ACTION_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Load keyboard shortcuts */
  keyboard_settings_load_shortcuts (kbd_channel, kbd_shortcuts_view, list_store);

  /* Connect to add/delete button signals */
  g_signal_connect_swapped (add_shortcut_button, "clicked", G_CALLBACK (keyboard_settings_add_shortcut), GTK_TREE_VIEW (kbd_shortcuts_view));
  g_signal_connect_swapped (delete_shortcut_button, "clicked", G_CALLBACK (keyboard_settings_delete_shortcut), GTK_TREE_VIEW (kbd_shortcuts_view));

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
      if (G_LIKELY (error))
        {
          /* print error */
          g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
          g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
          g_print ("\n");

          /* cleanup */
          g_error_free (error);
        }
      else
        {
          g_error ("Unable to open display.");
        }
        
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
  if (!xfconf_init (&error))
    {
      /* print error and exit */
      g_error ("Failed to connect to xfconf daemon: %s.", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }
  
  /* load channels */
  xsettings_channel = xfconf_channel_new ("xsettings");
  xkb_channel = xfconf_channel_new ("xkb");
  kbd_channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  /* Parse Glade XML */
  gxml = glade_xml_new_from_buffer (keyboard_dialog_glade, keyboard_dialog_glade_length, NULL, NULL);

  /* Create settings dialog and run it */
  dialog = keyboard_settings_dialog_new_from_xml (gxml);
  gtk_dialog_run(GTK_DIALOG(dialog));
  
  gtk_widget_destroy (dialog);
  
  g_object_unref (xsettings_channel);
  g_object_unref (xkb_channel);
  g_object_unref (kbd_channel);

  xfconf_shutdown();

  return EXIT_SUCCESS;
}
