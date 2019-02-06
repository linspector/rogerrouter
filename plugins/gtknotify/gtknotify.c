/*
 * Roger Router
 * Copyright (c) 2012-2017 Jan-Michael Brummer
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
#include <roger/uitools.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static guint uid = 0;
static GHashTable *hash_table = NULL;

typedef struct {
  RmConnection *connection;
  RmContact *contact;
  guint uid;
} GtkNotifyData;

/**
 * gtknotify_close:
 * @priv: pointer to notification data
 *
 * Close notification window
 */
void gtknotify_close(gpointer priv)
{
  GtkNotifyData *data = priv;
  GtkWidget *window = g_hash_table_lookup (hash_table, data);

  if (window != NULL) {
	  gtk_widget_destroy (window);
    g_hash_table_remove (hash_table, data);
  }
}

/**
 * gtknotify_timeout_close_cb:
 * @user_data: gtk notifcation data pointer
 *
 * Closes given window identified by @uid
 *
 * Returns: %G_SOURCE_REMOVE
 */
static gboolean gtknotify_timeout_close_cb(gpointer user_data)
{
  GtkNotifyData *data = user_data;

	gtknotify_close(data);

	return G_SOURCE_REMOVE;
}

static gboolean gtknotify_show_idle (gpointer user_data)
{
  GtkNotifyData *data = (GtkNotifyData *) user_data;
  RmConnection *connection = data->connection;
  RmContact *contact = data->contact;
	GtkWidget *window;
	GtkWidget *headerbar;
	GtkWidget *contact_company_label;
	GtkWidget *contact_number_label;
	GtkWidget *contact_name_label;
	GtkWidget *contact_street_label;
	GtkWidget *contact_city_label;
	GtkWidget *image;
	g_autofree gchar *tmp = NULL;
	GtkBuilder *builder;
  GtkApplication *app = GTK_APPLICATION(g_application_get_default());

	builder = gtk_builder_new_from_resource("/org/tabos/roger/plugins/gtknotify/gtknotify.glade");
	if (!builder) {
		g_warning("Could not load gtknotify ui");
    g_free (data);
		return G_SOURCE_REMOVE;
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));

	headerbar = GTK_WIDGET(gtk_builder_get_object(builder, "headerbar"));
	contact_name_label = GTK_WIDGET(gtk_builder_get_object(builder, "name_label"));
	contact_number_label = GTK_WIDGET(gtk_builder_get_object(builder, "number_label"));
	contact_company_label = GTK_WIDGET(gtk_builder_get_object(builder, "company_label"));
	contact_street_label = GTK_WIDGET(gtk_builder_get_object(builder, "street_label"));
	contact_city_label = GTK_WIDGET(gtk_builder_get_object(builder, "city_label"));
	image = GTK_WIDGET(gtk_builder_get_object(builder, "image"));

	tmp = connection->local_number ? g_strdup_printf(_("(on %s)"), connection->local_number) : g_strdup(_("(on ?)"));
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(headerbar), tmp);

	gtk_label_set_text(GTK_LABEL(contact_name_label), contact->name ? contact->name : "");
	gtk_label_set_text(GTK_LABEL(contact_number_label), contact->number ? contact->number : "");
	gtk_label_set_text(GTK_LABEL(contact_company_label), contact->company ? contact->company : "");
	gtk_label_set_text(GTK_LABEL(contact_street_label), contact->street ? contact->street : "");
	tmp = g_strdup_printf("%s%s%s", contact->zip ? contact->zip : "", contact->zip ? " " : "", contact->city ? contact->city : "");
	gtk_label_set_text(GTK_LABEL(contact_city_label), tmp);

	if (contact->image) {
		GdkPixbuf *buf = rm_image_scale(contact->image, 96);
		gtk_image_set_from_pixbuf(GTK_IMAGE(image), buf);
	}

	if (connection->type & RM_CONNECTION_TYPE_INCOMING) {
		gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), _("Incoming call"));

		if (connection->type & RM_CONNECTION_TYPE_SOFTPHONE) {
			GtkWidget *accept_button;
			GtkWidget *decline_button;
			GtkWidget *separator;

			separator = GTK_WIDGET(gtk_builder_get_object(builder, "separator"));
			gtk_widget_set_visible(separator, TRUE);

			accept_button = GTK_WIDGET(gtk_builder_get_object(builder, "pickup_button"));
			gtk_actionable_set_action_name(GTK_ACTIONABLE(accept_button), "app.pickup");
			gtk_actionable_set_action_target(GTK_ACTIONABLE(accept_button), "i", connection->id);
			gtk_widget_set_visible(accept_button, TRUE);

			decline_button = GTK_WIDGET(gtk_builder_get_object(builder, "hangup_button"));
			gtk_actionable_set_action_name(GTK_ACTIONABLE(decline_button), "app.hangup");
			gtk_actionable_set_action_target(GTK_ACTIONABLE(decline_button), "i", connection->id);
			gtk_widget_set_visible(decline_button, TRUE);
		}
	} else if (connection->type & RM_CONNECTION_TYPE_OUTGOING) {
		gint duration = 5;

		gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), _("Outgoing call"));

		g_timeout_add_seconds(duration, gtknotify_timeout_close_cb, data);
	}

	gtk_window_set_gravity (GTK_WINDOW(window), GDK_GRAVITY_SOUTH_EAST);
	gtk_window_move(GTK_WINDOW(window), gdk_screen_width(), gdk_screen_height());
	gtk_widget_show(window);

  gtk_application_add_window (app, GTK_WINDOW (window));

  g_hash_table_insert (hash_table, data, window);

  return G_SOURCE_REMOVE;
}

/**
 * gtknotify_show:
 * @connection: a #RmConnection
 * @contact: a #RmContact
 *
 * Shows a native gtk window with call details of @connection and @contact
 * Returns: %NULL
 */
gpointer gtknotify_show(RmConnection *connection, RmContact *contact)
{
  GtkNotifyData *data = g_new (GtkNotifyData, 1);

  data->connection = connection;
  data->contact = contact;
  data->uid = ++uid;

  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, gtknotify_show_idle, data, NULL);

  return data;
}

/**
 * gtknotify_update:
 * @connection: a #RmConnection
 * @contact: a #RmContact
 *
 * Updates notification (currently does nothing)
 */
void gtknotify_update(RmConnection *connection, RmContact *contact)
{
}

/** RmNotification declaration */
RmNotification gtknotify = {
	"GTK Notify",
	gtknotify_show,
	gtknotify_update,
	gtknotify_close,
};

/**
 * gtknotify_plugin_init:
 * @plugin: a #RmPlugin
 *
 * Activate plugin
 *
 * Returns: %TRUE
 */
gboolean gtknotify_plugin_init(RmPlugin *plugin)
{
	rm_notification_register(&gtknotify);
  hash_table = g_hash_table_new (NULL, NULL);
	return TRUE;
}

/**
 * gtknotify_plugin_shutdown:
 * @plugin: a #RmPlugin
 *
 * Deactivate plugin
 *
 * Returns: %TRUE
 */
gboolean gtknotify_plugin_shutdown(RmPlugin *plugin)
{
  g_hash_table_unref (hash_table);
	rm_notification_unregister(&gtknotify);

	return TRUE;
}

G_GNUC_END_IGNORE_DEPRECATIONS

RM_PLUGIN(gtknotify)
