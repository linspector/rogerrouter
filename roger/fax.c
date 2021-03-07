/**
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

#include "fax.h"

#include "contacts.h"
#include "contactsearch.h"
#include "journal.h"
#include "print.h"
#include "uitools.h"

#include <ctype.h>
#include <ghostscript/iapi.h>
#include <ghostscript/ierrors.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <rm/rm.h>

struct _RogerFax {
  HdyWindow parent_instance;

  GtkWidget *header_bar;
  GtkWidget *deck;
  GtkWidget *search_entry;
  GtkWidget *sender_label;
  GtkWidget *receiver_label;
  GtkWidget *progress_bar;
  GtkWidget *hangup_button;

  RmConnection *connection;
  RmFaxStatus status;
  char *file;
  gint status_timer_id;
};

G_DEFINE_TYPE (RogerFax, roger_fax, HDY_TYPE_WINDOW)

gboolean
roger_fax_status_timer_cb (gpointer user_data)
{
  RogerFax *self = ROGER_FAX (user_data);
  RmFaxStatus *fax_status = &self->status;
  RmFax *fax = rm_profile_get_fax (rm_profile_get_active ());
  g_autofree char *time_diff = NULL;
  char buffer[256];
  static gdouble old_percent = 0.0f;

  if (!rm_fax_get_status (fax, self->connection, fax_status))
    return TRUE;

  if (old_percent != fax_status->percentage) {
    old_percent = fax_status->percentage;

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), fax_status->percentage);
  }

  /* Update status information */
  switch (fax_status->phase) {
    case RM_FAX_PHASE_IDENTIFY:
      gtk_label_set_text (GTK_LABEL (self->receiver_label), fax_status->remote_ident);
      g_free (fax_status->remote_ident);

    /* Fall through */
    case RM_FAX_PHASE_SIGNALLING:
      snprintf (buffer, sizeof (buffer), _("Transferred %d of %d"), fax_status->pages_transferred, fax_status->pages_total);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->progress_bar), buffer);
      break;
    case RM_FAX_PHASE_RELEASE:
      if (!fax_status->error_code)
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->progress_bar), _("Fax transfer successful"));
      else
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->progress_bar), _("Fax transfer failed"));

      if (self->status_timer_id && g_settings_get_boolean (rm_profile_get_active ()->settings, "fax-report"))
        print_fax_report (fax_status, self->file, g_settings_get_string (rm_profile_get_active ()->settings, "fax-report-dir"));

      rm_fax_hangup (fax, self->connection);
      self->status_timer_id = 0;

      return G_SOURCE_REMOVE;
    case RM_FAX_PHASE_CALL:
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->progress_bar), _("Connecting…"));
      break;
    default:
      g_debug ("%s: Unhandled phase (%d)", __FUNCTION__, fax_status->phase);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->progress_bar), "");
      break;
  }

  time_diff = rm_connection_get_duration_time (self->connection);
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (self->header_bar), time_diff);

  return G_SOURCE_CONTINUE;
}

static void
roger_fax_start_status_timer (RogerFax *self)
{
  g_clear_handle_id (&self->status_timer_id, g_source_remove);
  self->status_timer_id = g_timeout_add (250, roger_fax_status_timer_cb, self);
}

static void
roger_fax_remove_status_timer (RogerFax *self)
{
  g_clear_handle_id (&self->status_timer_id, g_source_remove);
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (self->header_bar), "");

  gtk_widget_set_sensitive (self->hangup_button, FALSE);
}

static void
fax_connection_changed_cb (RmObject     *object,
                           gint          type,
                           RmConnection *connection,
                           gpointer      user_data)
{
  RogerFax *self = ROGER_FAX (user_data);

  g_assert (connection);
  g_assert (self);
  g_assert (self->connection);

  if (self->connection != connection)
    return;

  if (!(type & RM_CONNECTION_TYPE_DISCONNECT))
    return;

  roger_fax_remove_status_timer (self);
  self->connection = NULL;
}

gboolean
roger_fax_delete_event_cb (GtkWidget *window,
                           GdkEvent  *event,
                           gpointer   user_data)
{
  RogerFax *self = ROGER_FAX (window);

  if (self->file) {
    g_unlink (self->file);
    g_clear_pointer (&self->file, g_free);
  }

  return FALSE;
}

