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

#include "roger-voice-mail.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <rm/rm.h>

struct _RogerVoiceMail {
  AdwWindow parent_instance;

  GtkWidget *play_button;
  GtkWidget *scale;
  GtkWidget *time_label;

  gpointer vox_data;
  gint fraction;
  gint update_id;
  gboolean playing;
};

G_DEFINE_TYPE (RogerVoiceMail, roger_voice_mail, ADW_TYPE_WINDOW)

static gboolean
vox_update_ui (gpointer user_data)
{
  RogerVoiceMail *self = ROGER_VOICE_MAIL (user_data);
  gint fraction;

  /* Get current fraction (position) */
  fraction = rm_vox_get_fraction (self->vox_data);

  if (self->fraction != fraction) {
    g_autofree char *tmp = NULL;
    gfloat seconds = rm_vox_get_seconds (self->vox_data);

    self->fraction = fraction;

    gtk_range_set_value (GTK_RANGE (self->scale), (float)self->fraction / (float)100);
    tmp = g_strdup_printf ("%2.2d:%2.2d:%2.2d", (gint)seconds / 3600, (gint)seconds / 60, (gint)seconds % 60);
    gtk_label_set_text (GTK_LABEL (self->time_label), tmp);

    if (self->fraction == 100) {
      gtk_button_set_icon_name (GTK_BUTTON (self->play_button), "media-playback-start-symbolic");
      gtk_widget_set_sensitive (self->scale, FALSE);
      self->playing = FALSE;

      self->update_id = 0;

      return G_SOURCE_REMOVE;
    }
  }

  return G_SOURCE_CONTINUE;
}

static void
roger_voice_mail_start_playback (RogerVoiceMail *self)
{
  /* Reset scale range */
  gtk_range_set_value (GTK_RANGE (self->scale), 0.0f);

  /* Start playback */
  self->fraction = 0;
  rm_vox_play (self->vox_data);
  self->playing = TRUE;

  /* Change button image */
  gtk_button_set_icon_name (GTK_BUTTON (self->play_button), "media-playback-pause-symbolic");
  gtk_widget_set_sensitive (self->scale, TRUE);

  /* Timer which will update the ui every 250ms */
  self->update_id = g_timeout_add (250, vox_update_ui, self);
}

static void
roger_voice_mail_play_button_clicked (GtkWidget *button,
                                      gpointer   user_data)
{
  RogerVoiceMail *self = ROGER_VOICE_MAIL (user_data);

  if (self->fraction != 100) {
    rm_vox_set_pause (self->vox_data, self->playing);
    self->playing = !self->playing;

    if (self->playing)
      gtk_button_set_icon_name (GTK_BUTTON (self->play_button), "media-playback-pause-symbolic");
    else
      gtk_button_set_icon_name (GTK_BUTTON (self->play_button), "media-playback-start-symbolic");
  } else {
    roger_voice_mail_start_playback (self);
  }
}

static gboolean
roger_voice_mail_scale_change_value_cb (GtkRange      *range,
                                        GtkScrollType  scroll,
                                        gdouble        value,
                                        gpointer       user_data)
{
  RogerVoiceMail *self = ROGER_VOICE_MAIL (user_data);

  rm_vox_seek (self->vox_data, gtk_range_get_value (range));

  return FALSE;
}

void
roger_voice_mail_play (RogerVoiceMail *self,
                       GBytes         *voice_mail)
{
  g_autoptr (GError) error = NULL;

  if (self->vox_data) {
    g_clear_handle_id (&self->update_id, g_source_remove);
    rm_vox_shutdown (self->vox_data);
    self->vox_data = NULL;
  }

  self->vox_data = rm_vox_init (g_bytes_get_data (voice_mail, NULL), g_bytes_get_size (voice_mail), &error);
  if (!self->vox_data) {
    g_warning ("%s: Could not init rm vox!", __FUNCTION__);
    return;
  }

  roger_voice_mail_start_playback (self);
}

static void
roger_voice_mail_finalize (GObject *object)
{
  RogerVoiceMail *self = ROGER_VOICE_MAIL (object);

  g_clear_handle_id (&self->update_id, g_source_remove);
  rm_vox_shutdown (self->vox_data);
  self->vox_data = NULL;

  G_OBJECT_CLASS (roger_voice_mail_parent_class)->finalize (object);
}

static void
roger_voice_mail_class_init (RogerVoiceMailClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = roger_voice_mail_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/voice-mail.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerVoiceMail, play_button);
  gtk_widget_class_bind_template_child (widget_class, RogerVoiceMail, time_label);
  gtk_widget_class_bind_template_child (widget_class, RogerVoiceMail, scale);

  gtk_widget_class_bind_template_callback (widget_class, roger_voice_mail_play_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, roger_voice_mail_scale_change_value_cb);
}

static void
roger_voice_mail_init (RogerVoiceMail *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
roger_voice_mail_new (void)
{
  return g_object_new (ROGER_TYPE_VOICE_MAIL, NULL);
}
