/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 *               2008 Olivier Fourdan <olivier@xfce.org>
 * Portions based on xfkc
 * Copyright (c) 2007 Gauvain Pocentek <gauvainpocentek@gmail.com>
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

#include "xfce-keyboard-settings.h"
#include "command-dialog.h"
#include "gdk/gdk.h"

#include <libxfce4kbd-private/xfce-shortcut-dialog.h>
#include <libxfce4kbd-private/xfce-shortcuts-provider.h>
#include <libxfce4kbd-private/xfce-shortcuts.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <xkbcommon/xkbregistry.h>

#ifdef ENABLE_X11
#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define CUSTOM_BASE_PROPERTY "/commands/custom"



enum ComboName
{
  COMBO_MODELS,
  COMBO_LAYOUT_OPTION,
  COMBO_COMPOSE_KEY,
};

enum
{
  COMMAND_COLUMN,
  SHORTCUT_COLUMN,
  SNOTIFY_COLUMN,
  SHORTCUT_LABEL_COLUMN,
  TOOLTIP_COLUMN,
  N_COLUMNS
};

enum
{
  XKB_LAYOUTS_COMBO_DESCRIPTION = 0,
  XKB_LAYOUTS_COMBO_VALUE,
  XKB_LAYOUTS_COMBO_NUM_COLUMNS
};

enum
{
  XKB_TREE_LAYOUTS = 0,
  XKB_TREE_LAYOUTS_NAMES,
  XKB_TREE_VARIANTS,
  XKB_TREE_VARIANTS_NAMES,
  XKB_TREE_NUM_COLUMNS
};

enum
{
  XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION = 0,
  XKB_AVAIL_LAYOUTS_TREE_ID,
  XKB_AVAIL_LAYOUTS_TREE_NUM_COLUMNS
};

typedef enum
{
  MOVE_LAYOUT_UP,
  MOVE_LAYOUT_DOWN
} MOVE_LAYOUT_DIRECTION;

typedef struct
{
  gchar *name;
  gchar *description;
  GList *variants; /* LayoutVariant */
} Layout;

typedef struct
{
  gchar *name;
  gchar *description;
} LayoutVariant;


typedef struct _XfceKeyboardShortcutInfo XfceKeyboardShortcutInfo;

typedef void (*XfceKeyboardLayoutsComboInitFunc) (XfceKeyboardSettings *settings);

typedef void (*XfceKeyboardLayoutsComboChangedFunc) (GtkComboBox *combo,
                                                     XfceKeyboardSettings *settings);


