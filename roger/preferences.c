/*
 * Roger Router
 * Copyright (c) 2012-2020 Jan-Michael Brummer
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

#include <roger/main.h>
#include <roger/preferences.h>
#include <roger/preferences-audio.h>
#include <roger/preferences-telephony.h>

G_DEFINE_TYPE (RogerPreferencesWindow, roger_preferences_window, HDY_TYPE_PREFERENCES_WINDOW)

typedef struct {
  RogerPreferencesWindow *window;
  GtkWidget *row;
  GtkWidget *toggle;
} RogerNotificationHelper;

static RogerNotificationHelper *
notification_helper_new (RogerPreferencesWindow *window,
                         GtkWidget              *row,
                         GtkWidget              *toggle)
{
  RogerNotificationHelper *self;

  self = g_malloc0 (sizeof (RogerNotificationHelper));
  self->window = window;
  self->row = row;
  self->toggle = toggle;

  return self;
}

static void
notification_helper_free (RogerNotificationHelper *self)
{
  g_free (self);
}

static gboolean
roger_notification_incoming_get_mapping (GValue   *value,
                                         GVariant *variant,
                                         gpointer  user_data)
{
  RogerNotificationHelper *helper = user_data;
  const gchar **numbers;
  gboolean active;

	numbers = g_variant_get_strv (variant, NULL);
  active = rm_strv_contains((const gchar *const *)numbers, hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (helper->row)));
  g_value_set_boolean (value, active);

  return TRUE;
}

static GVariant *
roger_notification_incoming_set_mapping (const GValue       *value,
                                         const GVariantType *expected_type,
                                         gpointer            user_data)
{
  RogerNotificationHelper *helper = user_data;
  g_autofree GStrv numbers = NULL;
  gboolean active;

  numbers = rm_profile_get_notification_incoming_numbers (helper->window->profile);
  active = g_value_get_boolean (value);

  if (active)
    numbers = rm_strv_add (numbers, hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (helper->row)));
  else
    numbers = rm_strv_remove (numbers, hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (helper->row)));

  return g_variant_new_strv ((const gchar * const *)numbers, -1);
}

static gboolean
roger_notification_outgoing_get_mapping (GValue   *value,
                                         GVariant *variant,
                                         gpointer  user_data)
{
  RogerNotificationHelper *helper = user_data;
  const gchar **numbers;
  gboolean active;

	numbers = g_variant_get_strv (variant, NULL);
  active = rm_strv_contains((const gchar *const *)numbers, hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (helper->row)));
  g_value_set_boolean (value, active);

  return TRUE;
}

static GVariant *
roger_notification_outgoing_set_mapping (const GValue       *value,
                                         const GVariantType *expected_type,
                                         gpointer            user_data)
{
  RogerNotificationHelper *helper = user_data;
  g_autofree GStrv numbers = NULL;
  gboolean active;

  numbers = rm_profile_get_notification_outgoing_numbers (helper->window->profile);
  active = g_value_get_boolean (value);

  if (active)
    numbers = rm_strv_add (numbers, hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (helper->row)));
  else
    numbers = rm_strv_remove (numbers, hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (helper->row)));

  return g_variant_new_strv ((const gchar * const *)numbers, -1);
}

static void
roger_preferences_edit_filter (GtkWidget *widget,
                               gpointer   data)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (data);
	RmFilter *filter;
	GtkWidget *dialog;
	GtkWidget *grid;
	GtkWidget *content;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *save;
	GSList *list;
	RmFilterRule *rule;
	GValue ptr = { 0 };
	GtkListStore *list_store;
	gint result;
  gint table_y = 0;
	gboolean use_header = TRUE;

  filter = g_object_get_data (G_OBJECT (widget), "filter");
  g_assert (filter != NULL);

	dialog = g_object_new (GTK_TYPE_DIALOG, "use-header-bar", use_header, NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Edit filter"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (self));
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);

	save = gtk_dialog_add_button (GTK_DIALOG (dialog), _("Save"), GTK_RESPONSE_OK);

	content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	grid = gtk_grid_new ();
	gtk_container_add (GTK_CONTAINER (content), grid);

	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  label = gtk_label_new (_("Name:"));
	gtk_grid_attach(GTK_GRID(grid), label, 0, table_y, 1, 1);

	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), filter->name);
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, table_y, 1, 1);
	table_y++;

	GtkWidget *type_in_grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (type_in_grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (type_in_grid), 12);

	/*GtkWidget *type_grid = pref_group_create(type_in_grid, _("Rules"), TRUE, FALSE);
	gtk_grid_attach(GTK_GRID(grid), type_grid, 0, table_y, 2, 1);
	table_y++;*/

	/*pref_filters_current_rules = NULL;
	for (list = filter->rules; list != NULL; list = list->next) {
		rule = list->data;

		pref_filters_add_rule(type_in_grid, rule);
	}

	pref_filters_current_rules = filter->rules;*/

	gtk_widget_show_all (grid);

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	if (filter->name)
		g_free(filter->name);

	filter->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	//filter->rules = pref_filters_current_rules;
}

