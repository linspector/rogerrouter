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

#include "application.h"

#include "assistant.h"
#include "contacts.h"
#include "debug.h"
#include "fax.h"
#include "journal.h"
#include "phone.h"
#include "preferences.h"
#include "uitools.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>
#include <stdlib.h>
#include <string.h>

GtkApplication *roger_app;
static gboolean startup_called = FALSE;
GSettings *app_settings = NULL;
static RogerJournal *journal = NULL;

struct cmd_line_option_state {
  gboolean debug;
  gboolean start_hidden;
  gboolean quit;
  gboolean force_online;
  char *number;
  char *profile;
};

static struct cmd_line_option_state option_state;

void
app_show_contacts (void)
{
  app_contacts (NULL);
}

void
app_show_preferences (void)
{
  RogerPreferencesWindow *window = roger_preferences_window_new ();
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (journal));
  gtk_widget_show_all (GTK_WIDGET (window));
  /*app_show_settings(); */
}

void
app_show_help (void)
{
  char *uri = "http://www.tabos.org/forum";

  gtk_show_uri_on_window (GTK_WINDOW (journal_get_window ()), uri, gtk_get_current_event_time (), NULL);
}

void
app_quit (void)
{
  g_application_quit (G_APPLICATION (roger_app));
}

void
app_copy_ip (void)
{
  RmProfile *profile = rm_profile_get_active ();
  char *ip;

  ip = rm_router_get_ip (profile);
  if (ip) {
    g_debug ("%s(): IP: '%s'", __FUNCTION__, ip);
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE), ip, -1);
    g_free (ip);
  } else {
    g_warning ("Could not get ip");
  }
}

void
app_reconnect (void)
{
  RmProfile *profile = rm_profile_get_active ();

  rm_router_reconnect (profile);
}

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
  GtkBuilder *builder;
  GtkWidget *dialog;
  GtkWidget *description_label;
  GtkWidget *realm_label;
  GtkWidget *tmp;
  char *description;
  SoupURI *uri;

  g_debug ("%s(): called", __FUNCTION__);
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
  g_free (description);

  realm_label = GTK_WIDGET (gtk_builder_get_object (builder, "realm"));
  gtk_label_set_text (GTK_LABEL (realm_label), soup_auth_get_realm (auth_data->auth));

  tmp = GTK_WIDGET (gtk_builder_get_object (builder, "username_entry"));
  gtk_entry_set_text (GTK_ENTRY (tmp), auth_data->username);
  g_object_set_data (G_OBJECT (dialog), "user", tmp);
  tmp = GTK_WIDGET (gtk_builder_get_object (builder, "password_entry"));
  gtk_entry_set_text (GTK_ENTRY (tmp), auth_data->password);
  g_object_set_data (G_OBJECT (dialog), "password", tmp);

  gtk_builder_connect_signals (builder, NULL);
  g_object_unref (G_OBJECT (builder));

  g_signal_connect (dialog, "response", G_CALLBACK (auth_response_callback), auth_data);
  gtk_widget_show_all (dialog);
}

static void
application_shutdown (GObject *object)
{
  rm_shutdown ();

  g_object_unref (app_settings);
  app_settings = NULL;
}

static void
addressbook_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  app_show_contacts ();
}

static void
assistant_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  /*app_assistant(); */
  GtkWidget *assistant = roger_assistant_new ();
  gtk_window_set_transient_for (GTK_WINDOW (assistant), GTK_WINDOW (journal));
  gtk_window_set_modal (GTK_WINDOW (assistant), TRUE);
  gtk_widget_show (assistant);
}

static void
dialnumber_activated (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  app_phone (NULL, NULL);
}

static void
preferences_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  app_show_preferences ();
}

static void
donate_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  char *uri = "http://www.tabos.org/";

  gtk_show_uri_on_window (GTK_WINDOW (journal_get_window ()), uri, gtk_get_current_event_time (), NULL);
}

static void
forum_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  app_show_help ();
}

