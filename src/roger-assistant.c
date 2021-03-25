/*
 * Roger Router Copyright (c) 2012-2022 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "roger-assistant.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>
#include <string.h>

typedef struct {
  char *name;
  void (*setup)(RogerAssistant *assistant);
} AssistantPage;

struct _RogerAssistant {
  AdwWindow parent_instance;

  GtkWidget *next_button;
  GtkWidget *back_button;
  GtkWidget *profile_name_entry;
  GtkWidget *stack;
  GtkWidget *router_listview;
  GtkWidget *user_entry;
  GtkWidget *password_entry;
  GtkWidget *loading_page;
  GtkWidget *start_button;
  GtkStringList *router_model;

  gint current_page;
  guint presence_check_id;
  guint get_settings_id;
  char *router_uri;
  RmProfile *profile;
};

G_DEFINE_TYPE (RogerAssistant, roger_assistant, ADW_TYPE_WINDOW)

static void roger_assistant_back_button_clicked (GtkWidget *next,
                                                 gpointer   user_data);
static void roger_assistant_next_button_clicked (GtkWidget *next,
                                                 gpointer   user_data);

static void
roger_assistant_profile_entry_changed (GtkEditable *entry,
                                       gpointer     user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  RmProfile *profile;
  GList *profile_list = rm_profile_get_list ();
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));

  /* Loop through all known profiles and check for duplicate profile name */
  while (profile_list != NULL) {
    profile = profile_list->data;

    if (strcmp (profile->name, text) == 0 && (!self->profile || strcmp (self->profile->name, profile->name) != 0)) {
      /* Duplicate found: Update button state and icon entry */
      gtk_widget_set_sensitive (self->next_button, FALSE);
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, "dialog-error-symbolic");
      return;
    }

    profile_list = profile_list->next;
  }

  /* Update button state and icon entry */
  gtk_widget_set_sensitive (self->next_button, !RM_EMPTY_STRING (text));
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, NULL);
}

static void
assistant_profile_page_setup (RogerAssistant *self)
{
  gtk_widget_set_sensitive (self->back_button, TRUE);
  gtk_widget_set_sensitive (self->next_button, FALSE);

  roger_assistant_profile_entry_changed (GTK_EDITABLE (self->profile_name_entry), self);
}

static gboolean
assistant_scan_router (gpointer user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  GList *routers = rm_ssdp_get_routers ();
  GList *list;

  if (routers)
    gtk_widget_set_sensitive (self->next_button, TRUE);

  for (list = routers; list != NULL; list = list->next) {
    RmRouterInfo *router_info = list->data;
    GtkWidget *new_device;
    g_autofree char *tmp = NULL;

    tmp = g_strdup_printf ("<b>%s</b>\n<small>%s %s</small>", router_info->name, _("on"), router_info->host);

    new_device = gtk_label_new ("");
    gtk_label_set_justify (GTK_LABEL (new_device), GTK_JUSTIFY_CENTER);
    g_object_set_data (G_OBJECT (new_device), "host", router_info->host);
    gtk_label_set_markup (GTK_LABEL (new_device), tmp);
    gtk_widget_show (new_device);

    gtk_string_list_append (self->router_model, g_steal_pointer (&tmp));
  }

  return G_SOURCE_REMOVE;
}

static gboolean
check_router_presence (gpointer user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  gboolean present;

  present = rm_router_present (self->profile->router_info);
  gtk_widget_set_sensitive (self->next_button, present);

  return G_SOURCE_REMOVE;
}

static void
check_presence (RogerAssistant *self)
{
  if (!self->router_uri)
    return;

  rm_profile_set_host (self->profile, self->router_uri);

  if (self->presence_check_id) {
    g_source_remove (self->presence_check_id);
    self->presence_check_id = 0;
  }
  self->presence_check_id = g_idle_add (check_router_presence, self);
}

static void
roger_assistant_router_listview_activate (GtkListView *list_view,
                                          guint        position,
                                          gpointer     user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  GtkSelectionModel *model;
  g_autofree char *host = NULL;
  const GtkStringObject *so;
  const char *text;

  model = gtk_list_view_get_model (list_view);
  so = g_list_model_get_item (G_LIST_MODEL (model), position);
  text = gtk_string_object_get_string ((GtkStringObject *)so);
  g_print ("%s: %s\n", __FUNCTION__, text);

  /*host = g_object_get_data (G_OBJECT (child), "host"); */
  host = g_strdup ("192.168.178.1");

  g_clear_pointer (&self->router_uri, g_free);
  self->router_uri = g_strdup (host);

  check_presence (self);
}

static void
assistant_router_page_setup (RogerAssistant *self)
{
  const char *name = gtk_editable_get_text (GTK_EDITABLE (self->profile_name_entry));

  gtk_widget_set_sensitive (self->back_button, TRUE);
  gtk_widget_set_sensitive (self->next_button, FALSE);

  self->profile = rm_profile_add (name);
  g_idle_add (assistant_scan_router, self);
}

static void
assistant_get_settings_thread (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  RogerAssistant *self = ROGER_ASSISTANT (source_object);
  gboolean ret = FALSE;

  rm_router_set_active (self->profile);

  /* Get settings */
  adw_status_page_set_description (ADW_STATUS_PAGE (self->loading_page), _("Get Settings"));
  if (rm_router_login (self->profile) && rm_router_get_settings (self->profile))
    ret = TRUE;

  /* Enable telnet & capi port */
  adw_status_page_set_description (ADW_STATUS_PAGE (self->loading_page), _("Enable Ports"));
  if (rm_router_dial_number (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_TELNET))
    rm_router_hangup (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_TELNET);

  if (rm_router_dial_number (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_CAPI))
    rm_router_hangup (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_CAPI);

  /* Trigger network reconnect */
  adw_status_page_set_description (ADW_STATUS_PAGE (self->loading_page), _("Trigger Reconnect"));
  rm_netmonitor_reconnect ();

  g_task_return_boolean (task, ret);
}

