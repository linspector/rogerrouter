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

#include <gtk/gtk.h>
#include <rm/rm.h>

static GSList *
get_audio_devices (void)
{
  GSList *audio_plugins;
  RmAudio *audio;

  audio_plugins = rm_audio_get_plugins ();
  if (!audio_plugins)
    return NULL;

  /* FIXME: We are using the first one here as we only offer one plugin at the moment */
  audio = audio_plugins->data;

  return audio->get_devices ();
}

static gboolean
roger_audio_device_get_mapping (GValue   *value,
                                GVariant *variant,
                                gpointer  user_data)
{
  GSList *list;
  gint type = GPOINTER_TO_INT (user_data);
  guint idx = 0;

  for (list = get_audio_devices (); list; list = list->next) {
    RmAudioDevice *device = list->data;

    if (device->type != type)
      continue;

    if (g_strcmp0 (device->name, g_variant_get_string (variant, NULL)) == 0) {
      g_value_set_uint (value, idx);

      return TRUE;
    }

    idx++;
  }

  return TRUE;
}

static GVariant *
roger_audio_device_set_mapping (const GValue       *value,
                                const GVariantType *expected_type,
                                gpointer            user_data)
{
  GSList *list;
  gint type = GPOINTER_TO_INT (user_data);
  guint idx;

  idx = g_value_get_uint (value);

  for (list = get_audio_devices (); list; list = list->next) {
    RmAudioDevice *device = list->data;

    if (device->type != type)
      continue;

    if (idx == 0)
      return g_variant_new_string (device->name);

    idx--;
  }

  return g_variant_new_string ("");
}

void
roger_preferences_setup_audio (RogerPreferencesWindow *self)
{
  GSList *list;
  GtkStringList *microphone_list;
  GtkStringList *speaker_list;
  GtkStringList *ringer_list;

  g_settings_bind (self->profile->settings, "notification-play-ringtone", self->ringtone, "active", G_SETTINGS_BIND_DEFAULT);

  microphone_list = gtk_string_list_new (NULL);
  speaker_list = gtk_string_list_new (NULL);
  ringer_list = gtk_string_list_new (NULL);

  for (list = get_audio_devices (); list && list->data; list = list->next) {
    RmAudioDevice *device = list->data;

    if (device->type == RM_AUDIO_INPUT) {
      gtk_string_list_append (microphone_list, device->name);
    } else if (device->type == RM_AUDIO_OUTPUT) {
      gtk_string_list_append (speaker_list, device->name);
      gtk_string_list_append (ringer_list, device->name);
    }
  }

  adw_combo_row_set_model (ADW_COMBO_ROW (self->microphone),
                           G_LIST_MODEL (microphone_list));

  g_settings_bind_with_mapping (self->profile->settings,
                                "audio-input",
                                self->microphone,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_audio_device_get_mapping,
                                roger_audio_device_set_mapping,
                                GINT_TO_POINTER (RM_AUDIO_INPUT),
                                NULL);

  adw_combo_row_set_model (ADW_COMBO_ROW (self->speaker),
                           G_LIST_MODEL (speaker_list));

  g_settings_bind_with_mapping (self->profile->settings,
                                "audio-output",
                                self->speaker,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_audio_device_get_mapping,
                                roger_audio_device_set_mapping,
                                GINT_TO_POINTER (RM_AUDIO_OUTPUT),
                                NULL);

  adw_combo_row_set_model (ADW_COMBO_ROW (self->ringer),
                           G_LIST_MODEL (ringer_list));

  g_settings_bind_with_mapping (self->profile->settings,
                                "audio-output-ringtone",
                                self->ringer,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                roger_audio_device_get_mapping,
                                roger_audio_device_set_mapping,
                                GINT_TO_POINTER (RM_AUDIO_OUTPUT),
                                NULL);
}
