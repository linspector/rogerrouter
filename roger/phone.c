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

#include "phone.h"

#include "contacts.h"
#include "contactsearch.h"
#include "journal.h"
#include "uitools.h"

#include <ctype.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>

struct _RogerPhone {
  HdyWindow parent_instance;

  GtkWidget *grid;
  GtkWidget *header_bar;
  GtkWidget *search_entry;
  GtkWidget *mute_button;
  GtkWidget *hold_button;
  GtkWidget *record_button;
  GtkWidget *clear_button;
  GtkWidget *dial_button;
  GtkWidget *menu_button;
  GtkWidget *phone_box;

  gint status_timer_id;

  RmConnection *connection;
} PhoneState;

G_DEFINE_TYPE (RogerPhone, roger_phone, HDY_TYPE_WINDOW)

static gboolean
roger_phone_status_timer_cb (gpointer user_data)
{
  RogerPhone *self = ROGER_PHONE (user_data);
  g_autofree char *duration = NULL;

  duration = rm_connection_get_duration_time (self->connection);
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (self->header_bar), duration);

  return G_SOURCE_CONTINUE;
}

static void
roger_phone_update_buttons (RogerPhone *self)
{
  gboolean control_buttons = FALSE;
  GtkWidget *image;

  if (self->connection) {
    image = gtk_image_new_from_icon_name ("call-stop-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (self->dial_button), image);
    gtk_style_context_remove_class (gtk_widget_get_style_context (self->dial_button), GTK_STYLE_CLASS_SUGGESTED_ACTION);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->dial_button), GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
  } else {
    image = gtk_image_new_from_icon_name ("call-start-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (self->dial_button), image);
    gtk_style_context_remove_class (gtk_widget_get_style_context (self->dial_button), GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->dial_button), GTK_STYLE_CLASS_SUGGESTED_ACTION);
  }

  if (self->connection && self->connection->type & RM_CONNECTION_TYPE_SOFTPHONE)
    control_buttons = !!self->connection;

  /* Set control button sensitive value */
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
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (self->header_bar), "");
}

static void
roger_phone_connection_changed_cb (RmObject     *obj,
                                   gint          type,
                                   RmConnection *connection,
                                   RogerPhone   *self)
{
  g_assert (connection);
  g_assert (self);
  g_assert (self->connection);

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
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
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

  number = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
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
  RmPhone *active = rm_profile_get_phone (rm_profile_get_active ());

  for (const GSList *list = rm_phone_get_plugins (); list && list->data; list = list->next) {
    RmPhone *phone = list->data;
    GtkWidget *item;

    item = gtk_model_button_new ();
    gtk_widget_set_hexpand (item, TRUE);
    gtk_button_set_label (GTK_BUTTON (item), rm_phone_get_name (phone));
    gtk_actionable_set_action_name (GTK_ACTIONABLE (item), "phone.set-phone");
    gtk_actionable_set_action_target (GTK_ACTIONABLE (item), "s", rm_phone_get_name (phone));
    g_object_set (G_OBJECT (item), "xalign", 0.0f, NULL);
    gtk_widget_show (item);
    gtk_box_pack_start (GTK_BOX (self->phone_box), item, FALSE, FALSE, 0);

    if (g_strcmp0 (rm_phone_get_name (phone), rm_phone_get_name (active)) == 0)
      g_object_set (item, "active", TRUE, NULL);
  }
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
    const char *text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
    g_autofree char *tmp = g_strdup_printf ("%s%c", text, num);

    gtk_entry_set_text (GTK_ENTRY (self->search_entry), tmp);
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
  const char *text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  if (!RM_EMPTY_STRING (text)) {
    g_autofree char *new = g_strdup (text);

    new[strlen (text) - 1] = '\0';
    gtk_entry_set_text (GTK_ENTRY (self->search_entry), new);
  }
}

static gboolean
roger_phone_delete_event_cb (GtkWidget *window,
                             GdkEvent  *event,
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
roger_phone_class_init (RogerPhoneClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/phone.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerPhone, grid);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, dial_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, mute_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, hold_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, record_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, clear_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, menu_button);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, header_bar);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, phone_box);
  gtk_widget_class_bind_template_child (widget_class, RogerPhone, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, roger_phone_dtmf_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_dial_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_record_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_hold_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_mute_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_clear_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_phone_delete_event_cb);
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
  hdy_header_bar_set_title (HDY_HEADER_BAR (self->header_bar), name);

  g_simple_action_set_state (action, value);}

static void
roger_phone_set_suppression (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       user_data)
{
  g_simple_action_set_state (action, value);
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

  contact_search_completion_add (self->search_entry);
  roger_phone_create_menu (self);

  hdy_header_bar_set_title (HDY_HEADER_BAR (self->header_bar), phone ? rm_phone_get_name (phone) : _("Phone"));
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (self->header_bar), "");

  g_signal_connect_object (rm_object, "connection-changed", G_CALLBACK (roger_phone_connection_changed_cb), self, 0);

  roger_phone_update_buttons (self);
}

GtkWidget *
roger_phone_new (void)
{
  return g_object_new (ROGER_TYPE_PHONE, NULL);
}

void
roger_phone_set_dial_number (RogerPhone *self,
                             const char *number)
{
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), number);
}

void roger_phone_pickup_connection (RogerPhone   *self,
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

