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

#include "roger-shell.h"

#include "contacts.h"
#include "preferences.h"
#include "roger-assistant.h"
#include "roger-fax.h"
#include "roger-journal.h"
#include "roger-phone.h"
#include "roger-settings.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>
#include <stdlib.h>
#include <string.h>

struct _RogerShell {
  GtkApplication parent_instance;

  GObject *rm;
  GtkWidget *assistant;
  GtkWidget *journal;
  GtkWidget *debug;
  GtkWidget *phone;

  char *call_number;
};

enum {
  PROP_0,
  PROP_CALL_NUMBER,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static RogerShell *roger_shell = NULL;
static gboolean start_in_background = FALSE;
static gboolean debug_enabled = FALSE;
static char *startup_profile = NULL;
static char *call_number = NULL;

G_DEFINE_TYPE (RogerShell, roger_shell, GTK_TYPE_APPLICATION)

static void
auth_response_callback (GtkDialog *dialog,
                        gint       response_id,
                        gpointer   user_data)
{
  RmAuthData *auth_data = user_data;

  if (response_id == 1) {
    GtkWidget *user_entry = g_object_get_data (G_OBJECT (dialog), "user");
    GtkWidget *password_entry = g_object_get_data (G_OBJECT (dialog), "password");

    auth_data->username = g_strdup (gtk_entry_get_text (GTK_ENTRY (user_entry)));
    auth_data->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (password_entry)));
  }

  rm_network_authenticate (response_id == 1, auth_data);

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
app_authenticate_cb (RmObject   *app,
                     RmAuthData *auth_data)
{
  g_autoptr (GtkBuilder) builder = NULL;
  GtkWidget *dialog;
  GtkWidget *description_label;
  GtkWidget *realm_label;
  GtkWidget *tmp;
  g_autofree char *description = NULL;
  SoupURI *uri;

  builder = gtk_builder_new_from_resource ("/org/tabos/roger/authenticate.glade");
  if (!builder) {
    g_warning ("Could not load authentication ui");
    return;
  }

  /* Connect to builder objects */
  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "authentication"));

  description_label = GTK_WIDGET (gtk_builder_get_object (builder, "description"));
  uri = soup_message_get_uri (auth_data->msg);
  description = g_strdup_printf (_("A username and password are being requested by the site %s"), uri->host);
  gtk_label_set_text (GTK_LABEL (description_label), description);

  realm_label = GTK_WIDGET (gtk_builder_get_object (builder, "realm"));
  gtk_label_set_text (GTK_LABEL (realm_label), soup_auth_get_realm (auth_data->auth));

  tmp = GTK_WIDGET (gtk_builder_get_object (builder, "username_entry"));
  gtk_entry_set_text (GTK_ENTRY (tmp), auth_data->username);
  g_object_set_data (G_OBJECT (dialog), "user", tmp);
  tmp = GTK_WIDGET (gtk_builder_get_object (builder, "password_entry"));
  gtk_entry_set_text (GTK_ENTRY (tmp), auth_data->password);
  g_object_set_data (G_OBJECT (dialog), "password", tmp);

  gtk_builder_connect_signals (builder, NULL);

  g_signal_connect (dialog, "response", G_CALLBACK (auth_response_callback), auth_data);
  gtk_widget_show_all (dialog);
}

static void
addressbook_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  app_contacts (NULL);
}

static void
assistant_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);
  GtkWidget *assistant;

  assistant = roger_assistant_new ();
  gtk_window_set_transient_for (GTK_WINDOW (assistant), GTK_WINDOW (roger_shell_get_journal (self)));
  gtk_window_set_modal (GTK_WINDOW (assistant), TRUE);
  gtk_widget_show (assistant);
}

