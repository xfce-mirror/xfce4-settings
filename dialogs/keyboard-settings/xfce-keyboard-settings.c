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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <exo/exo.h>
#include <xfconf/xfconf.h>
#include <libxfcegui4/libxfcegui4.h>

#include "keyboard-dialog_glade.h"
#include "xfce-keyboard-settings.h"
#include "command-dialog.h"
#include "frap-shortcuts-dialog.h"
#include "frap-shortcuts-provider.h"

#ifdef HAVE_LIBXKLAVIER
#include <libxklavier/xklavier.h>
#endif /* HAVE_LIBXKLAVIER */

#define DEFAULT_BASE_PROPERTY        "/commands/default"
#define CUSTOM_BASE_PROPERTY         "/commands/custom"
#define DEFAULT_BASE_PROPERTY_FORMAT "/%s/default"
#define CUSTOM_BASE_PROPERTY_FORMAT  "/%s/custom"

#define COMMAND_COLUMN  0
#define SHORTCUT_COLUMN 1



#define XFCE_KEYBOARD_SETTINGS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), XFCE_TYPE_KEYBOARD_SETTINGS, XfceKeyboardSettingsPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_GLADE_XML,
};


enum
{
    DESC = 0,
    NOM,
    COMBO_NUM
};

enum
{
    LAYOUTS = 0,
    VARIANTS,
    TREE_NUM
};

enum
{
    AVAIL_LAYOUT_TREE_COL_DESCRIPTION = 0,
    AVAIL_LAYOUT_TREE_COL_ID,
    NUM
};



typedef struct _XfceKeyboardShortcutInfo    XfceKeyboardShortcutInfo;