static void
xfce_keyboard_settings_constructed (GObject *object);
static void
xfce_keyboard_settings_finalize (GObject *object);
static void
xfce_keyboard_settings_row_activated (GtkTreeView *tree_view,
                                      GtkTreePath *path,
                                      GtkTreeViewColumn *column,
                                      XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_initialize_shortcuts (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_load_shortcuts (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_edit_shortcut (XfceKeyboardSettings *settings,
                                      GtkTreeView *tree_view,
                                      GtkTreePath *path);
static void
xfce_keyboard_settings_edit_command (XfceKeyboardSettings *settings,
                                     GtkTreeView *tree_view,
                                     GtkTreePath *path);
static gboolean
xfce_keyboard_settings_validate_shortcut (XfceShortcutDialog *dialog,
                                          const gchar *shortcut,
                                          XfceKeyboardSettings *settings);
static XfceKeyboardShortcutInfo *
xfce_keyboard_settings_get_shortcut_info (XfceKeyboardSettings *settings,
                                          const gchar *shortcut);
static void
xfce_keyboard_settings_free_shortcut_info (XfceKeyboardShortcutInfo *info);
static void
xfce_keyboard_settings_shortcut_added (XfceShortcutsProvider *provider,
                                       const gchar *shortcut,
                                       XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_shortcut_removed (XfceShortcutsProvider *provider,
                                         const gchar *shortcut,
                                         XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_add_button_clicked (XfceKeyboardSettings *settings,
                                           GtkButton *button);
static void
xfce_keyboard_settings_edit_button_clicked (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_delete_button_clicked (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_reset_button_clicked (XfceKeyboardSettings *settings);

#ifdef ENABLE_X11
static gboolean
xfce_keyboard_settings_update_sensitive (GtkSwitch *widget,
                                         XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_system_default_cb (GtkSwitch *widget,
                                          gboolean state,
                                          XfceKeyboardSettings *settings);
#endif

static void
xfce_keyboard_settings_set_layout (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_init_layout (XfceKeyboardSettings *settings,
                                    gchar *default_layouts,
                                    gchar *default_variants);

static void
xfce_keyboard_settings_layouts_combo_populate (XfceKeyboardSettings *settings,
                                               enum ComboName combo_name,
                                               const gchar *combo_builder_name,
                                               XfceKeyboardLayoutsComboInitFunc combo_init_func,
                                               XfceKeyboardLayoutsComboChangedFunc combo_changed_func);
static void
xfce_keyboard_settings_layouts_combo_init (XfceKeyboardSettings *settings,
                                           const gchar *combo_name,
                                           const gchar *xfconf_prop_name,
                                           const gchar *default_value);
static void
xfce_keyboard_settings_layouts_combo_add (GtkListStore *store,
                                          const gchar *name,
                                          const gchar *description);
static void
xfce_keyboard_settings_layouts_combo_changed (GtkComboBox *combo,
                                              XfceKeyboardSettings *settings,
                                              const gchar *xfconf_prop_name);

static void
xfce_keyboard_settings_init_model (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_model_changed_cb (GtkComboBox *combo,
                                         XfceKeyboardSettings *settings);

static void
xfce_keyboard_settings_init_grpkey (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_grpkey_changed_cb (GtkComboBox *combo,
                                          XfceKeyboardSettings *settings);

static void
xfce_keyboard_settings_init_compkey (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_compkey_changed_cb (GtkComboBox *combo,
                                           XfceKeyboardSettings *settings);

static void
xfce_keyboard_settings_update_layout_buttons (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_edit_layout_button_cb (GtkWidget *widget,
                                              XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_add_layout_button_cb (GtkWidget *widget,
                                             XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_row_activated_cb (GtkTreeView *tree_view,
                                         GtkTreePath *path,
                                         GtkTreeViewColumn *column,
                                         XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_del_layout_button_cb (GtkWidget *widget,
                                             XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_layout_move (GtkWidget *widget,
                                    XfceKeyboardSettings *settings,
                                    MOVE_LAYOUT_DIRECTION direction);
static void
xfce_keyboard_settings_up_layout_button_cb (GtkWidget *widget,
                                            XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_down_layout_button_cb (GtkWidget *widget,
                                              XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_build_layout_list (XfceKeyboardSettings *settings);
static void
xfce_keyboard_settings_populate_layouts_treeview (XfceKeyboardSettings *settings);
static gchar **
xfce_keyboard_settings_layout_selection (XfceKeyboardSettings *settings,
                                         const gchar *layout,
                                         const gchar *variant);

static void
layout_free (Layout *layout);
static void
layout_variant_free (LayoutVariant *variant);

#ifdef ENABLE_X11
static gchar **
xfce_keyboard_settings_read_xkb_rules_names (GdkDisplay *display);
static gint
xfce_keyboard_settings_get_active_keyboard_group (GdkDisplay *display);
#endif


struct _XfceKeyboardSettingsPrivate
{
  XfceShortcutsProvider *provider;

  struct rxkb_context *xkb_registry;
  GHashTable *layouts;
  GList *layout_entries; /* Layout; owned by 'layouts' hashtable */

  GtkTreeStore *layout_selection_treestore;

  XfconfChannel *keyboards_channel;
  XfconfChannel *keyboard_layout_channel;
  XfconfChannel *xsettings_channel;

  gchar *cur_kb_model;
};

struct _XfceKeyboardShortcutInfo
{
  XfceShortcutsProvider *provider;
  XfceShortcut *shortcut;
};



G_DEFINE_TYPE_WITH_PRIVATE (XfceKeyboardSettings, xfce_keyboard_settings, GTK_TYPE_BUILDER)



static void
xfce_keyboard_settings_class_init (XfceKeyboardSettingsClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = xfce_keyboard_settings_constructed;
  gobject_class->finalize = xfce_keyboard_settings_finalize;
}



static void
xfce_keyboard_settings_init (XfceKeyboardSettings *settings)
{
  GError *error = NULL;

  settings->priv = xfce_keyboard_settings_get_instance_private (settings);

  settings->priv->keyboards_channel = xfconf_channel_new ("keyboards");
  settings->priv->keyboard_layout_channel = xfconf_channel_new ("keyboard-layout");
  settings->priv->xsettings_channel = xfconf_channel_new ("xsettings");

  settings->priv->provider = xfce_shortcuts_provider_new ("commands");
  g_signal_connect (settings->priv->provider, "shortcut-added",
                    G_CALLBACK (xfce_keyboard_settings_shortcut_added), settings);
  g_signal_connect (settings->priv->provider, "shortcut-removed",
                    G_CALLBACK (xfce_keyboard_settings_shortcut_removed), settings);

  if (gtk_builder_add_from_resource (GTK_BUILDER (settings), "/org/xfce/settings/keyboard-dialog.glade", &error) == 0)
    {
      g_error ("Failed to load the UI file: %s.", error->message);
      g_error_free (error);
    }
}



static void
xfce_keyboard_settings_layouts_combo_populate (XfceKeyboardSettings *settings,
                                               enum ComboName combo_name,
                                               const gchar *combo_builder_name,
                                               XfceKeyboardLayoutsComboInitFunc combo_init_func,
                                               XfceKeyboardLayoutsComboChangedFunc combo_changed_func)
{
  GtkListStore *list_store;
  GtkTreeIter iter;
  GObject *xkb_combo;
  GtkCellRenderer *renderer;

  list_store = gtk_list_store_new (XKB_LAYOUTS_COMBO_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), 0, GTK_SORT_ASCENDING);

  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter,
                      XKB_LAYOUTS_COMBO_DESCRIPTION, "-",
                      XKB_LAYOUTS_COMBO_VALUE, "", -1);

  if (combo_name == COMBO_MODELS)
    {
      for (struct rxkb_model *model = rxkb_model_first (settings->priv->xkb_registry);
           model != NULL;
           model = rxkb_model_next (model))
        {
          xfce_keyboard_settings_layouts_combo_add (list_store,
                                                    rxkb_model_get_name (model),
                                                    rxkb_model_get_description (model));
        }
    }
  else
    {
      const gchar *name;
      if (combo_name == COMBO_LAYOUT_OPTION)
        name = "grp";
      else if (combo_name == COMBO_COMPOSE_KEY)
        name = "Compose key";
      else
        g_assert_not_reached ();

      struct rxkb_option_group *group = NULL;
      for (group = rxkb_option_group_first (settings->priv->xkb_registry);
           group != NULL;
           group = rxkb_option_group_next (group))
        {
          if (g_strcmp0 (rxkb_option_group_get_name (group), name) == 0)
            break;
        }

      if (group != NULL)
        {
          for (struct rxkb_option *option = rxkb_option_first (group);
               option != NULL;
               option = rxkb_option_next (option))
            {
              xfce_keyboard_settings_layouts_combo_add (list_store,
                                                        rxkb_option_get_name (option),
                                                        rxkb_option_get_description (option));
            }
        }
    }

  xkb_combo = gtk_builder_get_object (GTK_BUILDER (settings), combo_builder_name);
  gtk_combo_box_set_model (GTK_COMBO_BOX (xkb_combo), GTK_TREE_MODEL (list_store));
  g_object_unref (G_OBJECT (list_store));

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (xkb_combo));
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (xkb_combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (xkb_combo), renderer, "text", 0);

  combo_init_func (settings);
  g_signal_connect (G_OBJECT (xkb_combo), "changed",
                    G_CALLBACK (combo_changed_func), settings);
}



static void
xfce_keyboard_settings_constructed (GObject *object)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GObject *xkb_key_repeat_rate;
  GObject *xkb_key_repeat_delay;
  GObject *net_cursor_blink_time;
  GtkListStore *list_store;
  GObject *xkb_key_repeat_check;
  GObject *xkb_key_repeat_box;
  GObject *net_cursor_blink_check;
  GObject *net_cursor_blink_box;
  GObject *kbd_shortcuts_view;
  GObject *xkb_numlock;
  GObject *button;
  GObject *xkb_use_system_default_switch;
  GObject *xkb_tab_layout_vbox;
  GObject *xkb_layout_view;
  GObject *xkb_layout_add_button;
  GObject *xkb_layout_edit_button;
  GObject *xkb_layout_delete_button;
  GObject *xkb_layout_up_button;
  GObject *xkb_layout_down_button;
  gchar *cur_layouts = NULL;
  gchar *cur_variants = NULL;

  settings->priv->xkb_registry = rxkb_context_new (RXKB_CONTEXT_NO_FLAGS);
  rxkb_context_parse_default_ruleset (settings->priv->xkb_registry);
  xfce_keyboard_settings_build_layout_list (settings);

  /* XKB settings */
  xkb_key_repeat_check = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_check");
  xkb_key_repeat_box = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_box");

  g_object_bind_property (G_OBJECT (xkb_key_repeat_check), "active",
                          G_OBJECT (xkb_key_repeat_box), "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat", G_TYPE_BOOLEAN, G_OBJECT (xkb_key_repeat_check), "active");

  xkb_key_repeat_rate = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_rate");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat/Rate", G_TYPE_INT, xkb_key_repeat_rate, "value");

  xkb_key_repeat_delay = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_key_repeat_delay");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat/Delay", G_TYPE_INT, xkb_key_repeat_delay, "value");

  xkb_numlock = gtk_builder_get_object (GTK_BUILDER (settings), "restore_numlock");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/RestoreNumlock", G_TYPE_BOOLEAN, xkb_numlock, "active");

  /* XSETTINGS */
  net_cursor_blink_check = gtk_builder_get_object (GTK_BUILDER (settings), "net_cursor_blink_check");
  net_cursor_blink_box = gtk_builder_get_object (GTK_BUILDER (settings), "net_cursor_blink_box");
  g_object_bind_property (G_OBJECT (net_cursor_blink_check), "active",
                          G_OBJECT (net_cursor_blink_box), "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  xfconf_g_property_bind (settings->priv->xsettings_channel, "/Net/CursorBlink", G_TYPE_BOOLEAN, G_OBJECT (net_cursor_blink_check), "active");

  net_cursor_blink_time = gtk_builder_get_object (GTK_BUILDER (settings), "net_cursor_blink_time");
  xfconf_g_property_bind (settings->priv->xsettings_channel, "/Net/CursorBlinkTime", G_TYPE_INT, net_cursor_blink_time, "value");

  /* Configure shortcuts tree view */
  kbd_shortcuts_view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (kbd_shortcuts_view)), GTK_SELECTION_MULTIPLE);
  g_signal_connect (kbd_shortcuts_view, "row-activated", G_CALLBACK (xfce_keyboard_settings_row_activated), settings);

  /* Create list store for keyboard shortcuts */
  list_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COMMAND_COLUMN, GTK_SORT_ASCENDING);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (kbd_shortcuts_view), TOOLTIP_COLUMN);
  gtk_tree_view_set_model (GTK_TREE_VIEW (kbd_shortcuts_view), GTK_TREE_MODEL (list_store));

  /* Create command column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Command"), renderer, "text", COMMAND_COLUMN, NULL);
  g_object_set (column, "expand", TRUE, "resizable", TRUE, NULL);
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_set_sort_column_id (column, COMMAND_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Create shortcut column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, "text", SHORTCUT_LABEL_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (column, SHORTCUT_LABEL_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Connect to add button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "add_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_add_button_clicked), settings);

  /* Connect to edit button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "edit_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_edit_button_clicked), settings);

  /* Connect to remove button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "delete_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_delete_button_clicked), settings);

  /* Connect to reset button */
  button = gtk_builder_get_object (GTK_BUILDER (settings), "reset_shortcuts_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_reset_button_clicked), settings);

  xfce_keyboard_settings_initialize_shortcuts (settings);
  xfce_keyboard_settings_load_shortcuts (settings);

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      gchar **xkb_rules_names = xfce_keyboard_settings_read_xkb_rules_names (gdk_display_get_default ());
      if (xkb_rules_names != NULL)
        {
          if (g_strv_length (xkb_rules_names) == 5)
            {
              settings->priv->cur_kb_model = g_strdup (xkb_rules_names[1]);
              cur_layouts = g_strdup (xkb_rules_names[2]);
              cur_variants = g_strdup (xkb_rules_names[3]);
            }
          g_strfreev (xkb_rules_names);
        }
    }
#endif

  if (cur_layouts == NULL)
    cur_layouts = g_strdup ("");
  if (cur_variants == NULL)
    cur_variants = g_strdup ("");

  /* Tab */
  xkb_tab_layout_vbox = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_tab_layout_vbox");
  gtk_widget_show (GTK_WIDGET (xkb_tab_layout_vbox));

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      /* Use system defaults, i.e., disable options */
      xkb_use_system_default_switch = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_use_system_default_switch");
      xfconf_g_property_bind (settings->priv->keyboard_layout_channel, "/Default/XkbDisable", G_TYPE_BOOLEAN,
                              (GObject *) xkb_use_system_default_switch, "active");
      g_signal_connect (G_OBJECT (xkb_use_system_default_switch),
                        "state-set",
                        G_CALLBACK (xfce_keyboard_settings_system_default_cb),
                        settings);
      xfce_keyboard_settings_update_sensitive (GTK_SWITCH (xkb_use_system_default_switch), settings);
    }
#endif

#ifdef ENABLE_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      GObject *use_system_defaults_box = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_tab_use_system_default_box");
      gtk_widget_hide (GTK_WIDGET (use_system_defaults_box));
    }
#endif

  /* Keyboard model combo */
  xfce_keyboard_settings_layouts_combo_populate (settings,
                                                 COMBO_MODELS,
                                                 "xkb_model_combo",
                                                 xfce_keyboard_settings_init_model,
                                                 xfce_keyboard_settings_model_changed_cb);
  /* Group key combo */
  xfce_keyboard_settings_layouts_combo_populate (settings,
                                                 COMBO_LAYOUT_OPTION,
                                                 "xkb_grpkey_combo",
                                                 xfce_keyboard_settings_init_grpkey,
                                                 xfce_keyboard_settings_grpkey_changed_cb);
  /* Compose key combo */
  xfce_keyboard_settings_layouts_combo_populate (settings,
                                                 COMBO_COMPOSE_KEY,
                                                 "xkb_composekey_combo",
                                                 xfce_keyboard_settings_init_compkey,
                                                 xfce_keyboard_settings_compkey_changed_cb);

  /* Keyboard layout/variant treeview */
  settings->priv->layout_selection_treestore = NULL;
  xkb_layout_view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (xkb_layout_view)), GTK_SELECTION_BROWSE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (xkb_layout_view), -1, _("Layout"), renderer, "text", XKB_TREE_LAYOUTS_NAMES, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (xkb_layout_view), -1, _("Variant"), renderer, "text", XKB_TREE_VARIANTS_NAMES, NULL);

  list_store = gtk_list_store_new (XKB_TREE_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (xkb_layout_view), GTK_TREE_MODEL (list_store));
  xfce_keyboard_settings_init_layout (settings, cur_layouts, cur_variants);
  g_signal_connect (G_OBJECT (xkb_layout_view), "row-activated", G_CALLBACK (xfce_keyboard_settings_row_activated_cb), settings);

  /* Layout buttons */
  xkb_layout_add_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_add_button");
  xkb_layout_edit_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_edit_button");
  xkb_layout_delete_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_delete_button");
  xkb_layout_up_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_up_button");
  xkb_layout_down_button = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_down_button");

  g_signal_connect (G_OBJECT (xkb_layout_add_button), "clicked", G_CALLBACK (xfce_keyboard_settings_add_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_edit_button), "clicked", G_CALLBACK (xfce_keyboard_settings_edit_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_delete_button), "clicked", G_CALLBACK (xfce_keyboard_settings_del_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_up_button), "clicked", G_CALLBACK (xfce_keyboard_settings_up_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_down_button), "clicked", G_CALLBACK (xfce_keyboard_settings_down_layout_button_cb), settings);

  xfce_keyboard_settings_update_layout_buttons (settings);

  g_free (cur_layouts);
  g_free (cur_variants);
}



static void
xfce_keyboard_settings_finalize (GObject *object)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);

  g_free (settings->priv->cur_kb_model);
  g_list_free (settings->priv->layout_entries);
  g_hash_table_destroy (settings->priv->layouts);
  rxkb_context_unref (settings->priv->xkb_registry);
  g_object_unref (G_OBJECT (settings->priv->provider));

  (*G_OBJECT_CLASS (xfce_keyboard_settings_parent_class)->finalize) (object);
}



XfceKeyboardSettings *
xfce_keyboard_settings_new (void)
{
  return g_object_new (XFCE_TYPE_KEYBOARD_SETTINGS, NULL);
}



GtkWidget *
xfce_keyboard_settings_create_dialog (XfceKeyboardSettings *settings)
{
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), NULL);
  return GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (settings), "keyboard-settings-dialog"));
}