static void
assistant_password_page_setup (RogerAssistant *self)
{
  gtk_widget_set_sensitive (self->back_button, TRUE);
  gtk_widget_set_sensitive (self->next_button, TRUE);
}

static void
assistant_get_settings_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  gboolean ret = g_task_propagate_boolean (G_TASK (res), &error);

  gtk_widget_set_visible (self->back_button, TRUE);
  gtk_widget_set_visible (self->next_button, TRUE);

  if (ret)
    roger_assistant_next_button_clicked (self->next_button, self);
  else
    roger_assistant_back_button_clicked (self->back_button, self);
}

static void
assistant_loading_page_setup (RogerAssistant *self)
{
  g_autoptr (GTask) task = NULL;
  const char *user = gtk_editable_get_text (GTK_EDITABLE (self->user_entry));
  const char *password = gtk_editable_get_text (GTK_EDITABLE (self->password_entry));

  g_print ("%s: host: %s\n", __FUNCTION__, self->profile->router_info->host);

  gtk_widget_set_visible (self->back_button, FALSE);
  gtk_widget_set_visible (self->next_button, FALSE);

  /* Create new profile based on user input */
  rm_profile_set_login_user (self->profile, user);
  rm_profile_set_login_password (self->profile, password);

  /* Release any previous lock */
  rm_router_release_lock ();

  task = g_task_new (self, NULL, assistant_get_settings_cb, self);
  g_task_run_in_thread (task, assistant_get_settings_thread);
}

static void
assistant_finish_page_setup (RogerAssistant *self)
{
  gtk_widget_set_visible (self->back_button, FALSE);
  gtk_widget_set_visible (self->next_button, FALSE);

  gtk_widget_grab_focus (self->start_button);
}

AssistantPage assistant_pages[] = {
  { "profile", assistant_profile_page_setup },
  { "router", assistant_router_page_setup },
  { "password", assistant_password_page_setup },
  { "loading", assistant_loading_page_setup },
  { "finish", assistant_finish_page_setup },
  { NULL, NULL }
};

static void
roger_assistant_back_button_clicked (GtkWidget *next,
                                     gpointer   user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);

  /* In case we are on the first page, exit assistant */
  if (self->current_page <= 0) {
    if (self->profile)
      rm_profile_remove (self->profile);

    gtk_window_destroy (GTK_WINDOW (self));
    return;
  }

  self->current_page--;

  /* If we have no previous page, change back button text to Quit */
  if (self->current_page == 0) {
    gtk_button_set_label (GTK_BUTTON (self->back_button), _("Quit"));
  }

  /* Set transition type back to slide *right* and set active child */
  gtk_stack_set_transition_type (GTK_STACK (self->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), assistant_pages[self->current_page].name);

  /* Run setup page function if necessary */
  if (assistant_pages[self->current_page].setup)
    assistant_pages[self->current_page].setup (self);
}

static void
roger_assistant_next_button_clicked (GtkWidget *next,
                                     gpointer   user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);

  gtk_button_set_label (GTK_BUTTON (self->back_button), _("Back"));

  self->current_page++;

  /* Set transition type back to slide *left* and set active child */
  gtk_stack_set_transition_type (GTK_STACK (self->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), assistant_pages[self->current_page].name);

  /* Run setup page function if necessary */
  if (assistant_pages[self->current_page].setup)
    assistant_pages[self->current_page].setup (self);
}

static void
roger_assistant_class_init (RogerAssistantClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/assistant.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, back_button);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, next_button);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, stack);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, profile_name_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, router_listview);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, user_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, password_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, loading_page);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, start_button);

  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_next_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_profile_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_router_listview_activate);
}

static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_widget_set_margin_start (label, 6);
  gtk_widget_set_margin_end (label, 6);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);

  gtk_list_item_set_child (list_item, label);
}

static void
bind_listitem_cb (GtkListItemFactory *factory,
                  GtkListItem        *list_item)
{
  GtkWidget *label;
  const char *text;
  const GtkStringObject *so;

  label = gtk_list_item_get_child (list_item);
  so = gtk_list_item_get_item (list_item);
  text = gtk_string_object_get_string ((GtkStringObject *)so);
  gtk_label_set_markup (GTK_LABEL (label), text);
}

static void
roger_assistant_init (RogerAssistant *self)
{
  g_autoptr (GList) childrens = NULL;
  GtkListItemFactory *factory;
  GtkSingleSelection *selection;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_grab_focus (self->profile_name_entry);
  gtk_widget_set_receives_default (self->next_button, TRUE);

  self->current_page = 0;

  self->router_model = gtk_string_list_new (NULL);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);
  gtk_list_view_set_factory (GTK_LIST_VIEW (self->router_listview), factory);
  selection = gtk_single_selection_new (G_LIST_MODEL (self->router_model));
  gtk_list_view_set_model (GTK_LIST_VIEW (self->router_listview), GTK_SELECTION_MODEL (selection));
}

GtkWidget *
roger_assistant_new (void)
{
  return g_object_new (ROGER_TYPE_ASSISTANT, NULL);
}