static void                      xfce_keyboard_settings_class_init            (XfceKeyboardSettingsClass *klass);
static void                      xfce_keyboard_settings_init                  (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_constructed           (GObject                   *object);
static void                      xfce_keyboard_settings_finalize              (GObject                   *object);
static void                      xfce_keyboard_settings_get_property          (GObject                   *object,
                                                                               guint                      prop_id,
                                                                               GValue                    *value,
                                                                               GParamSpec                *pspec);
static void                      xfce_keyboard_settings_set_property          (GObject                   *object,
                                                                               guint                      prop_id,
                                                                               const GValue              *value,
                                                                               GParamSpec                *pspec);
static void                      xfce_keyboard_settings_row_activated         (GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path,
                                                                               GtkTreeViewColumn         *column,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_initialize_shortcuts  (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_load_shortcuts        (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_edit_shortcut         (XfceKeyboardSettings      *settings,
                                                                               GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path);
static void                      xfce_keyboard_settings_edit_command          (XfceKeyboardSettings      *settings,
                                                                               GtkTreeView               *tree_view,
                                                                               GtkTreePath               *path);
static gboolean                  xfce_keyboard_settings_validate_shortcut     (FrapShortcutsDialog       *dialog,
                                                                               const gchar               *shortcut,
                                                                               XfceKeyboardSettings      *settings);
static XfceKeyboardShortcutInfo *xfce_keyboard_settings_get_shortcut_info     (XfceKeyboardSettings      *settings,
                                                                               const gchar               *shortcut,
                                                                               const gchar               *ignore_property);
static void                      xfce_keyboard_settings_free_shortcut_info    (XfceKeyboardShortcutInfo  *info);
static void                      xfce_keyboard_settings_shortcut_added        (FrapShortcutsProvider     *provider,
                                                                               const gchar               *shortcut,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_shortcut_removed      (FrapShortcutsProvider     *provider,
                                                                               const gchar               *shortcut,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_button_clicked    (XfceKeyboardSettings      *settings,
                                                                               GtkButton                 *button);
static void                      xfce_keyboard_settings_delete_button_clicked (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_reset_button_clicked  (XfceKeyboardSettings      *settings);

#ifdef HAVE_LIBXKLAVIER

static gchar *                   xfce_keyboard_settings_model_description     (XklConfigItem             *config_item);
static void                      xfce_keyboard_settings_set_layout            (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_init_layout           (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_model_to_combo    (XklConfigRegistry         *config_registry,
                                                                               XklConfigItem             *config_item,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_init_model            (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_cb_model_changed      (GtkComboBox               *combo,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_update_layout_buttons (XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_update_edit_button    (GtkTreeView               *tree_view,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_del_layout_button_cb         (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_update_edit_button    (GtkTreeView               *tree_view,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_edit_layout_button_cb (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_layout_button_cb  (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_del_layout_button_cb  (GtkWidget                 *widget,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_variant_to_list   (XklConfigRegistry         *config_registry,
                                                                               XklConfigItem             *config_item,
                                                                               XfceKeyboardSettings      *settings);
static void                      xfce_keyboard_settings_add_layout_to_list    (XklConfigRegistry         *config_registry,
                                                                               XklConfigItem             *config_item,
                                                                               XfceKeyboardSettings      *settings);
static gchar *                   xfce_keyboard_settings_layout_selection      (XfceKeyboardSettings      *settings);

#endif /* HAVE_LIBXKLAVIER */



struct _XfceKeyboardSettingsPrivate
{
  GladeXML              *glade_xml;

  FrapShortcutsProvider *provider;

#ifdef HAVE_LIBXKLAVIER
  XklEngine             *xkl_engine;
  XklConfigRegistry     *xkl_registry;
  XklConfigRec          *xkl_rec_config;
  GtkTreeIter            layout_selection_iter;
  GtkTreeStore          *layout_selection_treestore;
#endif

  XfconfChannel         *keyboards_channel;
  XfconfChannel         *keyboard_layout_channel;
  XfconfChannel         *xsettings_channel;
};

struct _XfceKeyboardShortcutInfo
{
  FrapShortcutsProvider *provider;
  FrapShortcut          *shortcut;
};



static GObjectClass *xfce_keyboard_settings_parent_class = NULL;



GType
xfce_keyboard_settings_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (XfceKeyboardSettingsClass),
        NULL,
        NULL,
        (GClassInitFunc) xfce_keyboard_settings_class_init,
        NULL,
        NULL,
        sizeof (XfceKeyboardSettings),
        0,
        (GInstanceInitFunc) xfce_keyboard_settings_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "XfceKeyboardSettings", &info, 0);
    }

  return type;
}



static void
xfce_keyboard_settings_class_init (XfceKeyboardSettingsClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (XfceKeyboardSettingsPrivate));

  /* Determine the parent type class */
  xfce_keyboard_settings_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
#if GLIB_CHECK_VERSION (2,12,0)
  gobject_class->constructed = xfce_keyboard_settings_constructed;
#endif
  gobject_class->finalize = xfce_keyboard_settings_finalize;
  gobject_class->get_property = xfce_keyboard_settings_get_property;
  gobject_class->set_property = xfce_keyboard_settings_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_GLADE_XML,
                                   g_param_spec_object ("glade-xml",
                                                        "glade-xml",
                                                        "glade-xml",
                                                        GLADE_TYPE_XML,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}



static void
xfce_keyboard_settings_init (XfceKeyboardSettings *settings)
{
  settings->priv = XFCE_KEYBOARD_SETTINGS_GET_PRIVATE (settings);
  settings->priv->glade_xml = NULL;

  settings->priv->keyboards_channel = xfconf_channel_new ("keyboards");
  settings->priv->keyboard_layout_channel = xfconf_channel_new ("keyboard-layout");
  settings->priv->xsettings_channel = xfconf_channel_new ("xsettings");

  settings->priv->provider = frap_shortcuts_provider_new ("commands");
  g_signal_connect (settings->priv->provider, "shortcut-added",
                    G_CALLBACK (xfce_keyboard_settings_shortcut_added), settings);
  g_signal_connect (settings->priv->provider, "shortcut-removed",
                    G_CALLBACK (xfce_keyboard_settings_shortcut_removed), settings);
}



static void
xfce_keyboard_settings_constructed (GObject *object)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);
  GtkTreeViewColumn    *column;
  GtkCellRenderer      *renderer;
  GtkAdjustment        *xkb_key_repeat_rate_scale;
  GtkAdjustment        *xkb_key_repeat_delay_scale;
  GtkAdjustment        *net_cursor_blink_time_scale;
  GtkListStore         *list_store;
  GtkWidget            *xkb_key_repeat_check;
  GtkWidget            *xkb_key_repeat_box;
  GtkWidget            *net_cursor_blink_check;
  GtkWidget            *net_cursor_blink_box;
  GtkWidget            *kbd_shortcuts_view;
  GtkWidget            *button;
#ifdef HAVE_LIBXKLAVIER
  GtkWidget            *xkb_tab_layout_vbox;
  GtkWidget            *xkb_model_combo;
  GtkWidget            *xkb_layout_view;
  GtkWidget            *xkb_layout_add_button;
  GtkWidget            *xkb_layout_edit_button;
  GtkWidget            *xkb_layout_delete_button;
#endif /* HAVE_LIBXKLAVIER */

  /* XKB settings */
  xkb_key_repeat_check = glade_xml_get_widget (settings->priv->glade_xml, "xkb_key_repeat_check");
  xkb_key_repeat_box = glade_xml_get_widget (settings->priv->glade_xml, "xkb_key_repeat_box");
  exo_binding_new (G_OBJECT (xkb_key_repeat_check), "active", G_OBJECT (xkb_key_repeat_box), "sensitive");
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat", G_TYPE_BOOLEAN, G_OBJECT (xkb_key_repeat_check), "active");

  xkb_key_repeat_rate_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (settings->priv->glade_xml, "xkb_key_repeat_rate_scale")));
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/Default/KeyRepeat/Rate", G_TYPE_INT, G_OBJECT (xkb_key_repeat_rate_scale), "value");

  xkb_key_repeat_delay_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (settings->priv->glade_xml, "xkb_key_repeat_delay_scale")));
  xfconf_g_property_bind (settings->priv->keyboards_channel, "/DefaultKeyRepeat/Delay", G_TYPE_INT, G_OBJECT (xkb_key_repeat_delay_scale), "value");

  /* XSETTINGS */
  net_cursor_blink_check = glade_xml_get_widget (settings->priv->glade_xml, "net_cursor_blink_check");
  net_cursor_blink_box = glade_xml_get_widget (settings->priv->glade_xml, "net_cursor_blink_box");
  exo_binding_new (G_OBJECT (net_cursor_blink_check), "active", G_OBJECT (net_cursor_blink_box), "sensitive");
  xfconf_g_property_bind (settings->priv->xsettings_channel, "/Net/CursorBlink", G_TYPE_BOOLEAN, G_OBJECT (net_cursor_blink_check), "active");

  net_cursor_blink_time_scale = gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (settings->priv->glade_xml, "net_cursor_blink_time_scale")));
  xfconf_g_property_bind (settings->priv->xsettings_channel, "/NetCursorBlinkTime", G_TYPE_INT, G_OBJECT (net_cursor_blink_time_scale), "value");

  /* Configure shortcuts tree view */
  kbd_shortcuts_view = glade_xml_get_widget (settings->priv->glade_xml, "kbd_shortcuts_view");
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (kbd_shortcuts_view)), GTK_SELECTION_MULTIPLE);
  g_signal_connect (kbd_shortcuts_view, "row-activated", G_CALLBACK (xfce_keyboard_settings_row_activated), settings);

  /* Create list store for keyboard shortcuts */
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COMMAND_COLUMN, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (kbd_shortcuts_view), GTK_TREE_MODEL (list_store));

  /* Create command column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Command"), renderer, "text", COMMAND_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Create shortcut column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, "text", SHORTCUT_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (kbd_shortcuts_view), column);

  /* Connect to add button */
  button = glade_xml_get_widget (settings->priv->glade_xml, "add_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_add_button_clicked), settings);

  /* Connect to remove button */
  button = glade_xml_get_widget (settings->priv->glade_xml, "delete_shortcut_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_delete_button_clicked), settings);

  /* Connect to reset button */
  button = glade_xml_get_widget (settings->priv->glade_xml, "reset_shortcuts_button");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (xfce_keyboard_settings_reset_button_clicked), settings);

  xfce_keyboard_settings_initialize_shortcuts (settings);
  xfce_keyboard_settings_load_shortcuts (settings);

  /* TODO: Handle property-changed signal */
  /* TODO: Handle 'Add' and 'Remove' buttons */

#ifdef HAVE_LIBXKLAVIER
  /* Init engine */
  settings->priv->xkl_engine = xkl_engine_get_instance (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  settings->priv->xkl_rec_config = xkl_config_rec_new ();
  settings->priv->xkl_registry = xkl_config_registry_get_instance (settings->priv->xkl_engine);
  xkl_config_registry_load (settings->priv->xkl_registry);
  xkl_config_rec_get_from_server (settings->priv->xkl_rec_config, settings->priv->xkl_engine);

  /* Tab */
  xkb_tab_layout_vbox = glade_xml_get_widget (settings->priv->glade_xml, "xkb_tab_layout_vbox");
  gtk_widget_show (GTK_WIDGET (xkb_tab_layout_vbox));

  /* Keyboard model combo */
  xkb_model_combo = glade_xml_get_widget (settings->priv->glade_xml, "xkb_model_combo");
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (xkb_model_combo));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (xkb_model_combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (xkb_model_combo), renderer, "text", 0);
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (xkb_model_combo), GTK_TREE_MODEL (list_store));
  xkl_config_registry_foreach_model (settings->priv->xkl_registry,
                                     (ConfigItemProcessFunc) xfce_keyboard_settings_add_model_to_combo,
                                     settings);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), 0, GTK_SORT_ASCENDING);
  xfce_keyboard_settings_init_model (settings);
  g_signal_connect (G_OBJECT (xkb_model_combo),
                    "changed",
                    G_CALLBACK (xfce_keyboard_settings_cb_model_changed),
                    settings);

  /* Keyboard layout/variant treeview */
  settings->priv->layout_selection_treestore = NULL;
  xkb_layout_view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (xkb_layout_view), -1, _("Layout"), renderer, "text", LAYOUTS, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (xkb_layout_view), -1, _("Variant"), renderer, "text", VARIANTS, NULL);
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (xkb_layout_view), GTK_TREE_MODEL (list_store));
  xfce_keyboard_settings_init_layout (settings);
  g_signal_connect (G_OBJECT (xkb_layout_view), "cursor-changed", G_CALLBACK (xfce_keyboard_settings_update_edit_button), settings);

  /* Layout buttons */
  xkb_layout_add_button = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_add_button");
  xkb_layout_edit_button = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_edit_button");
  xkb_layout_delete_button = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_delete_button");

  g_signal_connect (G_OBJECT (xkb_layout_add_button),    "clicked", G_CALLBACK (xfce_keyboard_settings_add_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_edit_button),   "clicked", G_CALLBACK (xfce_keyboard_settings_edit_layout_button_cb), settings);
  g_signal_connect (G_OBJECT (xkb_layout_delete_button), "clicked", G_CALLBACK (xfce_keyboard_settings_del_layout_button_cb), settings);

  xfce_keyboard_settings_update_edit_button (GTK_TREE_VIEW (xkb_layout_view), settings);
  xfce_keyboard_settings_update_layout_buttons (settings);
#endif /* HAVE_LIBXKLAVIER */
}



static void
xfce_keyboard_settings_finalize (GObject *object)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);

  g_object_unref (settings->priv->provider);

  (*G_OBJECT_CLASS (xfce_keyboard_settings_parent_class)->finalize) (object);
}



