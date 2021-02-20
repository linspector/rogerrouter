/*
 * Roger Router
 * Copyright (c) 2012-2021 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <gtk/gtk.h>

#include <rm/rm.h>
#include <rm/rmplugins.h>

#include <roger/main.h>
#include <roger/preferences.h>
#include <uitools.h>

/**
 * plugins_configure_button_clicked_cb:
 * @button: button #GtkWidget
 * @user_data: a #GtkListBox
 *
 * Creates and opens configure window of selected plugin
 */
static void
plugins_configure_button_clicked_cb (GtkWidget *button,
                                     gpointer   user_data)
{
  /*GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(user_data));
   *  GtkWidget *child = gtk_container_get_children(GTK_CONTAINER(row))->data;
   *  RmPlugin *plugin = g_object_get_data(G_OBJECT(child), "plugin");
   *
   *  if (!plugin) {
   *       return;
   *  }
   *
   *  if (plugin->configure) {
   *       GtkWidget *config = plugin->configure(plugin);
   *       GtkWidget *win;
   *       GtkWidget *headerbar;
   *
   *       if (!config) {
   *               return;
   *       }
   *
   *       win = g_object_new(GTK_TYPE_DIALOG, "use-header-bar", TRUE, NULL);;
   *       gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(plugins_window));
   *       gtk_window_set_modal(GTK_WINDOW(win), TRUE);
   *
   *       headerbar = gtk_dialog_get_header_bar(GTK_DIALOG(win));
   *       gtk_header_bar_set_title(GTK_HEADER_BAR (headerbar), plugin->name);
   *       gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);
   *
   *       gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(win))), config);
   *
   *       gtk_widget_show_all(config);
   *       gtk_dialog_run(GTK_DIALOG(win));
   *       gtk_widget_destroy(win);
   *  }*/
}

static gboolean
plugins_switch_state_set_cb (GtkSwitch *widget,
                             gboolean   state,
                             gpointer   user_data)
{
  RmPlugin *plugin = user_data;

  if (state)
    rm_plugins_enable (plugin);
  else
    rm_plugins_disable (plugin);

  return FALSE;
}

void
roger_preferences_setup_plugins (RogerPreferencesWindow *self)
{
  for (GList *list = rm_plugins_get (); list != NULL; list = list->next) {
    RmPlugin *plugin = list->data;
    GtkWidget *row;
    GtkWidget *toggle;
    GtkWidget *sub_row;
    GtkWidget *link;

    if (plugin->builtin) {
      continue;
    }

    row = hdy_expander_row_new ();

    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), plugin->name);
    hdy_expander_row_set_subtitle (HDY_ACTION_ROW (row), plugin->description);

    toggle = gtk_switch_new ();
    gtk_switch_set_active (GTK_SWITCH (toggle), plugin->enabled);
    g_signal_connect (toggle, "state-set", G_CALLBACK (plugins_switch_state_set_cb), plugin);
    gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
    hdy_expander_row_add_action (HDY_EXPANDER_ROW (row), toggle);

    sub_row = hdy_action_row_new ();
    gtk_container_add (GTK_CONTAINER (row), sub_row);
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (sub_row), _("Homepage"));
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (sub_row), TRUE);
    link = gtk_link_button_new_with_label (plugin->homepage, plugin->homepage);
    gtk_container_add (GTK_CONTAINER (sub_row), link);

    if (plugin->configure) {
      for (GList *sub_row = plugin->configure (plugin); sub_row && sub_row->data; sub_row = sub_row->next)
        gtk_container_add (GTK_CONTAINER (row), sub_row->data);
    }

    gtk_list_box_insert (GTK_LIST_BOX (self->plugins_listbox), row, -1);
  }
}
