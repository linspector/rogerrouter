/*
 * Roger Router
 * Copyright (c) 2012-2022 Jan-Michael Brummer
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

#include "roger-phone.h"

#include "contrib/suggestion-entry.h"
#include "roger-contactsearch.h"
#include "roger-journal.h"
#include "roger-shell.h"


#include <ctype.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>

struct _RogerPhone {
  AdwWindow parent_instance;

  GtkWidget *grid;
  GtkWidget *header_bar;
  GtkWidget *search_entry;
  GtkWidget *mute_button;
  GtkWidget *hold_button;
  GtkWidget *record_button;
  GtkWidget *clear_button;
  GtkWidget *dial_button;
  GtkWidget *menu_button;
  GtkWidget *window_title;

  gint status_timer_id;

  RmConnection *connection;
} PhoneState;

G_DEFINE_TYPE (RogerPhone, roger_phone, ADW_TYPE_WINDOW)

static gboolean
roger_phone_status_timer_cb (gpointer user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  g_autofree char *duration = NULL;

  duration = rm_connection_get_duration_time (self->connection);
  adw_window_title_set_subtitle (ADW_WINDOW_TITLE (self->window_title), duration);

  return G_SOURCE_CONTINUE;
}

static void
roger_phone_update_buttons (RogerPhone *self)
{
  gboolean control_buttons = FALSE;

  if (self->connection) {
    gtk_button_set_icon_name (GTK_BUTTON (self->dial_button), "call-stop-symbolic");
    gtk_style_context_remove_class (gtk_widget_get_style_context (self->dial_button), "suggested-action");
    gtk_style_context_add_class (gtk_widget_get_style_context (self->dial_button), "destructive-action");
  } else {
    gtk_button_set_icon_name (GTK_BUTTON (self->dial_button), "call-start-symbolic");
    gtk_style_context_remove_class (gtk_widget_get_style_context (self->dial_button), "destructive-action");
    gtk_style_context_add_class (gtk_widget_get_style_context (self->dial_button), "suggested-action");
  }

  if (self->connection && self->connection->type & RM_CONNECTION_TYPE_SOFTPHONE)
    control_buttons = !!self->connection;

  gtk_widget_set_sensitive (self->mute_button, control_buttons);
  gtk_widget_set_sensitive (self->hold_button, control_buttons);
  gtk_widget_set_sensitive (self->record_button, control_buttons);
}

static void
roger_phone_start_status_timer (RogerPhone *self)
{
  g_clear_handle_id (&self->status_timer_id, g_source_remove);
  self->status_timer_id = g_timeout_add (250, roger_phone_status_timer_cb, self);
}

static void
roger_phone_remove_status_timer (RogerPhone *self)
{
  g_clear_handle_id (&self->status_timer_id, g_source_remove);
}

static void
roger_phone_connection_changed_cb (RmObject     *obj,
                                   gint          type,
                                   RmConnection *connection,
                                   RogerPhone   *self)
{
  g_assert (connection);
  g_assert (self);

  if (self->connection != connection)
    return;

  if (!(type & RM_CONNECTION_TYPE_DISCONNECT))
    return;

  roger_phone_remove_status_timer (self);
  self->connection = NULL;
  roger_phone_update_buttons (self);
}

static void
roger_phone_active_call_dialog (RogerPhone *self)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   _("Cannot close window, a call is in progress"));

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_window_destroy),
                    NULL);
}

static void
roger_phone_dial_button_clicked_cb (GtkWidget *button,
                                    gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  RmProfile *profile;
  RmPhone *phone;
  const char *number;

  if (self->connection) {
    rm_phone_hangup (self->connection);
    return;
  }

  number = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  if (RM_EMPTY_STRING (number)) {
    return;
  }

  g_assert (!self->connection);

  profile = rm_profile_get_active ();
  phone = rm_profile_get_phone (profile);
  self->connection = rm_phone_dial (phone, number, rm_router_get_suppress_state (profile));
  if (self->connection) {
    roger_phone_update_buttons (self);
    roger_phone_start_status_timer (self);
  }
}

static void
roger_phone_create_menu (RogerPhone *self)
{
  RmProfile *profile = rm_profile_get_active ();
  RmPhone *active = rm_profile_get_phone (profile);
  GMenu *menu;
  GMenu *phone_section;
  GMenu *misc_section;

  phone_section = g_menu_new ();
  for (const GList *list = rm_phone_get_plugins (); list && list->data; list = list->next) {
    RmPhone *phone = list->data;
    GMenuItem *item;

    item = g_menu_item_new (rm_phone_get_name (phone), "phone.set-phone");
    g_menu_item_set_action_and_target (item, "phone.set-phone", "s", rm_phone_get_name (phone));
    g_menu_append_item (phone_section, item);
  }

  gtk_widget_activate_action_variant (GTK_WIDGET (self), "phone.set-phone", g_variant_new_string (rm_phone_get_name (active)));

  menu = g_menu_new ();
  g_menu_append_item (menu, g_menu_item_new_section (_("Phones"), G_MENU_MODEL (phone_section)));

  misc_section = g_menu_new ();
  g_menu_append (misc_section, _("Suppress number"), "phone.set-suppression");
  g_menu_append_item (menu, g_menu_item_new_section (_("Optional"), G_MENU_MODEL (misc_section)));
  g_print ("Suppression: %d\n", rm_router_get_suppress_state (profile));

  if (rm_router_get_suppress_state (profile))
    gtk_widget_activate_action_variant (GTK_WIDGET (self), "phone.set-suppression", NULL);

  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->menu_button), G_MENU_MODEL (menu));
}

static void
roger_phone_dtmf_button_clicked_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  RmProfile *profile = rm_profile_get_active ();
  RmPhone *phone = rm_profile_get_phone (profile);
  const char *name = gtk_widget_get_name (widget);
  gint num = name[7];

  if (self->connection) {
    rm_phone_dtmf (phone, self->connection, num);
  } else {
    const char *text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
    g_autofree char *tmp = g_strdup_printf ("%s%c", text, num);

    gtk_editable_set_text (GTK_EDITABLE (self->search_entry), tmp);
  }
}

static void
roger_phone_record_button_clicked_cb (GtkWidget *widget,
                                      gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  RmProfile *profile = rm_profile_get_active ();
  RmPhone *phone = rm_profile_get_phone (profile);

  rm_phone_record (phone, self->connection, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
roger_phone_hold_button_clicked_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  RmProfile *profile = rm_profile_get_active ();
  RmPhone *phone = rm_profile_get_phone (profile);

  rm_phone_hold (phone, self->connection, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
roger_phone_mute_button_clicked_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  RmProfile *profile = rm_profile_get_active ();
  RmPhone *phone = rm_profile_get_phone (profile);

  rm_phone_mute (phone, self->connection, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
roger_phone_clear_button_clicked_cb (GtkWidget *widget,
                                     gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));

  if (!RM_EMPTY_STRING (text)) {
    g_autofree char *new = g_strdup (text);

    new[strlen (text) - 1] = '\0';
    gtk_editable_set_text (GTK_EDITABLE (self->search_entry), new);
  }
}

static gboolean
roger_phone_close_request_cb (GtkWidget *window,
                              gpointer   user_data)
{
  RogerPhone *self = ROGER_PHONE (window);

  if (self->connection) {
    roger_phone_active_call_dialog (self);
    return TRUE;
  }

  return FALSE;
}

static void
roger_phone_dispose (GObject *object)
{
  RogerPhone *self = ROGER_PHONE (object);

  roger_phone_remove_status_timer (self);

  G_OBJECT_CLASS (roger_phone_parent_class)->dispose (object);
}

static void
roger_phone_class_init (RogerPhoneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = roger_phone_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/phone.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerPhone, grid);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, dial_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, mute_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, hold_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, record_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, clear_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, menu_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, header_bar);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, window_title);

  gtk_widget_class_bind_template_callback (widget_class, roger_phone_dtmf_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_dial_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_record_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_hold_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_mute_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_clear_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_close_request_cb);
}

static void
roger_phone_change_state (GSimpleAction *action,
                          GVariant      *value,
                          gpointer       user_data)
{
  const char *name = g_variant_get_string (value, NULL);
  RmPhone *phone = rm_phone_get (name);
  RogerPhone *self = ROGER_PHONE (user_data);

  rm_profile_set_phone (rm_profile_get_active (), phone);
  adw_window_title_set_title (ADW_WINDOW_TITLE (self->window_title), name);

  g_simple_action_set_state (action, value);
}

static void
roger_phone_set_suppression (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       user_data)
{
  /* RmProfile *profile = rm_profile_get_active (); */
  gboolean state = g_variant_get_boolean (value);

  g_print ("%s: %d\n", __FUNCTION__, state);
  g_simple_action_set_state (action, value);

  /*rm_router_set_suppress_state (profile, state); */
}