static void
xfce_keyboard_settings_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_GLADE_XML:
      g_value_set_object (value, settings->priv->glade_xml);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_keyboard_settings_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  XfceKeyboardSettings *settings = XFCE_KEYBOARD_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_GLADE_XML:
      if (GLADE_IS_XML (settings->priv->glade_xml))
        g_object_unref (settings->priv->glade_xml);

      settings->priv->glade_xml = g_value_get_object (value);

      g_object_notify (object, "glade-xml");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



XfceKeyboardSettings *
xfce_keyboard_settings_new (void)
{
  XfceKeyboardSettings *settings = NULL;
  GladeXML             *glade_xml;

  glade_xml = glade_xml_new_from_buffer (keyboard_dialog_glade, keyboard_dialog_glade_length, NULL, NULL);

  if (G_LIKELY (glade_xml != NULL))
    settings = g_object_new (XFCE_TYPE_KEYBOARD_SETTINGS, "glade-xml", glade_xml, NULL);

#if !GLIB_CHECK_VERSION (2,12,0)
  xfce_keyboard_settings_constructed (G_OBJECT (settings));
#endif

  return settings;
}



GtkWidget *
xfce_keyboard_settings_create_dialog (XfceKeyboardSettings *settings)
{
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), NULL);
  return glade_xml_get_widget (settings->priv->glade_xml, "keyboard-settings-dialog");
}