#define ABOUT_GROUP "About"

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GtkWidget *dialog = NULL;
  GtkWindow *window;
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
  gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (dialog), PACKAGE_BUGREPORT);
  gtk_about_dialog_set_website_label (GTK_ABOUT_DIALOG (dialog), _("Website"));
  gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (dialog), gdk_pixbuf_new_from_resource ("/org/tabos/roger/images/org.tabos.roger.svg", NULL));

  window = gtk_application_get_active_window (GTK_APPLICATION (user_data));
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
  app_quit ();
}

static void
copy_ip_activated (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  app_copy_ip ();
}

static void
reconnect_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  app_reconnect ();
}

static void
shortcuts_activated (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  static GtkWidget *shortcuts_window;
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (user_data));

  if (!shortcuts_window) {
    g_autoptr (GtkBuilder) builder = NULL;

    builder = gtk_builder_new_from_resource ("/org/tabos/roger/ui/shortcuts.ui");
    shortcuts_window = GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts_window"));

    g_signal_connect (shortcuts_window, "destroy", G_CALLBACK (gtk_widget_destroyed), &shortcuts_window);
  }

  if (gtk_window_get_transient_for (GTK_WINDOW (shortcuts_window)) != window)
    gtk_window_set_transient_for (GTK_WINDOW (shortcuts_window), window);

  gtk_window_present_with_time (GTK_WINDOW (shortcuts_window), gtk_get_current_event_time ());
}

void
app_pickup (RmConnection *connection)
{
  RmNotificationMessage *message;
  RmContact *contact;

  g_assert (connection != NULL);

  message = rm_notification_message_get (connection);
  g_assert (message != NULL);

  /* Close notification message */
  rm_notification_message_close (message);

  /** Ask for contact information */
  contact = rm_contact_find_by_number (connection->remote_number);

  /* Show phone window */
  app_phone (contact, connection);
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
  GtkWidget *journal_win = GTK_WIDGET (journal);

  gtk_widget_set_visible (GTK_WIDGET (journal_win), !gtk_widget_get_visible (GTK_WIDGET (journal_win)));
}

static void
debug_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  app_debug_window ();
}

static void
hideonquit_activated (GSimpleAction *simple,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  journal_set_hide_on_quit (journal, g_variant_get_boolean (parameter));
}

static void
hideonstart_activated (GSimpleAction *simple,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  journal_set_hide_on_start (journal, g_variant_get_boolean (parameter));
}

static GActionEntry apps_entries[] = {
  { "addressbook", addressbook_activated, NULL, NULL, NULL },
  { "assistant", assistant_activated, NULL, NULL, NULL },
  { "preferences", preferences_activated, NULL, NULL, NULL },
  { "phone", dialnumber_activated, NULL, NULL, NULL },
  { "copy_ip", copy_ip_activated, NULL, NULL, NULL },
  { "reconnect", reconnect_activated, NULL, NULL, NULL },
  { "donate", donate_activated, NULL, NULL, NULL },
  { "forum", forum_activated, NULL, NULL, NULL },
  { "about", about_activated, NULL, NULL, NULL },
  { "quit", quit_activated, NULL, NULL, NULL },
  { "pickup", pickup_activated, "i", NULL, NULL },
  { "hangup", hangup_activated, "i", NULL, NULL },
  { "journal", journal_activated, NULL, NULL, NULL },
  { "shortcuts", shortcuts_activated, NULL, NULL, NULL },
  { "debug", debug_activated, NULL, NULL, NULL },
  { "hideonquit", hideonquit_activated, "b" },
  { "hideonstart", hideonstart_activated, "b" },
};

static void
application_startup (GtkApplication *application)
{
  hdy_init ();
  startup_called = TRUE;
}

static void
rm_object_message_cb (RmObject *object,
                      char     *title,
                      char     *message,
                      gpointer  user_data)
{
  GtkWidget *dialog = gtk_message_dialog_new_with_markup (roger_app ? gtk_application_get_active_window (roger_app) : NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "<span weight=\"bold\" size=\"larger\">%s</span>", title);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message ? message : "");

  g_free (message);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