static const GActionEntry phone_actions [] = {
  {"set-phone", NULL, "s", "''", roger_phone_change_state},
  {"set-suppression", NULL, NULL, "false", roger_phone_set_suppression},
};

static void
roger_phone_init (RogerPhone *self)
{
  RmProfile *profile = rm_profile_get_active ();
  RmPhone *phone = rm_profile_get_phone (profile);
  GSimpleActionGroup *simple_action_group;

  gtk_widget_init_template (GTK_WIDGET (self));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   phone_actions,
                                   G_N_ELEMENTS (phone_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "phone",
                                  G_ACTION_GROUP (simple_action_group));

  self->search_entry = suggestion_entry_new ();
  gtk_widget_set_valign (self->search_entry, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (self->grid), self->search_entry, 0, 0, 3, 1);

  roger_contact_search_completion_add (self->search_entry);

  roger_phone_create_menu (self);

  adw_window_title_set_title (ADW_WINDOW_TITLE (self->window_title), phone ? rm_phone_get_name (phone) : _("Phone"));
  adw_window_title_set_subtitle (ADW_WINDOW_TITLE (self->window_title), "");

  g_signal_connect_object (rm_object, "connection-changed", G_CALLBACK (roger_phone_connection_changed_cb), self, 0);

  roger_phone_update_buttons (self);
}

GtkWidget *
roger_phone_new (void)
{
  return g_object_new (ROGER_TYPE_PHONE,
                       "application", GTK_APPLICATION (roger_shell_get_default ()),
                       NULL);
}

void
roger_phone_set_dial_number (RogerPhone *self,
                             const char *number)
{
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), number);
}

void
roger_phone_pickup_connection (RogerPhone   *self,
                               RmConnection *connection)
{
  g_assert (connection);
  g_assert (self);
  g_assert (!self->connection);

  if (!rm_phone_pickup (connection)) {
    self->connection = connection;

    roger_phone_update_buttons (self);
    roger_phone_start_status_timer (self);
  }
}
