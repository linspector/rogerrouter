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

#include "preferences.h"
#include "preferences-audio.h"
#include "preferences-telephony.h"
#include "roger-type-builtins.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>

static gboolean
roger_phone_number_get_mapping (GValue   *value,
                                GVariant *variant,
                                gpointer  user_data)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (user_data);
  g_autofree GStrv numbers = NULL;
  guint idx = 0;

  numbers = rm_router_get_numbers (self->profile);
  for (idx = 0; idx < g_strv_length (numbers); idx++) {
    if (g_strcmp0 (numbers[idx], g_variant_get_string (variant, NULL)) == 0) {
      g_value_set_uint (value, idx);

      rm_profile_update_numbers (rm_profile_get_active ());
      return TRUE;
    }
  }

  rm_profile_update_numbers (rm_profile_get_active ());
  return TRUE;
}

static GVariant *
roger_phone_number_set_mapping (const GValue       *value,
                                const GVariantType *expected_type,
                                gpointer            user_data)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (user_data);
  g_autofree GStrv numbers = NULL;
  guint idx;

  rm_profile_update_numbers (rm_profile_get_active ());
  numbers = rm_router_get_numbers (self->profile);

  idx = g_value_get_uint (value);

  if (g_strv_length (numbers) < idx)
    return g_variant_new_string ("");

  return g_variant_new_string (numbers[idx]);
}

/* static void */
/* softfax_directory_file_set (GtkFileChooserButton *widget, */
/*                             gpointer              user_data) */
/* { */
/*   RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (user_data); */
/*   const char *dir = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget)); */

/*   g_settings_set_string (self->profile->settings, "fax-report-dir", dir); */
/* } */

void
roger_preferences_setup_telephony (RogerPreferencesWindow *self)
{
  GtkStringList *number_list;
  GtkStringList *controllers;
  GtkStringList *resolutions;
  GtkStringList *services;
  g_autofree GStrv numbers = NULL;
  guint idx;

  /* Devices */
  g_settings_bind (self->profile->settings, "phone-active", self->softphone, "active", G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->softphone, "active", self->softphone_group, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_settings_bind (self->profile->settings, "fax-active", self->softfax, "active", G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->softfax, "active", self->softfax_group, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Phone */
  number_list = gtk_string_list_new (NULL);

  numbers = rm_router_get_numbers (self->profile);
  for (idx = 0; idx < g_strv_length (numbers); idx++)
    gtk_string_list_append (number_list, numbers[idx]);

  adw_combo_row_set_model (ADW_COMBO_ROW (self->softphone_number),
                           G_LIST_MODEL (number_list));

  g_settings_bind_with_mapping (self->profile->settings,
                                "phone-number",
                                self->softphone_number,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_phone_number_get_mapping,
                                roger_phone_number_set_mapping,
                                self,
                                NULL);

  controllers = gtk_string_list_new (NULL);
  gtk_string_list_append (controllers, _("ISDN 1"));
  gtk_string_list_append (controllers, _("ISDN 2"));
  gtk_string_list_append (controllers, _("System ISDN"));
  gtk_string_list_append (controllers, _("System Analog"));
  gtk_string_list_append (controllers, _("Internet 1"));
  gtk_string_list_append (controllers, _("Internet 2"));

  adw_combo_row_set_model (ADW_COMBO_ROW (self->softphone_controller),
                           G_LIST_MODEL (controllers));
  g_settings_bind (self->profile->settings, "phone-controller", self->softphone_controller, "selected", G_SETTINGS_BIND_DEFAULT);

  /* Fax */
  adw_combo_row_set_model (ADW_COMBO_ROW (self->softfax_number),
                           G_LIST_MODEL (number_list));
  g_settings_bind_with_mapping (self->profile->settings,
                                "fax-number",
                                self->softfax_number,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_phone_number_get_mapping,
                                roger_phone_number_set_mapping,
                                self,
                                NULL);

  adw_combo_row_set_model (ADW_COMBO_ROW (self->softfax_controller),
                           G_LIST_MODEL (controllers));
  g_settings_bind (self->profile->settings, "fax-controller", self->softfax_controller, "selected", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->profile->settings, "fax-header", self->softfax_header, "text", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->profile->settings, "fax-ident", self->softfax_ident, "text", G_SETTINGS_BIND_DEFAULT);

  resolutions = gtk_string_list_new (NULL);
  gtk_string_list_append (resolutions, _("Low (98dpi)"));
  gtk_string_list_append (resolutions, _("High (196dpi)"));

  adw_combo_row_set_model (ADW_COMBO_ROW (self->softfax_resolution),
                           G_LIST_MODEL (resolutions));
  g_settings_bind (self->profile->settings, "fax-resolution", self->softfax_resolution, "selected", G_SETTINGS_BIND_DEFAULT);

  services = gtk_string_list_new (NULL);
  gtk_string_list_append (services, _("Analog"));
  gtk_string_list_append (services, _("ISDN"));
  adw_combo_row_set_model (ADW_COMBO_ROW (self->softfax_service),
                           G_LIST_MODEL (services));
  g_settings_bind (self->profile->settings, "fax-cip", self->softfax_service, "selected", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->profile->settings, "fax-cip", self->softfax_service, "selected", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->profile->settings, "fax-report", self->softfax_report, "enable-expansion", G_SETTINGS_BIND_DEFAULT);
  /* gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->softfax_directory), g_settings_get_string (self->profile->settings, "fax-report-dir")); */
  /* g_signal_connect (self->softfax_directory, "file-set", G_CALLBACK (softfax_directory_file_set), self); */
  g_settings_bind (self->profile->settings, "fax-ecm", self->softfax_ecm, "active", G_SETTINGS_BIND_DEFAULT);
}