GtkWidget *
xfce_keyboard_settings_create_plug (XfceKeyboardSettings *settings,
                                    GdkNativeWindow       socket_id)
{
  GtkWidget *plug;
  GtkWidget *child;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), NULL);

  plug = gtk_plug_new (socket_id);
  gtk_widget_show (plug);

  child = glade_xml_get_widget (settings->priv->glade_xml, "plug-child");
  gtk_widget_reparent (child, plug);
  gtk_widget_show (child);

  return plug;
}



static void
xfce_keyboard_settings_row_activated (GtkTreeView          *tree_view,
                                      GtkTreePath          *path,
                                      GtkTreeViewColumn    *column,
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
_xfce_keyboard_settings_load_shortcut (FrapShortcut         *shortcut,
                                       XfceKeyboardSettings *settings)
{
  GtkTreeModel *tree_model;
  GtkTreeIter   iter;
  GtkWidget    *tree_view;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (shortcut != NULL);

  DBG ("property = %s, shortcut = %s, command = %s", shortcut->property_name, shortcut->shortcut, shortcut->command);

  tree_view = glade_xml_get_widget (settings->priv->glade_xml, "kbd_shortcuts_view");
  tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (tree_model), &iter, COMMAND_COLUMN, shortcut->command, SHORTCUT_COLUMN, shortcut->shortcut, -1);
}



static void
xfce_keyboard_settings_load_shortcuts (XfceKeyboardSettings *settings)
{
  GList *shortcuts;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (settings->priv->provider));

  /* Get all (custom) command shortcuts */
  shortcuts = frap_shortcuts_provider_get_shortcuts (settings->priv->provider);

  /* Load shortcuts one by one */
  g_list_foreach (shortcuts, (GFunc) _xfce_keyboard_settings_load_shortcut, settings);

  frap_shortcuts_free_shortcuts (shortcuts);
}



static void
xfce_keyboard_settings_edit_shortcut (XfceKeyboardSettings *settings,
                                      GtkTreeView          *tree_view,
                                      GtkTreePath          *path)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkWidget    *dialog;
  const gchar  *new_shortcut;
  gchar        *shortcut;
  gchar        *command;
  gint          response;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read current shortcut from the activated row */
      gtk_tree_model_get (model, &iter, COMMAND_COLUMN, &command, SHORTCUT_COLUMN, &shortcut, -1);

      DBG ("shortcut = %s, command = %s", shortcut, command);

      /* Request a new shortcut from the user */
      dialog = frap_shortcuts_dialog_new ("commands", command);
      g_signal_connect (dialog, "validate-shortcut", G_CALLBACK (xfce_keyboard_settings_validate_shortcut), settings);
      response = frap_shortcuts_dialog_run (FRAP_SHORTCUTS_DIALOG (dialog));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Remove old shortcut from the settings */
          frap_shortcuts_provider_reset_shortcut (settings->priv->provider, shortcut);

          /* Get the shortcut entered by the user */
          new_shortcut = frap_shortcuts_dialog_get_shortcut (FRAP_SHORTCUTS_DIALOG (dialog));

          /* Save new shortcut to the settings */
          frap_shortcuts_provider_set_shortcut (settings->priv->provider, new_shortcut, command);
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
                                     GtkTreeView          *tree_view,
                                     GtkTreePath          *path)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkWidget    *dialog;
  const gchar  *new_command;
  gchar        *shortcut;
  gchar        *command;
  gint          response;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  /* Get tree view model */
  model = gtk_tree_view_get_model (tree_view);

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read shortcut and current command from the activated row */
      gtk_tree_model_get (model, &iter, COMMAND_COLUMN, &command, SHORTCUT_COLUMN, &shortcut, -1);

      /* Request a new command from the user */
      dialog = command_dialog_new (shortcut, command);
      response = command_dialog_run (COMMAND_DIALOG (dialog), GTK_WIDGET (tree_view));

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get the command entered by the user */
          new_command = command_dialog_get_command (COMMAND_DIALOG (dialog));

          /* Save settings */
          frap_shortcuts_provider_set_shortcut (settings->priv->provider, shortcut, new_command);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (shortcut);
      g_free (command);
    }
}



