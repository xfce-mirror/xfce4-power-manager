/*
 * * Copyright (C) 2014 Eric Koegel <eric@xfce.org>
 * * Copyright (C) 2019 Kacper Piwiński
 * * Copyright (C) 2024 Andrzej Radecki <andrzejr@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "power-manager-dialog.h"

#include "common/xfpm-brightness.h"
#include "common/xfpm-common.h"
#include "common/xfpm-config.h"
#include "common/xfpm-debug.h"
#include "common/xfpm-enum-glib.h"
#include "common/xfpm-icons.h"
#include "common/xfpm-power-common.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>


static void
power_manager_dialog_configure_response (PowerManagerDialog *dialog,
                                         gint response);
static void
power_manager_dialog_panel_label_changed (XfconfChannel *channel,
                                          const gchar *property,
                                          const GValue *value,
                                          gpointer user_data);
static void
power_manager_dialog_combo_changed (PowerManagerDialog *dialog,
                                    GtkComboBox *combo);


enum
{
  COLUMN_INT,
  COLUMN_STRING,
  N_COLUMNS
};


struct _PowerManagerDialog
{
  GObject __parent__;

  PowerManagerPlugin *plugin;
  XfceTitledDialog *dialog;
  XfconfChannel *channel;
};


G_DEFINE_TYPE (PowerManagerDialog, power_manager_dialog, G_TYPE_OBJECT)



static void
power_manager_dialog_class_init (PowerManagerDialogClass *klass)
{
}



static void
power_manager_dialog_init (PowerManagerDialog *dialog)
{
  dialog->plugin = NULL;
  dialog->dialog = NULL;
  dialog->channel = NULL;
}



static void
power_manager_dialog_configure_response (PowerManagerDialog *dialog,
                                         gint response)
{
  if (response == GTK_RESPONSE_HELP)
  {
    if (!g_spawn_command_line_async ("exo-open --launch WebBrowser https://docs.xfce.org/xfce/xfce4-power-manager/start",
                                     NULL))
      g_warning ("Unable to open the following url: %s",
                 "https://docs.xfce.org/xfce/xfce4-power-manager/start");
  }
  else
  {
    xfce_panel_plugin_unblock_menu (XFCE_PANEL_PLUGIN (dialog->plugin));
    gtk_widget_hide (GTK_WIDGET (dialog->dialog));
  }
}


/* Update combo if property in channel changes */
static void
power_manager_dialog_panel_label_changed (XfconfChannel *channel,
                                          const gchar *property,
                                          const GValue *value,
                                          gpointer user_data)
{
  GtkWidget *combo = user_data;
  GtkListStore *list_store;
  GtkTreeIter iter;
  int show_panel_label, current_setting;

  list_store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo)));
  current_setting = g_value_get_int (value);

  for (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
       gtk_list_store_iter_is_valid (list_store, &iter);
       gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter))
  {
    gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter, 0, &show_panel_label, -1);
    if (show_panel_label == current_setting)
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
  }
}


/* Update xfconf property if combobox selection is changed */
static void
power_manager_dialog_combo_changed (PowerManagerDialog *dialog,
                                    GtkComboBox *combo)
{
  g_return_if_fail (POWER_MANAGER_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_COMBO_BOX (combo));

  GtkTreeModel *model;
  GtkTreeIter iter;
  int show_panel_label;

  if (!gtk_combo_box_get_active_iter (combo, &iter))
    return;

  model = gtk_combo_box_get_model (combo);

  gtk_tree_model_get (model, &iter, 0, &show_panel_label, -1);
  xfconf_channel_set_int (dialog->channel, XFPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL, show_panel_label);
}


void
power_manager_dialog_show (PowerManagerDialog *dialog,
                           GdkScreen *screen)
{
  g_return_if_fail (POWER_MANAGER_IS_DIALOG (dialog));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  /* block the plugin menu */
  xfce_panel_plugin_block_menu (XFCE_PANEL_PLUGIN (dialog->plugin));

  gtk_window_set_screen (GTK_WINDOW (dialog->dialog), screen);
  gtk_widget_show (GTK_WIDGET (dialog->dialog));
}