#ifdef ENABLE_X11
GtkWidget *
xfce_keyboard_settings_create_plug (XfceKeyboardSettings *settings,
                                    gint socket_id)
{
  GtkWidget *plug;
  GObject *child;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), NULL);

  plug = gtk_plug_new (socket_id);
  gtk_widget_show (plug);

  child = gtk_builder_get_object (GTK_BUILDER (settings), "plug-child");
  xfce_widget_reparent (GTK_WIDGET (child), plug);
  gtk_widget_show (GTK_WIDGET (child));

  return plug;
}
#endif



static void
xfce_keyboard_settings_row_activated (GtkTreeView *tree_view,
                                      GtkTreePath *path,
                                      GtkTreeViewColumn *column,
                                      XfceKeyboardSettings *settings)
{
  if (column == gtk_tree_view_get_column (tree_view, SHORTCUT_COLUMN))
    xfce_keyboard_settings_edit_shortcut (settings, tree_view, path);
  else
    xfce_keyboard_settings_edit_command (settings, tree_view, path);
}



static void
xfce_keyboard_settings_initialize_shortcuts (XfceKeyboardSettings *settings)
{
  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
}



static void
_xfce_keyboard_settings_load_shortcut (XfceShortcut *shortcut,
                                       XfceKeyboardSettings *settings)
{
  GdkModifierType modifiers;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;
  GObject *tree_view;
  guint keyval;
  gchar *label, *tooltip;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (shortcut != NULL);

  DBG ("property = %s, shortcut = %s, command = %s, snotify = %s",
       shortcut->property_name, shortcut->shortcut,
       shortcut->command, shortcut->snotify ? "true" : "false");

  tree_view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  /* Get the shortcut label */
  gtk_accelerator_parse (shortcut->shortcut, &keyval, &modifiers);
  label = gtk_accelerator_get_label (keyval, modifiers);

  /* shell commands may contain markup characters */
  tooltip = g_markup_escape_text (shortcut->command, -1);

  gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (tree_model), &iter,
                      COMMAND_COLUMN, shortcut->command,
                      SHORTCUT_COLUMN, shortcut->shortcut,
                      SNOTIFY_COLUMN, shortcut->snotify,
                      SHORTCUT_LABEL_COLUMN, label,
                      TOOLTIP_COLUMN, tooltip,
                      -1);

  g_free (label);
  g_free (tooltip);
}



static void
xfce_keyboard_settings_load_shortcuts (XfceKeyboardSettings *settings)
{
  GList *shortcuts;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (settings->priv->provider));

  /* Get all (custom) command shortcuts */
  shortcuts = xfce_shortcuts_provider_get_shortcuts (settings->priv->provider);

  /* Load shortcuts one by one */
  g_list_foreach (shortcuts, (GFunc) _xfce_keyboard_settings_load_shortcut, settings);

  xfce_shortcuts_free (shortcuts);
}



static void
xfce_keyboard_settings_edit_shortcut (XfceKeyboardSettings *settings,
                                      GtkTreeView *tree_view,
                                      GtkTreePath *path)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *dialog;
  const gchar *new_shortcut;
  gchar *shortcut;
  gchar *command;
  gint response;
  gboolean snotify;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read current shortcut from the activated row */
      gtk_tree_model_get (model, &iter,
                          COMMAND_COLUMN, &command,
                          SHORTCUT_COLUMN, &shortcut,
                          SNOTIFY_COLUMN, &snotify, -1);

      DBG ("shortcut = %s, command = %s, snotify = %s", shortcut, command, snotify ? "true" : "false");

      /* Request a new shortcut from the user */
      dialog = xfce_shortcut_dialog_new ("commands", command, command);
      g_signal_connect (dialog, "validate-shortcut", G_CALLBACK (xfce_keyboard_settings_validate_shortcut), settings);
      response = xfce_shortcut_dialog_run (XFCE_SHORTCUT_DIALOG (dialog), gtk_widget_get_toplevel (GTK_WIDGET (tree_view)));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Remove old shortcut from the settings */
          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
          xfce_shortcuts_provider_reset_shortcut (settings->priv->provider, shortcut);

          /* Get the shortcut entered by the user */
          new_shortcut = xfce_shortcut_dialog_get_shortcut (XFCE_SHORTCUT_DIALOG (dialog));

          /* Save new shortcut to the settings */
          xfce_shortcuts_provider_set_shortcut (settings->priv->provider, new_shortcut, command, snotify);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (command);
      g_free (shortcut);
    }
}