rm_object_profile_changed_cb (RmObject *object)
{
  journal_update_content (journal);
}

static void
app_init (GtkApplication *app)
{
  GError *error = NULL;

  const char *user_plugins = g_get_user_data_dir ();
  char *path = g_build_filename (user_plugins, "roger", G_DIR_SEPARATOR_S, "plugins", NULL);

  rm_plugins_add_search_path (path);
  rm_plugins_add_search_path (rm_get_directory (APP_PLUGINS));
  g_free (path);

  if (option_state.profile) {
    rm_set_requested_profile (option_state.profile);
  }

  rm_new (option_state.debug, &error);

  if (option_state.force_online) {
    rm_set_force_online (TRUE);
  }

  /* Set local bindings */
  g_debug ("%s(): locale: %s, package: '%s'", __FUNCTION__, rm_get_directory (APP_LOCALE), GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, rm_get_directory (APP_LOCALE));
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  const char *accels[] = { NULL, NULL, NULL, NULL };

  accels[0] = "<Primary>p";
  gtk_application_set_accels_for_action (app, "app.phone", accels);

  accels[0] = "F1";
  gtk_application_set_accels_for_action (app, "app.about", accels);

  accels[0] = "<Primary>b";
  gtk_application_set_accels_for_action (app, "app.addressbook", accels);

#if GTK_CHECK_VERSION (3, 20, 0)
  accels[0] = "<Primary>F1";
  gtk_application_set_accels_for_action (app, "app.shortcuts", accels);
#endif

  accels[0] = "<Primary>q";
  accels[1] = "<Primary>w";
  gtk_application_set_accels_for_action (app, "app.quit", accels);

  accels[0] = "<Primary>s";
  gtk_application_set_accels_for_action (app, "app.preferences", accels);

  accels[0] = "<Primary>d";
  gtk_application_set_accels_for_action (app, "app.debug", accels);

  g_action_map_add_action_entries (G_ACTION_MAP (app), apps_entries, G_N_ELEMENTS (apps_entries), app);

  if (option_state.debug || g_settings_get_boolean (app_settings, "debug")) {
    debug_activated (NULL, NULL, NULL);
  }

  g_signal_connect (rm_object, "authenticate", G_CALLBACK (app_authenticate_cb), NULL);
  g_signal_connect (rm_object, "message", G_CALLBACK (rm_object_message_cb), NULL);
  g_signal_connect (rm_object, "profile-changed", G_CALLBACK (rm_object_profile_changed_cb), NULL);

  journal = roger_journal_new ();

  if (rm_init (&error) == FALSE) {
    g_warning ("rm() failed: %s\n", error ? error->message : "");

    GtkWidget *dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "RM failed");
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error ? error->message : "");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    g_clear_error (&error);
    return;
  }

  fax_process_init ();


  if (option_state.start_hidden) {
    journal_set_hide_on_start (journal, TRUE);
    journal_set_hide_on_quit (journal, TRUE);
  }

  /*journal_window(G_APPLICATION(app)); */
  gtk_widget_show (GTK_WIDGET (journal));
  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (journal));
}

G_GNUC_NORETURN static gboolean
option_version_cb (const char  *option_name,
                   const char  *value,
                   gpointer     data,
                   GError     **error)
{
  g_message ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  exit (0);
}

const GOptionEntry all_options[] = {
  { "debug", 'd', 0, G_OPTION_ARG_NONE, &option_state.debug, "Enable debug", NULL },
  { "hidden", 'i', 0, G_OPTION_ARG_NONE, &option_state.start_hidden, "Start with hidden window", NULL },
  { "quit", 'q', 0, G_OPTION_ARG_NONE, &option_state.quit, "Quit", NULL },
  { "call", 'c', 0, G_OPTION_ARG_STRING, &option_state.number, "Remote phone number", NULL },
  { "version", 'v', G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
  { "profile", 'p', 0, G_OPTION_ARG_STRING, &option_state.profile, "Profile name", NULL },
  { "force-online", 'f', 0, G_OPTION_ARG_NONE, &option_state.force_online, "Force online", NULL },
  { NULL }
};

GOptionContext *
application_options_get_context (void)
{
  GOptionContext *context;

  context = g_option_context_new ("-");
  g_option_context_add_main_entries (context, all_options, GETTEXT_PACKAGE);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (FALSE));

  return context;
}

