/*
 * Roger Router Copyright (c) 2012-2017 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <rm/rm.h>

void
gtknotify_close (gpointer priv)
{
  gtk_widget_destroy (priv);
}

static gboolean
gtknotify_timeout_close_cb (gpointer window)
{
  gtknotify_close (window);

  return G_SOURCE_REMOVE;
}

static
void
on_pickup_clicked (GtkWidget *button,
                   gpointer   user_data)
{
  GApplication *app = g_application_get_default ();
  GAction *pickup_action = g_action_map_lookup_action (G_ACTION_MAP (app), "pickup");
  int id = GPOINTER_TO_INT (user_data);

  g_action_activate (pickup_action, g_variant_new_int32 (id));
}

static
void
on_hangup_clicked (GtkWidget *button,
                   gpointer   user_data)
{
  GApplication *app = g_application_get_default ();
  GAction *hangup_action = g_action_map_lookup_action (G_ACTION_MAP (app), "hangup");
  int id = GPOINTER_TO_INT (user_data);

  g_action_activate (hangup_action, g_variant_new_int32 (id));
}
static gpointer
gtknotify_show (RmConnection *connection,
                RmContact    *contact)
{
  GtkWidget *window;
  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkWidget *contact_company_label;
  GtkWidget *contact_number_label;
  GtkWidget *contact_name_label;
  GtkWidget *contact_street_label;
  GtkWidget *contact_city_label;
  GtkWidget *image;
  g_autofree char *tmp = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new_from_resource ("/org/tabos/roger/plugins/gtknotify/gtknotify.glade");
  if (!builder) {
    g_warning ("Could not load gtknotify ui");
    return NULL;
  }

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));

  title_label = GTK_WIDGET (gtk_builder_get_object (builder, "title_label"));
  subtitle_label = GTK_WIDGET (gtk_builder_get_object (builder, "subtitle_label"));
  contact_name_label = GTK_WIDGET (gtk_builder_get_object (builder, "name_label"));
  contact_number_label = GTK_WIDGET (gtk_builder_get_object (builder, "number_label"));
  contact_company_label = GTK_WIDGET (gtk_builder_get_object (builder, "company_label"));
  contact_street_label = GTK_WIDGET (gtk_builder_get_object (builder, "street_label"));
  contact_city_label = GTK_WIDGET (gtk_builder_get_object (builder, "city_label"));
  image = GTK_WIDGET (gtk_builder_get_object (builder, "image"));

  tmp = connection->local_number ? g_strdup_printf (_("(on %s)"), connection->local_number) : g_strdup (_("(on ?)"));
  gtk_label_set_text (GTK_LABEL (subtitle_label), tmp);
  g_free (tmp);

  gtk_label_set_text (GTK_LABEL (contact_name_label), contact->name ? contact->name : "");
  gtk_label_set_text (GTK_LABEL (contact_number_label), contact->number ? contact->number : "");
  gtk_label_set_text (GTK_LABEL (contact_company_label), contact->company ? contact->company : "");
  gtk_label_set_text (GTK_LABEL (contact_street_label), contact->street ? contact->street : "");

  tmp = g_strdup_printf ("%s%s%s", contact->zip ? contact->zip : "", contact->zip ? " " : "", contact->city ? contact->city : "");
  gtk_label_set_text (GTK_LABEL (contact_city_label), tmp);
  g_free (tmp);

  if (contact->image) {
    GdkPixbuf *buf = rm_image_scale (contact->image, 96);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), buf);
  }

  if (connection->type & RM_CONNECTION_TYPE_INCOMING) {
    gtk_label_set_text (GTK_LABEL (title_label), _("Incoming call"));

    if (connection->type & RM_CONNECTION_TYPE_SOFTPHONE) {
      GtkWidget *accept_button;
      GtkWidget *decline_button;
      GtkWidget *separator;
      GApplication *app = g_application_get_default ();
      GAction *hangup_action = g_action_map_lookup_action (G_ACTION_MAP (app), "hangup");

      separator = GTK_WIDGET (gtk_builder_get_object (builder, "separator"));
      gtk_widget_set_visible (separator, TRUE);

      accept_button = GTK_WIDGET (gtk_builder_get_object (builder, "pickup_button"));
      g_signal_connect (accept_button, "clicked", G_CALLBACK (on_pickup_clicked), GINT_TO_POINTER (connection->id));
      gtk_widget_set_visible (accept_button, TRUE);

      decline_button = GTK_WIDGET (gtk_builder_get_object (builder, "hangup_button"));
      g_signal_connect (decline_button, "clicked", G_CALLBACK (on_hangup_clicked), GINT_TO_POINTER (connection->id));
      gtk_widget_set_visible (decline_button, TRUE);
    }
  } else if (connection->type & RM_CONNECTION_TYPE_OUTGOING) {
    gint duration = 5;

    gtk_label_set_text (GTK_LABEL (title_label), _("Outgoing call"));

    g_timeout_add_seconds (duration, gtknotify_timeout_close_cb, window);
  }

  gtk_window_set_gravity (GTK_WINDOW (window), GDK_GRAVITY_SOUTH_EAST);
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  gtk_window_stick (GTK_WINDOW (window));
  gtk_window_move (GTK_WINDOW (window), gdk_screen_width (), gdk_screen_height ());
  gtk_window_present_with_time (GTK_WINDOW (window), GDK_CURRENT_TIME);

  return window;
}

static void
gtknotify_update (RmConnection *connection,
                  RmContact    *contact)
{
}

RmNotification gtknotify = {
  "GTK Notify",
  gtknotify_show,
  gtknotify_update,
  gtknotify_close,
};

gboolean
gtknotify_plugin_init (RmPlugin *plugin)
{
  rm_notification_register (&gtknotify);
  return TRUE;
}

gboolean
gtknotify_plugin_shutdown (RmPlugin *plugin)
{
  rm_notification_unregister (&gtknotify);
  return TRUE;
}

RM_PLUGIN (gtknotify)