static void
xfce_keyboard_settings_edit_command (XfceKeyboardSettings *settings,
                                     GtkTreeView *tree_view,
                                     GtkTreePath *path)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *dialog;
  const gchar *new_command;
  gchar *shortcut;
  gchar *command;
  gint response;
  gboolean snotify;
  gboolean new_snotify;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      gchar *shortcut_label;

      /* Read shortcut and current command from the activated row */
      gtk_tree_model_get (model, &iter,
                          COMMAND_COLUMN, &command,
                          SHORTCUT_COLUMN, &shortcut,
                          SHORTCUT_LABEL_COLUMN, &shortcut_label,
                          SNOTIFY_COLUMN, &snotify, -1);

      /* Request a new command from the user */
      dialog = command_dialog_new (shortcut_label, command, snotify);
      response = command_dialog_run (COMMAND_DIALOG (dialog), GTK_WIDGET (tree_view));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get the command entered by the user */
          new_command = command_dialog_get_command (COMMAND_DIALOG (dialog));
          new_snotify = command_dialog_get_snotify (COMMAND_DIALOG (dialog));

          if (g_strcmp0 (command, new_command) != 0
              || snotify != new_snotify)
            {
              /* Remove the row because we add new one from the shortcut-added signal */
              gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

              /* Save settings */
              xfce_shortcuts_provider_set_shortcut (settings->priv->provider, shortcut,
                                                    new_command, new_snotify);
            }
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (shortcut_label);
      g_free (shortcut);
      g_free (command);
    }
}



static gboolean
xfce_keyboard_settings_validate_shortcut (XfceShortcutDialog *dialog,
                                          const gchar *shortcut,
                                          XfceKeyboardSettings *settings)
{
  XfceKeyboardShortcutInfo *info;
  gboolean accepted = TRUE;
  gint response;

  g_return_val_if_fail (XFCE_IS_SHORTCUT_DIALOG (dialog), FALSE);
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);

  /* Ignore empty shortcuts */
  if (G_UNLIKELY (xfce_str_is_empty (shortcut)))
    return FALSE;

  /* Ignore raw 'Return' and 'space' since that may have been used to activate the shortcut row */
  if (G_UNLIKELY (g_utf8_collate (shortcut, "Return") == 0 || g_utf8_collate (shortcut, "space") == 0))
    return FALSE;

  DBG ("Validating shortcut = %s", shortcut);

  info = xfce_keyboard_settings_get_shortcut_info (settings, shortcut);

  if (G_UNLIKELY (info != NULL))
    {
      response = xfce_shortcut_conflict_dialog (GTK_WINDOW (dialog),
                                                xfce_shortcuts_provider_get_name (settings->priv->provider),
                                                xfce_shortcuts_provider_get_name (info->provider),
                                                shortcut,
                                                xfce_shortcut_dialog_get_action_name (dialog),
                                                info->shortcut->command,
                                                FALSE);

      if (G_UNLIKELY (response == GTK_RESPONSE_ACCEPT))
        {
          /* We want to use the shortcut with the new owner */
          DBG ("We want to use %s with %s", shortcut,
               xfce_shortcut_dialog_get_action_name (dialog));
          xfce_shortcuts_provider_reset_shortcut (info->provider, shortcut);

          /*Remove the shortcut manually from the treeview */
          xfce_keyboard_settings_shortcut_removed (settings->priv->provider,
                                                   shortcut,
                                                   settings);
        }
      else
        {
          /* We want to keep the old owner */
          DBG ("We want to keep using %s with %s", shortcut, info->shortcut->command);
          accepted = FALSE;
        }

      xfce_keyboard_settings_free_shortcut_info (info);
    }

  return accepted;
}



static XfceKeyboardShortcutInfo *
xfce_keyboard_settings_get_shortcut_info (XfceKeyboardSettings *settings,
                                          const gchar *shortcut)
{
  XfceKeyboardShortcutInfo *info = NULL;
  GList *iter;
  XfceShortcut *sc;
  GList *providers;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);

  DBG ("Looking for shortcut info for %s", shortcut);

  providers = xfce_shortcuts_provider_get_providers ();

  if (G_UNLIKELY (providers == NULL))
    return NULL;

  for (iter = providers; iter != NULL && info == NULL; iter = g_list_next (iter))
    {
      if (G_UNLIKELY (xfce_shortcuts_provider_has_shortcut (iter->data, shortcut)))
        {
          sc = xfce_shortcuts_provider_get_shortcut (iter->data, shortcut);

          if (G_LIKELY (sc != NULL))
            {
              info = g_new0 (XfceKeyboardShortcutInfo, 1);
              info->provider = g_object_ref (iter->data);
              info->shortcut = sc;
            }
        }
    }

  xfce_shortcuts_provider_free_providers (providers);

  return info;
}



static void
xfce_keyboard_settings_free_shortcut_info (XfceKeyboardShortcutInfo *info)
{
  g_object_unref (info->provider);
  xfce_shortcut_free (info->shortcut);
  g_free (info);
}



static void
xfce_keyboard_settings_shortcut_added (XfceShortcutsProvider *provider,
                                       const gchar *shortcut,
                                       XfceKeyboardSettings *settings)
{
  XfceShortcut *sc;
  GtkTreeModel *model;
  GObject *view;
  GtkTreeIter iter;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  sc = xfce_shortcuts_provider_get_shortcut (settings->priv->provider, shortcut);

  if (G_LIKELY (sc != NULL))
    {
      GdkModifierType modifiers;
      guint keyval;
      gchar *label, *tooltip;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);

      /* Get the shortcut label */
      gtk_accelerator_parse (sc->shortcut, &keyval, &modifiers);
      label = gtk_accelerator_get_label (keyval, modifiers);

      /* shell commands may contain markup characters */
      tooltip = g_markup_escape_text (sc->command, -1);

      DBG ("Add shortcut %s for command %s", shortcut, sc->command);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          SHORTCUT_COLUMN, shortcut,
                          COMMAND_COLUMN, sc->command,
                          SNOTIFY_COLUMN, sc->snotify,
                          SHORTCUT_LABEL_COLUMN, label,
                          TOOLTIP_COLUMN, tooltip,
                          -1);

      g_free (label);
      g_free (tooltip);
      xfce_shortcut_free (sc);
    }
}



static gboolean
_xfce_keyboard_settings_remove_shortcut (GtkTreeModel *model,
                                         GtkTreePath *path,
                                         GtkTreeIter *iter,
                                         const gchar *shortcut)
{
  gboolean finished = FALSE;
  gchar *row_shortcut;

  gtk_tree_model_get (model, iter, SHORTCUT_COLUMN, &row_shortcut, -1);

  if (G_UNLIKELY (g_utf8_collate (row_shortcut, shortcut) == 0))
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), iter);
      finished = TRUE;
    }

  g_free (row_shortcut);

  return finished;
}



static void
xfce_keyboard_settings_shortcut_removed (XfceShortcutsProvider *provider,
                                         const gchar *shortcut,
                                         XfceKeyboardSettings *settings)
{
  GtkTreeModel *model;
  GObject *view;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  DBG ("Remove shortcut %s from treeview", shortcut);

  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) _xfce_keyboard_settings_remove_shortcut,
                          (gpointer) shortcut);
}



