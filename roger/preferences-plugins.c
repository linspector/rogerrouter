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

#include "config.h"

#include "preferences.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>
#include <rm/rmplugins.h>

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
  for (GSList *list = rm_plugins_get (); list != NULL; list = list->next) {
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
    hdy_expander_row_set_subtitle (HDY_EXPANDER_ROW (row), plugin->description);

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