void
roger_shell_phone (RogerShell *self,
                   char       *number)
{
  GtkWidget *phone = roger_phone_new ();

  if (number)
    roger_phone_set_dial_number (ROGER_PHONE (phone), number);

  gtk_window_set_transient_for (GTK_WINDOW (phone), GTK_WINDOW (roger_shell_get_journal (self)));
  gtk_window_set_modal (GTK_WINDOW (phone), TRUE);
  gtk_widget_show_all (phone);
}

static void
phone_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);

  roger_shell_phone (self, NULL);
}

static void
preferences_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);
  RogerPreferencesWindow *preferences;

  preferences = roger_preferences_window_new ();
  gtk_window_set_transient_for (GTK_WINDOW (preferences), GTK_WINDOW (roger_shell_get_journal (self)));
  gtk_widget_show_all (GTK_WIDGET (preferences));
}

#define ABOUT_GROUP "About"

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);
  GtkWidget *dialog = NULL;
  GtkWidget *window;
  g_autoptr (GKeyFile) key_file = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_auto (GStrv) authors = NULL;
  g_auto (GStrv) documenters = NULL;
  g_auto (GStrv) artists = NULL;

  key_file = g_key_file_new ();
  bytes = g_resources_lookup_data ("/org/tabos/roger/about.ini", 0, NULL);
  if (!g_key_file_load_from_data (key_file, g_bytes_get_data (bytes, NULL), -1, 0, &error)) {
    g_warning ("Couldn't load about data: %s\n", error ? error->message : "");
    return;
  }

  authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Author", NULL, NULL);
  documenters = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", NULL, NULL);
  artists = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", NULL, NULL);

  dialog = gtk_about_dialog_new ();

  gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (dialog), PACKAGE_NAME);
  gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), PACKAGE_VERSION);
  gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (dialog), "(C) 2012-2021, Jan-Michael Brummer <jan.brummer@tabos.org>");
  gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (dialog), _("FRITZ!Box Journal, Soft/phone, and Fax\nDedicated to my father"));
  gtk_about_dialog_set_license_type (GTK_ABOUT_DIALOG (dialog), GTK_LICENSE_GPL_2_0_ONLY);
  gtk_about_dialog_set_wrap_license (GTK_ABOUT_DIALOG (dialog), TRUE);
  gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (dialog), (const char **)authors);
  gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (dialog), (const char **)documenters);
  gtk_about_dialog_set_artists (GTK_ABOUT_DIALOG (dialog), (const char **)artists);
  gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (dialog), "https://www.tabos.org");
  gtk_about_dialog_set_website_label (GTK_ABOUT_DIALOG (dialog), _("Website"));
  gtk_about_dialog_set_logo_icon_name (GTK_ABOUT_DIALOG (dialog), "org.tabos.roger");

  window = roger_shell_get_journal (self);
  if (window) {
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  }

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);

  g_application_quit (G_APPLICATION (self));
}

static void
copy_ip_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  RmProfile *profile = rm_profile_get_active ();
  char *ip;

  ip = rm_router_get_ip (profile);
  if (ip) {
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE), ip, -1);
    g_free (ip);
  } else {
    g_warning ("Could not get IP address");
  }
}

static void
reconnect_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  RmProfile *profile = rm_profile_get_active ();

  rm_router_reconnect (profile);
}

static void
shortcuts_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);
  static GtkWidget *shortcuts_window;

  if (!shortcuts_window) {
    g_autoptr (GtkBuilder) builder = NULL;

    builder = gtk_builder_new_from_resource ("/org/tabos/roger/ui/shortcuts.ui");
    shortcuts_window = GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts_window"));

    g_signal_connect (shortcuts_window, "destroy", G_CALLBACK (gtk_widget_destroyed), &shortcuts_window);
  }

  if (gtk_window_get_transient_for (GTK_WINDOW (shortcuts_window)) != GTK_WINDOW (roger_shell_get_journal (self)))
    gtk_window_set_transient_for (GTK_WINDOW (shortcuts_window), GTK_WINDOW (roger_shell_get_journal (self)));

  gtk_window_present_with_time (GTK_WINDOW (shortcuts_window), gtk_get_current_event_time ());
}

