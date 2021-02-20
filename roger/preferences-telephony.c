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

#include "roger-type-builtins.h"

static gchar *
controller_get_name (HdyEnumValueObject *value,
                     gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
    case RM_CONTROLLER_ISDN1:
      return g_strdup (_("ISDN 1"));
    case RM_CONTROLLER_ISDN2:
      return g_strdup (_("ISDN 2"));
    case RM_CONTROLLER_SYSTEM_ISDN:
      return g_strdup (_("System ISDN"));
    case RM_CONTROLLER_SYSTEM_ANALOG:
      return g_strdup (_("System Analog"));
    case RM_CONTROLLER_INTERNET1:
      return g_strdup (_("Internet 1"));
    case RM_CONTROLLER_INTERNET2:
      return g_strdup (_("Internet 2"));
    default:
      return NULL;
  }
}

static gboolean
roger_phone_number_get_mapping (GValue   *value,
                                GVariant *variant,
                                gpointer  user_data)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (user_data);
  g_autofree GStrv numbers = NULL;
  gint idx = 0;

  numbers = rm_router_get_numbers (self->profile);
  for (idx = 0; idx < g_strv_length (numbers); idx++) {
    if (g_strcmp0 (numbers[idx], g_variant_get_string (variant, NULL)) == 0) {
      g_value_set_int (value, idx);

      return TRUE;
    }
  }

  return TRUE;
}

static GVariant *
roger_phone_number_set_mapping (const GValue       *value,
                                const GVariantType *expected_type,
                                gpointer            user_data)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (user_data);
  g_autofree GStrv numbers = NULL;
  gint idx;

  numbers = rm_router_get_numbers (self->profile);

  idx = g_value_get_int (value);

  if (g_strv_length (numbers) < idx)
    return g_variant_new_string ("");

  return g_variant_new_string (numbers[idx]);
}

static gchar *
resolution_get_name (HdyEnumValueObject *value,
                     gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
    case RM_RESOLUTION_LOW:
      return g_strdup (_("Low (98dpi)"));
    case RM_RESOLUTION_HIGH:
      return g_strdup (_("High (196dpi)"));
    default:
      return NULL;
  }
}

static gchar *
service_get_name (HdyEnumValueObject *value,
                  gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
    case RM_SERVICE_ANALOG:
      return g_strdup (_("Analog"));
    case RM_SERVICE_ISDN:
      return g_strdup (_("ISDN"));
    default:
      return NULL;
  }
}

static void
softfax_directory_file_set (GtkFileChooserButton *widget,
                            gpointer              user_data)
{
  RogerPreferencesWindow *self = ROGER_PREFERENCES_WINDOW (user_data);
  const char *dir = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

  g_settings_set_string (self->profile->settings, "fax-report-dir", dir);
}

void
roger_preferences_setup_telephony (RogerPreferencesWindow *self)
{
  GListStore *number_list;
  g_autofree GStrv numbers = NULL;
  gint idx;

  /* Devices */
  g_settings_bind (self->profile->settings, "phone-active", self->softphone, "active", G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->softphone, "active", self->softphone_group, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_settings_bind (self->profile->settings, "fax-active", self->softfax, "active", G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->softfax, "active", self->softfax_group, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Phone */
  number_list = g_list_store_new (HDY_TYPE_VALUE_OBJECT);

  numbers = rm_router_get_numbers (self->profile);
  for (idx = 0; idx < g_strv_length (numbers); idx++) {
    HdyValueObject *obj;

    obj = hdy_value_object_new_string (numbers[idx]);
    g_list_store_append (number_list, obj);

    g_clear_object (&obj);
  }

  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->softphone_number),
                                 G_LIST_MODEL (number_list),
                                 (HdyComboRowGetNameFunc)hdy_value_object_dup_string,
                                 NULL,
                                 NULL);
  g_settings_bind_with_mapping (self->profile->settings,
                                "phone-number",
                                self->softphone_number,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_phone_number_get_mapping,
                                roger_phone_number_set_mapping,
                                self,
                                NULL);

  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (self->softphone_controller), RM_TYPE_CONTROLLER, controller_get_name, NULL, NULL);
  g_settings_bind (self->profile->settings, "phone-controller", self->softphone_controller, "selected-index", G_SETTINGS_BIND_DEFAULT);

  /* Fax */
  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->softfax_number),
                                 G_LIST_MODEL (number_list),
                                 (HdyComboRowGetNameFunc)hdy_value_object_dup_string,
                                 NULL,
                                 NULL);
  g_settings_bind_with_mapping (self->profile->settings,
                                "fax-number",
                                self->softfax_number,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_phone_number_get_mapping,
                                roger_phone_number_set_mapping,
                                self,
                                NULL);

  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (self->softfax_controller), RM_TYPE_CONTROLLER, controller_get_name, NULL, NULL);
  g_settings_bind (self->profile->settings, "fax-controller", self->softfax_controller, "selected-index", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->profile->settings, "fax-header", self->softfax_header, "text", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->profile->settings, "fax-ident", self->softfax_ident, "text", G_SETTINGS_BIND_DEFAULT);
  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (self->softfax_resolution), RM_TYPE_RESOLUTION, resolution_get_name, NULL, NULL);
  g_settings_bind (self->profile->settings, "fax-resolution", self->softfax_resolution, "selected-index", G_SETTINGS_BIND_DEFAULT);
  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (self->softfax_service), RM_TYPE_SERVICE, service_get_name, NULL, NULL);
  g_settings_bind (self->profile->settings, "fax-cip", self->softfax_service, "selected-index", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->profile->settings, "fax-report", self->softfax_report, "enable-expansion", G_SETTINGS_BIND_DEFAULT);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->softfax_directory), g_settings_get_string (self->profile->settings, "fax-report-dir"));
  g_signal_connect (self->softfax_directory, "file-set", G_CALLBACK (softfax_directory_file_set), self);
  g_settings_bind (self->profile->settings, "fax-ecm", self->softfax_ecm, "active", G_SETTINGS_BIND_DEFAULT);
}