static gboolean
xfce_keyboard_settings_validate_shortcut (FrapShortcutsDialog  *dialog,
                                          const gchar          *shortcut,
                                          XfceKeyboardSettings *settings)
{
  XfceKeyboardShortcutInfo *info;
  gchar                    *property;
  gboolean                  accepted = TRUE;
  gint                      response;

  g_return_val_if_fail (FRAP_IS_SHORTCUTS_DIALOG (dialog), FALSE);
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);

  /* Ignore empty shortcuts */
  if (G_UNLIKELY (g_utf8_strlen (shortcut, -1) == 0))
    return FALSE;

  /* Ignore raw 'Return' and 'space' since that may have been used to activate the shortcut row */
  if (G_UNLIKELY (g_utf8_collate (shortcut, "Return") == 0 || g_utf8_collate (shortcut, "space") == 0))
    return FALSE;

  DBG ("shortcut = %s", shortcut);

  property = g_strconcat (CUSTOM_BASE_PROPERTY, "/", shortcut, NULL);
  info = xfce_keyboard_settings_get_shortcut_info (settings, shortcut, property);
  g_free (property);

  if (G_UNLIKELY (info != NULL))
    {
      response = frap_shortcuts_conflict_dialog (frap_shortcuts_provider_get_name (settings->priv->provider),
                                                 frap_shortcuts_provider_get_name (info->provider),
                                                 shortcut,
                                                 frap_shortcuts_dialog_get_action (dialog),
                                                 info->shortcut->command,
                                                 FALSE);

      accepted = response == GTK_RESPONSE_ACCEPT;

      xfce_keyboard_settings_free_shortcut_info (info);
    }

  return accepted;
}



static XfceKeyboardShortcutInfo *
xfce_keyboard_settings_get_shortcut_info (XfceKeyboardSettings *settings,
                                          const gchar          *shortcut,
                                          const gchar          *ignore_property)
{
  XfceKeyboardShortcutInfo *info = NULL;
  GList                    *iter;
  FrapShortcut             *sc;
  GList                    *providers;
  gchar                    *value;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);

  DBG ("shortcut = %s, ignore_property = %s", shortcut, ignore_property);

  providers = frap_shortcuts_provider_get_providers ();

  if (G_UNLIKELY (providers == NULL))
    return NULL;

  for (iter = providers; iter != NULL && info == NULL; iter = g_list_next (iter))
    {
      if (G_UNLIKELY (frap_shortcuts_provider_has_shortcut (iter->data, shortcut)))
        {
          sc = frap_shortcuts_provider_get_shortcut (iter->data, shortcut);

          if (G_LIKELY (sc != NULL))
            {
              /* Check ignore_property and change shortcut info struct */
              info = g_new0 (XfceKeyboardShortcutInfo, 1);
              info->provider = g_object_ref (iter->data);
              info->shortcut = sc;
            }
        }
    }

  frap_shortcuts_provider_free_providers (providers);

  return info;
}



static void
xfce_keyboard_settings_free_shortcut_info (XfceKeyboardShortcutInfo *info)
{
  g_object_unref (info->provider);
  frap_shortcuts_free_shortcut (info->shortcut);
  g_free (info);
}



static void
xfce_keyboard_settings_shortcut_added (FrapShortcutsProvider *provider,
                                       const gchar           *shortcut,
                                       XfceKeyboardSettings  *settings)
{
  FrapShortcut *sc;
  GtkTreeModel *model;
  GtkWidget    *view;
  GtkTreeIter   iter;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = glade_xml_get_widget (settings->priv->glade_xml, "kbd_shortcuts_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  sc = frap_shortcuts_provider_get_shortcut (settings->priv->provider, shortcut);

  if (G_LIKELY (sc != NULL))
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, SHORTCUT_COLUMN, shortcut, COMMAND_COLUMN, sc->command, -1);

      frap_shortcuts_free_shortcut (sc);
    }
}




