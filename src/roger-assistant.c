/*
 * Roger Router Copyright (c) 2012-2021 Jan-Michael Brummer
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>
#include <string.h>

typedef struct {
  char *name;
  void (*setup)(RogerAssistant *assistant);
} AssistantPage;

struct _RogerAssistant {
  HdyWindow parent_instance;

  GtkWidget *next_button;
  GtkWidget *back_button;
  GtkWidget *profile_name_entry;
  GtkWidget *stack;
  GtkWidget *router_stack;
  GtkWidget *router_listbox;
  GtkWidget *router_entry;
  GtkWidget *user_entry;
  GtkWidget *password_entry;
  GtkWidget *ftp_user_label;
  GtkWidget *ftp_user_entry;
  GtkWidget *ftp_password_label;
  GtkWidget *ftp_password_entry;
  GtkWidget *loading_label;
  GtkWidget *start_button;

  gint current_page;
  gint max_page;
  guint presence_check_id;
  guint get_settings_id;
  char *router_uri;
  gboolean needs_ftp;
  RmProfile *profile;
};

G_DEFINE_TYPE (RogerAssistant, roger_assistant, HDY_TYPE_WINDOW)

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
  const char *text = gtk_entry_get_text (GTK_ENTRY (entry));

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

static void
assistant_router_listbox_destroy (GtkWidget *widget,
                                  gpointer   user_data)
{
  gtk_widget_destroy (widget);
}

static gboolean
assistant_scan_router (gpointer user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  GList *routers = rm_ssdp_get_routers ();
  GList *list;

  /* Clear list box */
  gtk_container_foreach (GTK_CONTAINER (self->router_listbox), assistant_router_listbox_destroy, NULL);

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

    gtk_list_box_prepend (GTK_LIST_BOX (self->router_listbox), new_device);
  }

  if (routers) {
    GtkListBoxRow *row;

    /* Pre-select first row */
    row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->router_listbox), 0);
    gtk_list_box_select_row (GTK_LIST_BOX (self->router_listbox), row);
  }

  return G_SOURCE_REMOVE;
}

static gboolean
check_router_presence (gpointer user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  gboolean present;

  present = rm_router_present (self->profile->router_info);
  self->needs_ftp = !rm_network_tr64_available (self->profile);
  gtk_widget_set_sensitive (self->next_button, present);

  return G_SOURCE_REMOVE;
}

static void
check_presence (RogerAssistant *self)
{
  g_assert (self->router_uri != NULL);

  rm_profile_set_host (self->profile, self->router_uri);

  if (self->presence_check_id) {
    g_source_remove (self->presence_check_id);
    self->presence_check_id = 0;
  }
  self->presence_check_id = g_idle_add (check_router_presence, self);
}

static void
roger_assistant_router_listbox_row_selected (GtkListBox    *box,
                                             GtkListBoxRow *row,
                                             gpointer       user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);

  if (row) {
    /* We have a selected row, get child and set host internally */
    GtkWidget *child = gtk_container_get_children (GTK_CONTAINER (row))->data;
    char *host = g_object_get_data (G_OBJECT (child), "host");

    g_clear_pointer (&self->router_uri, g_free);
    self->router_uri = g_strdup (host);
  }

  check_presence (self);
}

static void
roger_assistant_router_entry_changed (GtkEditable *entry,
                                      gpointer     user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
  gboolean valid;

  /* Check for valid ip entry */
  valid = g_hostname_is_ip_address (text);

  g_clear_pointer (&self->router_uri, g_free);
  self->router_uri = g_strdup (text);

  if (valid)
    check_presence (self);
}

static void
roger_assistant_router_stack_switcher_button_release_event (GtkWidget *entry,
                                                            GdkEvent  *event,
                                                            gpointer   user_data)
{
  RogerAssistant *self = ROGER_ASSISTANT (user_data);
  GtkListBoxRow *row;
  const char *name = gtk_stack_get_visible_child_name (GTK_STACK (self->router_stack));

  gtk_widget_set_sensitive (self->next_button, FALSE);

  /* Update next button state depending on stack visible child and values */
  if (strcmp (name, "manual") == 0) {
    roger_assistant_router_entry_changed (GTK_EDITABLE (self->router_entry), self);
    return;
  }

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->router_listbox));
  gtk_widget_set_sensitive (self->next_button, row != NULL);
}