static void
xfce_keyboard_settings_add_button_clicked (XfceKeyboardSettings *settings,
                                           GtkButton *button)
{
  GtkWidget *command_dialog;
  GtkWidget *shortcut_dialog;
  GtkWidget *parent;
  const gchar *shortcut;
  const gchar *command;
  gint response;
  gboolean snotify;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  /* Request a command from the user */
  command_dialog = command_dialog_new (NULL, NULL, FALSE);
  response = command_dialog_run (COMMAND_DIALOG (command_dialog), GTK_WIDGET (button));

  /* Abort if the dialog was cancelled */
  if (G_UNLIKELY (response == GTK_RESPONSE_OK))
    {
      /* Get the command */
      command = command_dialog_get_command (COMMAND_DIALOG (command_dialog));
      snotify = command_dialog_get_snotify (COMMAND_DIALOG (command_dialog));

      /* Hide the command dialog */
      gtk_widget_hide (command_dialog);

      /* Create shortcut dialog */
      shortcut_dialog = xfce_shortcut_dialog_new ("commands", command, command);
      g_signal_connect (shortcut_dialog, "validate-shortcut", G_CALLBACK (xfce_keyboard_settings_validate_shortcut), settings);

      /* Run shortcut dialog until a valid shortcut is entered or the dialog is cancelled */
      parent = gtk_widget_get_toplevel (GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (settings), "vbox5")));
      response = xfce_shortcut_dialog_run (XFCE_SHORTCUT_DIALOG (shortcut_dialog), parent);

      /* Only continue if the shortcut dialog succeeded */
      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get shortcut */
          shortcut = xfce_shortcut_dialog_get_shortcut (XFCE_SHORTCUT_DIALOG (shortcut_dialog));

          /* Save the new shortcut to xfconf */
          DBG ("Save shortcut %s with command %s to Xfconf", shortcut, command);
          xfce_shortcuts_provider_set_shortcut (settings->priv->provider, shortcut, command, snotify);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (shortcut_dialog);
    }

  /* Destroy the shortcut dialog */
  gtk_widget_destroy (command_dialog);
}



static void
xfce_keyboard_settings_edit_button_clicked (XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GObject *view;
  GList *rows;
  GList *row_iter;
  GList *row_references = NULL;

  DBG ("edit!");

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (row_iter = g_list_first (rows); row_iter != NULL; row_iter = g_list_next (row_iter))
    row_references = g_list_append (row_references, gtk_tree_row_reference_new (model, row_iter->data));

  for (row_iter = g_list_first (row_references); row_iter != NULL; row_iter = g_list_next (row_iter))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (row_iter->data);

      /* Conver tree path to tree iter */
      if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
        {
          GtkWidget *command_dialog;
          gboolean snotify;
          gchar *shortcut_label;
          gchar *shortcut;
          gchar *command;
          gint response;

          /* Read row values */
          gtk_tree_model_get (model, &iter,
                              SHORTCUT_LABEL_COLUMN, &shortcut_label,
                              SHORTCUT_COLUMN, &shortcut,
                              COMMAND_COLUMN, &command,
                              SNOTIFY_COLUMN, &snotify,
                              -1);

          DBG ("Edit shortcut %s / command %s", shortcut, command);

          /* Request a new command from the user */
          command_dialog = command_dialog_new (shortcut_label, command, snotify);
          response = command_dialog_run (COMMAND_DIALOG (command_dialog), GTK_WIDGET (view));

          /* Abort if the dialog was cancelled */
          if (G_UNLIKELY (response == GTK_RESPONSE_OK))
            {
              const gchar *new_command;
              GtkWidget *shortcut_dialog;
              gboolean new_snotify;
              GtkWidget *parent;

              /* Get the command */
              new_command = command_dialog_get_command (COMMAND_DIALOG (command_dialog));
              new_snotify = command_dialog_get_snotify (COMMAND_DIALOG (command_dialog));

              /* Hide the command dialog */
              gtk_widget_hide (command_dialog);

              /* Create shortcut dialog */
              shortcut_dialog =
                xfce_shortcut_dialog_new ("commands",
                                          new_command,
                                          new_command);

              g_signal_connect (shortcut_dialog, "validate-shortcut",
                                G_CALLBACK (xfce_keyboard_settings_validate_shortcut),
                                settings);

              /* Try to keep the window above as it grabs the keyboard, we don't
               * want users to wonder why the keyboard does not work in another
               * window */
              gtk_window_set_keep_above (GTK_WINDOW (shortcut_dialog), TRUE);

              /* Run shortcut dialog until a valid shortcut is entered or the dialog is cancelled */
              parent = gtk_widget_get_toplevel (GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (settings), "vbox5")));
              response =
                xfce_shortcut_dialog_run (XFCE_SHORTCUT_DIALOG (shortcut_dialog),
                                          parent);

              /* Only continue if the shortcut dialog succeeded */
              if (G_LIKELY (response == GTK_RESPONSE_OK))
                {
                  const gchar *new_shortcut;
                  gboolean test_new_shortcut;

                  /* Get shortcut */
                  new_shortcut =
                    xfce_shortcut_dialog_get_shortcut (XFCE_SHORTCUT_DIALOG (shortcut_dialog));
                  test_new_shortcut = (g_strcmp0 (shortcut, new_shortcut) != 0);
                  if (g_strcmp0 (command, new_command) != 0 || (test_new_shortcut) || snotify != new_snotify)
                    {
                      if (test_new_shortcut)
                        {
                          /* Remove the row because we add new one from the
                           * shortcut-added signal */
                          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

                          /* Remove old keyboard shortcut via xfconf */
                          xfce_shortcuts_provider_reset_shortcut (settings->priv->provider,
                                                                  shortcut);
                        }

                      /* Save settings */
                      xfce_shortcuts_provider_set_shortcut (settings->priv->provider,
                                                            new_shortcut,
                                                            new_command,
                                                            new_snotify);
                    }
                }

              /* Destroy the shortcut dialog */
              gtk_widget_destroy (shortcut_dialog);
            }

          g_free (shortcut_label);
          g_free (shortcut);
          g_free (command);
          gtk_widget_destroy (command_dialog);
        }

      gtk_tree_path_free (path);
    }

  /* Free row reference list */
  g_list_free_full (row_references, (GDestroyNotify) gtk_tree_row_reference_free);

  /* Free row list */
  g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);
}



static void
xfce_keyboard_settings_delete_button_clicked (XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GObject *view;
  GList *rows;
  GList *row_iter;
  GList *row_references = NULL;
  gchar *shortcut;

  DBG ("remove!");

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (row_iter = g_list_first (rows); row_iter != NULL; row_iter = g_list_next (row_iter))
    row_references = g_list_append (row_references, gtk_tree_row_reference_new (model, row_iter->data));

  for (row_iter = g_list_first (row_references); row_iter != NULL; row_iter = g_list_next (row_iter))
    {
      path = gtk_tree_row_reference_get_path (row_iter->data);

      /* Conver tree path to tree iter */
      if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
        {
          /* Read row values */
          gtk_tree_model_get (model, &iter, SHORTCUT_COLUMN, &shortcut, -1);

          DBG ("remove shortcut %s", shortcut);

          /* Remove keyboard shortcut via xfconf */
          xfce_shortcuts_provider_reset_shortcut (settings->priv->provider, shortcut);

          /* Free strings */
          g_free (shortcut);
        }

      gtk_tree_path_free (path);
    }

  /* Free row reference list */
  g_list_free_full (row_references, (GDestroyNotify) gtk_tree_row_reference_free);

  /* Free row list */
  g_list_free_full (rows, (GDestroyNotify) gtk_tree_path_free);
}



static void
xfce_keyboard_settings_reset_button_clicked (XfceKeyboardSettings *settings)
{
  gint response;
  GObject *view;
  GtkListStore *store;
  const gchar *text = _("This will reset all shortcuts to their default values. Do you really want to do this?");

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  response = xfce_message_dialog (NULL, _("Reset to Defaults"), "dialog-question", _("Reset to Defaults"),
                                  text, _("No"), GTK_RESPONSE_NO, _("Yes"), GTK_RESPONSE_YES,
                                  NULL);

  if (G_LIKELY (response == GTK_RESPONSE_YES))
    {
      view = gtk_builder_get_object (GTK_BUILDER (settings), "kbd_shortcuts_view");

      /* Clear out all the previous entries */
      store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
      gtk_list_store_clear (store);

      xfce_shortcuts_provider_reset_to_defaults (settings->priv->provider);
    }
}



#ifdef ENABLE_X11