void
app_pickup (RmConnection *connection)
{
  RmNotificationMessage *message;
  GtkWidget *phone;

  g_assert (connection != NULL);

  message = rm_notification_message_get (connection);
  g_assert (message != NULL);

  /* Close notification message */
  rm_notification_message_close (message);

  phone = roger_phone_new ();
  roger_phone_pickup_connection (ROGER_PHONE (phone), connection);
  gtk_widget_show_all (phone);
}

static void
pickup_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  RmConnection *connection;
  gint32 id = g_variant_get_int32 (parameter);

  connection = rm_connection_find_by_id (id);

  app_pickup (connection);
}

void
app_hangup (RmConnection *connection)
{
  RmNotificationMessage *message;

  g_assert (connection != NULL);

  message = rm_notification_message_get (connection);
  g_assert (message != NULL);

  /* Close notification message */
  rm_notification_message_close (message);

  rm_phone_hangup (connection);
}

static void
hangup_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  RmConnection *connection;
  gint32 id = g_variant_get_int32 (parameter);

  connection = rm_connection_find_by_id (id);

  app_hangup (connection);
}

static void
journal_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);

  gtk_widget_set_visible (GTK_WIDGET (roger_shell_get_journal (self)), !gtk_widget_get_visible (roger_shell_get_journal (self)));
}

static GActionEntry apps_entries[] = {
  { "addressbook", addressbook_activated, NULL, NULL, NULL },
  { "assistant", assistant_activated, NULL, NULL, NULL },
  { "preferences", preferences_activated, NULL, NULL, NULL },
  { "phone", phone_activated, NULL, NULL, NULL },
  { "copy_ip", copy_ip_activated, NULL, NULL, NULL },
  { "reconnect", reconnect_activated, NULL, NULL, NULL },
  { "about", about_activated, NULL, NULL, NULL },
  { "quit", quit_activated, NULL, NULL, NULL },
  { "pickup", pickup_activated, "i", NULL, NULL },
  { "hangup", hangup_activated, "i", NULL, NULL },
  { "journal", journal_activated, NULL, NULL, NULL },
  { "shortcuts", shortcuts_activated, NULL, NULL, NULL },
};

static void
rm_object_message_cb (RmObject *object,
                      char     *title,
                      char     *message,
                      gpointer  user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);
  GtkWidget *dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (roger_shell_get_journal (self)),
                                                          GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                                          GTK_MESSAGE_INFO,
                                                          GTK_BUTTONS_CLOSE,
                                                          "<span weight=\"bold\" size=\"larger\">%s</span>",
                                                          title);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message ? message : "");

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
rm_object_profile_changed_cb (RmObject *object)
{
  RogerShell *self = roger_shell_get_default ();

  roger_journal_reload (ROGER_JOURNAL (roger_shell_get_journal (self)));
}

static void
rm_object_fax_process_cb (GtkWidget *widget,
                          char      *file_name,
                          gpointer   user_data)
{
  RogerShell *self = ROGER_SHELL (user_data);
  GtkWidget *fax = roger_fax_new ();

  roger_fax_set_transfer_file (ROGER_FAX (fax), file_name);
  gtk_window_set_transient_for (GTK_WINDOW (fax), GTK_WINDOW (roger_shell_get_journal (self)));
  gtk_window_set_modal (GTK_WINDOW (fax), TRUE);
  gtk_widget_show_all (fax);
}

const struct {
  const char *action_and_target;
  const char *accelerators[9];
} accels [] = {
  { "app.about", { "F1", NULL } },
  { "app.contacts", { "<Primary>b", NULL } },
  { "app.debug", { "<Primary>d", NULL } },
  { "app.phone", { "<Primary>p", NULL } },
  { "app.preferences", { "<Primary>s", NULL } },
  { "app.shortcuts", { "<Primary>F1", NULL } },
  { "app.quit", { "<Primary>q", "<Primary>w", NULL } },
};