static void
application_options_process (GtkApplication                     *app,
                             const struct cmd_line_option_state *options)
{
  if (options->quit) {
    gdk_notify_startup_complete ();
    exit (0);
  }
}

/**
 * application_command_line_cb:
 * @app: a #GtkApplication
 * @command_line: a #GApplicationCommandLine
 * @data: unused
 *
 * Handles command line parameters of application
 *
 * Returns: 0 on success, otherwise != 0 on error
 */
static gint
application_command_line_cb (GtkApplication          *app,
                             GApplicationCommandLine *command_line,
                             gpointer                 data)
{
  GOptionContext *context;
  int argc;
  char **argv;

  /* Get arguments */
  argv = g_application_command_line_get_arguments (command_line, &argc);

  memset (&option_state, 0, sizeof (option_state));

  context = application_options_get_context ();
  g_option_context_set_help_enabled (context, TRUE);
  if (g_option_context_parse (context, &argc, &argv, NULL) == FALSE) {
    g_option_context_free (context);
    g_strfreev (argv);
    return 1;
  }

  /* In case we have more than one argument, guess that it is a number */
  if (argc > 1) {
    /* Guess it is a number to call */
    char *number = argv[1];
    option_state.number = number;
  }

  g_option_context_free (context);

  application_options_process (app, &option_state);

  g_debug ("%s(): Startup_called?: %d", __FUNCTION__, startup_called);
  if (startup_called) {
    /* Initialize app and mark startup as done */
    app_init (app);
    gdk_notify_startup_complete ();
    startup_called = FALSE;
  } else {
    /* Application is already running, present journal window */
    /*gtk_widget_set_visible(GTK_WIDGET(journal_get_window ()), TRUE); */
  }

  /* Check if we should start hidden */
  if (!option_state.start_hidden) {
    /* In case we have a number, setup phone window */
    if (option_state.number) {
      RmContact *contact;
      char *full_number;

      g_debug ("%s(): number: %s", __FUNCTION__, option_state.number);
      full_number = rm_number_full (option_state.number, FALSE);

      /** Ask for contact information */
      contact = rm_contact_find_by_number (full_number);

      app_phone (contact, NULL);
      g_free (full_number);
    }
  }

  g_strfreev (argv);

  return 0;
}

/**
 * application_activated:
 * @app: a #GtkApplication
 * @data: unused
 *
 * Reacts on activate signal.
 */
static void
application_activate_cb (GtkApplication *app,
                         gpointer        data)
{
  journal_activated (NULL, NULL, NULL);
}

/**
 * application_new:
 *
 * Creates a new #GtkApplication
 *
 * Retuns: a #GtkApplication
 */
GtkApplication *
application_new (void)
{
  /* Set application name */
  g_set_prgname (PACKAGE_NAME);
  g_set_application_name (PACKAGE_NAME);

  /* Create application */
  roger_app = gtk_application_new (APP_GSETTINGS_SCHEMA, G_APPLICATION_HANDLES_COMMAND_LINE);
  gtk_window_set_default_icon_name ("org.tabos.roger");

  /* Create application settings */
  app_settings = rm_settings_new (APP_GSETTINGS_SCHEMA);

  /* Connect to application signals */
  g_signal_connect (roger_app, "activate", G_CALLBACK (application_activate_cb), roger_app);
  g_signal_connect (roger_app, "startup", G_CALLBACK (application_startup), roger_app);
  g_signal_connect (roger_app, "shutdown", G_CALLBACK (application_shutdown), roger_app);
  g_signal_connect (roger_app, "command-line", G_CALLBACK (application_command_line_cb), roger_app);

  return roger_app;
}

void
app_present_journal (void)
{
  journal_set_visible (journal, TRUE);
}