static gboolean
xfce_keyboard_settings_update_sensitive (GtkSwitch *widget,
                                         XfceKeyboardSettings *settings)
{
  GObject *xkb_model_frame;
  GObject *xkb_layout_frame;
  GObject *xkb_grpkey_frame;
  GObject *xkb_compkey_frame;
  gboolean active;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);

  active = gtk_switch_get_active (widget);
  xkb_model_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_model_frame");
  xkb_layout_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_frame");
  xkb_grpkey_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_grpkey_frame");
  xkb_compkey_frame = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_composekey_frame");

  gtk_widget_set_sensitive (GTK_WIDGET (xkb_model_frame), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (xkb_layout_frame), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (xkb_grpkey_frame), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (xkb_compkey_frame), !active);

  return active;
}



static void
xfce_keyboard_settings_system_default_cb (GtkSwitch *widget,
                                          gboolean state,
                                          XfceKeyboardSettings *settings)
{
  GtkWidget *warning_dialog;
  gboolean use_system_defaults;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  use_system_defaults = xfce_keyboard_settings_update_sensitive (widget, settings);
  if (use_system_defaults)
    {
      warning_dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                               _("The system defaults will be "
                                                 "restored next time you log in."));
      gtk_window_set_title (GTK_WINDOW (warning_dialog), _("Warning"));
      gtk_dialog_run (GTK_DIALOG (warning_dialog));
      gtk_widget_destroy (warning_dialog);
    }
}

#endif /* ENABLE_X11 */



static void
xfce_keyboard_settings_set_layout (XfceKeyboardSettings *settings)
{
  GObject *view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *val_layout;
  gchar *val_variant;
  gchar *variants;
  gchar *layouts;
  gchar *tmp;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;
  gtk_tree_model_get (model, &iter,
                      XKB_TREE_LAYOUTS, &val_layout,
                      XKB_TREE_VARIANTS, &val_variant, -1);
  if (val_variant == NULL)
    val_variant = g_strdup ("");

  if (val_layout)
    {
      layouts = g_strdup (val_layout);
      g_free (val_layout);

      variants = g_strdup (val_variant);
      g_free (val_variant);
    }
  else
    {
      layouts = g_strdup ("");
      /* If the layout was NULL, we ignore the variant */
      variants = g_strdup ("");
      g_free (val_variant);
    }

  while (gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          XKB_TREE_LAYOUTS, &val_layout,
                          XKB_TREE_VARIANTS, &val_variant, -1);
      if (val_variant == NULL)
        val_variant = g_strdup ("");

      if (val_layout)
        {
          tmp = g_strconcat (layouts, ",", val_layout, NULL);
          g_free (val_layout);
          g_free (layouts);
          layouts = tmp;

          tmp = g_strconcat (variants, ",", val_variant, NULL);
          g_free (val_variant);
          g_free (variants);
          variants = tmp;
        }
      else
        {
          g_free (val_layout);
          g_free (val_variant);
        }
    }

  xfconf_channel_set_string (settings->priv->keyboard_layout_channel,
                             "/Default/XkbLayout", layouts);
  xfconf_channel_set_string (settings->priv->keyboard_layout_channel,
                             "/Default/XkbVariant", variants);

  g_free (layouts);
  g_free (variants);
}



static void
xfce_keyboard_settings_init_layout (XfceKeyboardSettings *settings,
                                    gchar *default_layouts,
                                    gchar *default_variants)
{
  GObject *view;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *val_layout;
  gchar *val_variant;
  gchar **cfg_layouts;
  gchar **layout_id;
  gchar **cfg_variants;
  gchar **variant_id;
  gint current_group = -1;
  gint group_id;

  val_layout = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbLayout", default_layouts);
  val_variant = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbVariant", default_variants);

  cfg_layouts = g_strsplit (val_layout, ",", 0);
  cfg_variants = g_strsplit (val_variant, ",", 0);

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    current_group = xfce_keyboard_settings_get_active_keyboard_group (gdk_display_get_default ());
#endif

  for (layout_id = cfg_layouts, variant_id = cfg_variants, group_id = 0; *layout_id != NULL; ++layout_id, ++group_id)
    {
      const gchar *layout_desc = NULL;
      const gchar *variant_desc = NULL;

      Layout *layout = g_hash_table_lookup (settings->priv->layouts, *layout_id);
      if (layout != NULL)
        {
          layout_desc = layout->description;

          for (GList *vp = layout->variants; vp != NULL; vp = vp->next)
            {
              LayoutVariant *variant = vp->data;
              if (strcmp (*variant_id, variant->name) == 0)
                {
                  variant_desc = variant->description;
                  break;
                }
            }
        }

      if (xfce_str_is_empty (layout_desc))
        layout_desc = *layout_id;
      if (xfce_str_is_empty (variant_desc))
        variant_desc = *variant_id;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          XKB_TREE_LAYOUTS, *layout_id,
                          XKB_TREE_LAYOUTS_NAMES, layout_desc,
                          XKB_TREE_VARIANTS, *variant_id,
                          XKB_TREE_VARIANTS_NAMES, variant_desc,
                          -1);
      if (current_group == group_id)
        gtk_tree_selection_select_iter (selection, &iter);

      if (*variant_id)
        variant_id++;
    }

  g_strfreev (cfg_layouts);
  g_strfreev (cfg_variants);
  g_free (val_layout);
  g_free (val_variant);
}



static void
xfce_keyboard_settings_layouts_combo_add (GtkListStore *store,
                                          const gchar *name,
                                          const gchar *description)
{
  GtkTreeIter iter;

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      XKB_LAYOUTS_COMBO_DESCRIPTION, description,
                      XKB_LAYOUTS_COMBO_VALUE, name, -1);
}

static void
xfce_keyboard_settings_layouts_combo_init (XfceKeyboardSettings *settings,
                                           const gchar *combo_name,
                                           const gchar *xfconf_prop_name,
                                           const gchar *default_value)
{
  GObject *view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *id;
  gchar *xfconf_prop_value;
  gboolean item;
  gboolean found = FALSE;

  view = gtk_builder_get_object (GTK_BUILDER (settings), combo_name);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));

  xfconf_prop_value = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, xfconf_prop_name, default_value);
  item = gtk_tree_model_get_iter_first (model, &iter);

  if (xfconf_prop_value == NULL || *xfconf_prop_value == 0)
    {
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (view), &iter);
      return;
    }

  while (item && !found)
    {
      gtk_tree_model_get (model, &iter, XKB_LAYOUTS_COMBO_VALUE, &id, -1);
      found = !strcmp (id, xfconf_prop_value);
      g_free (id);

      if (found)
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (view), &iter);
          break;
        }
      item = gtk_tree_model_iter_next (model, &iter);
    }
  g_free (xfconf_prop_value);
}

static void
xfce_keyboard_settings_init_model (XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_init (settings,
                                             "xkb_model_combo",
                                             "/Default/XkbModel",
                                             settings->priv->cur_kb_model);
}



static void
xfce_keyboard_settings_init_grpkey (XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_init (settings,
                                             "xkb_grpkey_combo",
                                             "/Default/XkbOptions/Group",
                                             NULL);
}



static void
xfce_keyboard_settings_init_compkey (XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_init (settings,
                                             "xkb_composekey_combo",
                                             "/Default/XkbOptions/Compose",
                                             NULL);
}



static void
xfce_keyboard_settings_row_activated_cb (GtkTreeView *tree_view,
                                         GtkTreePath *path,
                                         GtkTreeViewColumn *column,
                                         XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_edit_layout_button_cb (NULL, settings);
}



static void
xfce_keyboard_settings_layouts_combo_changed (GtkComboBox *combo,
                                              XfceKeyboardSettings *settings,
                                              const gchar *xfconf_prop_name)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *xfconf_prop_value;

  if (G_LIKELY (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)))
    {
      model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
      gtk_tree_model_get (model, &iter, XKB_LAYOUTS_COMBO_VALUE, &xfconf_prop_value, -1);
      xfconf_channel_set_string (settings->priv->keyboard_layout_channel,
                                 xfconf_prop_name, xfconf_prop_value);
      g_free (xfconf_prop_value);
    }
}


static void
xfce_keyboard_settings_model_changed_cb (GtkComboBox *combo,
                                         XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_changed (combo, settings,
                                                "/Default/XkbModel");
}



static void
xfce_keyboard_settings_grpkey_changed_cb (GtkComboBox *combo,
                                          XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_changed (combo, settings,
                                                "/Default/XkbOptions/Group");
}



static void
xfce_keyboard_settings_compkey_changed_cb (GtkComboBox *combo,
                                           XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layouts_combo_changed (combo, settings,
                                                "/Default/XkbOptions/Compose");
}