char *
convert_to_fax (const char *file_name)
{
  char *args[13];
  char *output;
  char *out_file;
  RmProfile *profile = rm_profile_get_active ();
  gint ret;
  gint ret1;
  void *minst = NULL;

  /* convert ps to fax */
  args[0] = "gs";
  args[1] = "-q";
  args[2] = "-dNOPAUSE";
  args[3] = "-dSAFER";
  args[4] = "-dBATCH";

  {
    char *ofile = g_strdup_printf ("%s.tif", g_path_get_basename (file_name));
    args[5] = "-sDEVICE=tiffg4";
    out_file = g_build_filename (rm_get_user_cache_dir (), ofile, NULL);
    g_free (ofile);
  }

  args[6] = "-dPDFFitPage";
  args[7] = "-dMaxStripSize=0";
  switch (g_settings_get_int (profile->settings, "fax-resolution")) {
    case 2:
      /* Super - fine */
      args[8] = "-r204x392";
      break;
    case 1:
      /* Fine */
      args[8] = "-r204x196";
      break;
    default:
      /* Standard */
      args[8] = "-r204x98";
      break;
  }
  output = g_strdup_printf ("-sOutputFile=%s", out_file);
  args[9] = output;
  args[10] = "-f";
  args[11] = (char *)file_name;
  args[12] = NULL;

  ret = gsapi_new_instance (&minst, NULL);
  if (ret < 0) {
    return NULL;
  }
  ret = gsapi_set_arg_encoding (minst, GS_ARG_ENCODING_UTF8);
  ret = gsapi_init_with_args (minst, 12, args);

  ret1 = gsapi_exit (minst);
  if ((ret == 0) || (ret == gs_error_Quit))
    ret = ret1;

  gsapi_delete_instance (minst);

  if (!g_file_test (out_file, G_FILE_TEST_EXISTS)) {
    g_warning ("%s(): Error converting print file to FAX format!", __FUNCTION__);
    g_free (out_file);

    return NULL;
  }

  return out_file;
}

void
roger_fax_set_transfer_file (RogerFax   *self,
                             const char *file)
{
  self->file = convert_to_fax (file);
}

static void
roger_fax_dial_button_clicked_cb (GtkWidget *button,
                                  gpointer   user_data)
{
  RogerFax *self = ROGER_FAX (user_data);
  RmProfile *profile = rm_profile_get_active ();
  const char *number;

  g_assert (!self->connection);

  number = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  if (RM_EMPTY_STRING (number))
    return;

  self->connection = rm_fax_send (rm_profile_get_fax (profile), self->file, number, rm_router_get_suppress_state (profile));
  if (self->connection) {
    hdy_deck_set_visible_child_name (HDY_DECK (self->deck), "transfer");
    roger_fax_start_status_timer (self);
  }
}

static void
roger_fax_hangup_button_clicked_cb (GtkWidget *button,
                                    gpointer   user_data)
{
  RogerFax *self = ROGER_FAX (user_data);
  RmProfile *profile = rm_profile_get_active ();

  g_assert (self->connection);

  rm_fax_hangup (rm_profile_get_fax (profile), self->connection);

  gtk_widget_set_sensitive (button, FALSE);
}

static void
roger_fax_number_button_clicked_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  RogerFax *self = ROGER_FAX (user_data);
  const char *name = gtk_widget_get_name (widget);
  gint num = name[7];

  const char *text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  g_autofree char *tmp = g_strdup_printf ("%s%c", text, num);

  gtk_entry_set_text (GTK_ENTRY (self->search_entry), tmp);
}

static void
roger_fax_clear_button_clicked_cb (GtkWidget *widget,
                                   gpointer   user_data)
{
  RogerFax *self = ROGER_FAX (user_data);
  const char *text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  if (!RM_EMPTY_STRING (text)) {
    g_autofree char *new = g_strdup (text);

    new[strlen (text) - 1] = '\0';
    gtk_entry_set_text (GTK_ENTRY (self->search_entry), new);
  }
}

static void
roger_fax_dispose (GObject *object)
{
  RogerFax *self = ROGER_FAX (object);

  roger_fax_remove_status_timer (self);

  G_OBJECT_CLASS (roger_fax_parent_class)->dispose (object);
}

static void
roger_fax_class_init (RogerFaxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = roger_fax_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tabos/roger/ui/fax.ui");

  gtk_widget_class_bind_template_child (widget_class, RogerFax, header_bar);
  gtk_widget_class_bind_template_child (widget_class, RogerFax, deck);
  gtk_widget_class_bind_template_child (widget_class, RogerFax, search_entry);
  gtk_widget_class_bind_template_child (widget_class, RogerFax, sender_label);
  gtk_widget_class_bind_template_child (widget_class, RogerFax, receiver_label);
  gtk_widget_class_bind_template_child (widget_class, RogerFax, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, RogerFax, hangup_button);

  gtk_widget_class_bind_template_callback (widget_class, roger_fax_number_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_fax_dial_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_fax_hangup_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_fax_clear_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, roger_fax_delete_event_cb);
}

static void
roger_fax_set_suppression (GSimpleAction *action,
                           GVariant      *value,
                           gpointer       user_data)
{
  g_simple_action_set_state (action, value);
}

static const GActionEntry fax_actions [] = {
  {"set-suppression", NULL, NULL, "false", roger_fax_set_suppression},
};

static void
roger_fax_init (RogerFax *self)
{
  GSimpleActionGroup *simple_action_group;
  RmProfile *profile = rm_profile_get_active ();

  gtk_widget_init_template (GTK_WIDGET (self));

  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   fax_actions,
                                   G_N_ELEMENTS (fax_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "fax",
                                  G_ACTION_GROUP (simple_action_group));

  contact_search_completion_add (self->search_entry);

  gtk_label_set_text (GTK_LABEL (self->sender_label), g_settings_get_string (profile->settings, "fax-header"));

  g_signal_connect_object (rm_object, "connection-changed", G_CALLBACK (fax_connection_changed_cb), self, 0);
}

GtkWidget *
roger_fax_new (void)
{
  return g_object_new (ROGER_TYPE_FAX, NULL);
}