static void
roger_preferences_setup_journal (RogerPreferencesWindow *self)
{
  g_autofree GStrv numbers = NULL;
  GtkWidget *row;
  GSList *list;
  gint idx;

  /* Incoming Notifications */
  numbers = rm_router_get_numbers (self->profile);

  for (idx = 0; idx < g_strv_length (numbers); idx++) {
    GtkWidget *toggle;
    RogerNotificationHelper *helper;

    row = GTK_WIDGET (hdy_action_row_new ());
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), numbers[idx]);

    toggle = gtk_switch_new ();
    gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
    gtk_container_add (GTK_CONTAINER (row), toggle);
    helper = notification_helper_new (self, row, toggle);
    g_settings_bind_with_mapping (self->profile->settings,
                                  "notification-incoming-numbers",
                                  toggle,
                                  "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  roger_notification_incoming_get_mapping,
                                  roger_notification_incoming_set_mapping,
                                  helper,
                                  NULL);

    gtk_container_add (GTK_CONTAINER (self->notification_incoming), row);

    row = GTK_WIDGET (hdy_action_row_new ());
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), numbers[idx]);

    toggle = gtk_switch_new ();
    gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
    gtk_container_add (GTK_CONTAINER (row), toggle);
    helper = notification_helper_new (self, row, toggle);
    g_settings_bind_with_mapping (self->profile->settings,
                                  "notification-outgoing-numbers",
                                  toggle,
                                  "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  roger_notification_outgoing_get_mapping,
                                  roger_notification_outgoing_set_mapping,
                                  helper,
                                  NULL);

    gtk_container_add (GTK_CONTAINER (self->notification_outgoing), row);
  }

  /* Filter */
	for (list = rm_filter_get_list (rm_profile_get_active()); list; list = list->next) {
    GtkWidget *edit;
		RmFilter *filter = list->data;

    row = GTK_WIDGET (hdy_action_row_new ());
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), filter->name);

    edit = gtk_button_new_from_icon_name ("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
    g_object_set_data (G_OBJECT (edit), "filter", filter);
    g_signal_connect (edit, "clicked", G_CALLBACK (roger_preferences_edit_filter), self);
    gtk_widget_set_valign (edit, GTK_ALIGN_CENTER);
    gtk_container_add (GTK_CONTAINER (row), edit);

    gtk_container_add (GTK_CONTAINER (self->filter), row);
	}

  row = GTK_WIDGET (hdy_action_row_new ());
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), _("Add filter"));
  gtk_container_add (GTK_CONTAINER (self->filter), row);
}

static void
roger_preferences_window_constructed (GObject *object)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (object);

  self->profile = rm_profile_get_active();
	if (!self->profile) {
		g_warning("No active profile, exiting preferences");

    return;
	}

  G_OBJECT_CLASS (roger_preferences_window_parent_class)->constructed (object);

  /* Router */
	g_settings_bind (self->profile->settings, "host", self->host, "text", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->profile->settings, "login-user", self->login_user, "text", G_SETTINGS_BIND_DEFAULT);
 	gtk_entry_set_text (GTK_ENTRY(self->login_password), rm_router_get_login_password (self->profile));

  /* Codes */
	g_settings_bind (self->profile->settings, "external-access-code", self->external_code, "text", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->profile->settings, "international-access-code", self->international_code, "text", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->profile->settings, "country-code", self->country_code, "text", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->profile->settings, "national-access-code", self->national_code, "text", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->profile->settings, "area-code", self->area_code, "text", G_SETTINGS_BIND_DEFAULT);

  /* Telephony */
  roger_preferences_setup_telephony (self);

  /* Journal */
  roger_preferences_setup_journal (self);

  /* Audio */
  roger_preferences_setup_audio (self);
}

static void
roger_preferences_window_class_init (RogerPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  //object_class->finalize = prefs_dialog_finalize;
  object_class->constructed = roger_preferences_window_constructed;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/tabos/roger/ui/preferences.ui");

  /* Router */
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, host);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, login_user);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, login_password);

  /* Codes */
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, external_code);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, international_code);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, country_code);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, national_code);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, area_code);

  /* Telephony */
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softphone);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softphone_group);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softphone_number);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softphone_controller);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_group);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_number);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_controller);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_header);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_ident);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_resolution);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_service);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_report);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_directory);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, softfax_ecm);

  /* Journal */
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, notification_incoming);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, notification_outgoing);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, filter);

  /* Audio */
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, ringtone);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, microphone);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, speaker);
  gtk_widget_class_bind_template_child (widget_class, RogerPreferencesWindow, ringer);
}

static void
roger_preferences_window_init (RogerPreferencesWindow *window)
{
  gtk_widget_init_template (GTK_WIDGET (window));
}

RogerPreferencesWindow *
roger_preferences_window_new (void)
{
  return g_object_new (ROGER_TYPE_PREFERENCES_WINDOW, NULL);
}