static void
assistant_router_page_setup (RogerAssistant *self)
{
  const char *name = gtk_entry_get_text (GTK_ENTRY (self->profile_name_entry));

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
  gtk_label_set_text (GTK_LABEL (self->loading_label), _("Get Settings"));
  if (rm_router_login (self->profile) && rm_router_get_settings (self->profile))
    ret = TRUE;

  if (self->needs_ftp) {
    const char *host = g_object_get_data (G_OBJECT (self->router_stack), "server");
    const char *ftp_user = gtk_entry_get_text (GTK_ENTRY (self->ftp_user_entry));
    const char *ftp_password = gtk_entry_get_text (GTK_ENTRY (self->ftp_password_entry));
    char *message;
    RmFtp *ftp;

    /* Test ftp login */
    gtk_label_set_text (GTK_LABEL (self->loading_label), _("Test FTP login"));
    ftp = rm_ftp_init (host);
    if (ftp) {
      if (!rm_ftp_login (ftp, ftp_user, ftp_password)) {
        /* Error: Could not login to ftp */
        message = g_strdup (_("Please check your ftp user/password."));
        rm_object_emit_message (_("Login failed"), message);
        rm_ftp_shutdown (ftp);
        ret = FALSE;

        g_task_return_boolean (task, ret);

        return;
      }

      rm_ftp_shutdown (ftp);

      /* Store FTP credentials */
      g_settings_set_string (self->profile->settings, "ftp-user", ftp_user);
      rm_password_set (self->profile, "ftp-password", ftp_password);
    }
  }

  /* Enable telnet & capi port */
  gtk_label_set_text (GTK_LABEL (self->loading_label), _("Enable Ports"));
  if (rm_router_dial_number (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_TELNET))
    rm_router_hangup (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_TELNET);

  if (rm_router_dial_number (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_CAPI))
    rm_router_hangup (self->profile, ROUTER_DIAL_PORT_AUTO, ROUTER_ENABLE_CAPI);

  /* Trigger network reconnect */
  gtk_label_set_text (GTK_LABEL (self->loading_label), _("Trigger Reconnect"));
  rm_netmonitor_reconnect ();

  g_task_return_boolean (task, ret);
}

static void
assistant_password_page_setup (RogerAssistant *self)
{
  gtk_widget_set_sensitive (self->back_button, TRUE);
  gtk_widget_set_sensitive (self->next_button, TRUE);

  gtk_widget_set_visible (self->ftp_user_label, self->needs_ftp);
  gtk_widget_set_visible (self->ftp_user_entry, self->needs_ftp);
  gtk_widget_set_visible (self->ftp_password_label, self->needs_ftp);
  gtk_widget_set_visible (self->ftp_password_entry, self->needs_ftp);
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
  const char *user = gtk_entry_get_text (GTK_ENTRY (self->user_entry));
  const char *password = gtk_entry_get_text (GTK_ENTRY (self->password_entry));

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

    gtk_widget_destroy (GTK_WIDGET (self));
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

static gboolean
roger_assistant_delete_event (GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   user_data)

{
  RogerAssistant *self = ROGER_ASSISTANT (widget);

  if (self->profile)
    rm_profile_remove (self->profile);

  return FALSE;
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
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, router_stack);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, router_listbox);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, router_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, user_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, password_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, ftp_user_label);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, ftp_user_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, ftp_password_label);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, ftp_password_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, loading_label);
  gtk_widget_class_bind_template_child (widget_class, RogerAssistant, start_button);

  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_back_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_next_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_profile_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_delete_event);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_router_stack_switcher_button_release_event);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_router_listbox_row_selected);
  gtk_widget_class_bind_template_callback (widget_class, roger_assistant_router_entry_changed);
}

static void
roger_assistant_init (RogerAssistant *self)
{
  GtkWidget *placeholder;
  g_autoptr (GList) childrens = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  placeholder = gtk_label_new (_("No router detected"));
  gtk_widget_show (placeholder);
  gtk_list_box_set_placeholder (GTK_LIST_BOX (self->router_listbox), placeholder);

  g_object_bind_property (self->user_entry, "text", self->ftp_user_entry, "text", G_BINDING_DEFAULT);
  g_object_bind_property (self->password_entry, "text", self->ftp_password_entry, "text", G_BINDING_DEFAULT);
  gtk_entry_set_text (GTK_ENTRY (self->ftp_user_entry), "ftpuser");

  gtk_widget_grab_focus (self->profile_name_entry);

  /* Set internal start point & limit */
  childrens = gtk_container_get_children (GTK_CONTAINER (self->stack));
  self->current_page = 0;
  self->max_page = childrens ? g_list_length (childrens) : 0;
}

GtkWidget *
roger_assistant_new (void)
{
  return g_object_new (ROGER_TYPE_ASSISTANT, NULL);
}