static void
xfce_keyboard_settings_update_layout_buttons (XfceKeyboardSettings *settings)
{
  GObject *view;
  GtkTreeModel *model;
  GObject *object;
  gint n_layouts;
  gint max_layouts;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  n_layouts = gtk_tree_model_iter_n_children (model, NULL);

#ifdef ENABLE_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    max_layouts = XkbNumKbdGroups;
  else
#endif
    {
      // Wayland itself doesn't have a limit, but the xkb_v1 keymap format
      // string, which Wayland uses, only supports a max of 4.  The v2 format
      // raises this cap to 32, but the Walyand protocol does not yet support
      // it.
      max_layouts = 4;
    }

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_add_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts < max_layouts));

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_delete_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts > 1));

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_up_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts > 1));

  object = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_down_button");
  gtk_widget_set_sensitive (GTK_WIDGET (object), (n_layouts > 1));
}



static void
xfce_keyboard_settings_edit_layout_button_cb (GtkWidget *widget,
                                              XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GObject *view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *current_layout;
  gchar *current_variant;
  gchar **layout_selection;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (G_LIKELY (gtk_tree_selection_get_selected (selection, &model, &iter)))
    {
      gtk_tree_model_get (model, &iter,
                          XKB_TREE_LAYOUTS, &current_layout,
                          XKB_TREE_VARIANTS, &current_variant,
                          -1);

      layout_selection =
        xfce_keyboard_settings_layout_selection (settings, current_layout, current_variant);
      if (layout_selection)
        {
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              XKB_TREE_LAYOUTS, layout_selection[0],
                              XKB_TREE_LAYOUTS_NAMES, layout_selection[1],
                              XKB_TREE_VARIANTS, layout_selection[2],
                              XKB_TREE_VARIANTS_NAMES, layout_selection[3],
                              -1);
          xfce_keyboard_settings_set_layout (settings);
          g_strfreev (layout_selection);
        }
      g_free (current_layout);
      g_free (current_variant);
    }
}



static void
xfce_keyboard_settings_add_layout_button_cb (GtkWidget *widget,
                                             XfceKeyboardSettings *settings)
{
  GObject *view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar **layout_selection;

  layout_selection = xfce_keyboard_settings_layout_selection (settings, NULL, NULL);
  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  if (layout_selection)
    {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          XKB_TREE_LAYOUTS, layout_selection[0],
                          XKB_TREE_LAYOUTS_NAMES, layout_selection[1],
                          XKB_TREE_VARIANTS, layout_selection[2],
                          XKB_TREE_VARIANTS_NAMES, layout_selection[3],
                          -1);
      xfce_keyboard_settings_update_layout_buttons (settings);
      xfce_keyboard_settings_set_layout (settings);
      g_strfreev (layout_selection);
    }
}



static void
xfce_keyboard_settings_del_layout_button_cb (GtkWidget *widget,
                                             XfceKeyboardSettings *settings)
{
  GObject *view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      GtkTreeIter iter2;

      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      if (gtk_tree_model_get_iter_first (model, &iter2))
        gtk_tree_selection_select_iter (selection, &iter2);

      xfce_keyboard_settings_update_layout_buttons (settings);
      xfce_keyboard_settings_set_layout (settings);
    }
}



static void
xfce_keyboard_settings_up_layout_button_cb (GtkWidget *widget,
                                            XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layout_move (widget, settings, MOVE_LAYOUT_UP);
}



static void
xfce_keyboard_settings_down_layout_button_cb (GtkWidget *widget,
                                              XfceKeyboardSettings *settings)
{
  xfce_keyboard_settings_layout_move (widget, settings, MOVE_LAYOUT_DOWN);
}



static void
xfce_keyboard_settings_layout_move (GtkWidget *widget,
                                    XfceKeyboardSettings *settings,
                                    MOVE_LAYOUT_DIRECTION direction)
{
  GObject *view;
  GtkTreeModel *model;
  GtkTreeIter iter_a, iter_b;
  GtkTreeSelection *selection;
  GtkTreePath *path;

  view = gtk_builder_get_object (GTK_BUILDER (settings), "xkb_layout_view");
  g_return_if_fail (GTK_IS_WIDGET (view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter_a));

  switch (direction)
    {
    case MOVE_LAYOUT_UP:
      path = gtk_tree_model_get_path (model, &iter_a);
      if (!gtk_tree_path_prev (path))
        {
          gtk_tree_path_free (path);
          return;
        }
      gtk_tree_model_get_iter (model, &iter_b, path);
      gtk_tree_path_free (path);
      break;

    case MOVE_LAYOUT_DOWN:
      iter_b = iter_a;
      if (!gtk_tree_model_iter_next (model, &iter_b))
        return;
      break;

    default:
      return;
    }

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter_a, &iter_b);
  xfce_keyboard_settings_set_layout (settings);
}



static void
xfce_keyboard_settings_populate_layouts_treeview (XfceKeyboardSettings *settings)
{
  GObject *treeview = gtk_builder_get_object (GTK_BUILDER (settings), "layout_selection_view");
  GtkTreeStore *treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));

  for (GList *lp = settings->priv->layout_entries; lp != NULL; lp = lp->next)
    {
      Layout *layout = lp->data;
      GtkTreeIter root_iter;

      gtk_tree_store_append (treestore, &root_iter, NULL);
      gtk_tree_store_set (treestore, &root_iter,
                          XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, layout->description,
                          XKB_AVAIL_LAYOUTS_TREE_ID, layout->name,
                          -1);

      for (GList *vp = layout->variants; vp != NULL; vp = vp->next)
        {
          LayoutVariant *variant = vp->data;
          GtkTreeIter iter;
          gchar *description;

          if (variant->description == NULL)
            description = g_strdup_printf ("%s (%s)", layout->name, variant->name);
          else
            description = NULL;

          gtk_tree_store_append (treestore, &iter, &root_iter);
          gtk_tree_store_set (treestore, &iter,
                              XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, variant->description != NULL ? variant->description : description,
                              XKB_AVAIL_LAYOUTS_TREE_ID, variant->name,
                              -1);

          g_free (description);
        }
    }
}


static void
xfce_keyboard_settings_layout_activate_cb (GtkTreeView *tree_view,
                                           GtkTreePath *path,
                                           GtkTreeViewColumn *column,
                                           GtkDialog *dialog)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}