static void
roger_shell_startup (GApplication *application)
{
  G_APPLICATION_CLASS (roger_shell_parent_class)->startup (application);

  hdy_init ();
}

static void
roger_shell_init (RogerShell *self)
{
  RogerShell **ptr = &roger_shell;

  g_assert (!roger_shell);
  roger_shell = self;
  g_object_add_weak_pointer (G_OBJECT (roger_shell), (gpointer *)ptr);
}

static void
roger_shell_dispose (GObject *object)
{
  rm_shutdown ();
  G_OBJECT_CLASS (roger_shell_parent_class)->dispose (object);
}

static void
roger_shell_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (roger_shell_parent_class)->constructed)
    G_OBJECT_CLASS (roger_shell_parent_class)->constructed (object);
}

static void
roger_shell_activate (GApplication *application)
{
  RogerShell *self = ROGER_SHELL (application);
  GList *list = gtk_application_get_windows (GTK_APPLICATION (self));
  g_autoptr (GError) error = NULL;
  guint i;

  if (g_list_length (list)) {
    gtk_window_present (GTK_WINDOW (roger_shell_get_journal (self)));

    if (self->call_number && strlen (self->call_number) > 0)
      roger_shell_phone (self, self->call_number);

    return;
  }

  for (i = 0; i < G_N_ELEMENTS (accels); i++) {
    gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                           accels[i].action_and_target,
                                           accels[i].accelerators);
  }
  g_action_map_add_action_entries (G_ACTION_MAP (self), apps_entries, G_N_ELEMENTS (apps_entries), self);

  const char *user_plugins = g_get_user_data_dir ();
  char *path = g_build_filename (user_plugins, "roger", G_DIR_SEPARATOR_S, "plugins", NULL);

  rm_plugins_add_search_path (path);
  rm_plugins_add_search_path (rm_get_directory (APP_PLUGINS));

  rm_new (debug_enabled, &error);
  self->rm = rm_object;
  g_signal_connect_object (self->rm, "authenticate", G_CALLBACK (app_authenticate_cb), self, 0);
  g_signal_connect_object (self->rm, "message", G_CALLBACK (rm_object_message_cb), self, 0);
  g_signal_connect_object (self->rm, "profile-changed", G_CALLBACK (rm_object_profile_changed_cb), self, 0);
  g_signal_connect_object (self->rm, "fax-process", G_CALLBACK (rm_object_fax_process_cb), self, 0);

  self->journal = roger_journal_new ();

  if (rm_init (&error) == FALSE) {
    g_warning ("rm failed: %s\n", error ? error->message : "");

    GtkWidget *dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "RM failed");
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error ? error->message : "");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    g_clear_error (&error);
    return;
  }

  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (roger_shell_get_journal (self)));

  if (start_in_background) {
    journal_set_hide_on_start (ROGER_JOURNAL (roger_shell_get_journal (self)), TRUE);
    journal_set_hide_on_quit (ROGER_JOURNAL (roger_shell_get_journal (self)), TRUE);
  } else {
    gtk_widget_show (GTK_WIDGET (roger_shell_get_journal (self)));
  }
}