static gboolean
_xfce_keyboard_settings_remove_shortcut (GtkTreeModel *model,
                                         GtkTreePath  *path,
                                         GtkTreeIter  *iter,
                                         const gchar  *shortcut)
{
  gboolean finished = FALSE;
  gchar   *row_shortcut;

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
xfce_keyboard_settings_shortcut_removed (FrapShortcutsProvider *provider,
                                         const gchar           *shortcut,
                                         XfceKeyboardSettings  *settings)
{
  GtkTreeModel *model;
  GtkWidget    *view;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = glade_xml_get_widget (settings->priv->glade_xml, "kbd_shortcuts_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) _xfce_keyboard_settings_remove_shortcut,
                          (gpointer) shortcut);
}



static void
xfce_keyboard_settings_add_button_clicked (XfceKeyboardSettings *settings,
                                           GtkButton            *button)
{
  GtkWidget   *command_dialog;
  GtkWidget   *shortcut_dialog;
  const gchar *shortcut;
  const gchar *command;
  gboolean     finished = FALSE;
  gint         response;

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  /* Create command dialog */
  command_dialog = command_dialog_new (NULL, NULL);

  /* Run command dialog until a vaild (non-empty) command is entered or the dialog is cancelled */
  do
    {
      response = command_dialog_run (COMMAND_DIALOG (command_dialog), GTK_WIDGET (button));

      if (G_UNLIKELY (response == GTK_RESPONSE_OK &&
                      g_utf8_strlen (command_dialog_get_command (COMMAND_DIALOG (command_dialog)), -1) == 0))
        xfce_err (_("Shortcut command may not be empty."));
      else
        finished = TRUE;
    }
  while (!finished);

  /* Abort if the dialog was cancelled */
  if (G_UNLIKELY (response == GTK_RESPONSE_OK))
    {
      /* Get the command */
      command = command_dialog_get_command (COMMAND_DIALOG (command_dialog));

      /* Hide the command dialog */
      gtk_widget_hide (command_dialog);

      /* Create shortcut dialog */
      shortcut_dialog = frap_shortcuts_dialog_new ("commands", command);
      g_signal_connect (shortcut_dialog, "validate-shortcut", G_CALLBACK (xfce_keyboard_settings_validate_shortcut), settings);

      /* Run shortcut dialog until a valid shortcut is entered or the dialog is cancelled */
      response = frap_shortcuts_dialog_run (FRAP_SHORTCUTS_DIALOG (shortcut_dialog));

      /* Only continue if the shortcut dialog succeeded */
      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Get shortcut */
          shortcut = frap_shortcuts_dialog_get_shortcut (FRAP_SHORTCUTS_DIALOG (shortcut_dialog));

          /* Save the new shortcut to xfconf */
          frap_shortcuts_provider_set_shortcut (settings->priv->provider, shortcut, command);
        }

      /* Destroy the shortcut dialog */
      gtk_widget_destroy (shortcut_dialog);
    }

  /* Destroy the shortcut dialog */
  gtk_widget_destroy (command_dialog);
}



static void
xfce_keyboard_settings_delete_button_clicked (XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  GtkWidget        *view;
  GList            *rows;
  GList            *row_iter;
  GList            *row_references = NULL;
  gchar            *shortcut;

  DBG ("remove!");

  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));

  view = glade_xml_get_widget (settings->priv->glade_xml, "kbd_shortcuts_view");
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
          frap_shortcuts_provider_reset_shortcut (settings->priv->provider, shortcut);

          /* Free strings */
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
xfce_keyboard_settings_reset_button_clicked (XfceKeyboardSettings *settings)
{
  g_return_if_fail (XFCE_IS_KEYBOARD_SETTINGS (settings));
  frap_shortcuts_provider_reset_to_defaults (settings->priv->provider);
}



#ifdef HAVE_LIBXKLAVIER

static gchar *
xfce_keyboard_settings_xkb_description (XklConfigItem *config_item)
{
  gchar *ci_description;
  gchar *description;

  ci_description = g_strstrip (config_item->description);

  if (ci_description[0] == 0)
    description = g_strdup (config_item->name);
  else
    description = g_locale_to_utf8 (ci_description, -1, NULL, NULL, NULL);

  return description;
}



static void
xfce_keyboard_settings_set_layout (XfceKeyboardSettings *settings)
{
  GtkWidget    *view;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *val_layout;
  gchar        *val_variant;
  gchar        *variants;
  gchar        *layouts;
  gchar        *tmp;

  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
  gtk_tree_model_get_iter_first (model, &iter);
  gtk_tree_model_get (model, &iter, LAYOUTS, &val_layout, VARIANTS, &val_variant, -1);
  layouts = g_strdup (val_layout);
  g_free (val_layout);

  if (val_variant)
    {
      variants = g_strdup (val_variant);
      g_free (val_variant);
    }
  else
    variants = g_strdup ("");

  while (gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get (model, &iter, LAYOUTS, &val_layout, VARIANTS, &val_variant, -1);
      tmp = g_strconcat (layouts, ",", val_layout, NULL);
      g_free (val_layout);
      g_free (layouts);
      layouts = tmp;

      if (val_variant)
        {
          tmp = g_strconcat (variants, ",", val_variant, NULL);
          g_free (val_variant);
          g_free (variants);
        }
      else
        {
          tmp = g_strconcat (variants, ",", NULL);
          g_free (variants);
        }
        variants = tmp;
    }

  xfconf_channel_set_string (settings->priv->keyboard_layout_channel, "/Default/XkbLayout", layouts);
  xfconf_channel_set_string (settings->priv->keyboard_layout_channel, "/Default/XkbVariant", variants);

  g_free (layouts);
  g_free (variants);
}



static void
xfce_keyboard_settings_init_layout (XfceKeyboardSettings *settings)
{
  GtkWidget    *view;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *val_layout;
  gchar        *val_variant;
  gchar        *default_layouts;
  gchar        *default_variants;
  gchar       **layouts;
  gchar       **layout;
  gchar       **variants;
  gchar       **variant;

  default_layouts  = g_strjoinv (",", settings->priv->xkl_rec_config->layouts);
  default_variants = g_strjoinv (",", settings->priv->xkl_rec_config->variants);

  val_layout  = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbLayout",  default_layouts);
  val_variant = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbVariant",  default_variants);

  layouts = g_strsplit (val_layout, ",", 0);
  variants = g_strsplit (val_variant, ",", 0);

  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  for (layout = layouts, variant = variants; *layout != NULL; ++layout)
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, LAYOUTS, *layout, VARIANTS, *variant, -1);

      if (*variant)
        variant++;
    }

  g_strfreev (layouts);
  g_strfreev (variants);
  g_free (default_layouts);
  g_free (default_variants);
}