static gchar **
xfce_keyboard_settings_layout_selection (XfceKeyboardSettings *settings,
                                         const gchar *edit_layout,
                                         const gchar *edit_variant)
{
  GObject *keyboard_layout_selection_dialog;
  GObject *layout_selection_view;
  GtkTreePath *path;
  GtkCellRenderer *renderer;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  gchar **val_layout;
  gchar *layout_desc;
  gchar *variant_desc;
  gchar *layout;
  gchar *variant;
  gint result;

  keyboard_layout_selection_dialog = gtk_builder_get_object (GTK_BUILDER (settings), "keyboard-layout-selection-dialog");
  layout_selection_view = gtk_builder_get_object (GTK_BUILDER (settings), "layout_selection_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (layout_selection_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  if (!settings->priv->layout_selection_treestore)
    {
      settings->priv->layout_selection_treestore = gtk_tree_store_new (XKB_AVAIL_LAYOUTS_TREE_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
      renderer = gtk_cell_renderer_text_new ();
      column = gtk_tree_view_column_new_with_attributes (NULL, renderer, "text",
                                                         XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (layout_selection_view), GTK_TREE_MODEL (settings->priv->layout_selection_treestore));
      gtk_tree_view_append_column (GTK_TREE_VIEW (layout_selection_view), column);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (settings->priv->layout_selection_treestore), 0, GTK_SORT_ASCENDING);
      xfce_keyboard_settings_populate_layouts_treeview (settings);
      g_signal_connect (GTK_TREE_VIEW (layout_selection_view), "row-activated",
                        G_CALLBACK (xfce_keyboard_settings_layout_activate_cb), keyboard_layout_selection_dialog);
      gtk_dialog_set_default_response (GTK_DIALOG (keyboard_layout_selection_dialog), GTK_RESPONSE_OK);
    }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (layout_selection_view));
  gtk_tree_view_collapse_all (GTK_TREE_VIEW (layout_selection_view));

  /* Selected and expand the layout/variant to be edited */
  if (edit_layout && g_strcmp0 (edit_layout, ""))
    {
      gboolean found;

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          gchar *tmp_layout;
          found = FALSE;

          do
            {
              gtk_tree_model_get (model, &iter, XKB_AVAIL_LAYOUTS_TREE_ID, &tmp_layout, -1);
              path = gtk_tree_model_get_path (model, &iter);

              if (found)
                break;

              if (g_strcmp0 (tmp_layout, edit_layout) == 0)
                {
                  if (edit_variant && g_strcmp0 (edit_variant, "") && gtk_tree_model_iter_has_child (model, &iter))
                    {
                      GtkTreeIter iter2;
                      gint n, i;

                      n = gtk_tree_model_iter_n_children (model, &iter);

                      for (i = 0; i < n; i++)
                        {
                          if (gtk_tree_model_iter_nth_child (model, &iter2, &iter, i))
                            {
                              gchar *tmp_variant;

                              gtk_tree_model_get (model, &iter2, XKB_AVAIL_LAYOUTS_TREE_ID, &tmp_variant, -1);

                              if (g_strcmp0 (tmp_variant, edit_variant) == 0)
                                {
                                  GtkTreePath *path2;

                                  path2 = gtk_tree_model_get_path (model, &iter2);

                                  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                                                path2, NULL,
                                                                TRUE, 0.5, 0);
                                  gtk_tree_view_expand_row (GTK_TREE_VIEW (layout_selection_view),
                                                            path,
                                                            TRUE);
                                  gtk_tree_selection_select_iter (selection, &iter2);

                                  found = TRUE;
                                  g_free (tmp_variant);
                                  gtk_tree_path_free (path2);
                                  break;
                                }

                              g_free (tmp_variant);
                            }
                        }
                    }
                  else
                    {
                      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                                    path, NULL,
                                                    TRUE, 0.5, 0);
                      gtk_tree_selection_select_iter (selection, &iter);
                      found = TRUE;
                      break;
                    }
                }

              gtk_tree_path_free (path);
              path = NULL;
              g_free (tmp_layout);
              tmp_layout = NULL;
            }
          while (gtk_tree_model_iter_next (model, &iter));
          g_free (tmp_layout);
          if (path)
            gtk_tree_path_free (path);

          if (!found)
            {
              /* We did not find the iter to be edited, fallback to the first one */
              if (gtk_tree_model_get_iter_first (model, &iter))
                {
                  path = gtk_tree_model_get_path (model, &iter);
                  gtk_tree_selection_select_iter (selection, &iter);

                  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                                path, NULL,
                                                TRUE, 0.5, 0);
                  gtk_tree_path_free (path);
                }
            }
        }
    }
  else
    {
      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          path = gtk_tree_model_get_path (model, &iter);
          gtk_tree_selection_select_iter (selection, &iter);

          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (layout_selection_view),
                                        path, NULL,
                                        TRUE, 0.5, 0);
          gtk_tree_path_free (path);
        }
    }

  val_layout = NULL;
  gtk_widget_show (GTK_WIDGET (keyboard_layout_selection_dialog));
  result = gtk_dialog_run (GTK_DIALOG (keyboard_layout_selection_dialog));
  if (result == GTK_RESPONSE_OK)
    {
      if (G_LIKELY (gtk_tree_selection_get_selected (selection, &model, &iter)))
        {
          gtk_tree_model_get (model, &iter, XKB_AVAIL_LAYOUTS_TREE_ID, &layout,
                              XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, &layout_desc, -1);

          path = gtk_tree_model_get_path (model, &iter);
          if (gtk_tree_path_get_depth (path) == 1)
            {
              variant = g_strdup ("");
              variant_desc = g_strdup ("");
            }
          else
            {
              variant = layout;
              variant_desc = layout_desc;
              gtk_tree_path_up (path);
              if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
                {
                  gtk_tree_model_get (model, &iter, XKB_AVAIL_LAYOUTS_TREE_ID, &layout,
                                      XKB_AVAIL_LAYOUTS_TREE_DESCRIPTION, &layout_desc, -1);
                }
            }

          val_layout = g_new0 (typeof (gchar *), 5);
          val_layout[0] = layout;
          val_layout[1] = layout_desc;
          val_layout[2] = variant;
          val_layout[3] = variant_desc;
          val_layout[4] = NULL;

          gtk_tree_path_free (path);
        }
    }

  gtk_widget_hide (GTK_WIDGET (keyboard_layout_selection_dialog));
  return val_layout;
}



static gint
layout_cmp (gconstpointer a,
            gconstpointer b)
{
  const Layout *la = a;
  const Layout *lb = b;
  return g_utf8_collate (la->description, lb->description);
}



static void
layout_free (Layout *layout)
{
  if (layout != NULL)
    {
      g_free (layout->name);
      g_free (layout->description);
      g_list_free_full (layout->variants, (GDestroyNotify) layout_variant_free);
      g_free (layout);
    }
}



static gint
layout_variant_cmp (gconstpointer a,
                    gconstpointer b)
{
  const LayoutVariant *va = a;
  const LayoutVariant *vb = b;
  return g_utf8_collate (va->description, vb->description);
}



static void
layout_variant_free (LayoutVariant *variant)
{
  if (variant != NULL)
    {
      g_free (variant->name);
      g_free (variant->description);
      g_free (variant);
    }
}


static void
xfce_keyboard_settings_build_layout_list (XfceKeyboardSettings *settings)
{
  GHashTable *layouts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) layout_free);

  for (struct rxkb_layout *layout = rxkb_layout_first (settings->priv->xkb_registry);
       layout != NULL;
       layout = rxkb_layout_next (layout))
    {
      const char *layout_name = rxkb_layout_get_name (layout);
      const char *variant_name = rxkb_layout_get_variant (layout);
      const char *raw_description = rxkb_layout_get_description (layout);
      gchar *description = raw_description != NULL ? g_utf8_make_valid (raw_description, -1) : NULL;

      Layout *entry = g_hash_table_lookup (layouts, layout_name);
      if (entry == NULL)
        {
          entry = g_new0 (Layout, 1);
          entry->name = g_strdup (layout_name);
          g_hash_table_insert (layouts, g_strdup (layout_name), entry);
        }

      if (xfce_str_is_empty (variant_name))
        {
          g_free (entry->description);
          entry->description = description;
        }
      else
        {
          LayoutVariant *variant = g_new0 (LayoutVariant, 1);
          variant->name = g_strdup (variant_name);
          variant->description = description;
          entry->variants = g_list_insert_sorted (entry->variants, variant, layout_variant_cmp);
        }
    }

  GList *entries = g_hash_table_get_values (layouts);
  settings->priv->layout_entries = g_list_sort (entries, layout_cmp);
  settings->priv->layouts = layouts;
}



#ifdef ENABLE_X11

static gchar **
xfce_keyboard_settings_read_xkb_rules_names (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_X11_DISPLAY (display), NULL);

  gchar **values = NULL;

  GdkWindow *root = gdk_screen_get_root_window (gdk_display_get_default_screen (display));

  GdkAtom string_atom = gdk_atom_intern ("STRING", FALSE);
  GdkAtom property_type;
  gint format = 0;
  gint length = 0;
  gchar *data = NULL;
  if (gdk_property_get (root,
                        gdk_atom_intern ("_XKB_RULES_NAMES", FALSE),
                        string_atom,
                        0,
                        G_MAXLONG,
                        FALSE,
                        &property_type,
                        &format,
                        &length,
                        (guchar **) &data)
      && property_type == string_atom
      && format == 8
      && length > 0
      && data != NULL)
    {
      GStrvBuilder *builder = g_strv_builder_new ();

      gchar *start = data;
      for (;;)
        {
          if (start - data >= length)
            break;

          gchar *cur = start;
          for (; cur - data < length && *cur != '\0'; cur++)
            ;
          if (cur - data < length && *cur == '\0')
            {
              g_strv_builder_add (builder, start);
              start = cur + 1;
            }
          else
            break;
        }

      values = g_strv_builder_end (builder);
      g_strv_builder_unref (builder);
    }

  g_free (data);

  return values;
}

static gint
xfce_keyboard_settings_get_active_keyboard_group (GdkDisplay *display)
{
  XkbStateRec rec;

  if (XkbGetState (gdk_x11_display_get_xdisplay (display), XkbUseCoreKbd, &rec) == Success)
    return rec.group;
  else
    return -1;
}

#endif /* ENABLE_X11 */
