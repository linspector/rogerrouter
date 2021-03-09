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

#include "roger-debug.h"

#include "application.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <rm/rm.h>

struct _RogerDebug {
  HdyWindow parent_instance;

  GtkWidget *textview;
};

G_DEFINE_TYPE (RogerDebug, roger_debug, HDY_TYPE_WINDOW)

static void
roger_debug_log_handler (const gchar    *log_domain,
                         GLogLevelFlags  log_level,
                         const gchar    *message,
                         gpointer        user_data)
{
  RogerDebug *self = ROGER_DEBUG (user_data);
  GtkTextBuffer *text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->textview));
  GtkTextIter iter;
  g_autofree char *type = NULL;
  g_autofree char *tag = NULL;
  g_autoptr (GDateTime) datetime = NULL;
  g_autoptr (GString) output = NULL;
  g_autofree char *time = NULL;

  output = g_string_new ("");
  datetime = g_date_time_new_now_local ();
  time = g_date_time_format (datetime, "%H:%M:%S");
  g_string_append_printf (output, "(%s)", time);

  switch (log_level) {
    case G_LOG_LEVEL_CRITICAL:
      type = g_strdup (" CRITICAL: ");
      tag = g_strdup ("red_fg");
      break;
    case G_LOG_LEVEL_ERROR:
      type = g_strdup (" ERROR: ");
      tag = g_strdup ("red_fg");
      break;
    case G_LOG_LEVEL_WARNING:
      type = g_strdup (" WARNING: ");
      tag = g_strdup ("orange_fg");
      break;
    case G_LOG_LEVEL_DEBUG:
      type = g_strdup (" DEBUG: ");
      tag = NULL;
      break;
    case G_LOG_LEVEL_INFO:
      type = g_strdup (" INFO: ");
      tag = g_strdup ("blue_fg");
      break;
    default:
      type = g_strdup (" SYSTEM: ");
      tag = g_strdup ("blue_fg");
      break;
  }

  gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (text_buffer), &iter);
  gtk_text_buffer_insert_with_tags_by_name (text_buffer, &iter, output->str, output->len, tag, NULL);
  gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (text_buffer), &iter);
  gtk_text_buffer_insert_with_tags_by_name (text_buffer, &iter, type, -1, "bold", tag, NULL);
  gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (text_buffer), &iter);
  gtk_text_buffer_insert_with_tags_by_name (text_buffer, &iter, message, -1, "lmarg", tag, NULL);
  gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (text_buffer), &iter);
  gtk_text_buffer_insert_with_tags_by_name (text_buffer, &iter, "\n", -1, tag, NULL);

  /* Scroll to end */
  GtkTextMark *mark = gtk_text_buffer_get_insert (text_buffer);
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (self->textview), mark, 0.0, TRUE, 0.5, 1);
}

static gboolean
roger_debug_delete_event_cb (GtkWidget *widget,
                             GdkEvent  *event,
                             gpointer   user_data)
{
  g_settings_set_boolean (app_settings, "debug", FALSE);

  rm_log_set_app_handler (NULL);

  return FALSE;
}

static void
roger_debug_clear_clicked_cb (GtkWidget *widget,
                              gpointer   user_data)
{
  RogerDebug *self = ROGER_DEBUG (user_data);
  GtkTextBuffer *text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->textview));

  gtk_text_buffer_set_text (text_buffer, "", -1);
}

static void
roger_debug_save_clicked_cb (GtkWidget *widget,
                             gpointer   user_data)
{
  RogerDebug *self = ROGER_DEBUG (user_data);
  GtkTextBuffer *text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->textview));
  g_autoptr (GtkFileChooserNative) native = NULL;
  g_autofree char *time = NULL;
  g_autofree char *buf = NULL;
  g_autoptr (GDateTime) datetime = g_date_time_new_now_local ();
  GtkFileChooser *chooser;
  gint res;

  time = g_date_time_format (datetime, "%Y-%m-%d-%H-%M-%S");
  buf = g_strdup_printf ("roger-log-%s.txt", time);

  native = gtk_file_chooser_native_new ("Save Log", NULL, GTK_FILE_CHOOSER_ACTION_SAVE, _("Save"), _("Cancel"));
  chooser = GTK_FILE_CHOOSER (native);

  gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
  gtk_file_chooser_set_current_name (chooser, buf);

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));
  if (res == GTK_RESPONSE_ACCEPT) {
    GtkTextIter start, end;
    g_autofree char *filename = NULL;

    filename = gtk_file_chooser_get_filename (chooser);
    gtk_text_buffer_get_start_iter (text_buffer, &start);
    gtk_text_buffer_get_end_iter (text_buffer, &end);
    rm_file_save (filename, gtk_text_buffer_get_text (text_buffer, &start, &end, FALSE), -1);
  }
}

static void
roger_debug_dispose (GObject *object)
{
  g_log_set_default_handler (NULL, NULL);

  G_OBJECT_CLASS (roger_debug_parent_class)->dispose (object);
}

static void
roger_debug_class_init (RogerDebugClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = roger_debug_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/debug.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerDebug, textview);

  gtk_widget_class_bind_template_callback (widget_class, roger_debug_delete_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_debug_save_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_debug_clear_clicked_cb);
}

static void
roger_debug_init (RogerDebug *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Persist debug state and set app handler for logging */
  g_settings_set_boolean (app_settings, "debug", TRUE);
  g_log_set_default_handler (roger_debug_log_handler, self);
}

GtkWidget *
roger_debug_new (void)
{
  return g_object_new (ROGER_TYPE_DEBUG, NULL);
}