static void
roger_shell_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
roger_shell_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  RogerShell *self = ROGER_SHELL (object);

  switch (prop_id) {
    case PROP_CALL_NUMBER:
      self->call_number = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
roger_shell_add_platform_data (GApplication    *application,
                               GVariantBuilder *builder)
{
  RogerShell *self = ROGER_SHELL (application);
  GVariantBuilder *ctx_builder;

  G_APPLICATION_CLASS (roger_shell_parent_class)->add_platform_data (application,
                                                                     builder);

  ctx_builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

  if (self->call_number)
    g_variant_builder_add (ctx_builder, "{iv}", 0, g_variant_new_string (self->call_number));

  g_variant_builder_add (builder, "{sv}",
                       "roger-shell-startup-context",
                       g_variant_builder_end (ctx_builder));

  g_variant_builder_unref (ctx_builder);
}

static void
roger_shell_before_emit (GApplication *application,
                         GVariant     *platform_data)
{
  GVariantIter iter, ctx_iter;
  const char *key;
  int ctx_key;
  GVariant *value, *ctx_value;
  RogerShell *self = ROGER_SHELL (application);

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value)) {
    if (strcmp (key, "roger-shell-startup-context") == 0) {
      g_variant_iter_init (&ctx_iter, value);
      while (g_variant_iter_loop (&ctx_iter, "{iv}", &ctx_key, &ctx_value)) {
        switch (ctx_key) {
          case 0:
            self->call_number = g_variant_dup_string (ctx_value, NULL);
            break;
          default:
            g_assert_not_reached ();
            break;
        }
      }
      break;
    }
  }

  G_APPLICATION_CLASS (roger_shell_parent_class)->before_emit (application,
                                                               platform_data);
}

static void
roger_shell_class_init (RogerShellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = roger_shell_dispose;
  object_class->constructed = roger_shell_constructed;
  object_class->get_property = roger_shell_get_property;
  object_class->set_property = roger_shell_set_property;

  application_class->startup = roger_shell_startup;
  application_class->activate = roger_shell_activate;
  application_class->before_emit = roger_shell_before_emit;
  application_class->add_platform_data = roger_shell_add_platform_data;

  object_properties[PROP_CALL_NUMBER] =
    g_param_spec_string ("call-number",
                       "Call number",
                       "The number to call.",
                       "",
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);
}

RogerShell *
roger_shell_get_default (void)
{
  return roger_shell;
}

GtkWidget *
roger_shell_get_journal (RogerShell *self)
{
  return self->journal;
}

RogerShell *
roger_shell_new (char *call_number)
{
  return ROGER_SHELL (g_object_new (ROGER_TYPE_SHELL,
                                    "application-id", "org.tabos.roger",
                                    "call-number", call_number,
                                    NULL));
}

static gboolean
option_version_cb (const char  *option_name,
                   const char  *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);

  exit (0);

  return FALSE;
}

const GOptionEntry option_entries[] = {
  { "background", 'b', 0, G_OPTION_ARG_NONE, &start_in_background, "Start in background", NULL },
  { "call", 'c', 0, G_OPTION_ARG_STRING, &call_number, "Call a number", NULL },
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &startup_profile, "Start in custom profile", NULL },
  { "version", 'v', G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
  { NULL }
};

int
main (int    argc,
      char **argv)
{
  g_autoptr (GOptionContext) option_context = NULL;
  g_autoptr (GOptionGroup) option_group = NULL;
  RogerShell *shell = NULL;
  g_autoptr (GError) error = NULL;
  int status;

  bindtextdomain (GETTEXT_PACKAGE, rm_get_directory (APP_LOCALE));
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Set application name */
  g_set_prgname (PACKAGE_NAME);
  g_set_application_name (PACKAGE_NAME);
  gtk_window_set_default_icon_name ("org.tabos.roger");

  option_context = g_option_context_new ("");
  option_group = g_option_group_new ("roger",
                                     N_("Roger Router"),
                                     N_("Roger Router options"),
                                     NULL, NULL);

  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
  g_option_group_add_entries (option_group, option_entries);
  g_option_context_set_main_group (option_context, option_group);
  g_option_context_add_group (option_context, gtk_get_option_group (TRUE));

  if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
    g_print ("Failed to parse arguments: %s\n", error->message);
    g_option_context_free (option_context);
    exit (1);
  }

  if (startup_profile)
    rm_set_requested_profile (startup_profile);

  shell = roger_shell_new (call_number);

  status = g_application_run (G_APPLICATION (shell), argc, argv);

  roger_settings_shutdown ();

  return status;
}