PowerManagerDialog *
power_manager_dialog_new (PowerManagerPlugin *plugin,
                          PowerManagerConfig *config)
{
  PowerManagerDialog *dialog;
  GtkWidget *grid, *combo, *label, *gtkswitch;
  gint show_panel_label;
  GtkListStore *list_store;
  GtkTreeIter iter, active_iter;
  GtkCellRenderer *cell;
  gint i;
  gchar *options[] = { _("None"), _("Percentage"), _("Remaining time"), _("Percentage and remaining time") };

  dialog = g_object_new (POWER_MANAGER_TYPE_DIALOG, NULL);
  g_return_val_if_fail (POWER_MANAGER_IS_DIALOG (dialog), NULL);

  dialog->plugin = plugin;
  dialog->dialog =
    XFCE_TITLED_DIALOG (xfce_titled_dialog_new_with_mixed_buttons (_("Power Manager Plugin Settings"),
                                                                   GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                                   "help-browser", _("_Help"), GTK_RESPONSE_HELP,
                                                                   "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
                                                                   NULL));
  dialog->channel = xfconf_channel_get (XFPM_CHANNEL);

  gtk_window_set_position (GTK_WINDOW (dialog->dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_icon_name (GTK_WINDOW (dialog->dialog), "org.xfce.powermanager");
  gtk_widget_set_name (GTK_WIDGET (dialog->dialog), "power-manager-dialog");

  /* Set up the main grid for all settings */
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_widget_set_margin_start (grid, 12);
  gtk_widget_set_margin_end (grid, 12);
  gtk_widget_set_margin_top (grid, 12);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_container_add_with_properties (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog->dialog))),
                                     grid, "expand", TRUE, "fill", TRUE, NULL);

  /* show-panel-label setting */
  label = gtk_label_new (_("Show label:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label), 0, 0, 1, 1);
  show_panel_label = xfconf_channel_get_int (dialog->channel, XFPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL, DEFAULT_SHOW_PANEL_LABEL);

  list_store = gtk_list_store_new (N_COLUMNS,
                                   G_TYPE_INT,
                                   G_TYPE_STRING);

  for (i = PANEL_LABEL_NONE; i < N_PANEL_LABELS; i++)
  {
    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        COLUMN_INT, i,
                        COLUMN_STRING, options[i],
                        -1);
    if (i == show_panel_label)
      active_iter = iter;
  }
  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list_store));
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", COLUMN_STRING, NULL);
  gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), COLUMN_STRING);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &active_iter);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (combo), 1, 0, 1, 1);
  g_signal_connect_swapped (G_OBJECT (combo), "changed",
                            G_CALLBACK (power_manager_dialog_combo_changed),
                            dialog);
  g_signal_connect_object (G_OBJECT (dialog->channel), "property-changed::" XFPM_PROPERTIES_PREFIX SHOW_PANEL_LABEL,
                           G_CALLBACK (power_manager_dialog_panel_label_changed),
                           combo, 0);

  label = gtk_label_new (_("Show 'Presentation mode' indicator:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label), 0, 1, 1, 1);

  gtkswitch = gtk_switch_new ();
  gtk_widget_set_halign (gtkswitch, GTK_ALIGN_END);
  gtk_widget_set_valign (gtkswitch, GTK_ALIGN_CENTER);

  xfconf_g_property_bind (dialog->channel, XFPM_PROPERTIES_PREFIX SHOW_PRESENTATION_INDICATOR, G_TYPE_BOOLEAN,
                          G_OBJECT (gtkswitch), "active");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (gtkswitch), 1, 1, 1, 1);

  g_signal_connect_swapped (G_OBJECT (dialog->dialog), "response",
                            G_CALLBACK (power_manager_dialog_configure_response), dialog);
  gtk_widget_show_all (grid);

  return dialog;
}
