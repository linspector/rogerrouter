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

#include "about.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define ABOUT_GROUP "About"

void
app_show_about (GtkWidget *window)
{
  GtkWidget *dialog = NULL;
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


  if (window) {
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  }

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}