static void
xfce_keyboard_settings_add_model_to_combo (XklConfigRegistry    *config_registry, 
                                           XklConfigItem        *config_item, 
                                           XfceKeyboardSettings *settings)
{
  GtkWidget    *view;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *model_name;

  model_name = xfce_keyboard_settings_xkb_description (config_item);

  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_model_combo");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));

  gtk_list_store_append (GTK_LIST_STORE (model), &iter );
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, DESC, model_name, NOM, config_item->name, -1);
  g_free (model_name);
}



static void
xfce_keyboard_settings_init_model (XfceKeyboardSettings *settings)
{
  GtkWidget    *view;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *id;
  gchar        *xkbmodel;
  gboolean      item;
  gboolean      found = FALSE;

  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_model_combo");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (view));

  xkbmodel = xfconf_channel_get_string (settings->priv->keyboard_layout_channel, "/Default/XkbModel", settings->priv->xkl_rec_config->model);
  item = gtk_tree_model_get_iter_first (model, &iter);

  while (item && !found)
    {
      gtk_tree_model_get (model, &iter, NOM, &id, -1);
      found = !strcmp (id, xkbmodel);
      g_free (id);

      if (found)
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (view), &iter);
          break;
        }
      item = gtk_tree_model_iter_next (model, &iter);
    }
  g_free (xkbmodel);
}



static void
xfce_keyboard_settings_cb_model_changed (GtkComboBox          *combo, 
                                         XfceKeyboardSettings *settings)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *xkbmodel;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  gtk_tree_model_get (model, &iter, NOM, &xkbmodel, -1);
  xfconf_channel_set_string (settings->priv->keyboard_layout_channel, "/Default/XkbModel", xkbmodel);
  g_free (xkbmodel);
}



static void
xfce_keyboard_settings_update_layout_buttons (XfceKeyboardSettings *settings)
{
  GtkWidget    *view;
  GtkTreeModel *model;
  GtkWidget    *xkb_layout_add_button;
  GtkWidget    *xkb_layout_delete_button;
  gint          n_layouts;
  gint          max_layouts;

  xkb_layout_add_button = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_add_button");
  xkb_layout_delete_button = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_delete_button");
  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

  n_layouts = gtk_tree_model_iter_n_children (model, NULL);
  max_layouts = xkl_engine_get_max_num_groups (settings->priv->xkl_engine);

  if (n_layouts < max_layouts)
    gtk_widget_set_sensitive (xkb_layout_add_button, TRUE);
  else
    gtk_widget_set_sensitive (xkb_layout_add_button, FALSE);

  if (n_layouts > 1)
    gtk_widget_set_sensitive (xkb_layout_delete_button, TRUE);
  else
    gtk_widget_set_sensitive (xkb_layout_delete_button, FALSE);
}



static void
xfce_keyboard_settings_update_edit_button (GtkTreeView          *tree_view, 
                                           XfceKeyboardSettings *settings)
{
  GtkWidget         *xkb_layout_edit_button;
  GtkTreePath       *path;
  GtkTreeViewColumn *column;

  xkb_layout_edit_button = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_edit_button");

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (tree_view), &path, &column);
  if (path)
    gtk_widget_set_sensitive (xkb_layout_edit_button, TRUE);
  else
    gtk_widget_set_sensitive (xkb_layout_edit_button, FALSE);
}



static void
xfce_keyboard_settings_edit_layout_button_cb (GtkWidget            *widget, 
                                              XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkWidget        *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *layout;
  gchar           **strings;

  layout = xfce_keyboard_settings_layout_selection (settings);
  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  if (layout)
    {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      strings = g_strsplit_set (layout, ",", 0);
      gtk_tree_selection_get_selected (selection, &model, &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, LAYOUTS, strings[0], VARIANTS, strings[1], -1);
      xfce_keyboard_settings_set_layout (settings);
      g_strfreev (strings);
    }
  g_free (layout);
  xfce_keyboard_settings_update_edit_button (GTK_TREE_VIEW (view), settings);
}



static void
xfce_keyboard_settings_add_layout_button_cb (GtkWidget            *widget, 
                                             XfceKeyboardSettings *settings)
{
  GtkTreeSelection *selection;
  GtkWidget        *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *layout;
  gchar           **strings;

  layout = xfce_keyboard_settings_layout_selection (settings);
  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  if (layout)
    {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      strings = g_strsplit_set (layout, ",", 0);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, LAYOUTS, strings[0], VARIANTS, strings[1], -1);
      xfce_keyboard_settings_update_layout_buttons (settings);
      xfce_keyboard_settings_set_layout (settings);
      g_strfreev (strings);
    }
  g_free (layout);
  xfce_keyboard_settings_update_edit_button (GTK_TREE_VIEW (view), settings);
}



static void
xfce_keyboard_settings_del_layout_button_cb (GtkWidget            *widget, 
                                             XfceKeyboardSettings *settings)
{
  GtkWidget        *view;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  view = glade_xml_get_widget (settings->priv->glade_xml, "xkb_layout_view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      xfce_keyboard_settings_update_edit_button (GTK_TREE_VIEW (view), settings);
      xfce_keyboard_settings_update_layout_buttons (settings);
      xfce_keyboard_settings_set_layout (settings);
    }
}



static void
xfce_keyboard_settings_add_variant_to_list (XklConfigRegistry    *config_registry,
                                            XklConfigItem        *config_item,
                                            XfceKeyboardSettings *settings)
{
  GtkTreeModel *model;
  GtkTreeStore *treestore;
  GtkTreeIter   iter;
  GtkWidget    *treeview;
  char         *variant;

  variant = xfce_keyboard_settings_xkb_description (config_item);
  treeview = glade_xml_get_widget (settings->priv->glade_xml, "layout_selection_view");
  treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
  gtk_tree_store_append (treestore, &iter, &settings->priv->layout_selection_iter);
  gtk_tree_store_set (treestore, &iter,
                      AVAIL_LAYOUT_TREE_COL_DESCRIPTION, variant,
                      AVAIL_LAYOUT_TREE_COL_ID, config_item->name, -1);
  g_free (variant);
}



static void
xfce_keyboard_settings_add_layout_to_list (XklConfigRegistry    *config_registry,
                                           XklConfigItem        *config_item,
                                           XfceKeyboardSettings *settings)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GtkTreeStore *treestore;
  GtkWidget    *treeview;
  gchar        *layout;

  layout = xfce_keyboard_settings_xkb_description (config_item);
  treeview = glade_xml_get_widget (settings->priv->glade_xml, "layout_selection_view");
  treestore = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));
  gtk_tree_store_append (treestore, &settings->priv->layout_selection_iter, NULL);
  gtk_tree_store_set (treestore, &settings->priv->layout_selection_iter,
                      AVAIL_LAYOUT_TREE_COL_DESCRIPTION, layout,
                      AVAIL_LAYOUT_TREE_COL_ID, config_item->name, -1);
  g_free (layout);

  xkl_config_registry_foreach_layout_variant (config_registry, config_item->name,
      (ConfigItemProcessFunc)xfce_keyboard_settings_add_variant_to_list, settings);
}



static void
xfce_keyboard_settings_layout_activate_cb (GtkTreeView       *tree_view,
                                           GtkTreePath       *path,
                                           GtkTreeViewColumn *column,
                                           GtkDialog         *dialog)
{
  gtk_dialog_response (dialog, 1);
}



static gchar *
xfce_keyboard_settings_layout_selection (XfceKeyboardSettings *settings)
{
  GtkWidget         *keyboard_layout_selection_dialog;
  GtkWidget         *layout_selection_ok_button;
  GtkWidget         *layout_selection_cancel_button;
  GtkWidget         *layout_selection_view;
  GtkTreePath       *path;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *model;
  GtkTreeIter        iter;
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkTreeIter        selected_iter;
  gchar             *val_layout;
  gchar             *layout;
  gchar             *variant;
  gint               result;

  keyboard_layout_selection_dialog = glade_xml_get_widget (settings->priv->glade_xml, "keyboard-layout-selection-dialog");
  layout_selection_view = glade_xml_get_widget (settings->priv->glade_xml, "layout_selection_view");

  if (!settings->priv->layout_selection_treestore)
    {
      settings->priv->layout_selection_treestore = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
      renderer = gtk_cell_renderer_text_new ();
      column   = gtk_tree_view_column_new_with_attributes (NULL, renderer, "text",
                                        AVAIL_LAYOUT_TREE_COL_DESCRIPTION, NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (layout_selection_view), GTK_TREE_MODEL (settings->priv->layout_selection_treestore));
      gtk_tree_view_append_column (GTK_TREE_VIEW (layout_selection_view), column);
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (settings->priv->layout_selection_treestore), 0, GTK_SORT_ASCENDING);
      xkl_config_registry_foreach_layout (settings->priv->xkl_registry,
          (ConfigItemProcessFunc) xfce_keyboard_settings_add_layout_to_list, settings);
      g_signal_connect (GTK_TREE_VIEW (layout_selection_view), "row-activated", G_CALLBACK (xfce_keyboard_settings_layout_activate_cb), keyboard_layout_selection_dialog);
    }
  val_layout = NULL;
  gtk_widget_show (keyboard_layout_selection_dialog);
  result = gtk_dialog_run (GTK_DIALOG (keyboard_layout_selection_dialog));
  if (result)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (layout_selection_view));
      gtk_tree_selection_get_selected (selection, &model, &iter);
      gtk_tree_model_get (model, &iter, AVAIL_LAYOUT_TREE_COL_ID, &layout, -1);

      path = gtk_tree_model_get_path (model, &iter);
      if (gtk_tree_path_get_depth (path) == 1)
        variant = g_strdup ("");
      else
        {
          variant = layout;
          gtk_tree_path_up (path);
          gtk_tree_model_get_iter (model, &iter, path);
          gtk_tree_model_get (model, &iter, AVAIL_LAYOUT_TREE_COL_ID, &layout, -1);
        }

      val_layout = g_strconcat (layout, ",", variant, NULL);
      g_free (layout);
      g_free (variant);
    }

  gtk_widget_hide (keyboard_layout_selection_dialog);
  return val_layout;
}

#endif /* HAVE_LIBXKLAVIER */
